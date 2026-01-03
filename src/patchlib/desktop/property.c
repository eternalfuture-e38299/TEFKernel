/*******************************************************************************
 * tefkernel - property
 * Copyright (C) 2025 eternalfuture-e38299
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
 * Created: 2025/11/23
 *******************************************************************************/

#include "patchlib/property.h"

#include "net_api.h"
#include "internal/log.h"

const char* patchlib_property_get_name(const patch_handle_t property) {
    TEKLOG_DEBUG("patchlib_property_get_name called: property=%d", property);

    if (!patchlib_is_valid(property)) {
        TEKLOG_WARN("Invalid property handle");
        return NULL;
    }

    const char* result = net_property_get_name(property);
    TEKLOG_DEBUG("Property name: %s", result ? result : "NULL");
    return result;
}

patch_handle_t patchlib_property_get_get_method(const patch_handle_t property) {
    TEKLOG_DEBUG("patchlib_property_get_get_method called: property=%d", property);

    if (!patchlib_is_valid(property)) {
        TEKLOG_WARN("Invalid property handle");
        return PATCH_NULL;
    }

    const patch_handle_t result = net_property_get_get_method(property);
    TEKLOG_DEBUG("Property getter method: %d", result);
    return result;
}

patch_handle_t patchlib_property_get_set_method(const patch_handle_t property) {
    TEKLOG_DEBUG("patchlib_property_get_set_method called: property=%d", property);

    if (!patchlib_is_valid(property)) {
        TEKLOG_WARN("Invalid property handle");
        return PATCH_NULL;
    }

    const patch_handle_t result = net_property_get_set_method(property);
    TEKLOG_DEBUG("Property setter method: %d", result);
    return result;
}

bool patchlib_property_free(const patch_handle_t property) {
    TEKLOG_DEBUG("patchlib_property_free called: property=%d", property);

    if (!patchlib_is_valid(property)) {
        TEKLOG_WARN("Attempted to free invalid property");
        return false;
    }

    net_property_free(property);

    return true;
}