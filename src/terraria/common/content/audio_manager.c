/*******************************************************************************
 * tefkernel - audio_manager
 * Copyright (C) 2026 eternalfuture-e38299
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2026/10/29
 *******************************************************************************/

#include "miniaudio.h"

#include "patchlib/type.h"
#include "patchlib/field.h"
#include "patchlib/property.h"
#include "patchlib/method.h"
#include "terraria/content/audio_manager.h"
#include "internal/platform_threads.h"
#include "internal/log.h"
#include "tefstd/vector.h"

#include <math.h>
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>

#define TEF_AUDIO_PERIOD_MS 10

/* voice 状态 */
enum {
    VOICE_FREE = 0,
    VOICE_ACTIVE = 1,
    VOICE_STOPPING = 2
};

typedef struct {
    _Atomic int               state;
    _Atomic int               ready;   /* 字段填好后才置 1，回调只在 ready 时读取 */
    terraria_audio_t         *audio;
    _Atomic double           current_pos;
    _Atomic int               paused;
    _Atomic int               stop_requested;
    _Atomic float             volume;
    _Atomic float             pitch;
} tef_voice_t;

/* ---------------------------------------------------------------------------
 * 全局状态
 * ------------------------------------------------------------------------- */

static ma_device     g_device;
static bool          g_device_ready = false;
static _Atomic float g_master_volume = 1.0f;

/* 已登记音源：tefstd_vector_t<terraria_audio_t*> */
static tefstd_vector_t g_sources;
static bool            g_sources_ready = false;
static mtx_t           g_sources_mtx;
static bool            g_mtx_ready = false;

/* voice 表：固定槽位数组，voice_id 即下标。
 * 回调线程无锁遍历，因此数组地址/长度必须恒定：不能扩容 realloc。 */
#define TEF_AUDIO_MAX_VOICES 48
static tef_voice_t g_voices[TEF_AUDIO_MAX_VOICES];
static bool        g_voices_ready = false;

/* ---------------------------------------------------------------------------
 * 格式辅助
 * ------------------------------------------------------------------------- */

static ma_format tef_to_ma_format(const int fmt) {
    switch (fmt) {
        case 1: return ma_format_u8;
        case 2: return ma_format_s16;
        case 3: return ma_format_s24;
        case 4: return ma_format_s32;
        case 5: return ma_format_f32;
        default: return ma_format_unknown;
    }
}

static unsigned int tef_bytes_per_sample(const int fmt) {
    switch (fmt) {
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        case 4:
        case 5: return 4;
        default: return 0;
    }
}

static float tef_read_sample_f32(const void *pcm, const int fmt,
                                        const unsigned long long frame, const int channel,
                                        const int channels) {
    const unsigned char *base = pcm;
    unsigned int bps = tef_bytes_per_sample(fmt);
    unsigned long long off =
        (frame * (unsigned long long)channels + (unsigned long long)channel) * bps;
    switch (fmt) {
        case 2: { short v; memcpy(&v, base + off, 2); return (float)v / 32768.0f; }
        case 4: { int v; memcpy(&v, base + off, 4); return (float)v / 2147483648.0f; }
        case 5: { float v; memcpy(&v, base + off, 4); return v; }
        case 1: { unsigned char v = base[off]; return ((float)v - 128.0f) / 128.0f; }
        case 3: {
            int v = (int)base[off] | ((int)base[off + 1] << 8) | ((int)base[off + 2] << 16);
            if (v & 0x800000) v |= ~0xFFFFFF;
            return (float)v / 8388608.0f;
        }
        default: return 0.0f;
    }
}

static float tef_clampf(const float v, const float lo, const float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ---------------------------------------------------------------------------
 * 音频回调
 * ------------------------------------------------------------------------- */

static void tef_audio_callback(ma_device *dev, void *out, const void *in,
                               const ma_uint32 frame_count) {
    (void)in;
    float *dst = out;
    const unsigned int out_channels = dev->playback.channels;
    const float master = atomic_load_explicit(&g_master_volume, memory_order_relaxed);

    memset(dst, 0, (size_t)frame_count * out_channels * sizeof(float));

    if (!g_voices_ready) return;

    for (size_t i = 0; i < TEF_AUDIO_MAX_VOICES; ++i) {
        tef_voice_t *v = &g_voices[i];
        const int state = atomic_load_explicit(&v->state, memory_order_acquire);
        if (state == VOICE_FREE) continue;
        if (state == VOICE_STOPPING) {
            atomic_store_explicit(&v->state, VOICE_FREE, memory_order_release);
            continue;
        }
        if (!atomic_load_explicit(&v->ready, memory_order_acquire)) continue;
        if (atomic_load_explicit(&v->stop_requested, memory_order_relaxed) ||
            atomic_load_explicit(&v->paused, memory_order_relaxed)) {
            continue;
        }

        terraria_audio_t *a = v->audio;
        if (!a || !a->pcm_data || a->pcm_frames == 0) {
            atomic_store_explicit(&v->state, VOICE_STOPPING, memory_order_release);
            continue;
        }

        const int in_fmt = a->format;
        const int in_ch = a->channels;
        const unsigned long long total = a->pcm_frames;
        const float vol = atomic_load_explicit(&v->volume, memory_order_relaxed) * master;
        float pitch = atomic_load_explicit(&v->pitch, memory_order_relaxed);
        if (pitch <= 0.0f) pitch = 1.0f;

        /* 重采样：源采样率与设备采样率不一致时按比例推进，
         * 否则 44.1k 的素材在 48k 设备上会整体快 8.8%、音调偏高。
         * 位置用 double 保存，pitch 为小数时也能平滑推进（整数截断会卡死/无效果）。 */
        const double rate_scale =
            (dev->sampleRate > 0) ? (double) a->sample_rate / (double) dev->sampleRate : 1.0;
        const double step = (double) pitch * rate_scale;

        double pos = atomic_load_explicit(&v->current_pos, memory_order_relaxed);
        if (pos < 0.0) pos = 0.0;

        for (ma_uint32 f = 0; f < frame_count; ++f) {
            if (pos >= (double) total) {
                if (a->looping) {
                    pos = total > 0 ? fmod(pos, (double) total) : 0.0;
                } else {
                    atomic_store_explicit(&v->state, VOICE_STOPPING, memory_order_release);
                    break;
                }
            }
            const unsigned long long p0 = (unsigned long long) pos;
            const unsigned long long p1 = p0 + 1 < total ? p0 + 1 : (a->looping ? 0 : p0);
            const float frac = (float) (pos - (double) p0);

            for (unsigned int c = 0; c < out_channels; ++c) {
                const int src_c = c < (unsigned int)in_ch ? (int)c : (in_ch - 1);
                const float s0 = tef_read_sample_f32(a->pcm_data, in_fmt, p0, src_c, in_ch);
                const float s1 = tef_read_sample_f32(a->pcm_data, in_fmt, p1, src_c, in_ch);
                const float s = s0 + (s1 - s0) * frac;
                dst[(size_t)f * out_channels + c] += s * vol;
            }
            pos += step;
        }

        if (atomic_load_explicit(&v->state, memory_order_relaxed) == VOICE_ACTIVE) {
            atomic_store_explicit(&v->current_pos, pos, memory_order_relaxed);
        }
    }

    const size_t total_out = (size_t)frame_count * out_channels;
    for (size_t k = 0; k < total_out; ++k) {
        const float x = dst[k];
        dst[k] = tef_clampf(x, -1.0f, 1.0f);
    }
}

/* ---------------------------------------------------------------------------
 * 设备
 * ------------------------------------------------------------------------- */

static bool tef_audio_ensure_device(void) {
    if (g_device_ready) return true;

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format   = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.sampleRate        = 0;
    cfg.dataCallback      = tef_audio_callback;
    cfg.pUserData         = NULL;
    cfg.periodSizeInMilliseconds = TEF_AUDIO_PERIOD_MS;
    cfg.periods                  = 2;
    cfg.performanceProfile       = ma_performance_profile_low_latency;

    if (ma_device_init(NULL, &cfg, &g_device) != MA_SUCCESS) return false;
    if (ma_device_start(&g_device) != MA_SUCCESS) {
        ma_device_uninit(&g_device);
        return false;
    }
    g_device_ready = true;
    return true;
}

/* ---------------------------------------------------------------------------
 * 内部辅助
 * ------------------------------------------------------------------------- */

static bool tef_ensure_tables(void) {
    if (!g_mtx_ready) {
        if (mtx_init(&g_sources_mtx, mtx_plain) != thrd_success) return false;
        g_mtx_ready = true;
    }
    if (!g_sources_ready) {
        if (!tefstd_vector_init(&g_sources, sizeof(terraria_audio_t *))) return false;
        g_sources_ready = true;
    }
    if (!g_voices_ready) {
        for (size_t i = 0; i < TEF_AUDIO_MAX_VOICES; ++i) {
            atomic_init(&g_voices[i].state, VOICE_FREE);
            atomic_init(&g_voices[i].ready, 0);
            atomic_init(&g_voices[i].current_pos, 0.0);
            atomic_init(&g_voices[i].paused, 0);
            atomic_init(&g_voices[i].stop_requested, 0);
            atomic_init(&g_voices[i].volume, 1.0f);
            atomic_init(&g_voices[i].pitch, 1.0f);
            g_voices[i].audio = NULL;
        }
        g_voices_ready = true;
    }
    return true;
}

static int tef_find_source(const terraria_audio_t *audio) {
    const size_t n = tefstd_vector_size(&g_sources);
    for (size_t i = 0; i < n; ++i) {
        terraria_audio_t **p = tefstd_vector_at(&g_sources, i);
        if (p && *p == audio) return (int)i;
    }
    return -1;
}

/* 在固定槽位表中分配一个空闲 voice；满了返回 -1（丢弃本次播放，
 * 避免无限增长导致 CPU/内存爆掉）。 */
static int tef_alloc_voice_slot(void) {
    for (size_t i = 0; i < TEF_AUDIO_MAX_VOICES; ++i) {
        int expected = VOICE_FREE;
        if (atomic_compare_exchange_strong_explicit(
                &g_voices[i].state, &expected, VOICE_ACTIVE,
                memory_order_acq_rel, memory_order_relaxed)) {
            atomic_store_explicit(&g_voices[i].ready, 0, memory_order_relaxed);
            return (int)i;
        }
    }
    return -1;
}

/* ---------------------------------------------------------------------------
 * 公开 API
 * ------------------------------------------------------------------------- */

bool terraria_audio_manager_register(terraria_audio_t *audio) {
    if (!audio || !audio->pcm_data || audio->pcm_frames == 0) return false;
    if (audio->sample_rate <= 0 || audio->channels <= 0) return false;
    if (tef_bytes_per_sample(audio->format) == 0) return false;
    if (audio->volume <= 0.0f) audio->volume = 1.0f;
    if (!tef_ensure_tables()) return false;

    mtx_lock(&g_sources_mtx);
    if (tef_find_source(audio) < 0) {
        if (!tefstd_vector_push_back(&g_sources, &audio)) {
            mtx_unlock(&g_sources_mtx);
            return false;
        }
    }
    mtx_unlock(&g_sources_mtx);
    return true;
}

bool terraria_audio_manager_unregister(terraria_audio_t *audio) {
    if (!audio || !g_sources_ready) return false;

    terraria_audio_manager_stop_all(audio);

    mtx_lock(&g_sources_mtx);
    const int idx = tef_find_source(audio);
    if (idx < 0) {
        mtx_unlock(&g_sources_mtx);
        return false;
    }
    tefstd_vector_erase(&g_sources, (size_t)idx, NULL);
    mtx_unlock(&g_sources_mtx);
    return true;
}

int terraria_audio_manager_play(terraria_audio_t *audio, float volume, float pitch) {
    if (!audio) return -1;
    if (volume < 0.0f) volume = audio->volume;
    if (pitch <= 0.0f) pitch = 1.0f;
    if (!tef_ensure_tables()) return -1;
    if (!tef_audio_ensure_device()) return -1;

    mtx_lock(&g_sources_mtx);
    int id = tef_alloc_voice_slot();
    if (id >= 0) {
        tef_voice_t *v = &g_voices[id];
        v->audio = audio;
        atomic_store_explicit(&v->current_pos, 0.0, memory_order_relaxed);
        atomic_store_explicit(&v->paused, 0, memory_order_relaxed);
        atomic_store_explicit(&v->stop_requested, 0, memory_order_relaxed);
        atomic_store_explicit(&v->volume, volume, memory_order_relaxed);
        atomic_store_explicit(&v->pitch, pitch, memory_order_relaxed);
        atomic_store_explicit(&v->ready, 1, memory_order_release);
    }
    mtx_unlock(&g_sources_mtx);
    return id;
}

/* ---------------------------------------------------------------------------
 * 位置衰减（与原版 LegacySoundPlayer.PlaySound 一致）
 *
 * 原版在 LegacySoundPlayer.PlaySound 内部按 x/y 与 Main.Camera.Center 的
 * 距离衰减音量，距离 >= SoundAttenuationDistance(2500) 直接不发声。
 * 替换音效时跳过了原版，这一段必须由播放方补上，否则全地图都按满音量播放
 * （多人时尤其明显：其他玩家的走路/挖矿/战斗音效全都听得到）。
 *
 * Main.screenPosition / screenWidth / screenHeight 在 PC 上是静态字段，
 * 在手机端是静态属性，这里字段优先、属性兜底，两个平台都能取到。
 * ------------------------------------------------------------------------- */

typedef struct {
    float x, y;
} tef_audio_vec2_t;

#define TEF_AUDIO_ATTENUATION_DISTANCE 2500.0f

static patch_handle_t g_f_screen_pos = NULL;
static patch_handle_t g_f_screen_w = NULL;
static patch_handle_t g_f_screen_h = NULL;
static patch_handle_t g_m_screen_pos = NULL;
static patch_handle_t g_m_screen_w = NULL;
static patch_handle_t g_m_screen_h = NULL;
static _Atomic int g_camera_state = 0; /* 0=未解析 1=可用 -1=不可用 */

static void tef_audio_resolve_member(patch_handle_t type, const char *name,
                                     patch_handle_t *field,
                                     patch_handle_t *getter) {
    *field = patchlib_type_get_field(type, name);
    if (patchlib_is_valid(*field))
        return;

    patch_handle_t prop = patchlib_type_get_property(type, name);
    if (patchlib_is_valid(prop)) {
        *getter = patchlib_property_get_get_method(prop);
        patchlib_free(prop);
    }
}

static bool tef_audio_read_int(patch_handle_t field, patch_handle_t getter,
                               int *out) {
    if (patchlib_is_valid(field)) {
        patchlib_field_get_value(field, NULL, out);
        return true;
    }
    if (patchlib_is_valid(getter)) {
        int v = 0;
        if (!patchlib_method_invoke_args(getter, NULL, &v, NULL))
            return false;
        *out = v;
        return true;
    }
    return false;
}

static bool tef_audio_read_vec2(patch_handle_t field, patch_handle_t getter,
                                tef_audio_vec2_t *out) {
    if (patchlib_is_valid(field)) {
        patchlib_field_get_value(field, NULL, out);
        return true;
    }
    if (patchlib_is_valid(getter)) {
        tef_audio_vec2_t v = {0.0f, 0.0f};
        if (!patchlib_method_invoke_args(getter, NULL, &v, NULL))
            return false;
        *out = v;
        return true;
    }
    return false;
}

static bool tef_audio_camera_center(float *cx, float *cy) {
    int state = atomic_load_explicit(&g_camera_state, memory_order_acquire);
    if (state == 0) {
        patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
        if (patchlib_is_valid(main_type)) {
            tef_audio_resolve_member(main_type, "screenPosition",
                                     &g_f_screen_pos, &g_m_screen_pos);
            tef_audio_resolve_member(main_type, "screenWidth",
                                     &g_f_screen_w, &g_m_screen_w);
            tef_audio_resolve_member(main_type, "screenHeight",
                                     &g_f_screen_h, &g_m_screen_h);
            patchlib_free(main_type);
        }
        const bool pos_ok = patchlib_is_valid(g_f_screen_pos) ||
                            patchlib_is_valid(g_m_screen_pos);
        const bool w_ok = patchlib_is_valid(g_f_screen_w) ||
                          patchlib_is_valid(g_m_screen_w);
        const bool h_ok = patchlib_is_valid(g_f_screen_h) ||
                          patchlib_is_valid(g_m_screen_h);
        state = (pos_ok && w_ok && h_ok) ? 1 : -1;
        atomic_store_explicit(&g_camera_state, state, memory_order_release);
        TEKLOG_INFO("audio: camera fields pos(f=%d,m=%d) w(f=%d,m=%d) h(f=%d,m=%d) -> %s",
                    patchlib_is_valid(g_f_screen_pos),
                    patchlib_is_valid(g_m_screen_pos),
                    patchlib_is_valid(g_f_screen_w),
                    patchlib_is_valid(g_m_screen_w),
                    patchlib_is_valid(g_f_screen_h),
                    patchlib_is_valid(g_m_screen_h),
                    state == 1 ? "ok" : "unavailable");
    }
    if (state != 1)
        return false;

    tef_audio_vec2_t pos = {0.0f, 0.0f};
    int w = 0, h = 0;
    if (!tef_audio_read_vec2(g_f_screen_pos, g_m_screen_pos, &pos) ||
        !tef_audio_read_int(g_f_screen_w, g_m_screen_w, &w) ||
        !tef_audio_read_int(g_f_screen_h, g_m_screen_h, &h))
        return false;

    *cx = pos.x + (float) w * 0.5f;
    *cy = pos.y + (float) h * 0.5f;

    /* 字段/属性读失败时可能得到 (0,0) 或非法的屏幕尺寸；这时宁可放弃衰减，
     * 也不要把它当成"在遥远的地图角落"而把所有音效判成超距静音。 */
    if (w <= 0 || h <= 0) return false;
    if (*cx == 0.0f && *cy == 0.0f) return false;
    return true;
}

int terraria_audio_manager_play_at(terraria_audio_t *audio, float volume,
                                   float pitch, float x, float y) {
    if (!audio)
        return -1;

    if (x >= 0.0f && y >= 0.0f) {
        float cx = 0.0f, cy = 0.0f;
        if (tef_audio_camera_center(&cx, &cy)) {
            const float dx = x - cx;
            const float dy = y - cy;
            const float distance = sqrtf(dx * dx + dy * dy);
            static _Atomic int atten_logged = 0;
            if (atomic_exchange_explicit(&atten_logged, 1, memory_order_relaxed) == 0)
                TEKLOG_INFO("audio: first attenuation center=(%.0f,%.0f) pos=(%.0f,%.0f) dist=%.0f",
                            cx, cy, x, y, distance);
            if (distance >= TEF_AUDIO_ATTENUATION_DISTANCE)
                return -1;
            volume *= 1.0f - distance / TEF_AUDIO_ATTENUATION_DISTANCE;
        }
    }

    return terraria_audio_manager_play(audio, volume, pitch);
}

void terraria_audio_manager_pause(const int voice_id) {
    if (voice_id < 0 || voice_id >= TEF_AUDIO_MAX_VOICES || !g_voices_ready) return;
    tef_voice_t *v = &g_voices[voice_id];
    if (atomic_load_explicit(&v->state, memory_order_acquire) != VOICE_ACTIVE) return;
    atomic_store_explicit(&v->paused, 1, memory_order_relaxed);
}

void terraria_audio_manager_resume(const int voice_id) {
    if (voice_id < 0 || voice_id >= TEF_AUDIO_MAX_VOICES || !g_voices_ready) return;
    tef_voice_t *v = &g_voices[voice_id];
    if (atomic_load_explicit(&v->state, memory_order_acquire) != VOICE_ACTIVE) return;
    atomic_store_explicit(&v->paused, 0, memory_order_relaxed);
}

void terraria_audio_manager_stop(const int voice_id) {
    if (voice_id < 0 || voice_id >= TEF_AUDIO_MAX_VOICES || !g_voices_ready) return;
    tef_voice_t *v = &g_voices[voice_id];
    if (atomic_load_explicit(&v->state, memory_order_acquire) != VOICE_ACTIVE) return;
    atomic_store_explicit(&v->stop_requested, 1, memory_order_relaxed);
}

void terraria_audio_manager_set_volume(const int voice_id, const float volume) {
    if (voice_id < 0 || voice_id >= TEF_AUDIO_MAX_VOICES || !g_voices_ready) return;
    tef_voice_t *v = &g_voices[voice_id];
    atomic_store_explicit(&v->volume, tef_clampf(volume, 0.0f, 1.0f),
                          memory_order_relaxed);
}

bool terraria_audio_manager_is_playing(const int voice_id) {
    if (voice_id < 0 || voice_id >= TEF_AUDIO_MAX_VOICES || !g_voices_ready) return false;
    tef_voice_t *v = &g_voices[voice_id];
    if (atomic_load_explicit(&v->state, memory_order_acquire) != VOICE_ACTIVE) return false;
    if (atomic_load_explicit(&v->paused, memory_order_relaxed)) return false;
    if (atomic_load_explicit(&v->stop_requested, memory_order_relaxed)) return false;
    return true;
}

void terraria_audio_manager_stop_all(terraria_audio_t *audio) {
    if (!audio || !g_voices_ready) return;
    for (size_t i = 0; i < TEF_AUDIO_MAX_VOICES; ++i) {
        tef_voice_t *v = &g_voices[i];
        if (atomic_load_explicit(&v->state, memory_order_acquire) == VOICE_ACTIVE &&
            v->audio == audio) {
            atomic_store_explicit(&v->stop_requested, 1, memory_order_relaxed);
        }
    }
}

void terraria_audio_manager_set_master_volume(const float volume) {
    atomic_store_explicit(&g_master_volume, tef_clampf(volume, 0.0f, 1.0f),
                          memory_order_relaxed);
}

void terraria_audio_manager_shutdown(void) {
    if (g_device_ready) {
        ma_device_stop(&g_device);
        ma_device_uninit(&g_device);
        g_device_ready = false;
    }
    if (g_voices_ready) {
        for (size_t i = 0; i < TEF_AUDIO_MAX_VOICES; ++i) {
            atomic_store_explicit(&g_voices[i].state, VOICE_FREE, memory_order_release);
            atomic_store_explicit(&g_voices[i].ready, 0, memory_order_relaxed);
            g_voices[i].audio = NULL;
        }
        g_voices_ready = false;
    }
    if (g_sources_ready) {
        tefstd_vector_destroy(&g_sources);
        g_sources_ready = false;
    }
    if (g_mtx_ready) {
        mtx_destroy(&g_sources_mtx);
        g_mtx_ready = false;
    }
}