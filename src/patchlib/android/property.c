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

#include "il2cpp_api.h"

const char* patchlib_property_get_name(patch_handle_t property) {
    if (!patchlib_is_valid(property))
        return false;

    return il2cpp_property_get_name(property);
}

patch_handle_t patchlib_property_get_get_method(patch_handle_t property) {
    if (!patchlib_is_valid(property))
        return PATCH_NULL;

    return il2cpp_property_get_get_method(property);
}

patch_handle_t patchlib_property_get_set_method(patch_handle_t property) {
    if (!patchlib_is_valid(property))
        return PATCH_NULL;

    return il2cpp_property_get_set_method(property);
}

uint8_t patchlib_free_property(patch_handle_t property) { return true; }