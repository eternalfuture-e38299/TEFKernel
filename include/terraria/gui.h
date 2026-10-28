/*******************************************************************************
 * tefkernel - gui
 *
 * 内核级屏幕覆盖层：注册绘制回调，每帧在 Main.Draw 之后用 SpriteBatch 画。
 * 不依赖任何加载器；LuaLoader 的 GUI 与本模块互不影响。
 *
 * 用法（原生插件）：
 *   terraria_gui_add_draw(my_draw, user);
 *   static void my_draw(const tef_gui_frame_t *f, void *user) {
 *       terraria_gui_rect(10, 10, 64, 64, 0x80000000);
 *       terraria_gui_text("HP", 20, 20, 0.8f, 0xFFFFFFFF);
 *   }
 *
 * 坐标是屏幕像素（左上原点）。颜色为 0xAARRGGBB。
 *******************************************************************************/
#ifndef TEFFKERNEL_TERRARIA_GUI_H
#define TEFFKERNEL_TERRARIA_GUI_H

#include "../tef_api.h"
#include "../patchlib/type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tef_gui_frame_t {
    int   screen_w;      //<< 屏幕逻辑宽（Main.screenWidth）
    int   screen_h;      //<< 屏幕逻辑高（Main.screenHeight）
    float mouse_x;       //<< 指针 X（屏幕坐标）
    float mouse_y;       //<< 指针 Y
    bool  mouse_down;    //<< 按住
    bool  mouse_pressed; //<< 本帧刚按下
} tef_gui_frame_t;

typedef void (*tef_gui_draw_cb)(const tef_gui_frame_t *frame, void *user);

/**
 * @brief 注册每帧绘制回调（按注册顺序绘制）
 */
DEFINE_FUNCTION(void, terraria_gui_add_draw, tef_gui_draw_cb cb, void *user)

/**
 * @brief 移除绘制回调
 */
DEFINE_FUNCTION(void, terraria_gui_remove_draw, tef_gui_draw_cb cb)

/**
 * @brief 填充矩形（仅在绘制回调内有效）
 * @param rgba 0xAARRGGBB
 */
DEFINE_FUNCTION(void, terraria_gui_rect, float x, float y, float w, float h, unsigned rgba)

/**
 * @brief 画文字（仅在绘制回调内有效）
 */
DEFINE_FUNCTION(void, terraria_gui_text, const char *text, float x, float y, float scale,
                unsigned rgba)

/**
 * @brief 画贴图（Texture2D 句柄，仅在绘制回调内有效）
 */
DEFINE_FUNCTION(void, terraria_gui_image, patch_handle_t texture, float x, float y,
                float w, float h, unsigned rgba)

/* 用指定字体画文字（SpriteFont 句柄），无描边，与原版 Buff 计时文字一致 */
DEFINE_FUNCTION(void, terraria_gui_draw_string, patch_handle_t font, const char *text,
                float x, float y, float scale, unsigned rgba)

/* 同 image，但颜色按游戏 Color 的内存原值传（用于复制原版 UI 的 tint） */
DEFINE_FUNCTION(void, terraria_gui_image_raw, patch_handle_t texture, float x, float y,
                float w, float h, unsigned raw_color)

/**
 * @brief 借用游戏已开启的 SpriteBatch（不再自己 Begin/End）
 *
 * 适用于「在游戏 UI 绘制流程中」绘制：坐标变换、混合状态、显隐都与原生 UI 一致。
 * 调用 end_external 之前，terraria_gui_rect/text/image 都画到这个批次里。
 */
DEFINE_FUNCTION(void, terraria_gui_begin_external, void *sprite_batch)
DEFINE_FUNCTION(void, terraria_gui_end_external)

#ifdef __cplusplus
}
#endif

#endif /* TEFFKERNEL_TERRARIA_GUI_H */
