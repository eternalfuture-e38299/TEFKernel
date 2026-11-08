/*******************************************************************************
 * tefkernel - recipe_manager
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

#include <string_view>
#include <vector>

#include "../init/bootstrap.hpp"
#include "tef/patchlib/handle.hpp"
#include "tef/patchlib/type.hpp"
#include "tef/patchlib/field.hpp"
#include "tef/patchlib/method.hpp"
#include "terraria/recipe_manager.h"   // 回调类型（ABI，不许改）

namespace terraria::content {
    class recipe_manager final : public init::subsystem {
    public:
        static recipe_manager &instance();

        [[nodiscard]] std::string_view name() const override { return "recipe"; }

        void early_init() override;
        void on_main_initialize_prefix() override;   // 装 AddRecipe / SetupRecipes 钩子
        void shutdown() override;

        // ===== 便捷 API（abi_impl 转发） =====
        [[nodiscard]] tef::patchlib::handle get_current_recipe() const;

        void set_result(int item_id, int stack);
        void set_material(const int *materials, int count) const;
        void set_station(int tile_id) const;
        void add();

        void register_callback(terraria_recipe_manager_addrecipes_t cb);
        bool unregister_callback(terraria_recipe_manager_addrecipes_t cb);

    private:
        recipe_manager() = default;

        void ensure_capacity();

#if !defined(__ANDROID__)
        static void resize_crafting_ui_recipe_y(int new_size);

        static void resize_new_crafting_ui_lookup(int new_size);
#endif

        void run_callbacks() const;

        static bool setup_recipes_prefix(patch_handle_t this_, void **args,
                                         const patch_method_signature_t *sig, void *result);
        static bool recipe_add_prefix(patch_handle_t this_, void **args,
                                      const patch_method_signature_t *sig, void *result);

#if defined(__ANDROID__)
        static bool find_recipes_prefix(patch_handle_t this_, void **args,
                                        const patch_method_signature_t *sig, void *result);
        static void find_recipes_postfix(patch_handle_t this_, void **args,
                                         void *result, const patch_method_signature_t *sig);
        void grow_available_recipe_arrays(int total) const;
#endif

        // ---- 句柄 ----
        tef::patchlib::type   recipe_cls_;
        tef::patchlib::method m_recipe_ctor_;
        tef::patchlib::method m_set_ingredients_;
        tef::patchlib::method m_set_station_;
        tef::patchlib::method m_add_recipe_;
        tef::patchlib::method m_set_defaults_;

        tef::patchlib::field f_current_recipe_;
        tef::patchlib::field f_create_item_;
        tef::patchlib::field f_max_recipes_;
        tef::patchlib::field f_num_recipes_;
        tef::patchlib::field f_stack_;
        tef::patchlib::field f_is_a_material_;
        tef::patchlib::field f_recipe_;
        tef::patchlib::field f_available_recipe_;

#if defined(__ANDROID__)
        tef::patchlib::field  f_available_recipe_y_;
        tef::patchlib::method m_get_available_recipe_;
        tef::patchlib::method m_set_available_recipe_;
        tef::patchlib::method m_get_focus_recipe_;
        tef::patchlib::method m_get_local_player_;
        tef::patchlib::method m_get_in_guide_menu_;
        tef::patchlib::method m_collect_items_;
        tef::patchlib::method m_meets_tile_;
        tef::patchlib::method m_meets_env_;
        tef::patchlib::method m_enough_items_;
        tef::patchlib::method m_add_available_;
        tef::patchlib::method m_refocus_;

        int  vanilla_max_recipes_ = 3700;
        int  old_focus_recipe_ = 0;
        int  last_added_beyond_ = -1;
        bool scan_beyond_max_ = false;
#endif

        std::vector<terraria_recipe_manager_addrecipes_t> callbacks_;

        patch_hook_id_t h_setup_recipes_ = PATCH_HOOK_INVALID_ID;
        patch_hook_id_t h_add_recipe_    = PATCH_HOOK_INVALID_ID;
#if defined(__ANDROID__)
        patch_hook_id_t h_find_recipes_  = PATCH_HOOK_INVALID_ID;
#endif
    };
}