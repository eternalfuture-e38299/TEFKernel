/*******************************************************************************
 * tefkernel - array
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
 * Created: 2026/1/10
 *******************************************************************************/

#include "patchlib/struct/array.h"

#include "../net_api.h"
#include "patchlib/type.h"

patch_handle_t patchlib_array_create(const size_t size, const patch_handle_t type) {
    if (size <= 0 || !patchlib_is_valid(type))
        return PATCH_NULL;

    return net_array_create(size, type);
}

bool patchlib_array_at(const patch_handle_t array, const size_t index, void* out_value, patch_type_t value_type) {
    if (!patchlib_is_valid(array))
        return false;

    return net_array_at(array, (int)index, out_value);
}

bool patchlib_array_set(const patch_handle_t array, const size_t index, void* new_value, const patch_type_t value_type) {
    if (!patchlib_is_valid(array))
        return false;

    return net_array_set(array, (int)index, new_value, value_type);
}

bool patchlib_array_fill(const patch_handle_t array, void* value, const patch_type_t value_type) {
    if (!value || !patchlib_is_valid(array))
        return false;

    return net_array_fill(array, value, value_type);
}

bool patchlib_array_empty(const patch_handle_t array) {
    return patchlib_array_length(array) <= 0;
}

size_t patchlib_array_length(const patch_handle_t array) {
    if (!patchlib_is_valid(array))
        return -1;

    return net_array_length(array);
}

bool patchlib_array_clear(patch_handle_t array, patch_type_t value_type) {
    if (!patchlib_is_valid(array))
        return false;

    return net_array_clear(array);
}