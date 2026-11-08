/*******************************************************************************
 * tefkernel - language_manager
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

#include "terraria/language_manager.h"                         // include_abi，不许改
#include "terraria/content/language_manager.hpp"
#include "terraria/abi/guard.hpp"

namespace {
    using terraria::abi::guard;
    using terraria::abi::guard_void;
    using terraria::content::language_manager;
} // namespace

extern "C" {
patch_handle_t terraria_language_manager_create_localized_text(const char *key,
                                                               const char *value) {
    return guard([&] {
        return language_manager::instance().create_localized_text(key, value).release();
    }, static_cast<patch_handle_t>(nullptr));
}

patch_handle_t terraria_language_manager_get_localized_text(const char *key) {
    return guard([&] {
        return language_manager::instance().get_localized_text(key).release();
    }, static_cast<patch_handle_t>(nullptr));
}

void terraria_language_manager_register_callback(
    const terraria_languager_manager_get_localized_text_t callback) {
    guard_void([&] { language_manager::instance().register_callback(callback); });
}

bool terraria_language_manager_unregister_callback(
    const terraria_languager_manager_get_localized_text_t callback) {
    return guard([&] {
        return language_manager::instance().unregister_callback(callback);
    }, false);
}
}
