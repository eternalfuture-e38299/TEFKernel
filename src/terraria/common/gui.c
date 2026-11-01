/*******************************************************************************
 * tefkernel - gui
 *
 * 屏幕覆盖层：挂 Main.Draw 后置，用 Main.spriteBatch 自行 Begin / 画 / End。
 *
 * 实现方式与 LuaLoader 的原生 GUI 一致（它在手机上已验证可用）：
 *   · Begin 用 8 参重载 + 单位矩阵，避免继承世界相机变换；
 *   · 矩形走 Utils.DrawInvBG(sb, x, y, w, h, Color)；
 *   · 文字走 Utils.DrawBorderString(sb, string, Vector2, Color, scale, 0, 0, -1)；
 *   · 贴图走 SpriteBatch.Draw(tex, Rectangle, Color)；
 *   · 含结构体的托管方法按 C ABI 直调函数指针，绕开 patchlib 的按值结构体 FFI。
 *******************************************************************************/

#include "internal/terraria/gui.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/il2cpp_api.h"
#include "patchlib/method.h"
#include "patchlib/property.h"
#include "patchlib/struct/string.h"
#include "patchlib/type.h"
#include "tefstd/vector.h"

#define GUI_MAX_TEXT 96

typedef struct { float x, y; } v2f;
typedef struct { uint8_t r, g, b, a; } color4;

/* Terraria/Unity 的 UI 批次是 AlphaBlend(One, InvSrcAlpha)，纹理和**顶点色**都按
 * 预乘 alpha 处理。只改 alpha 通道不会把图画淡，反而会把背景透上来显得更亮；
 * 必须把 RGB 也乘上 alpha。 */
static unsigned premul_color(const unsigned c) {
    const unsigned a = (c >> 24) & 0xFFu;
    if (a == 255u) return c;
    const unsigned r = ((c & 0xFFu) * a + 127u) / 255u;
    const unsigned g = (((c >> 8) & 0xFFu) * a + 127u) / 255u;
    const unsigned b = (((c >> 16) & 0xFFu) * a + 127u) / 255u;
    return r | (g << 8) | (b << 16) | (a << 24);
}

static color4 premul_color4(color4 c) {
    const unsigned a = c.a;
    if (a != 255u) {
        c.r = (uint8_t) (((unsigned) c.r * a + 127u) / 255u);
        c.g = (uint8_t) (((unsigned) c.g * a + 127u) / 255u);
        c.b = (uint8_t) (((unsigned) c.b * a + 127u) / 255u);
    }
    return c;
}
typedef struct { int x, y, w, h; } rect4;
typedef struct { uint8_t has; uint8_t pad[3]; rect4 r; } nrect;
typedef struct { uint8_t has; uint8_t pad[3]; float m[16]; } nmat;

/* 与 LuaLoader 完全一致的直调签名（这几个重载在手机上验证过） */
typedef void (*fn_invbg)(void *sb, rect4 r, color4 c);
typedef v2f (*fn_border_string)(void *sb, void *str, v2f pos, color4 c, float scale,
                                float ax, float ay, int32_t max_chars);
typedef void (*fn_draw_tex)(void *sb, void *tex, v2f pos, rect4 *src,
                            unsigned int color, float rotation, v2f origin, v2f scale,
                            int effects, float depth);
typedef void (*fn_draw_string)(void *sb, void *font, void *text, v2f pos, unsigned int color,
                               float rotation, v2f origin, float scale, int effects, float depth);
typedef void (*fn_begin8)(void *self, int sort_mode, void *blend, void *sampler,
                          void *depth, void *raster, void *effect, nmat *xform,
                          bool deferred);

typedef struct {
    tef_gui_draw_cb cb;
    void *user;
} gui_entry_t;

/* 绘制回调表：动态数组，元素类型 gui_entry_t */
static tefstd_vector_t g_entries;
static bool g_entries_ready;

static patch_handle_t t_main;
static patch_handle_t f_sprite_batch;
static patch_handle_t m_sb_begin;      /* Begin() */
static patch_handle_t m_sb_begin8;     /* Begin(8) */
static patch_handle_t m_sb_end;
static patch_handle_t m_inv_bg;        /* Utils.DrawInvBG(SB, float, float, float, float, Color) */
static patch_handle_t m_draw_string; /* SpriteBatch.DrawString(font, text, pos, color, ...) */
static patch_handle_t m_border_string; /* Utils.DrawBorderString(SB, string, Vector2, Color, ...) */
static patch_handle_t m_draw_tex;      /* SpriteBatch.Draw(tex, Rectangle, Color) */
static patch_handle_t f_blend_alpha;   /* BlendState.AlphaBlend（静态字段/属性） */
static patch_handle_t m_get_blend_alpha;
static patch_handle_t g_blend;         /* 当帧的 AlphaBlend 实例 */

static patch_handle_t a_screen_w, a_screen_h;   /* 字段或属性 */
static patch_handle_t m_get_screen_w, m_get_screen_h;
static patch_handle_t a_mouse_x, a_mouse_y, a_mouse_left, a_mouse_left_release;
static patch_handle_t a_show_splash;
static patch_handle_t m_get_show_splash;
/* 与原版 Buff 栏一致的可见性条件 */
static patch_handle_t a_game_menu, a_options_window, a_inventory, a_fancy_ui;
static patch_handle_t m_get_game_menu, m_get_options_window, m_get_inventory, m_get_fancy_ui;
static patch_handle_t m_get_mouse_x, m_get_mouse_y, m_get_mouse_left, m_get_mouse_left_release;

static patch_hook_id_t g_draw_hook = PATCH_HOOK_INVALID_ID;
static bool g_enabled;
static bool g_in_draw;
static bool g_batch_open;
static void *g_sb;
static tef_gui_frame_t g_frame;
static bool g_prev_mouse_down;

/* 文字句柄 LRU 缓存：容量 512，避免无限创建 GCHandle 导致 GC 停顿雪崩 */
#undef GUI_MAX_TEXT
#define GUI_MAX_TEXT 512

typedef struct {
    uint32_t hash;
    char *text;
    patch_handle_t handle;
    void *pin;      /* 托管字符串必须钉住：C 侧的裸指针 GC 看不见 */
    uint64_t last_used;
} gui_text_entry_t;

static gui_text_entry_t g_text_cache[GUI_MAX_TEXT];
static int g_text_count = 0;
static uint64_t g_lru_clock = 0;

typedef struct {
    char *text;
    patch_handle_t handle;
    void *pin;
} gui_deferred_release_t;

static tefstd_vector_t g_deferred_releases;
static bool g_deferred_ready = false;

static inline uint32_t string_hash(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

/* ------------------------------------------------------------------ */
/* 取字段 / 属性                                                       */
/* ------------------------------------------------------------------ */

static patch_handle_t resolve_field(patch_handle_t type, const char *name) {
    if (!t_main || !name) return PATCH_NULL;
    patch_handle_t f = patchlib_type_get_field(type, name);
    if (f && !patchlib_is_valid(f)) return PATCH_NULL;
    return f;
}

static patch_handle_t resolve_getter(patch_handle_t type, const char *name) {
    patch_handle_t prop = patchlib_type_get_property(type, name);
    if (!prop || !patchlib_is_valid(prop)) return PATCH_NULL;
    patch_handle_t getter = patchlib_property_get_get_method(prop);
    patchlib_free(prop);
    return getter;
}

/* 统一用 64 字节缓冲收值：内核可能按返回类型/字段大小写 8 字节，
 * 直接写进 int/bool 会踩坏栈（表现为 __stack_chk_fail -> SIGABRT）。 */
static unsigned char g_read_buf[64];

static int read_int_field(patch_handle_t field) {
#if defined(__ANDROID__)
    void *p = patchlib_field_get_pointer(field, NULL);
    if (p) return *(int *) p;
#endif
    memset(g_read_buf, 0, sizeof(g_read_buf));
    patchlib_field_get_value(field, NULL, g_read_buf);
    int v;
    memcpy(&v, g_read_buf, sizeof(v));
    return v;
}

static int read_int_getter(patch_handle_t m) {
    memset(g_read_buf, 0, sizeof(g_read_buf));
    patchlib_method_invoke_args(m, NULL, g_read_buf, NULL);
    int v;
    memcpy(&v, g_read_buf, sizeof(v));
    return v;
}

static bool read_bool_field(patch_handle_t field) {
#if defined(__ANDROID__)
    void *p = patchlib_field_get_pointer(field, NULL);
    if (p) return *(uint8_t *) p != 0;
#endif
    memset(g_read_buf, 0, sizeof(g_read_buf));
    patchlib_field_get_value(field, NULL, g_read_buf);
    return g_read_buf[0] != 0;
}

static bool read_bool_getter(patch_handle_t m) {
    memset(g_read_buf, 0, sizeof(g_read_buf));
    patchlib_method_invoke_args(m, NULL, g_read_buf, NULL);
    return g_read_buf[0] != 0;
}

static int read_stat_int(patch_handle_t field, patch_handle_t getter, int fallback) {
    if (field != PATCH_NULL) return read_int_field(field);
    if (getter != PATCH_NULL) return read_int_getter(getter);
    return fallback;
}

static bool read_stat_bool(patch_handle_t field, patch_handle_t getter) {
    if (field != PATCH_NULL) return read_bool_field(field);
    if (getter != PATCH_NULL) return read_bool_getter(getter);
    return false;
}

/* ------------------------------------------------------------------ */
/* SpriteBatch                                                         */
/* ------------------------------------------------------------------ */

static bool sb_begin(void) {
    if (g_sb == NULL) return false;

#if defined(__ANDROID__)
    static int logged;
    if (!logged) {
        logged = 1;
        TEKLOG_INFO("gui: begin8=%d fn=%p begin0=%d end=%d",
                    m_sb_begin8 != PATCH_NULL,
                    m_sb_begin8 != PATCH_NULL
                            ? patchlib_method_get_pointer(m_sb_begin8)
                            : NULL,
                    m_sb_begin != PATCH_NULL, m_sb_end != PATCH_NULL);
    }
    if (m_sb_begin8 != PATCH_NULL) {
        static nmat ident;
        static bool init;
        if (!init) {
            init = true;
            ident.has = 1;
            ident.m[0] = ident.m[5] = ident.m[10] = ident.m[15] = 1.0f;
        }
        fn_begin8 fn = (fn_begin8) patchlib_method_get_pointer(m_sb_begin8);
        if (fn) {
            /* 显式传 AlphaBlend：blend 传 NULL 会沿用上一笔绘制的混合状态（常见是
             * additive），表现就是图标发蓝/发亮。 */
            fn(g_sb, 0, g_blend, NULL, NULL, NULL, NULL, &ident, true);
            return true;
        }
    }
#endif
    if (m_sb_begin != PATCH_NULL) {
        patchlib_method_invoke_args(m_sb_begin, g_sb, NULL, NULL);
        return true;
    }
    return false;
}

static void sb_end(void) {
    if (m_sb_end != PATCH_NULL && g_sb != NULL)
        patchlib_method_invoke_args(m_sb_end, g_sb, NULL, NULL);
}

static bool ensure_batch(void) {
    if (g_batch_open) return true;
    if (!sb_begin()) return false;
    g_batch_open = true;
    return true;
}

static color4 color_of(unsigned rgba) {
    color4 c;
    c.a = (uint8_t) ((rgba >> 24) & 0xFF);
    c.r = (uint8_t) (rgba & 0xFF);
    c.g = (uint8_t) ((rgba >> 8) & 0xFF);
    c.b = (uint8_t) ((rgba >> 16) & 0xFF);
    return c;
}

static void terraria_gui_end_frame_cleanup(void) {
    if (!g_deferred_ready) return;
    const size_t count = tefstd_vector_size(&g_deferred_releases);
    if (count == 0) return;

    for (size_t i = 0; i < count; ++i) {
        gui_deferred_release_t *rel = (gui_deferred_release_t *) tefstd_vector_at(&g_deferred_releases, i);
        if (rel) {
#if defined(__ANDROID__)
            if (rel->pin && il2cpp_gchandle_free) {
                il2cpp_gchandle_free(rel->pin);
            }
#endif
            if (rel->handle) {
                patchlib_free(rel->handle);
            }
            if (rel->text) {
                free(rel->text);
            }
        }
    }
    tefstd_vector_clear(&g_deferred_releases);
}

/* 返回托管字符串；带有 LRU 淘汰与后置帧生命周期清理机制 */
static void *text_handle(const char *text, void **temp_pin) {
    if (temp_pin) *temp_pin = NULL;
    if (!text || !*text) return NULL;

    const uint32_t hval = string_hash(text);
    g_lru_clock++;

    for (int i = 0; i < g_text_count; ++i) {
        if (g_text_cache[i].hash == hval && g_text_cache[i].text && strcmp(g_text_cache[i].text, text) == 0) {
            g_text_cache[i].last_used = g_lru_clock;
            return g_text_cache[i].handle;
        }
    }

    patch_handle_t h = patchlib_string_create(text);
    if (!h) return NULL;
    void *pin = NULL;
#if defined(__ANDROID__)
    if (il2cpp_gchandle_new) pin = il2cpp_gchandle_new(h, false);
#endif

    int slot = -1;
    if (g_text_count < GUI_MAX_TEXT) {
        slot = g_text_count++;
    } else {
        /* LRU 淘汰：淘汰最久未使用的项，并将其加入当前帧延迟释放列表（避免 SpriteBatch use-after-free） */
        int lru_idx = 0;
        uint64_t min_used = g_text_cache[0].last_used;
        for (int i = 1; i < GUI_MAX_TEXT; ++i) {
            if (g_text_cache[i].last_used < min_used) {
                min_used = g_text_cache[i].last_used;
                lru_idx = i;
            }
        }
        if (g_deferred_ready) {
            gui_deferred_release_t rel = {
                .text = g_text_cache[lru_idx].text,
                .handle = g_text_cache[lru_idx].handle,
                .pin = g_text_cache[lru_idx].pin,
            };
            tefstd_vector_push_back(&g_deferred_releases, &rel);
        } else {
#if defined(__ANDROID__)
            if (g_text_cache[lru_idx].pin && il2cpp_gchandle_free)
                il2cpp_gchandle_free(g_text_cache[lru_idx].pin);
#endif
            if (g_text_cache[lru_idx].handle)
                patchlib_free(g_text_cache[lru_idx].handle);
            free(g_text_cache[lru_idx].text);
        }
        slot = lru_idx;
    }

    size_t n = strlen(text);
    char *copy = malloc(n + 1);
    if (copy) {
        memcpy(copy, text, n + 1);
        g_text_cache[slot].hash = hval;
        g_text_cache[slot].text = copy;
        g_text_cache[slot].handle = h;
        g_text_cache[slot].pin = pin;
        g_text_cache[slot].last_used = g_lru_clock;
        return h;
    }

    /* malloc 失败兜底：作为非缓存临时句柄，画完在帧末回收 */
    if (temp_pin) *temp_pin = pin;
    return h;
}

static void text_release_temp(void *temp_pin) {
    if (!temp_pin) return;
    if (g_deferred_ready) {
        gui_deferred_release_t rel = {.text = NULL, .handle = PATCH_NULL, .pin = temp_pin};
        tefstd_vector_push_back(&g_deferred_releases, &rel);
    } else {
#if defined(__ANDROID__)
        if (il2cpp_gchandle_free)
            il2cpp_gchandle_free(temp_pin);
#endif
    }
}

/* ------------------------------------------------------------------ */
/* 绘制原语                                                            */
/* ------------------------------------------------------------------ */

void terraria_gui_rect(float x, float y, float w, float h, unsigned rgba) {
    if (!g_in_draw || g_sb == NULL || m_inv_bg == PATCH_NULL || w <= 0 || h <= 0)
        return;
    if (!ensure_batch()) return;
    color4 c = color_of(rgba);
    rect4 r = {(int) x, (int) y, (int) w, (int) h};
#if defined(__ANDROID__)
    fn_invbg fn = (fn_invbg) patchlib_method_get_pointer(m_inv_bg);
    if (fn) fn(g_sb, r, c);
#else
    void *args[3] = {&g_sb, &r, &c};
    patchlib_method_invoke_args(m_inv_bg, NULL, NULL, args);
#endif
}

void terraria_gui_text(const char *text, float x, float y, float scale, unsigned rgba) {
    if (!g_in_draw || g_sb == NULL || m_border_string == PATCH_NULL || !text || !*text)
        return;
    void *temp_pin = NULL;
    void *str = text_handle(text, &temp_pin);
    if (!str) return;
    if (!ensure_batch()) return;
    v2f pos = {x, y};
    color4 c = premul_color4(color_of(rgba));
    float sc = scale <= 0 ? 1.0f : scale;
    float ax = 0.0f, ay = 0.0f;
    int32_t max_chars = -1;
#if defined(__ANDROID__)
    fn_border_string fn = (fn_border_string) patchlib_method_get_pointer(m_border_string);
    if (fn) fn(g_sb, str, pos, c, sc, 0.0f, 0.0f, -1);
#else
    void *args[8] = {&g_sb, &str, &pos, &c, &sc, &ax, &ay, &max_chars};
    patchlib_method_invoke_args(m_border_string, NULL, NULL, args);
#endif
    text_release_temp(temp_pin);
}

void terraria_gui_draw_string(patch_handle_t font, const char *text, float x, float y,
                              float scale, unsigned rgba) {
    if (!g_in_draw || g_sb == NULL || !font || !text || !*text) return;
    if (m_draw_string == PATCH_NULL) {
        terraria_gui_text(text, x, y, scale, rgba);
        return;
    }
    void *temp_pin = NULL;
    void *str = text_handle(text, &temp_pin);
    if (!str) return;
    if (!ensure_batch()) return;
    const color4 c = premul_color4(color_of(rgba));
#if defined(__ANDROID__)
    const unsigned packed = (unsigned) c.r | ((unsigned) c.g << 8) |
                            ((unsigned) c.b << 16) | ((unsigned) c.a << 24);
    fn_draw_string fn = (fn_draw_string) patchlib_method_get_pointer(m_draw_string);
    v2f pos = {x, y};
    v2f origin = {0.0f, 0.0f};
    if (fn) fn(g_sb, font, str, pos, packed, 0.0f, origin, scale > 0 ? scale : 1.0f, 0, 0.0f);
#else
    v2f pos = {x, y};
    v2f origin = {0.0f, 0.0f};
    float sc = scale > 0 ? scale : 1.0f;
    float rot = 0.0f, depth = 0.0f;
    int effects = 0;
    void *args[9] = {&font, &str, &pos, (void *) &c, &rot, &origin, &sc, &effects, &depth};
    patchlib_method_invoke_args(m_draw_string, g_sb, NULL, args);
#endif
    text_release_temp(temp_pin);
}

void terraria_gui_image(patch_handle_t texture, float x, float y, float w, float h,
                        unsigned rgba) {
    if (!g_in_draw || g_sb == NULL || m_draw_tex == PATCH_NULL || !texture || w <= 0 || h <= 0)
        return;
    if (!ensure_batch()) return;
    color4 c = color_of(rgba);
#if defined(__ANDROID__)
    /* 图标统一按 32x32 源尺寸缩放 */
    v2f pos = {x, y};
    v2f origin = {0.0f, 0.0f};
    v2f scale = {w / 32.0f, h / 32.0f};
    unsigned int packed = (unsigned int) c.r | ((unsigned int) c.g << 8) |
                          ((unsigned int) c.b << 16) | ((unsigned int) c.a << 24);
    packed = premul_color(packed);
    fn_draw_tex fn = (fn_draw_tex) patchlib_method_get_pointer(m_draw_tex);
    rect4 src = {0, 0, 32, 32};
    if (fn) fn(g_sb, texture, pos, &src, packed, 0.0f, origin, scale, 0, 0.0f);
#else
    rect4 dest = {(int) x, (int) y, (int) w, (int) h};
    c = premul_color4(c);
    void *args[3] = {&texture, &dest, &c};
    patchlib_method_invoke_args(m_draw_tex, g_sb, NULL, args);
#endif
}

void terraria_gui_begin_external(void *sprite_batch) {
    if (!sprite_batch) return;
    g_sb = sprite_batch;
    g_in_draw = true;
    g_batch_open = true;   /* 借用游戏的批次，不自己 Begin */
}

void terraria_gui_end_external(void) {
    g_in_draw = false;
    g_batch_open = false;
    g_sb = NULL;
}

void terraria_gui_image_raw(patch_handle_t texture, float x, float y, float w, float h,
                            unsigned raw_color) {
    if (!g_in_draw || g_sb == NULL || m_draw_tex == PATCH_NULL || !texture || w <= 0 || h <= 0)
        return;
    if (!ensure_batch()) return;
#if defined(__ANDROID__)
    v2f pos = {x, y};
    v2f origin = {0.0f, 0.0f};
    v2f scale = {w / 32.0f, h / 32.0f};
    fn_draw_tex fn = (fn_draw_tex) patchlib_method_get_pointer(m_draw_tex);
    rect4 src = {0, 0, 32, 32};
    if (fn) fn(g_sb, texture, pos, &src, premul_color(raw_color), 0.0f, origin, scale, 0, 0.0f);
#else
    color4 c;
    memcpy(&c, &raw_color, sizeof(c));
    c = premul_color4(c);
    rect4 dest = {(int) x, (int) y, (int) w, (int) h};
    void *args[3] = {&texture, &dest, &c};
    patchlib_method_invoke_args(m_draw_tex, g_sb, NULL, args);
#endif
}

/* ------------------------------------------------------------------ */
/* 回调注册                                                            */
/* ------------------------------------------------------------------ */

void terraria_gui_add_draw(tef_gui_draw_cb cb, void *user) {
    if (!cb) return;
    if (!g_entries_ready) {
        TEKLOG_ERROR("gui: add_draw before init");
        return;
    }

    /* 已注册：只更新 user，不占新槽位 */
    const size_t n = tefstd_vector_size(&g_entries);
    for (size_t i = 0; i < n; ++i) {
        gui_entry_t *e = (gui_entry_t *) tefstd_vector_at(&g_entries, i);
        if (e && e->cb == cb) {
            e->user = user;
            return;
        }
    }

    gui_entry_t e = {cb, user};
    if (!tefstd_vector_push_back(&g_entries, &e)) {
        TEKLOG_ERROR("gui: push_back failed");
        return;
    }
    TEKLOG_INFO("gui: draw callback added (%zu)", tefstd_vector_size(&g_entries));
}

void terraria_gui_remove_draw(tef_gui_draw_cb cb) {
    if (!cb || !g_entries_ready) return;
    const size_t n = tefstd_vector_size(&g_entries);
    for (size_t i = 0; i < n; ++i) {
        gui_entry_t *e = (gui_entry_t *) tefstd_vector_at(&g_entries, i);
        if (e && e->cb == cb) {
            tefstd_vector_erase(&g_entries, i, NULL);
            TEKLOG_INFO("gui: draw callback removed (%zu)", tefstd_vector_size(&g_entries));
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* 帧钩子                                                              */
/* ------------------------------------------------------------------ */

static void frame_postfix(patch_handle_t this, void **args, void *result,
                          const patch_method_signature_t *sig) {
    (void) this; (void) args; (void) result; (void) sig;
    if (g_in_draw || !g_entries_ready || f_sprite_batch == PATCH_NULL) {
        terraria_gui_end_frame_cleanup();
        return;
    }
    const size_t n_entries = tefstd_vector_size(&g_entries);
    if (n_entries == 0) {
        terraria_gui_end_frame_cleanup();
        return;
    }
    /* 启动画面 / 加载阶段：贴图与字体尚未就绪，此时绘制会抛托管异常；
     * 异常跨 hook 帧无法展开，最终 SIGABRT（闪退）。加载完成后再画。 */
    if (read_stat_bool(a_show_splash, m_get_show_splash)) {
        terraria_gui_end_frame_cleanup();
        return;
    }
    /* 和原版 Buff 栏一致：菜单/背包/暂停/特殊 UI 打开时不画 */
    if (read_stat_bool(a_game_menu, m_get_game_menu) ||
        read_stat_bool(a_options_window, m_get_options_window) ||
        read_stat_bool(a_inventory, m_get_inventory) ||
        read_stat_bool(a_fancy_ui, m_get_fancy_ui)) {
        terraria_gui_end_frame_cleanup();
        return;
    }

    void *sb = NULL;
    memset(g_read_buf, 0, sizeof(g_read_buf));
    patchlib_field_get_value(f_sprite_batch, NULL, g_read_buf);
    memcpy(&sb, g_read_buf, sizeof(sb));
    if (!sb) {
        terraria_gui_end_frame_cleanup();
        return;
    }

    g_frame.screen_w = read_stat_int(a_screen_w, m_get_screen_w, 0);
    g_frame.screen_h = read_stat_int(a_screen_h, m_get_screen_h, 0);
    g_frame.mouse_x = (float) read_stat_int(a_mouse_x, m_get_mouse_x, 0);
    g_frame.mouse_y = (float) read_stat_int(a_mouse_y, m_get_mouse_y, 0);
    g_frame.mouse_down = read_stat_bool(a_mouse_left, m_get_mouse_left);
    g_frame.mouse_pressed = read_stat_bool(a_mouse_left_release, m_get_mouse_left_release) &&
                            !g_prev_mouse_down;
    g_prev_mouse_down = g_frame.mouse_down;

    /* 取一次 AlphaBlend 实例 */
    if (f_blend_alpha != PATCH_NULL) {
        memset(g_read_buf, 0, sizeof(g_read_buf));
        patchlib_field_get_value(f_blend_alpha, NULL, g_read_buf);
        memcpy(&g_blend, g_read_buf, sizeof(g_blend));
    } else if (m_get_blend_alpha != PATCH_NULL) {
        memset(g_read_buf, 0, sizeof(g_read_buf));
        patchlib_method_invoke_args(m_get_blend_alpha, NULL, g_read_buf, NULL);
        memcpy(&g_blend, g_read_buf, sizeof(g_blend));
    }

    g_sb = sb;
    g_in_draw = true;
    g_batch_open = false;
    /* 快照遍历：回调内 add/remove 触发的扩容不影响本轮，且每轮用 at 重新取址 */
    for (size_t i = 0; i < n_entries; ++i) {
        gui_entry_t *e = (gui_entry_t *) tefstd_vector_at(&g_entries, i);
        if (e && e->cb) e->cb(&g_frame, e->user);
    }
    if (g_batch_open)
        sb_end();
    g_batch_open = false;
    g_in_draw = false;
    g_sb = NULL;

    terraria_gui_end_frame_cleanup();
}

/* ------------------------------------------------------------------ */
/* 生命周期                                                            */
/* ------------------------------------------------------------------ */

void terraria_gui_init(void) {
    t_main = patchlib_type_get_type("Terraria", "Main");
    if (!patchlib_is_valid(t_main)) {
        TEKLOG_ERROR("gui: Terraria.Main not found");
        return;
    }

    /* 回调表在 init 中初始化；失败则整个 GUI 不可用 */
    if (!tefstd_vector_init(&g_entries, sizeof(gui_entry_t))) {
        TEKLOG_ERROR("gui: vector_init(entries) failed");
        return;
    }
    g_entries_ready = true;

    if (!g_deferred_ready) {
        tefstd_vector_init(&g_deferred_releases, sizeof(gui_deferred_release_t));
        g_deferred_ready = true;
    }

    f_sprite_batch = resolve_field(t_main, "spriteBatch");

    a_screen_w = resolve_field(t_main, "screenWidth");
    if (a_screen_w == PATCH_NULL) m_get_screen_w = resolve_getter(t_main, "screenWidth");
    a_screen_h = resolve_field(t_main, "screenHeight");
    if (a_screen_h == PATCH_NULL) m_get_screen_h = resolve_getter(t_main, "screenHeight");

    a_mouse_x = resolve_field(t_main, "mouseX");
    if (a_mouse_x == PATCH_NULL) m_get_mouse_x = resolve_getter(t_main, "mouseX");
    a_mouse_y = resolve_field(t_main, "mouseY");
    if (a_mouse_y == PATCH_NULL) m_get_mouse_y = resolve_getter(t_main, "mouseY");
    a_mouse_left = resolve_field(t_main, "mouseLeft");
    if (a_mouse_left == PATCH_NULL) m_get_mouse_left = resolve_getter(t_main, "mouseLeft");
    a_mouse_left_release = resolve_field(t_main, "mouseLeftRelease");
    if (a_mouse_left_release == PATCH_NULL)
        m_get_mouse_left_release = resolve_getter(t_main, "mouseLeftRelease");
    a_show_splash = resolve_field(t_main, "showSplash");
    if (a_show_splash == PATCH_NULL)
        m_get_show_splash = resolve_getter(t_main, "showSplash");

    a_game_menu = resolve_field(t_main, "gameMenu");
    if (a_game_menu == PATCH_NULL) m_get_game_menu = resolve_getter(t_main, "gameMenu");
    a_options_window = resolve_field(t_main, "ingameOptionsWindow");
    if (a_options_window == PATCH_NULL)
        m_get_options_window = resolve_getter(t_main, "ingameOptionsWindow");
    a_inventory = resolve_field(t_main, "playerInventory");
    if (a_inventory == PATCH_NULL) m_get_inventory = resolve_getter(t_main, "playerInventory");
    a_fancy_ui = resolve_field(t_main, "inFancyUI");
    if (a_fancy_ui == PATCH_NULL) m_get_fancy_ui = resolve_getter(t_main, "inFancyUI");

    patch_handle_t t_sb = patchlib_type_get_type("Microsoft.Xna.Framework.Graphics",
                                                 "SpriteBatch");
    if (patchlib_is_valid(t_sb)) {
        m_sb_begin = patchlib_type_get_method_by_param_count(t_sb, "Begin", 0);
        m_sb_begin8 = patchlib_type_get_method_by_param_count(t_sb, "Begin", 8);
        m_sb_end = patchlib_type_get_method_by_param_count(t_sb, "End", 0);
#if defined(__ANDROID__)
        {
            patch_handle_t t_tex = patchlib_type_get_type(
                    "Microsoft.Xna.Framework.Graphics", "Texture2D");
            patch_handle_t t_vec = patchlib_type_get_type(
                    "Microsoft.Xna.Framework", "Vector2");
            patch_handle_t t_rect = patchlib_type_get_type(
                    "Microsoft.Xna.Framework", "Rectangle");
            patch_handle_t t_color = patchlib_type_get_type(
                    "Microsoft.Xna.Framework.Graphics", "Color");
            patch_handle_t t_single = patchlib_type_get_type("System", "Single");
            patch_handle_t t_effects = patchlib_type_get_type(
                    "Microsoft.Xna.Framework.Graphics", "SpriteEffects");
            patch_handle_t types[9] = {t_tex, t_vec, t_rect, t_color, t_single,
                                       t_vec, t_vec, t_effects, t_single};
            const char *names[9] = {"texture", "position", "srcRect", "color",
                                    "rotation", "origin", "scale", "effects",
                                    "layerDepth"};
            m_draw_tex = patchlib_type_get_method_by_signature(t_sb, "Draw", 9,
                                                               types, names);
            if (!patchlib_is_valid(m_draw_tex))
                TEKLOG_ERROR("gui: 未找到 9 参数 SpriteBatch.Draw（位置/缩放/颜色会错）");
        }
#else
        /* 必须按参数类型取 Draw(Texture2D, Rectangle, Color)：
         * 按参数个数取到的可能是 Draw(Texture2D, Vector2, Color)，
         * 我们的 Rectangle 会被当成 Vector2 读（int 当 float = 0），
         * 图标就画到左上角 (0,0) 了。 */
        {
            patch_handle_t t_tex = patchlib_type_get_type(
                    "Microsoft.Xna.Framework.Graphics", "Texture2D");
            patch_handle_t t_rect = patchlib_type_get_type(
                    "Microsoft.Xna.Framework", "Rectangle");
            patch_handle_t t_col = patchlib_type_get_type(
                    "Microsoft.Xna.Framework", "Color");
            void *types[3] = {t_tex, t_rect, t_col};
            m_draw_tex = patchlib_type_get_method_by_param_types(t_sb, "Draw", 3, types);
            if (!patchlib_is_valid(m_draw_tex)) {
                TEKLOG_WARN("gui: 未找到 Draw(Texture2D, Rectangle, Color)，退回按参数个数");
                m_draw_tex = patchlib_type_get_method_by_param_count(t_sb, "Draw", 3);
            }
        }
#endif
        /* DrawString：注意必须在这里解析，t_sb 释放后再用会拿到已释放的 GCHandle
         * （桌面版 GCHandle.FromIntPtr 直接 SIGSEGV）。 */
        {
            patch_handle_t t_font = patchlib_type_get_type(
                    "Microsoft.Xna.Framework.Graphics", "SpriteFont");
            patch_handle_t t_str = patchlib_type_get_type("System", "String");
            patch_handle_t t_vec2 = patchlib_type_get_type(
                    "Microsoft.Xna.Framework", "Vector2");
#if defined(__ANDROID__)
            patch_handle_t t_col = patchlib_type_get_type(
                    "Microsoft.Xna.Framework.Graphics", "Color");
#else
            patch_handle_t t_col = patchlib_type_get_type(
                    "Microsoft.Xna.Framework", "Color");
#endif
            patch_handle_t t_sgl = patchlib_type_get_type("System", "Single");
            patch_handle_t t_fx = patchlib_type_get_type(
                    "Microsoft.Xna.Framework.Graphics", "SpriteEffects");
            patch_handle_t types[9] = {t_font, t_str, t_vec2, t_col, t_sgl,
                                       t_vec2, t_sgl, t_fx, t_sgl};
            const char *names[9] = {"spriteFont", "text", "position", "color",
                                    "rotation", "origin", "scale", "spriteEffects",
                                    "layerDepth"};
            m_draw_string = patchlib_type_get_method_by_signature(t_sb, "DrawString", 9,
                                                                  types, names);
            if (!patchlib_is_valid(m_draw_string))
                TEKLOG_WARN("gui: 未找到 9 参数 SpriteBatch.DrawString");
        }
        patchlib_free(t_sb);
    }

    patch_handle_t t_blend =
            patchlib_type_get_type("Microsoft.Xna.Framework.Graphics", "BlendState");
    if (patchlib_is_valid(t_blend)) {
        f_blend_alpha = patchlib_type_get_field(t_blend, "AlphaBlend");
        if (!patchlib_is_valid(f_blend_alpha))
            m_get_blend_alpha = resolve_getter(t_blend, "AlphaBlend");
        patchlib_free(t_blend);
    }

    patch_handle_t t_utils = patchlib_type_get_type("Terraria", "Utils");
    if (patchlib_is_valid(t_utils)) {
        m_inv_bg = patchlib_type_get_method_by_param_count(t_utils, "DrawInvBG", 3);
        m_border_string =
                patchlib_type_get_method_by_param_count(t_utils, "DrawBorderString", 8);

        patchlib_free(t_utils);
    }

    g_enabled = f_sprite_batch != PATCH_NULL && m_sb_end != PATCH_NULL &&
                m_inv_bg != PATCH_NULL && m_border_string != PATCH_NULL &&
                (m_sb_begin != PATCH_NULL || m_sb_begin8 != PATCH_NULL);
    if (!g_enabled) {
        TEKLOG_ERROR("gui: backend incomplete (sb=%d begin=%d begin8=%d end=%d bg=%d text=%d)",
                     f_sprite_batch != PATCH_NULL, m_sb_begin != PATCH_NULL,
                     m_sb_begin8 != PATCH_NULL, m_sb_end != PATCH_NULL,
                     m_inv_bg != PATCH_NULL, m_border_string != PATCH_NULL);
        /* 后端不可用：回收刚初始化的回调表与延迟释放队列 */
        tefstd_vector_destroy(&g_entries);
        g_entries_ready = false;
        if (g_deferred_ready) {
            tefstd_vector_destroy(&g_deferred_releases);
            g_deferred_ready = false;
        }
        return;
    }

    patch_handle_t draw = patchlib_type_get_method_by_param_count(t_main, "Draw", 1);
    if (!patchlib_is_valid(draw))
        draw = patchlib_type_get_method(t_main, "Draw");
    if (patchlib_is_valid(draw)) {
        g_draw_hook = patchlib_install_prepost_hook(draw, NULL, frame_postfix);
        TEKLOG_INFO("gui: ready (draw hook=%d, tex=%d)", (int) g_draw_hook,
                    m_draw_tex != PATCH_NULL);
    } else {
        TEKLOG_ERROR("gui: Main.Draw not found");
        g_enabled = false;
        tefstd_vector_destroy(&g_entries);
        g_entries_ready = false;
        if (g_deferred_ready) {
            tefstd_vector_destroy(&g_deferred_releases);
            g_deferred_ready = false;
        }
    }
}

void terraria_gui_shutdown(void) {
    if (g_draw_hook != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(g_draw_hook);
        g_draw_hook = PATCH_HOOK_INVALID_ID;
    }
    if (g_entries_ready) {
        tefstd_vector_destroy(&g_entries);
        g_entries_ready = false;
    }
    terraria_gui_end_frame_cleanup();
    if (g_deferred_ready) {
        tefstd_vector_destroy(&g_deferred_releases);
        g_deferred_ready = false;
    }
    for (int i = 0; i < g_text_count; ++i) {
#if defined(__ANDROID__)
        if (g_text_cache[i].pin && il2cpp_gchandle_free) {
            il2cpp_gchandle_free(g_text_cache[i].pin);
        }
#endif
        if (g_text_cache[i].handle) patchlib_free(g_text_cache[i].handle);
        if (g_text_cache[i].text) free(g_text_cache[i].text);
        g_text_cache[i].text = NULL;
        g_text_cache[i].handle = PATCH_NULL;
        g_text_cache[i].pin = NULL;
    }
    g_text_count = 0;
    g_enabled = false;
}