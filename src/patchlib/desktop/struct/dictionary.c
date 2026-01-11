/*******************************************************************************
 * tefkernel - dictionary
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

#include "patchlib/struct/dictionary.h"
#include "../net_api.h"

patch_handle_t patchlib_dictionary_create(const patch_handle_t key_type, const patch_handle_t value_type, const size_t capacity) {
    if (!patchlib_is_valid(key_type) || !patchlib_is_valid(value_type)) {
        return PATCH_NULL;
    }
    return net_dictionary_create(key_type, value_type, (int)capacity);
}

bool patchlib_dictionary_add(const patch_handle_t dictionary, void* key, void* value) {
    if (!patchlib_is_valid(dictionary) || !key || !value) {
        return false;
    }
    return net_dictionary_add(dictionary, key, value);
}

bool patchlib_dictionary_get_value(const patch_handle_t dictionary, void* key, void* out_value) {
    if (!patchlib_is_valid(dictionary) || !key || !out_value) {
        return false;
    }
    return net_dictionary_get_value(dictionary, key, out_value);
}

bool patchlib_dictionary_set_value(const patch_handle_t dictionary, void* key, void* value) {
    if (!patchlib_is_valid(dictionary) || !key || !value) {
        return false;
    }
    return net_dictionary_set_value(dictionary, key, value);
}

bool patchlib_dictionary_clear(const patch_handle_t dictionary) {
    if (!patchlib_is_valid(dictionary)) {
        return false;
    }
    return net_dictionary_clear(dictionary);
}

size_t patchlib_dictionary_length(const patch_handle_t dictionary) {
    if (!patchlib_is_valid(dictionary)) {
        return 0;
    }
    return (size_t)net_dictionary_length(dictionary);
}

bool patchlib_dictionary_remove(const patch_handle_t dictionary, void* key) {
    if (!patchlib_is_valid(dictionary) || !key) {
        return false;
    }
    return net_dictionary_remove(dictionary, key);
}