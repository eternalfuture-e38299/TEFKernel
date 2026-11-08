/*******************************************************************************
 * tefkernel - npc_registry
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

#include <string>
#include <string_view>

#include "entity_registry.hpp"
#include "../init/bootstrap.hpp"
#include "tef/patchlib/field.hpp"
#include "tef/patchlib/handle.hpp"
#include "tef/patchlib/method.hpp"
#include "tef/patchlib/type.hpp"
#include "terraria/content/npc_manager.h"   // ABI

namespace terraria::content {
    using namespace tef;

    class npc_registry final : public init::subsystem {
    public:
        static npc_registry &instance() noexcept;

        [[nodiscard]] std::string_view name() const noexcept override { return "npc"; }

        void early_init() override;
        void on_main_initialize_prefix() override;
        void on_main_initialize_postfix() override;
        void shutdown() override;

        // ===== ABI =====
        bool register_npc(terraria_npc_handle_t *h);
        bool unregister_by_id(int runtime_id);
        bool unregister(terraria_npc_handle_t *h);
        [[nodiscard]] terraria_npc_handle_t *get(int runtime_id) const;
        [[nodiscard]] tefstd_vector_t *get_npcs();
        [[nodiscard]] terraria_npc_handle_t *find(std::string_view loader,
                                                   std::string_view mod,
                                                   std::string_view name) const;
        [[nodiscard]] int id_count() const;
        [[nodiscard]] int spawn(int runtime_id, float x, float y) const;
        [[nodiscard]] static int drop_item(int item_id, float x, float y, int count);

        // ===== 内部 =====
        [[nodiscard]] int count() const { return static_cast<int>(registry_.size()); }
        bool assign_ids(int base);
        void resize();
        void init_texture2d();
        void init_localized_text();

    private:
        npc_registry() = default;

        [[nodiscard]] int get_npc_type(patch_handle_t inst) const;
        [[nodiscard]] terraria_npc_handle_t *custom_from_instance(patch_handle_t inst) const;
        void ensure_local_player_aggro_array() const;

        static void fill_new_slots_with_modal(const patchlib::handle &old_arr,
                                              const patchlib::handle &new_arr,
                                              size_t old_len, size_t new_len);

        static void resize_static_array(const patchlib::field &f, std::string_view name,
                                        int base, int new_size, bool modal,
                                        int int_fill, bool fill_with_int);

        static int  resize_sets_arrays(std::string_view ns, std::string_view cls,
                                       std::string_view inner, int base, int new_size,
                                       bool exclude_town_groups);

        static int  resize_all_static_arrays(std::string_view ns, std::string_view cls,
                                             int base, int new_size,
                                             const char *const *excludes, size_t n_excludes,
                                             bool modal);
        static void resize_player_arrays(int base, int new_size);
        static void resize_npc_name_cache(int base, int new_size);
        static void resize_scene_metrics(int base, int new_size);
        void resize_town_room_instance(int base, int new_size);

        static void resize_npc_set_factory(int new_size);

        void register_frames(const terraria_npc_handle_t *h, int type) const;

        // ---- 钩子（static 自由函数） ----
        static void set_defaults_postfix(patch_handle_t inst, void **args, void *result,
                                          const patch_method_signature_t *sig);
        static void town_room_ctor_postfix(patch_handle_t inst, void **args, void *result,
                                            const patch_method_signature_t *sig);
        static void npc_loot_postfix(patch_handle_t inst, void **args, void *result,
                                      const patch_method_signature_t *sig);
        static bool ai_prefix(patch_handle_t inst, void **args,
                              const patch_method_signature_t *sig, void *result);
        static void ai_postfix(patch_handle_t inst, void **args, void *result,
                                const patch_method_signature_t *sig);

#if defined(__ANDROID__)
        static void draw_npcs_postfix(patch_handle_t inst, void **args, void *result,
                                       const patch_method_signature_t *sig);
#endif

        // ---- 状态 ----
        entity_registry<terraria_npc_handle_t> registry_;
        bool ids_assigned_ = false;
        int base_count_ = 697;
        int type_offset_ = 0x164;

        patchlib::type   npc_class_;
        patchlib::field  f_type_, f_active_, f_npc_boss_;
        patchlib::field  f_npc_frame_count_, f_texture_npc_, f_npc_head_boss_;
        patchlib::field  f_boss_head_textures_;
        patchlib::field  f_npc_id_count_;
        patchlib::field  f_has_room_;
        patchlib::method m_set_defaults_, m_loot_, m_ai_, m_draw_direct_,
                         m_screen_pos_get_;

#if defined(__ANDROID__)
        patchlib::field f_npc_hide_, f_npc_behind_tiles_, f_main_sprite_batch_,
                        f_main_npc_arr_;
#endif
    };
}