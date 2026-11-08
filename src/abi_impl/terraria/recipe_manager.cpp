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

#include "terraria/recipe_manager.h"                       // include_abi，不许改
#include "terraria/content/recipe_manager.hpp"
#include "tef/patchlib/handle.hpp"
#include "terraria/abi/guard.hpp"

namespace {
    using terraria::content::recipe_manager;
    using terraria::abi::guard;
    using terraria::abi::guard_void;
} // namespace

extern "C" {
patch_handle_t terraria_recipe_manager_get_current_recipe(void) {
    return guard([] {
        return recipe_manager::instance().get_current_recipe().release();
    }, static_cast<patch_handle_t>(nullptr));
}

void terraria_recipe_set_result(const int item_id, const int stack) {
    guard_void([&] { recipe_manager::instance().set_result(item_id, stack); });
}

void terraria_recipe_set_material(const int *materials, const int count) {
    guard_void([&] { recipe_manager::instance().set_material(materials, count); });
}

void terraria_recipe_set_station(const int tile_id) {
    guard_void([&] { recipe_manager::instance().set_station(tile_id); });
}

void terraria_recipe_add(void) {
    guard_void([] { recipe_manager::instance().add(); });
}

void terraria_recipe_manager_register_callback(const terraria_recipe_manager_addrecipes_t callback) {
    guard_void([&] { recipe_manager::instance().register_callback(callback); });
}

bool terraria_recipe_manager_unregister_callback(const terraria_recipe_manager_addrecipes_t callback) {
    return guard([&] { return recipe_manager::instance().unregister_callback(callback); }, false);
}
} // extern "C"
