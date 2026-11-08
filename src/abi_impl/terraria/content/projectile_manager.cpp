/*******************************************************************************
 * tefkernel - projectile_manager
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

#include "terraria/content/projectile_manager.h"                 // include_abi
#include "terraria/content/projectile_registry.hpp"
#include "terraria/abi/guard.hpp"

namespace {
    using terraria::abi::guard;
    using terraria::content::projectile_registry;
} // namespace

extern "C" {
bool terraria_projectile_manager_register_projectile(terraria_projectile_handle_t *projectile_handle) {
    return guard([&] { return projectile_registry::instance().register_projectile(projectile_handle); }, false);
}

bool terraria_projectile_manager_unregister_projectile_by_id(const int runtime_id) {
    return guard([&] { return projectile_registry::instance().unregister_by_id(runtime_id); }, false);
}

bool terraria_projectile_manager_unregister_projectile(terraria_projectile_handle_t *projectile_handle) {
    return guard([&] { return projectile_registry::instance().unregister(projectile_handle); }, false);
}

terraria_projectile_handle_t *terraria_projectile_manager_get_projectile(const int runtime_id) {
    return guard([&] { return projectile_registry::instance().get(runtime_id); },
                 static_cast<terraria_projectile_handle_t *>(nullptr));
}

tefstd_vector_t *terraria_projectile_manager_get_projectiles(void) {
    return guard([&] { return projectile_registry::instance().get_projectiles(); },
                 static_cast<tefstd_vector_t *>(nullptr));
}

terraria_projectile_handle_t *terraria_projectile_manager_find_projectile(
    const char *loader, const char *mod, const char *name) {
    return guard([&] {
        return projectile_registry::instance().find(loader ? loader : "",
                                                    mod ? mod : "",
                                                    name ? name : "");
    }, static_cast<terraria_projectile_handle_t *>(nullptr));
}

int terraria_projectile_id_get_count(void) {
    return guard([] { return projectile_registry::instance().id_count(); }, 0);
}
} // extern "C"
