/*******************************************************************************
 * tefkernel - gui_overlay
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
 * Created: 2026/11/7
 *******************************************************************************/

#include "terraria/content/gui_overlay.hpp"

#include <cstring>

#include "tef/core/log.hpp"
#include "tef/patchlib/struct/string.hpp"
#include "abi_impl/patchlib/il2cpp_api.h"   // il2cpp_gchandle_new/free（Android 文字 pin）
#include "tef/patchlib/property.hpp"

namespace terraria::content {
    using namespace tef;

    namespace {
        struct v2_f {
            float x [[maybe_unused]], y [[maybe_unused]];
        };

        struct color4 {
            uint8_t r, g, b, a;
        };

        struct rect4 {
            int x [[maybe_unused]], y [[maybe_unused]], w [[maybe_unused]], h [[maybe_unused]];
        };

        struct nmat {
            uint8_t has;
            [[maybe_unused]] uint8_t pad[3];
            float m[16];
        };

        color4 premul_color4(color4 c) {
            if (const unsigned a = c.a; a != 255u) {
                c.r = static_cast<uint8_t>((static_cast<unsigned>(c.r) * a + 127u) / 255u);
                c.g = static_cast<uint8_t>((static_cast<unsigned>(c.g) * a + 127u) / 255u);
                c.b = static_cast<uint8_t>((static_cast<unsigned>(c.b) * a + 127u) / 255u);
            }
            return c;
        }

        color4 color_of(const unsigned rgba) {
            return {
                .r = static_cast<uint8_t>(rgba & 0xFF),
                .g = static_cast<uint8_t>((rgba >> 8) & 0xFF),
                .b = static_cast<uint8_t>((rgba >> 16) & 0xFF),
                .a = static_cast<uint8_t>((rgba >> 24) & 0xFF)
            };
        }

        uint32_t string_hash(const char *s) {
            uint32_t h = 5381;
            int c;
            while ((c = static_cast<unsigned char>(*s++))) h = ((h << 5) + h) + c;
            return h;
        }

        patchlib::field resolve_field(const patchlib::type &t, const char *name) {
            return t ? t.get_field(name) : patchlib::field{};
        }

        patchlib::method resolve_getter(const tef::patchlib::type &t, const char *name) {
            if (!t) return {};
            if (const auto prop = t.get_property(name)) return prop.get_get_method();
            return {};
        }

        int read_int(const patchlib::field &f, const patchlib::method &getter, const int fb) {
            if (f) return f.get_value<int>();
            if (getter) return getter.invoke<int>();
            return fb;
        }

        bool read_bool(const tef::patchlib::field &f, const patchlib::method &getter) {
            if (f) return f.get_value<bool>();
            if (getter) return getter.invoke<bool>();
            return false;
        }
    } // namespace

    gui_overlay &gui_overlay::instance() {
        static gui_overlay inst;
        return inst;
    }

    // -----------------------------------------------------------------------
    // 初始化
    // -----------------------------------------------------------------------

    void gui_overlay::early_init() {
        t_main_ = patchlib::type("Terraria", "Main");
        if (!t_main_) {
            log::error("gui: Terraria.Main not found");
            return;
        }

        f_sprite_batch_ = resolve_field(t_main_, "spriteBatch");
        if (!f_sprite_batch_) {
            log::error("gui: Main.spriteBatch not found");
            return;
        }

        a_screen_w_ = resolve_field(t_main_, "screenWidth");
        if (!a_screen_w_) m_get_screen_w_ = resolve_getter(t_main_, "screenWidth");
        a_screen_h_ = resolve_field(t_main_, "screenHeight");
        if (!a_screen_h_) m_get_screen_h_ = resolve_getter(t_main_, "screenHeight");

        a_mouse_x_ = resolve_field(t_main_, "mouseX");
        if (!a_mouse_x_) m_get_mouse_x_ = resolve_getter(t_main_, "mouseX");
        a_mouse_y_ = resolve_field(t_main_, "mouseY");
        if (!a_mouse_y_) m_get_mouse_y_ = resolve_getter(t_main_, "mouseY");
        a_mouse_left_ = resolve_field(t_main_, "mouseLeft");
        if (!a_mouse_left_) m_get_mouse_left_ = resolve_getter(t_main_, "mouseLeft");
        a_mouse_left_release_ = resolve_field(t_main_, "mouseLeftRelease");
        if (!a_mouse_left_release_)
            m_get_mouse_left_release_ = resolve_getter(t_main_, "mouseLeftRelease");
        a_show_splash_ = resolve_field(t_main_, "showSplash");
        if (!a_show_splash_) m_get_show_splash_ = resolve_getter(t_main_, "showSplash");

        a_game_menu_ = resolve_field(t_main_, "gameMenu");
        if (!a_game_menu_) m_get_game_menu_ = resolve_getter(t_main_, "gameMenu");
        a_options_window_ = resolve_field(t_main_, "ingameOptionsWindow");
        if (!a_options_window_) m_get_options_window_ = resolve_getter(t_main_, "ingameOptionsWindow");
        a_inventory_ = resolve_field(t_main_, "playerInventory");
        if (!a_inventory_) m_get_inventory_ = resolve_getter(t_main_, "playerInventory");
        a_fancy_ui_ = resolve_field(t_main_, "inFancyUI");
        if (!a_fancy_ui_) m_get_fancy_ui_ = resolve_getter(t_main_, "inFancyUI");

        // SpriteBatch
        if (const auto t_sb = patchlib::type("Microsoft.Xna.Framework.Graphics", "SpriteBatch")) {
            m_sb_begin_ = t_sb.get_method("Begin", 0);
            m_sb_begin8_ = t_sb.get_method("Begin", 8);
            m_sb_end_ = t_sb.get_method("End", 0);

            const auto t_tex = patchlib::type("Microsoft.Xna.Framework.Graphics", "Texture2D");
            const auto t_vec = patchlib::type("Microsoft.Xna.Framework", "Vector2");
            const auto t_rect = patchlib::type("Microsoft.Xna.Framework", "Rectangle");
            const auto t_col = patchlib::type("Microsoft.Xna.Framework.Graphics", "Color");
            const auto t_sgl = patchlib::type("System", "Single");
            const auto t_fx = patchlib::type("Microsoft.Xna.Framework.Graphics", "SpriteEffects");

            // Draw(Texture2D, Vector2, Rectangle?, Color, float, Vector2, Vector2, SpriteEffects, float)
            {
                const std::vector types = {
                    t_tex, t_vec, t_rect, t_col, t_sgl,
                    t_vec, t_vec, t_fx, t_sgl
                };
                const std::vector<std::string> names = {
                    "texture", "position", "srcRect", "color",
                    "rotation", "origin", "scale", "effects",
                    "layerDepth"
                };
                m_draw_tex_ = t_sb.get_method("Draw", types, names);
            }

            // DrawString(SpriteFont, String, Vector2, Color, float, Vector2, float, SpriteEffects, float)
            {
                const auto t_font = patchlib::type("Microsoft.Xna.Framework.Graphics", "SpriteFont");
                const auto t_str = patchlib::type("System", "String");
                const std::vector types = {
                    t_font, t_str, t_vec, t_col, t_sgl,
                    t_vec, t_sgl, t_fx, t_sgl
                };
                const std::vector<std::string> names = {
                    "spriteFont", "text", "position", "color",
                    "rotation", "origin", "scale", "spriteEffects",
                    "layerDepth"
                };
                m_draw_string_ = t_sb.get_method("DrawString", types, names);
            }
        }

        // BlendState.AlphaBlend
        if (const auto t_blend = patchlib::type("Microsoft.Xna.Framework.Graphics", "BlendState")) {
            f_blend_alpha_ = t_blend.get_field("AlphaBlend");
            if (!f_blend_alpha_) m_get_blend_alpha_ = resolve_getter(t_blend, "AlphaBlend");
        }

        // Utils
        if (const auto t_utils = patchlib::type("Terraria", "Utils")) {
            m_inv_bg_ = t_utils.get_method("DrawInvBG", 3);
            m_border_string_ = t_utils.get_method("DrawBorderString", 8);
        }

        enabled_ = f_sprite_batch_ && m_sb_end_ && m_inv_bg_ && m_border_string_ &&
                   (m_sb_begin_ || m_sb_begin8_);
        if (!enabled_) {
            log::error("gui: backend incomplete (sb={} begin={} begin8={} end={} bg={} text={})",
                       static_cast<bool>(f_sprite_batch_), static_cast<bool>(m_sb_begin_),
                       static_cast<bool>(m_sb_begin8_),
                       static_cast<bool>(m_sb_end_), static_cast<bool>(m_inv_bg_), static_cast<bool>(m_border_string_));
            return;
        }

        entries_.reserve(16);
        text_cache_.resize(k_text_cache_size);
        deferred_.reserve(32);
        entries_ready_ = true;
    }

    void gui_overlay::on_main_initialize_postfix() {
        if (!enabled_ || !t_main_) return;

        if (const auto draw = t_main_.get_method("Draw", 1)) {
            h_draw_ = patchlib_install_prepost_hook(
                draw.raw(), nullptr, frame_postfix);
            log::info("gui: ready (draw hook={}, tex={})", static_cast<int>(h_draw_), static_cast<bool>(m_draw_tex_));
        } else {
            log::error("gui: Main.Draw not found");
            enabled_ = false;
        }
    }

    void gui_overlay::shutdown() {
        end_frame_cleanup();
        for (auto &e: text_cache_) {
#if defined(__ANDROID__)
            if (e.pin && il2cpp_gchandle_free) il2cpp_gchandle_free(e.pin);
#endif
            e.handle.reset();
            e.text.clear();
        }
        text_cache_count_ = 0;
        entries_.clear();
        entries_ready_ = false;
        enabled_ = false;
    }

    // -----------------------------------------------------------------------
    // SpriteBatch（无平台分支，method::invoke 内部分流）
    // -----------------------------------------------------------------------

    bool gui_overlay::sb_begin() const {
        if (!sb_) return false;
        if (m_sb_begin8_) {
            static nmat ident;
            static bool init = false;
            if (!init) {
                init = true;
                ident.has = 1;
                ident.m[0] = ident.m[5] = ident.m[10] = ident.m[15] = 1.0f;
            }
            // 显式传 AlphaBlend；blend 传 null 会沿用上一笔的混合状态
            m_sb_begin8_.invoke<void>(0, g_blend_.raw(), nullptr, nullptr,
                                      nullptr, nullptr, &ident, true);
            return true;
        }
        if (m_sb_begin_) {
            m_sb_begin_.invoke<void>();
            return true;
        }
        return false;
    }

    void gui_overlay::sb_end() const {
        if (m_sb_end_ && sb_) m_sb_end_.invoke<void>();
    }

    bool gui_overlay::ensure_batch() const {
        if (batch_open_) return true;
        if (!sb_begin()) return false;
        batch_open_ = true;
        return true;
    }

    // -----------------------------------------------------------------------
    // 文字 LRU
    // -----------------------------------------------------------------------

    patchlib::handle gui_overlay::text_handle(const char *text, void **temp_pin) {
        if (temp_pin) *temp_pin = nullptr;
        if (!text || !*text) return {};

        const uint32_t hval = string_hash(text);
        ++lru_clock_;

        for (size_t i = 0; i < text_cache_count_; ++i) {
            if (auto &e = text_cache_[i]; e.hash == hval && e.text == text) {
                e.last_used = lru_clock_;
                return e.handle;
            }
        }

        patchlib::structs::string s(text);
        if (!s) return {};
        patchlib::handle h = s.release();
        void *pin = nullptr;
#if defined(__ANDROID__)
        if (il2cpp_gchandle_new) pin = il2cpp_gchandle_new(h.raw(), false);
#endif

        size_t slot;
        if (text_cache_count_ < k_text_cache_size) {
            slot = text_cache_count_++;
        } else {
            size_t lru = 0;
            uint64_t min_used = text_cache_[0].last_used;
            for (size_t i = 1; i < k_text_cache_size; ++i) {
                if (text_cache_[i].last_used < min_used) {
                    min_used = text_cache_[i].last_used;
                    lru = i;
                }
            }
            deferred_.push_back({
                .text = std::move(text_cache_[lru].text),
                .handle = std::move(text_cache_[lru].handle),
                .pin = text_cache_[lru].pin
            });
            text_cache_[lru].pin = nullptr;
            slot = lru;
        }

        auto &e = text_cache_[slot];
        e.text = text;
        e.handle = std::move(h);
        e.pin = pin;
        e.last_used = lru_clock_;
        e.hash = hval;
        return e.handle;
    }

    void gui_overlay::release_temp(void *temp_pin) {
        if (!temp_pin) return;
        deferred_.push_back({.text = std::string(), .handle = patchlib::handle{}, .pin = temp_pin});
    }

    void gui_overlay::end_frame_cleanup() {
        for (auto &d: deferred_) {
#if defined(__ANDROID__)
            if (d.pin && il2cpp_gchandle_free) il2cpp_gchandle_free(d.pin);
#endif
            d.handle.reset();
        }
        deferred_.clear();
    }

    // -----------------------------------------------------------------------
    // 绘制原语（统一走 method::invoke，无 #if 直调）
    // -----------------------------------------------------------------------

    void gui_overlay::rect(const float x, const float y, const float w, const float h, const unsigned rgba) const {
        if (!in_draw_ || !sb_ || !m_inv_bg_ || w <= 0 || h <= 0) return;
        if (!ensure_batch()) return;
        const color4 c = color_of(rgba);
        const rect4 r = {
            .x = static_cast<int>(x), .y = static_cast<int>(y), .w = static_cast<int>(w), .h = static_cast<int>(h)
        };
        m_inv_bg_[patchlib::handle{}](sb_, r, c);
    }

    void gui_overlay::text(const char *text, const float x, const float y, const float scale,
                           const unsigned rgba) const {
        if (!in_draw_ || !sb_ || !m_border_string_ || !text || !*text) return;
        auto &self = const_cast<gui_overlay &>(*this);
        void *temp_pin = nullptr;
        const auto sh = self.text_handle(text, &temp_pin);
        if (!sh) return;
        if (!ensure_batch()) {
            self.release_temp(temp_pin);
            return;
        }
        const v2_f pos = {.x = x, .y = y};
        const color4 c = premul_color4(color_of(rgba));
        const float sc = scale <= 0 ? 1.0f : scale;
        m_border_string_[patchlib::handle{}](sb_, sh, pos, c, sc, 0.0f, 0.0f, -1);
        self.release_temp(temp_pin);
    }

    void gui_overlay::string_text(patch_handle_t text, const float x, const float y, const float scale,
                                  const unsigned rgba) const {
        if (!in_draw_ || !sb_ || !m_border_string_ || !text) return;
        const patchlib::handle sh(text, false);
        const v2_f pos = {.x = x, .y = y};
        const color4 c = premul_color4(color_of(rgba));
        const float sc = scale <= 0 ? 1.0f : scale;
        m_border_string_[patchlib::handle{}](sb_, sh, pos, c, sc, 0.0f, 0.0f, -1);
    }

    void gui_overlay::image(patch_handle_t texture, const float x, const float y, const float w, const float h,
                            const unsigned rgba) const {
        if (!in_draw_ || !sb_ || !m_draw_tex_ || !texture || w <= 0 || h <= 0) return;
        if (!ensure_batch()) return;

        const patchlib::handle tex(texture, false);
        const v2_f pos = {.x = x, .y = y};
        constexpr v2_f origin = {.x = 0.0f, .y = 0.0f};
        const v2_f scale = {.x = w / 32.0f, .y = h / 32.0f};
        const color4 c = premul_color4(color_of(rgba));
        constexpr rect4 src = {.x = 0, .y = 0, .w = 32, .h = 32};
        m_draw_tex_[patchlib::handle{}](tex, pos, &src, c, 0.0f, origin, scale, 0, 0.0f);
    }

    void gui_overlay::draw_string(patch_handle_t font, const char *str, const float x, const float y,
                                  const float scale, const unsigned rgba) const {
        if (!in_draw_ || !sb_ || !font || !str || !*str) return;
        if (!m_draw_string_) {
            text(str, x, y, scale, rgba);
            return;
        }
        auto &self = const_cast<gui_overlay &>(*this);
        void *temp_pin = nullptr;
        const auto sh = self.text_handle(str, &temp_pin);
        if (!sh) return;
        if (!ensure_batch()) {
            self.release_temp(temp_pin);
            return;
        }
        const patchlib::handle font_h(font, false);
        const v2_f pos = {.x = x, .y = y};
        constexpr v2_f origin = {.x = 0.0f, .y = 0.0f};
        const color4 c = premul_color4(color_of(rgba));
        const float sc = scale > 0 ? scale : 1.0f;
        constexpr float rot = 0.0f, depth = 0.0f;
        constexpr int effects = 0;
        m_draw_string_[patchlib::handle{}](font_h, sh, pos, c, rot, origin, sc, effects, depth);
        self.release_temp(temp_pin);
    }

    void gui_overlay::image_raw(patch_handle_t texture, const float x, const float y, const float w, const float h,
                                const unsigned raw_color) const {
        if (!in_draw_ || !sb_ || !m_draw_tex_ || !texture || w <= 0 || h <= 0) return;
        if (!ensure_batch()) return;

        const patchlib::handle tex(texture, false);
        const v2_f pos = {.x = x, .y = y};
        constexpr v2_f origin = {.x = 0.0f, .y = 0.0f};
        const v2_f scale = {.x = w / 32.0f, .y = h / 32.0f};
        color4 c{};
        std::memcpy(&c, &raw_color, sizeof(c));
        c = premul_color4(c);
        constexpr rect4 src = {.x = 0, .y = 0, .w = 32, .h = 32};
        m_draw_tex_[patchlib::handle{}](tex, pos, &src, c, 0.0f, origin, scale, 0, 0.0f);
    }

    // -----------------------------------------------------------------------
    // 外部批次
    // -----------------------------------------------------------------------

    void gui_overlay::begin_external(void *sprite_batch) const {
        if (!sprite_batch) return;
        sb_ = sprite_batch;
        in_draw_ = true;
        batch_open_ = true;
    }

    void gui_overlay::end_external() const {
        in_draw_ = false;
        batch_open_ = false;
        sb_ = nullptr;
    }

    // -----------------------------------------------------------------------
    // 回调注册
    // -----------------------------------------------------------------------

    void gui_overlay::add_draw(const tef_gui_draw_cb cb, void *user) {
        if (!cb || !entries_ready_) {
            if (!entries_ready_) log::error("gui: add_draw before init");
            return;
        }
        for (auto &e: entries_)
            if (e.cb == cb) {
                e.user = user;
                return;
            }
        entries_.push_back({.cb = cb, .user = user});
    }

    void gui_overlay::remove_draw(const tef_gui_draw_cb cb) {
        if (!cb || !entries_ready_) return;
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->cb == cb) {
                entries_.erase(it);
                return;
            }
        }
    }

    // -----------------------------------------------------------------------
    // 帧钩子
    // -----------------------------------------------------------------------

    void gui_overlay::frame_postfix(patch_handle_t, void **, void *,
                                    const patch_method_signature_t *) {
        auto &self = instance();
        if (self.in_draw_) return;
        if (!self.entries_ready_ || !self.f_sprite_batch_ || self.entries_.empty()) {
            self.end_frame_cleanup();
            return;
        }
        // 启动画面 / 加载阶段：贴图与字体未就绪，绘制会抛托管异常
        if (read_bool(self.a_show_splash_, self.m_get_show_splash_)) {
            self.end_frame_cleanup();
            return;
        }
        // 与原版 Buff 栏一致：菜单/背包/暂停/特殊 UI 打开时不画
        if (read_bool(self.a_game_menu_, self.m_get_game_menu_) ||
            read_bool(self.a_options_window_, self.m_get_options_window_) ||
            read_bool(self.a_inventory_, self.m_get_inventory_) ||
            read_bool(self.a_fancy_ui_, self.m_get_fancy_ui_)) {
            self.end_frame_cleanup();
            return;
        }

        const auto sb_h = self.f_sprite_batch_.get_value<patchlib::handle>();
        void *sb = sb_h.raw();
        if (!sb) {
            self.end_frame_cleanup();
            return;
        }

        self.frame_.screen_w = read_int(self.a_screen_w_, self.m_get_screen_w_, 0);
        self.frame_.screen_h = read_int(self.a_screen_h_, self.m_get_screen_h_, 0);
        self.frame_.mouse_x = static_cast<float>(read_int(self.a_mouse_x_, self.m_get_mouse_x_, 0));
        self.frame_.mouse_y = static_cast<float>(read_int(self.a_mouse_y_, self.m_get_mouse_y_, 0));
        self.frame_.mouse_down = read_bool(self.a_mouse_left_, self.m_get_mouse_left_);
        self.frame_.mouse_pressed =
                read_bool(self.a_mouse_left_release_, self.m_get_mouse_left_release_) &&
                !self.prev_mouse_down_;
        self.prev_mouse_down_ = self.frame_.mouse_down;

        // AlphaBlend 实例
        if (self.f_blend_alpha_) self.g_blend_ = self.f_blend_alpha_.get_value<patchlib::handle>();
        else if (self.m_get_blend_alpha_) self.g_blend_ = self.m_get_blend_alpha_.invoke<patchlib::handle>();

        self.sb_ = sb;
        self.in_draw_ = true;
        self.batch_open_ = false;

        // 快照遍历
        for (const auto snapshot = self.entries_; const auto &[cb, user]: snapshot) if (cb) cb(&self.frame_, user);

        if (self.batch_open_) self.sb_end();
        self.batch_open_ = false;
        self.in_draw_ = false;
        self.sb_ = nullptr;

        self.end_frame_cleanup();
    }
}
