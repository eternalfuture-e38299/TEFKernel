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

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "../init/bootstrap.hpp"
#include "tef/patchlib/handle.hpp"
#include "tef/patchlib/type.hpp"
#include "tef/patchlib/field.hpp"
#include "tef/patchlib/method.hpp"
#include "terraria/gui.h"   // tef_gui_frame_t / tef_gui_draw_cb（ABI）

namespace terraria::content {
    class gui_overlay final : public init::subsystem {
    public:
        static gui_overlay &instance();

        [[nodiscard]] std::string_view name() const override { return "gui"; }

        void early_init() override;
        void on_main_initialize_postfix() override;
        void shutdown() override;

        // ===== ABI 转发 =====
        void add_draw(tef_gui_draw_cb cb, void *user);
        void remove_draw(tef_gui_draw_cb cb);

        void rect(float x, float y, float w, float h, unsigned rgba) const;
        void text(const char *text, float x, float y, float scale, unsigned rgba) const;
        void string_text(patch_handle_t text, float x, float y, float scale, unsigned rgba) const;
        void image(patch_handle_t texture, float x, float y, float w, float h, unsigned rgba) const;
        void draw_string(patch_handle_t font, const char *text, float x, float y,
                         float scale, unsigned rgba) const;
        void image_raw(patch_handle_t texture, float x, float y, float w, float h,
                       unsigned raw_color) const;

        void begin_external(void *sprite_batch) const;
        void end_external() const;

    private:
        gui_overlay() = default;

        struct entry { tef_gui_draw_cb cb = nullptr; void *user = nullptr; };

        struct text_entry {
            std::string text;
            tef::patchlib::handle handle;
            void *pin = nullptr;     // Android: il2cpp GCHandle
            uint64_t last_used = 0;
            uint32_t hash = 0;
        };

        struct deferred_release {
            std::string text;
            tef::patchlib::handle handle;
            void *pin = nullptr;
        };

        static void frame_postfix(patch_handle_t this_, void **args, void *result,
                                  const patch_method_signature_t *sig);

        bool ensure_batch() const;
        bool sb_begin() const;
        void sb_end() const;

        tef::patchlib::handle text_handle(const char *text, void **temp_pin);
        void release_temp(void *temp_pin);
        void end_frame_cleanup();

        // 帧状态
        mutable bool in_draw_ = false;
        mutable bool batch_open_ = false;
        mutable void *sb_ = nullptr;
        tef_gui_frame_t frame_{};
        bool prev_mouse_down_ = false;

        // 回调表
        std::vector<entry> entries_;
        bool entries_ready_ = false;

        // 文字 LRU
        static constexpr size_t k_text_cache_size = 512;
        std::vector<text_entry> text_cache_;
        size_t text_cache_count_ = 0;
        uint64_t lru_clock_ = 0;
        std::vector<deferred_release> deferred_;

        // 句柄
        tef::patchlib::type   t_main_;
        tef::patchlib::field  f_sprite_batch_;
        tef::patchlib::method m_sb_begin_;
        tef::patchlib::method m_sb_begin8_;
        tef::patchlib::method m_sb_end_;
        tef::patchlib::method m_inv_bg_;
        tef::patchlib::method m_draw_string_;
        tef::patchlib::method m_border_string_;
        tef::patchlib::method m_draw_tex_;
        tef::patchlib::field  f_blend_alpha_;
        tef::patchlib::method m_get_blend_alpha_;
        mutable tef::patchlib::handle g_blend_;

        tef::patchlib::field  a_screen_w_, a_screen_h_;
        tef::patchlib::field  a_mouse_x_, a_mouse_y_;
        tef::patchlib::field  a_mouse_left_, a_mouse_left_release_;
        tef::patchlib::field  a_show_splash_;
        tef::patchlib::field  a_game_menu_, a_options_window_;
        tef::patchlib::field  a_inventory_, a_fancy_ui_;
        tef::patchlib::method m_get_screen_w_, m_get_screen_h_;
        tef::patchlib::method m_get_mouse_x_, m_get_mouse_y_;
        tef::patchlib::method m_get_mouse_left_, m_get_mouse_left_release_;
        tef::patchlib::method m_get_show_splash_;
        tef::patchlib::method m_get_game_menu_, m_get_options_window_;
        tef::patchlib::method m_get_inventory_, m_get_fancy_ui_;

        patch_hook_id_t h_draw_ = PATCH_HOOK_INVALID_ID;
        bool enabled_ = false;
    };
}