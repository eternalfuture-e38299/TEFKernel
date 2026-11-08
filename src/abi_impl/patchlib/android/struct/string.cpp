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
#include "patchlib/type.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "tef/core/log.hpp"

extern "C" {
#include "../../il2cpp_api.h"
}

using tef::log::debug;
using tef::log::error;
using tef::log::warn;

namespace {

/* ============================================================================
 * UTF-16 → UTF-8 手动转换
 *
 * Android 的 il2cpp_string_chars 返回 Il2CppChar*（UTF-16 code unit），
 * 但 C ABI 的 patchlib_string_cstr 返回 char*（UTF-8）——必须手动转换。
 *
 * 代理对处理：0xD800-0xDBFF + 0xDC00-0xDFFF → 0x10000 + ((hi - 0xD800) << 10)
 * + (lo - 0xDC00)，编码成 4 字节 UTF-8（emoji 等 BMP 外字符必须走这条）。
 *
 * 分配大小：len * 3 + 1（UTF-16 code unit 最多编成 3 字节 UTF-8；
 * 代理对是 2 个 code unit → 4 字节，2*3=6 > 4，覆盖足够）。
 * ============================================================================ */

char* utf16_to_utf8(const uint16_t* utf16, size_t len) {
    if (!utf16 || len == 0) return nullptr;

    const auto out = static_cast<char*>(std::malloc(len * 3 + 1));
    if (!out) return nullptr;

    char* p = out;
    for (size_t i = 0; i < len; ++i) {
        uint32_t cp = utf16[i];

        /* 代理对：hi(0xD800-0xDBFF) + lo(0xDC00-0xDFFF) → 0x10000+ */
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < len) {
            if (const uint32_t lo = utf16[i + 1]; lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                ++i;
            }
        }

        if (cp <= 0x7F) {
            *p++ = static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
            *p++ = static_cast<char>(0xC0 | (cp >> 6));
            *p++ = static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            *p++ = static_cast<char>(0xE0 | (cp >> 12));
            *p++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            *p++ = static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            *p++ = static_cast<char>(0xF0 | (cp >> 18));
            *p++ = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            *p++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            *p++ = static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    *p = '\0';
    debug("UTF-16 -> UTF-8: {} code units -> {} bytes",
          len, static_cast<size_t>(p - out));
    return out;
}

} // namespace

extern "C" {

char* patchlib_string_cstr(patch_handle_t str) {
    if (!patchlib_is_valid(str)) {
        warn("Invalid string handle");
        return nullptr;
    }

    const auto* wstr = static_cast<const wchar_t *>(il2cpp_string_chars(str));
    if (!wstr) {
        error("Failed to get wide string chars");
        return nullptr;
    }

    const size_t len = patchlib_string_length(str);
    if (len == static_cast<size_t>(-1)) {
        error("Failed to get string length");
        return nullptr;
    }

    char* utf8 = utf16_to_utf8(reinterpret_cast<const uint16_t*>(wstr), len);
    if (!utf8) {
        error("UTF-16 -> UTF-8 conversion failed");
        return nullptr;
    }
    return utf8;
}

}