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
 * Created: 2026/5/24
 *******************************************************************************/

#include "patchlib/struct/string.h"

#include <stdint.h>

#include "internal/log.h"
#include "../../il2cpp_api.h"

patch_handle_t patchlib_string_create(const char* str) {
    TEKLOG_DEBUG("patchlib_string_create called: str=%s", str ? str : "NULL");

    if (!str) {
        TEKLOG_ERROR("Input string is NULL");
        return PATCH_NULL;
    }

    // 使用 il2cpp_string_new 创建字符串
    patch_handle_t result = il2cpp_string_new(str);

    if (!result) {
        TEKLOG_ERROR("Failed to create string");
        return PATCH_NULL;
    }

    TEKLOG_DEBUG("String created: %p", result);
    return result;
}

bool patchlib_string_empty(patch_handle_t str) {
    TEKLOG_DEBUG("patchlib_string_empty called: str=%p", str);

    if (!patchlib_is_valid(str)) {
        TEKLOG_WARN("Invalid string handle, treating as empty");
        return true;
    }

    const int32_t length = il2cpp_string_length(str);
    const bool result = (length == 0);

    TEKLOG_DEBUG("String empty: %s", result ? "true" : "false");
    return result;
}

size_t patchlib_string_length(patch_handle_t str) {
    TEKLOG_DEBUG("patchlib_string_length called: str=%p", str);

    if (!patchlib_is_valid(str)) {
        TEKLOG_ERROR("Invalid string handle");
        return 0;
    }

    const int32_t length = il2cpp_string_length(str);
    TEKLOG_DEBUG("String length: %d", length);
    return (size_t)length;
}
