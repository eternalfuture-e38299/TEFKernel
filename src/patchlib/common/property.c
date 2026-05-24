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

#include "internal/log.h"
#include "../il2cpp_api.h"
#include "patchlib/type.h"

const char* patchlib_property_get_name(patch_handle_t property) {
    TEKLOG_DEBUG("patchlib_property_get_name called: property=%p", property);

    if (!patchlib_is_valid(property)) {
        TEKLOG_WARN("Invalid property handle");
        return NULL;
    }

    const char* result = il2cpp_property_get_name(property);
    TEKLOG_DEBUG("Property name: %s", result ? result : "NULL");
    return result;
}

patch_handle_t patchlib_property_get_get_method(patch_handle_t property) {
    TEKLOG_DEBUG("patchlib_property_get_get_method called: property=%p", property);

    if (!patchlib_is_valid(property)) {
        TEKLOG_WARN("Invalid property handle");
        return PATCH_NULL;
    }

    patch_handle_t result = il2cpp_property_get_get_method(property);
    TEKLOG_DEBUG("Property getter method: %p", result);
    return result;
}

patch_handle_t patchlib_property_get_set_method(patch_handle_t property) {
    TEKLOG_DEBUG("patchlib_property_get_set_method called: property=%p", property);

    if (!patchlib_is_valid(property)) {
        TEKLOG_WARN("Invalid property handle");
        return PATCH_NULL;
    }

    patch_handle_t result = il2cpp_property_get_set_method(property);
    TEKLOG_DEBUG("Property setter method: %p", result);
    return result;
}