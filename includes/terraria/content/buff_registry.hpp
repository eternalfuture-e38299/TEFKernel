/*******************************************************************************
 * tefkernel - buff_registry
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
 * Created: 2026/11/8
 *******************************************************************************/

#pragma once

#include <array>
#include <string>
#include <string_view>

#include "entity_registry.hpp"
#include "../init/bootstrap.hpp"
#include "tef/patchlib/field.hpp"
#include "tef/patchlib/handle.hpp"
#include "tef/patchlib/method.hpp"
#include "tef/patchlib/type.hpp"
#include "terraria/content/buff_manager.h"   // ABI

namespace terraria::content {
    using namespace tef;

    class buff_registry final : public init::subsystem {
    public:
        static buff_registry &instance() noexcept;

        [[nodiscard]] std::string_view name() const noexcept override { return "buff"; }

        void early_init() override;
        void on_main_initialize_prefix() override;
        void on_main_initialize_postfix() override;
        void shutdown() override;

        // ===== ABI =====
        bool register_buff(terraria_buff_handle_t *h);
        bool unregister_by_id(int runtime_id);
        bool unregister(terraria_buff_handle_t *h);
        [[nodiscard]] terraria_buff_handle_t *get(int runtime_id) const;
        [[nodiscard]] tefstd_vector_t *get_buffs();
        [[nodiscard]] terraria_buff_handle_t *find(std::string_view loader,
                                                    std::string_view mod,
                                                    std::string_view name) const;
        [[nodiscard]] int id_count() const { return 401; }

        void clear_debuffs(patch_handle_t player);
        void visit_active(patch_handle_t player,
                          terraria_buff_active_visitor_t visitor, void *user) const;
        int add(terraria_buff_handle_t *h, patch_handle_t player, int time);
        int time_of(terraria_buff_handle_t *h, patch_handle_t player) const;
        void remove(terraria_buff_handle_t *h, patch_handle_t player);

        // ===== 内部 =====
        [[nodiscard]] int count() const { return int(registry_.size()); }
        bool assign_ids(int base);
        void init_texture2d();
        void init_localized_text();

    private:
        buff_registry() = default;

        [[nodiscard]] terraria_buff_handle_t *find_by_runtime_id(int runtime_id) const;
        [[nodiscard]] int player_who(patch_handle_t player) const;
        [[nodiscard]] patchlib::handle local_player_obj() const;
        [[nodiscard]] int main_my_player() const;
        [[nodiscard]] int local_custom_count() const;
        void update_mouse_state();
        void draw_buff_tooltip(const terraria_buff_handle_t *h) const;
        void player_tick(patch_handle_t inst);
        void clear_active_of(int runtime_id);

        // 原生 Buff 栏
        static bool buffs_draw_prefix(patch_handle_t inst, void **args,
                                      const patch_method_signature_t *sig, void *result);
        static void buffs_draw_postfix(patch_handle_t inst, void **args, void *result,
                                       const patch_method_signature_t *sig);
        static void buff_item_draw_postfix(patch_handle_t inst, void **args, void *result,
                                           const patch_method_signature_t *sig);
        static bool item_grid_draw_prefix(patch_handle_t inst, void **args,
                                          const patch_method_signature_t *sig, void *result);
        static bool buff_item_draw_prefix(patch_handle_t inst, void **args,
                                          const patch_method_signature_t *sig, void *result);

        // PC Buff 栏
        static bool pc_draw_buff_icon_prefix(patch_handle_t inst, void **args,
                                             const patch_method_signature_t *sig, void *result);
        static bool pc_buffs_draw_prefix(patch_handle_t inst, void **args,
                                         const patch_method_signature_t *sig, void *result);
        static void pc_buffs_draw_postfix(patch_handle_t inst, void **args, void *result,
                                          const patch_method_signature_t *sig);

        // UpdateBuffs
        static void update_buffs_postfix(patch_handle_t inst, void **args, void *result,
                                         const patch_method_signature_t *sig);
        static void nurse_service_postfix(patch_handle_t inst, void **args, void *result,
                                          const patch_method_signature_t *sig);

        // ---- 状态 ----
        entity_registry<terraria_buff_handle_t> registry_;
        bool ids_assigned_ = false;

        // 活跃列表
        struct active_buff { int who = -1; int buff_id = -1; int time = 0; };
        static constexpr int kMaxActive = 32;
        std::array<active_buff, kMaxActive> active_{};
        int active_count_ = 0;

        // 引擎句柄
        patchlib::field  f_main_player_, f_sprite_batch_, f_main_my_player_;
        patchlib::method m_get_my_player_;
        patchlib::field  f_main_mouse_x_, f_main_mouse_y_,
                         f_main_mouse_left_, f_main_mouse_right_;
        patchlib::method m_get_mouse_x_, m_get_mouse_y_,
                         m_get_mouse_left_, m_get_mouse_right_;
        patchlib::field  f_player_buff_type_, f_player_who_am_i_;
        patchlib::field  f_buff_name_cache_, f_buff_desc_cache_;
        patchlib::method m_from_language_key_;
#if defined(__ANDROID__)
        patchlib::field f_main_inst_;
        patchlib::method m_pe_mouse_text_;
#endif

        // 布局
        patchlib::method m_layout_instance_;
        patchlib::field  f_layout_item_scale_, f_layout_selected_scale_,
                         f_layout_normal_colour_, f_layout_text_scale_,
                         f_layout_text_offset_;

        // 鼠标 / 帧号
        int  mx_ = -1, my_ = -1;
        bool left_down_ = false, right_down_ = false;
        bool left_tap_ = false, right_tap_ = false;
        std::array<float, 32> buff_alpha_{};
        int  last_tap_buff_ = -1;
        unsigned last_tap_frame_ = 0;
        unsigned frame_no_ = 0;

        // 原生 Buff 栏
        int  in_bar_ = 0;
        int  bar_base_ = -1;
        unsigned bar_epoch_ = 0;
        unsigned bar_mask_epoch_ = 0;
        unsigned long long bar_mask_ = 0;

        // PC Buff 栏
        int pc_icon_count_ = 0;
        int pc_icon_x_ = -1, pc_icon_y_ = -1;
        int pc_icon_prev_x_ = -1, pc_icon_prev_y_ = -1;
        int pc_first_x_ = -1, pc_first_y_ = -1;

        // Android 字体 / tooltip
        mutable patchlib::handle font_;
#if defined(__ANDROID__)
        patchlib::handle pe_sn_, pe_sd_;
        void *pe_pn_ = nullptr, *pe_pd_ = nullptr;
        int pe_id_ = -1;
#endif
    };
} // namespace terraria::content