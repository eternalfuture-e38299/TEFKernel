/*******************************************************************************
 * tefkernel - buff_manager
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

#include "terraria/content/buff_manager.h"
#include "terraria/content/buff_registry.hpp"
#include "terraria/abi/guard.hpp"

namespace {
    using terraria::abi::guard;
    using terraria::abi::guard_void;
    using terraria::content::buff_registry;
}

extern "C" {
bool terraria_buff_manager_register_buff(terraria_buff_handle_t *buff_handle) {
    return guard([&] { return buff_registry::instance().register_buff(buff_handle); }, false);
}

bool terraria_buff_manager_unregister_buff_by_id(const int runtime_id) {
    return guard([&] { return buff_registry::instance().unregister_by_id(runtime_id); }, false);
}

bool terraria_buff_manager_unregister_buff(terraria_buff_handle_t *buff_handle) {
    return guard([&] { return buff_registry::instance().unregister(buff_handle); }, false);
}

terraria_buff_handle_t *terraria_buff_manager_get_buff(const int runtime_id) {
    return guard([&] { return buff_registry::instance().get(runtime_id); },
                 static_cast<terraria_buff_handle_t *>(nullptr));
}

tefstd_vector_t *terraria_buff_manager_get_buffs(void) {
    return guard([&] { return buff_registry::instance().get_buffs(); },
                 static_cast<tefstd_vector_t *>(nullptr));
}

terraria_buff_handle_t *terraria_buff_manager_find_buff(const char *loader, const char *mod,
                                                        const char *name) {
    return guard([&] {
        return buff_registry::instance().find(loader ? loader : "",
                                              mod ? mod : "",
                                              name ? name : "");
    }, static_cast<terraria_buff_handle_t *>(nullptr));
}

int terraria_buff_id_get_count(void) {
    return guard([] { return buff_registry::instance().id_count(); }, 401);
}

void terraria_buff_manager_clear_debuffs(patch_handle_t player) {
    guard_void([&] { buff_registry::instance().clear_debuffs(player); });
}

void terraria_buff_manager_visit_active(patch_handle_t player,
                                        const terraria_buff_active_visitor_t visitor, void *user) {
    guard_void([&] { buff_registry::instance().visit_active(player, visitor, user); });
}

int terraria_buff_manager_add(terraria_buff_handle_t *buff_handle, patch_handle_t player, const int time) {
    return guard([&] { return buff_registry::instance().add(buff_handle, player, time); }, -1);
}

int terraria_buff_manager_time(terraria_buff_handle_t *buff_handle, patch_handle_t player) {
    return guard([&] { return buff_registry::instance().time_of(buff_handle, player); }, 0);
}

void terraria_buff_manager_remove(terraria_buff_handle_t *buff_handle, patch_handle_t player) {
    guard_void([&] { buff_registry::instance().remove(buff_handle, player); });
}
} // extern "C"
