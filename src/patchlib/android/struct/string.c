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
#include "../../il2cpp_api.h"

// 手动UTF-16到UTF-8转换
static char* convert_utf16_to_utf8_manual(const uint16_t* utf16_str, const size_t len) {
    if (!utf16_str || len == 0) return NULL;

    // 估算最大大小（UTF-8最多是UTF-16的3倍）
    // ReSharper disable once CppDFAMemoryLeak
    char* utf8_str = malloc(len * 3 + 1);
    if (!utf8_str) return NULL;

    char* p = utf8_str;

    for (size_t i = 0; i < len; i++) {
        uint32_t code_point = utf16_str[i];

        // 处理UTF-16代理对（如果需要）
        if (code_point >= 0xD800 && code_point <= 0xDBFF && i + 1 < len) {
            uint32_t low_surrogate = utf16_str[i + 1];
            if (low_surrogate >= 0xDC00 && low_surrogate <= 0xDFFF) {
                code_point = 0x10000 + ((code_point - 0xD800) << 10) + (low_surrogate - 0xDC00);
                i++; // 跳过低位代理
            }
        }

        // 转换为UTF-8
        if (code_point <= 0x7F) {
            *p++ = (char)code_point;
        } else if (code_point <= 0x7FF) {
            *p++ = (char)(0xC0 | (code_point >> 6));
            *p++ = (char)(0x80 | (code_point & 0x3F));
        } else if (code_point <= 0xFFFF) {
            *p++ = (char)(0xE0 | (code_point >> 12));
            *p++ = (char)(0x80 | ((code_point >> 6) & 0x3F));
            *p++ = (char)(0x80 | (code_point & 0x3F));
        } else {
            *p++ = (char)(0xF0 | (code_point >> 18));
            *p++ = (char)(0x80 | ((code_point >> 12) & 0x3F));
            *p++ = (char)(0x80 | ((code_point >> 6) & 0x3F));
            *p++ = (char)(0x80 | (code_point & 0x3F));
        }
    }

    *p = '\0';
    TEKLOG_DEBUG("Manual conversion: %zu UTF-16 chars -> %zu UTF-8 bytes", len, p - utf8_str);
    return utf8_str;
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

    // 直接使用转换函数
    // ReSharper disable once CppDFAMemoryLeak
    char* utf8_str = convert_utf16_to_utf8_manual((const uint16_t*)wstr, len);
    if (!utf8_str) {
        TEKLOG_ERROR("UTF-16 to UTF-8 conversion failed");
        return NULL;
    }

    TEKLOG_DEBUG("String conversion successful: %s", utf8_str);
    return utf8_str;
}