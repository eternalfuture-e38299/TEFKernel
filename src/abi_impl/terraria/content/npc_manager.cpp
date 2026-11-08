/*******************************************************************************
 * tefkernel - npc_manager
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

#include "terraria/content/npc_manager.h"                 // include_abi
#include "terraria/content/npc_registry.hpp"
#include "terraria/abi/guard.hpp"

namespace {
    using terraria::abi::guard;
    using terraria::content::npc_registry;
} // namespace

extern "C" {
bool terraria_npc_manager_register_npc(terraria_npc_handle_t *npc_handle) {
    return guard([&] { return npc_registry::instance().register_npc(npc_handle); }, false);
}

bool terraria_npc_manager_unregister_npc_by_id(const int runtime_id) {
    return guard([&] { return npc_registry::instance().unregister_by_id(runtime_id); }, false);
}

bool terraria_npc_manager_unregister_npc(terraria_npc_handle_t *npc_handle) {
    return guard([&] { return npc_registry::instance().unregister(npc_handle); }, false);
}

terraria_npc_handle_t *terraria_npc_manager_get_npc(const int runtime_id) {
    return guard([&] { return npc_registry::instance().get(runtime_id); },
                 static_cast<terraria_npc_handle_t *>(nullptr));
}

tefstd_vector_t *terraria_npc_manager_get_npcs(void) {
    return guard([&] { return npc_registry::instance().get_npcs(); },
                 static_cast<tefstd_vector_t *>(nullptr));
}

terraria_npc_handle_t *terraria_npc_manager_find_npc(const char *loader, const char *mod,
                                                     const char *name) {
    return guard([&] {
        return npc_registry::instance().find(loader ? loader : "",
                                             mod ? mod : "",
                                             name ? name : "");
    }, static_cast<terraria_npc_handle_t *>(nullptr));
}

int terraria_npc_id_get_count(void) {
    return guard([] { return npc_registry::instance().id_count(); }, 0);
}

int terraria_npc_manager_spawn(const int runtime_id, const float x, const float y) {
    return guard([&] { return npc_registry::instance().spawn(runtime_id, x, y); }, -1);
}

int terraria_npc_manager_drop_item(const int item_id, const float x, const float y, const int count) {
    return guard([&] { return npc_registry::drop_item(item_id, x, y, count); }, -1);
}
} // extern "C"
