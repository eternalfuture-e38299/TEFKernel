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
#include "terraria/content/audio_manager.h"
#include "internal/platform_threads.h"
#include "tefstd/vector.h"

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
    terraria_audio_t         *audio;
    _Atomic unsigned long long current_frame;
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

/* voice 表：tefstd_vector_t<tef_voice_t>，voice_id 即下标 */
static tefstd_vector_t g_voices;
static bool            g_voices_ready = false;

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
    const size_t n = g_voices.size;
    tef_voice_t *voices = g_voices.data;

    for (size_t i = 0; i < n; ++i) {
        tef_voice_t *v = &voices[i];
        const int state = atomic_load_explicit(&v->state, memory_order_acquire);
        if (state == VOICE_FREE) continue;
        if (state == VOICE_STOPPING) {
            atomic_store_explicit(&v->state, VOICE_FREE, memory_order_release);
            continue;
        }
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

        unsigned long long pos = atomic_load_explicit(&v->current_frame, memory_order_relaxed);
        const double step = pitch;

        for (ma_uint32 f = 0; f < frame_count; ++f) {
            if (pos >= total) {
                if (a->looping) pos = 0;
                else {
                    atomic_store_explicit(&v->state, VOICE_STOPPING, memory_order_release);
                    break;
                }
            }
            const unsigned long long p0 = pos;
            const unsigned long long p1 = p0 + 1 < total ? p0 + 1 : (a->looping ? 0 : p0);
            const float frac = (float)((double)pos - (double)p0);

            for (unsigned int c = 0; c < out_channels; ++c) {
                const int src_c = c < (unsigned int)in_ch ? (int)c : (in_ch - 1);
                const float s0 = tef_read_sample_f32(a->pcm_data, in_fmt, p0, src_c, in_ch);
                const float s1 = tef_read_sample_f32(a->pcm_data, in_fmt, p1, src_c, in_ch);
                const float s = s0 + (s1 - s0) * frac;
                dst[(size_t)f * out_channels + c] += s * vol;
            }
            pos += (unsigned long long)step;
        }

        if (atomic_load_explicit(&v->state, memory_order_relaxed) == VOICE_ACTIVE) {
            atomic_store_explicit(&v->current_frame, pos, memory_order_relaxed);
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
        if (!tefstd_vector_init(&g_voices, sizeof(tef_voice_t))) return false;
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

/* 在 voice 表中分配一个空闲槽位；不够就 push_back 新槽位。
 * 调用方必须持有 g_sources_mtx（用于串行化扩容）。 */
static int tef_alloc_voice_slot(void) {
    const size_t n = tefstd_vector_size(&g_voices);
    tef_voice_t *arr = g_voices.data;
    for (size_t i = 0; i < n; ++i) {
        int expected = VOICE_FREE;
        if (atomic_compare_exchange_strong_explicit(
                &arr[i].state, &expected, VOICE_ACTIVE,
                memory_order_acq_rel, memory_order_relaxed)) {
            return (int)i;
        }
    }
    /* 没有空闲槽位：追加新槽位 */
    tef_voice_t nv = {0};
    atomic_init(&nv.state, VOICE_ACTIVE);
    atomic_init(&nv.current_frame, 0);
    atomic_init(&nv.paused, 0);
    atomic_init(&nv.stop_requested, 0);
    atomic_init(&nv.volume, 1.0f);
    atomic_init(&nv.pitch, 1.0f);
    nv.audio = NULL;
    if (!tefstd_vector_push_back(&g_voices, &nv)) return -1;
    return (int)(tefstd_vector_size(&g_voices) - 1);
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
        tef_voice_t *v = tefstd_vector_at(&g_voices, (size_t)id);
        if (v) {
            v->audio = audio;
            atomic_store_explicit(&v->current_frame, 0, memory_order_relaxed);
            atomic_store_explicit(&v->paused, 0, memory_order_relaxed);
            atomic_store_explicit(&v->stop_requested, 0, memory_order_relaxed);
            atomic_store_explicit(&v->volume, volume, memory_order_relaxed);
            atomic_store_explicit(&v->pitch, pitch, memory_order_relaxed);
        } else {
            id = -1;
        }
    }
    mtx_unlock(&g_sources_mtx);
    return id;
}

void terraria_audio_manager_pause(const int voice_id) {
    if (voice_id < 0 || !g_voices_ready) return;
    tef_voice_t *v = tefstd_vector_at(&g_voices, (size_t)voice_id);
    if (!v) return;
    if (atomic_load_explicit(&v->state, memory_order_acquire) != VOICE_ACTIVE) return;
    atomic_store_explicit(&v->paused, 1, memory_order_relaxed);
}

void terraria_audio_manager_resume(const int voice_id) {
    if (voice_id < 0 || !g_voices_ready) return;
    tef_voice_t *v = tefstd_vector_at(&g_voices, (size_t)voice_id);
    if (!v) return;
    if (atomic_load_explicit(&v->state, memory_order_acquire) != VOICE_ACTIVE) return;
    atomic_store_explicit(&v->paused, 0, memory_order_relaxed);
}

void terraria_audio_manager_stop(const int voice_id) {
    if (voice_id < 0 || !g_voices_ready) return;
    tef_voice_t *v = tefstd_vector_at(&g_voices, (size_t)voice_id);
    if (!v) return;
    if (atomic_load_explicit(&v->state, memory_order_acquire) != VOICE_ACTIVE) return;
    atomic_store_explicit(&v->stop_requested, 1, memory_order_relaxed);
}

void terraria_audio_manager_set_volume(const int voice_id, const float volume) {
    if (voice_id < 0 || !g_voices_ready) return;
    tef_voice_t *v = tefstd_vector_at(&g_voices, (size_t)voice_id);
    if (!v) return;
    atomic_store_explicit(&v->volume, tef_clampf(volume, 0.0f, 1.0f),
                          memory_order_relaxed);
}

bool terraria_audio_manager_is_playing(const int voice_id) {
    if (voice_id < 0 || !g_voices_ready) return false;
    tef_voice_t *v = tefstd_vector_at(&g_voices, (size_t)voice_id);
    if (!v) return false;
    if (atomic_load_explicit(&v->state, memory_order_acquire) != VOICE_ACTIVE) return false;
    if (atomic_load_explicit(&v->paused, memory_order_relaxed)) return false;
    if (atomic_load_explicit(&v->stop_requested, memory_order_relaxed)) return false;
    return true;
}

void terraria_audio_manager_stop_all(terraria_audio_t *audio) {
    if (!audio || !g_voices_ready) return;
    const size_t n = tefstd_vector_size(&g_voices);
    tef_voice_t *arr = g_voices.data;
    for (size_t i = 0; i < n; ++i) {
        tef_voice_t *v = &arr[i];
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
        const size_t n = tefstd_vector_size(&g_voices);
        tef_voice_t *arr = g_voices.data;
        for (size_t i = 0; i < n; ++i) {
            atomic_store_explicit(&arr[i].state, VOICE_FREE, memory_order_release);
            arr[i].audio = NULL;
        }
        tefstd_vector_destroy(&g_voices);
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