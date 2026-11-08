/*******************************************************************************
 * tefkernel - property
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
 * Created: 2026/5/24
 *******************************************************************************/

#include "patchlib/property.h"

#include "tef/core/log.hpp"

#include "../il2cpp_api.h"


using tef::log::debug;
using tef::log::warn;

/* ============================================================================
 * 导出函数
 * ============================================================================ */

extern "C" const char *patchlib_property_get_name(patch_handle_t property) {
    debug("patchlib_property_get_name: property={}", fmt::ptr(property));

    if (!patchlib_is_valid(property)) {
        warn("Invalid property handle");
        return nullptr;
    }

    const char *result = il2cpp_property_get_name(property);
    debug("Property name: {}", result ? result : "NULL");
    return result;
}

extern "C" patch_handle_t patchlib_property_get_get_method(patch_handle_t property) {
    debug("patchlib_property_get_get_method: property={}", fmt::ptr(property));

    if (!patchlib_is_valid(property)) {
        warn("Invalid property handle");
        return nullptr;
    }

    patch_handle_t result = il2cpp_property_get_get_method(property);
    debug("Property getter method: {}", fmt::ptr(result));
    return result;
}

extern "C" patch_handle_t patchlib_property_get_set_method(patch_handle_t property) {
    debug("patchlib_property_get_set_method: property={}", fmt::ptr(property));

    if (!patchlib_is_valid(property)) {
        warn("Invalid property handle");
        return nullptr;
    }

    patch_handle_t result = il2cpp_property_get_set_method(property);
    debug("Property setter method: {}", fmt::ptr(result));
    return result;
}
