/*******************************************************************************
 * tefkernel - item_manager
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

#include "terraria/content/item_manager.h"                 // include_abi，不许改
#include "terraria/content/item_registry.hpp"
#include "terraria/abi/guard.hpp"

namespace {
    using terraria::abi::guard;
    using terraria::abi::guard_void;
    using terraria::content::item_registry;
} // namespace

extern "C" {
bool terraria_item_manager_register_item(terraria_item_handle_t *item_handle) {
    return guard([&] {
        return item_registry::instance().register_item(item_handle);
    }, false);
}

bool terraria_item_manager_unregister_item_by_id(const int runtime_id) {
    return guard([&] {
        return item_registry::instance().unregister_item_by_id(runtime_id);
    }, false);
}

bool terraria_item_manager_unregister_item(terraria_item_handle_t *item_handle) {
    return guard([&] {
        return item_registry::instance().unregister_item(item_handle);
    }, false);
}

terraria_item_handle_t *terraria_item_manager_get_item(const int runtime_id) {
    return guard([&] {
        return item_registry::instance().get_item(runtime_id);
    }, static_cast<terraria_item_handle_t *>(nullptr));
}

tefstd_vector_t *terraria_item_manager_get_items(void) {
    return guard([&] {
        return item_registry::instance().get_items();
    }, static_cast<tefstd_vector_t *>(nullptr));
}

void terraria_item_manager_apply_prefix_classes(void) {
    guard_void([] {
        item_registry::instance().apply_prefix_classes();
    });
}

void terraria_item_manager_set_prefix_class(terraria_item_handle_t *item_handle,
                                            const int prefix_class) {
    guard_void([&] {
        item_registry::instance().set_prefix_class(item_handle, prefix_class);
    });
}

int terraria_item_manager_reserve_accessory_slot(const char *slot) {
    return guard([&] {
        return item_registry::reserve_accessory_slot(slot);
    }, -1);
}

bool terraria_item_manager_set_accessory_texture(const char *slot, const int index, patch_handle_t asset) {
    return guard([&] {
        return item_registry::instance().set_accessory_texture(slot, index, asset);
    }, false);
}

bool terraria_item_manager_accessory_texture_ready(const char *slot, const int index) {
    return guard([&] {
        return item_registry::accessory_texture_ready(slot, index);
    }, false);
}

bool terraria_item_manager_set_wing_stats(const int index, const int fly_time, const float fly_speed,
                                          const float acceleration, const bool hover,
                                          const float hover_speed, const float hover_acceleration) {
    return guard([&] {
        return item_registry::set_wing_stats(
            index, fly_time, fly_speed, acceleration,
            hover, hover_speed, hover_acceleration);
    }, false);
}
} // extern "C"
