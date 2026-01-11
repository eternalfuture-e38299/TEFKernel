/*******************************************************************************
 * tefkernel - string
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

#include "patchlib/struct/string.h"
#include "../net_api.h"

#include <string.h>


patch_handle_t patchlib_string_create(const char* str) {
    if (!str) {
        return net_string_create(NULL, 0);
    }
    return net_string_create(str, (int)strlen(str));
}

const wchar_t* patchlib_string_cstr16(const patch_handle_t str) {
    if (!patchlib_is_valid(str)) {
        return NULL;
    }
    return net_string_cstr16(str);
}

char* patchlib_string_cstr(const patch_handle_t str) {
    if (!patchlib_is_valid(str)) {
        return NULL;
    }
    return net_string_cstr(str);
}

bool patchlib_string_empty(const patch_handle_t str) {
    if (!patchlib_is_valid(str))
        return true;
    return net_string_empty(str);
}

size_t patchlib_string_length(const patch_handle_t str) {
    if (!patchlib_is_valid(str))
        return 0;
    return (size_t)net_string_length(str);
}