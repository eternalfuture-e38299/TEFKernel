/*******************************************************************************
 * tefkernel - list
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
 * Created: 2026/1/11
 *******************************************************************************/

#include "patchlib/struct/list.h"
#include "../net_api.h"

patch_handle_t patchlib_list_create(const size_t capacity, const patch_handle_t type) {
    if (!patchlib_is_valid(type)) {
        return PATCH_NULL;
    }
    return net_list_create((int)capacity, type);
}

bool patchlib_list_copy_from(const patch_handle_t list, const patch_handle_t array) {
    if (!patchlib_is_valid(list) || !patchlib_is_valid(array)) {
        return false;
    }
    return net_list_copy_from(list, array);
}

bool patchlib_list_add(const patch_handle_t list, void* value) {
    if (!patchlib_is_valid(list) || !value) {
        return false;
    }
    return net_list_add(list, value);
}

bool patchlib_list_remove(const patch_handle_t list, void* value) {
    if (!patchlib_is_valid(list) || !value) {
        return false;
    }
    return net_list_remove(list, value);
}

bool patchlib_list_remove_at(const patch_handle_t list, const size_t index) {
    if (!patchlib_is_valid(list)) {
        return false;
    }
    return net_list_remove_at(list, (int)index);
}

bool patchlib_list_clear(const patch_handle_t list) {
    if (!patchlib_is_valid(list)) {
        return false;
    }
    return net_list_clear(list);
}

patch_handle_t patchlib_list_get_array(const patch_handle_t list) {
    if (!patchlib_is_valid(list)) {
        return PATCH_NULL;
    }
    return net_list_get_array(list);
}