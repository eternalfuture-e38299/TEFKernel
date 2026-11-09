/*******************************************************************************
 * tefkernel - projectile_registry
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
#include <string_view>

#include "entity_registry.hpp"
#include "../init/bootstrap.hpp"
#include "tef/patchlib/field.hpp"
#include "tef/patchlib/handle.hpp"
#include "tef/patchlib/method.hpp"
#include "tef/patchlib/type.hpp"
#include "terraria/content/projectile_manager.h"   // ABI

namespace terraria::content {
    using namespace tef;

    class projectile_registry final : public init::subsystem {
    public:
        static projectile_registry &instance() noexcept;

        [[nodiscard]] std::string_view name() const noexcept override { return "projectile"; }

        void early_init() override;
        void on_main_initialize_prefix() override;
        void on_main_initialize_postfix() override;
        void shutdown() override;

        // ===== ABI =====
        bool register_projectile(terraria_projectile_handle_t *h);
        bool unregister_by_id(int runtime_id);
        bool unregister(terraria_projectile_handle_t *h);
        [[nodiscard]] terraria_projectile_handle_t *get(int runtime_id) const;
        [[nodiscard]] tefstd_vector_t *get_projectiles();
        [[nodiscard]] terraria_projectile_handle_t *find(std::string_view loader,
                                                          std::string_view mod,
                                                          std::string_view name) const;
        [[nodiscard]] int id_count() const;

        // ===== 内部 =====
        [[nodiscard]] int count() const { return static_cast<int>(registry_.size()); }
        bool assign_ids(int base);
        void resize();
        void init_texture2d();
        void init_localized_text();

    private:
        projectile_registry() = default;

        [[nodiscard]] int get_projectile_type(patch_handle_t inst) const;
        void ensure_player_owned_counts(int new_size);

        static void ensure_projectile_ai_arrays(patch_handle_t inst);
        void clamp_static_immunity_identity(patch_handle_t inst);

        static void fill_new_slots_with_modal(const patchlib::handle &old_arr,
                                              const patchlib::handle &new_arr,
                                              size_t old_len, size_t new_len);

        static void resize_static_array(const patchlib::field &f, std::string_view name,
                                        int base, int new_size, bool modal,
                                        int int_fill, bool fill_with_int);

        void resize_projectile_set_factory(int new_size);

        void register_item_animation(const terraria_projectile_handle_t *h, int type) const;

        // ---- 钩子（static 自由函数） ----
        static void set_defaults_postfix(patch_handle_t inst, void **args, void *result,
                                         const patch_method_signature_t *sig);
        static bool ai_prefix(patch_handle_t inst, void **args,
                              const patch_method_signature_t *sig, void *result);
        static void ai_postfix(patch_handle_t inst, void **args, void *result,
                               const patch_method_signature_t *sig);
        static void kill_postfix(patch_handle_t inst, void **args, void *result,
                                 const patch_method_signature_t *sig);
        static bool damage_prefix(patch_handle_t inst, void **args,
                                  const patch_method_signature_t *sig, void *result);
        static void damage_postfix(patch_handle_t inst, void **args, void *result,
                                   const patch_method_signature_t *sig);
        static void strike_npc_postfix(patch_handle_t inst, void **args, void *result,
                                       const patch_method_signature_t *sig);
        static void hurt_postfix(patch_handle_t inst, void **args, void *result,
                                 const patch_method_signature_t *sig);
        static bool handle_movement_prefix(patch_handle_t inst, void **args,
                                           const patch_method_signature_t *sig, void *result);
        static void handle_movement_postfix(patch_handle_t inst, void **args, void *result,
                                            const patch_method_signature_t *sig);

        // ---- 状态 ----
        entity_registry<terraria_projectile_handle_t> registry_;
        bool ids_assigned_ = false;
        int base_count_ = 0;

        // 引擎句柄
        patchlib::type   projectile_class_;
        patchlib::field  f_type_, f_active_, f_velocity_;
        patchlib::field  f_texture_projectile_;
        patchlib::field  f_proj_frames_;
        patchlib::field  f_main_players_;
        patchlib::field  f_owned_counts_;
        patchlib::field  f_proj_name_cache_;
        patchlib::field  f_uses_id_static_, f_immunity_identity_;
        patchlib::field  f_projectile_id_count_;
        patchlib::method m_ai_, m_kill_, m_damage_, m_move_,
                         m_set_defaults_, m_strike_npc_, m_hurt_;

        int type_offset_ = 0x64;

        // Damage 浅栈
        static constexpr int k_max_damage_depth = 8;
        std::array<patchlib::handle, k_max_damage_depth> damage_stack_{};
        int damage_depth_ = 0;

        // HandleMovement 快照
        patchlib::handle move_projectile_;
        bool move_pending_ = false;
        float move_vx_ = 0.0f;
        float move_vy_ = 0.0f;

        // 玩家数组补齐
        mutable bool owned_counts_checked_ = false;
    };
}