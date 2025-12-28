/*******************************************************************************
 * tefkernel - string
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
 * Created: 2025/12/27
 *******************************************************************************/

#include "patchlib/struct/string.h"
#include "internal/log.h"

#include <stdlib.h>
#include <string.h>
#include "../il2cpp_api.h"

/*
typedef struct il2cpp_string_t {
    void *m_class;
    void *m_monitor;
    uint32_t length;
    wchar_t chars[0];
} il2cpp_string_t;
*/

patch_handle_t patchlib_string_create(const char* str) {
    TEKLOG_DEBUG("patchlib_string_create called: str=%s", str ? str : "NULL");

    if (!str) {
        TEKLOG_WARN("NULL string provided");
        return PATCH_NULL;
    }

    const size_t len = strlen(str);
    TEKLOG_DEBUG("Creating string from C string, length: %zu", len);

    patch_handle_t result = il2cpp_string_new(str);
    if (result) {
        TEKLOG_DEBUG("String created successfully: %p (length: %zu)", result, len);
    } else {
        TEKLOG_ERROR("Failed to create string");
    }

    return result;
}

const wchar_t* patchlib_string_cstr16(patch_handle_t str) {
    TEKLOG_DEBUG("patchlib_string_cstr16 called: str=%p", str);

    if (!patchlib_is_valid(str)) {
        TEKLOG_WARN("Invalid string handle");
        return NULL;
    }

    const wchar_t* result = il2cpp_string_chars(str);
    if (result) {
        const size_t len = patchlib_string_length(str);
        TEKLOG_DEBUG("Wide string pointer: %p, length: %zu", result, len);
    } else {
        TEKLOG_ERROR("Failed to get wide string pointer");
    }

    return result;
}

char* patchlib_string_cstr(patch_handle_t str) {
    TEKLOG_DEBUG("patchlib_string_cstr called: str=%p", str);

    if (!patchlib_is_valid(str)) {
        TEKLOG_WARN("Invalid string handle");
        return NULL;
    }

    const wchar_t* wstr = il2cpp_string_chars(str);
    if (!wstr) {
        TEKLOG_ERROR("Failed to get wide string characters");
        return NULL;
    }
    TEKLOG_DEBUG("Wide string pointer: %p", wstr);

    // 获取字符串长度
    const size_t len = patchlib_string_length(str);
    if (len == (size_t)-1) {
        TEKLOG_ERROR("Failed to get string length");
        return NULL;
    }
    TEKLOG_DEBUG("String length: %zu", len);

    // 分配内存
    char* utf8_str = malloc(len + 1);
    if (!utf8_str) {
        TEKLOG_ERROR("Memory allocation failed for UTF-8 string");
        return NULL;
    }
    TEKLOG_DEBUG("Allocated UTF-8 buffer: %p, size: %zu", utf8_str, len + 1);

    // 转换宽字符串到UTF-8
    const size_t converted = wcstombs(utf8_str, wstr, len + 1);
    if (converted == (size_t)-1) {
        TEKLOG_ERROR("Wide string to UTF-8 conversion failed");
        free(utf8_str);
        return NULL;
    }

    TEKLOG_DEBUG("String conversion successful: converted %zu characters", converted);
    TEKLOG_DEBUG("UTF-8 string: %s", utf8_str);

    return utf8_str;
}

char* patchlib_string_cstr_safe(patch_handle_t str) {
    TEKLOG_DEBUG("patchlib_string_cstr_safe called: str=%p", str);

    if (!patchlib_is_valid(str)) {
        TEKLOG_WARN("Invalid string handle");
        return NULL;
    }

    // 获取字符串长度
    const size_t len = patchlib_string_length(str);
    if (len == (size_t)-1 || len == 0) {
        TEKLOG_DEBUG("Empty string or invalid length");
        char* empty_str = malloc(1);
        if (empty_str) {
            empty_str[0] = '\0';
        }
        return empty_str;
    }

    const wchar_t* wstr = il2cpp_string_chars(str);
    if (!wstr) {
        TEKLOG_ERROR("Failed to get wide string characters");
        return NULL;
    }

    // 计算所需的缓冲区大小
    const size_t buffer_size = wcstombs(NULL, wstr, 0) + 1;
    if (buffer_size == (size_t)-1) {
        TEKLOG_ERROR("Failed to calculate required buffer size");
        return NULL;
    }
    TEKLOG_DEBUG("Required buffer size: %zu", buffer_size);

    char* utf8_str = malloc(buffer_size);
    if (!utf8_str) {
        TEKLOG_ERROR("Memory allocation failed for UTF-8 string");
        return NULL;
    }

    size_t converted = wcstombs(utf8_str, wstr, buffer_size);
    if (converted == (size_t)-1) {
        TEKLOG_ERROR("Wide string to UTF-8 conversion failed");
        free(utf8_str);
        return NULL;
    }

    TEKLOG_DEBUG("Safe string conversion successful: converted %zu characters", converted);
    return utf8_str;
}

bool patchlib_string_empty(patch_handle_t str) {
    TEKLOG_DEBUG("patchlib_string_empty called: str=%p", str);

    if (!patchlib_is_valid(str)) {
        TEKLOG_WARN("Invalid string handle, considering as empty");
        return true;
    }

    const size_t len = il2cpp_string_length(str);
    const bool result = (len == 0);
    TEKLOG_DEBUG("String empty check: length=%zu, empty=%s", len, result ? "true" : "false");

    return result;
}

size_t patchlib_string_length(patch_handle_t str) {
    TEKLOG_DEBUG("patchlib_string_length called: str=%p", str);

    if (!patchlib_is_valid(str)) {
        TEKLOG_WARN("Invalid string handle");
        return 0;
    }

    const size_t result = il2cpp_string_length(str);
    TEKLOG_DEBUG("String length: %zu", result);
    return result;
}