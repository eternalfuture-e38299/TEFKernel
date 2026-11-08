/*******************************************************************************
 * tefkernel - array
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
 * Modified: 2026/08/04 - Removed element size limit to support structs
 *******************************************************************************/

#include "patchlib/struct/array.h"
#include "patchlib/type.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "tef/core/log.hpp"

extern "C" {
#include "../../il2cpp_api.h"
}

using tef::log::debug;
using tef::log::error;

namespace {

/* Il2CppArray 头布局：m_class / m_monitor / m_bounds / m_length
 * 元素数据从 sizeof(Il2CppArray) 开始。 */
struct il2cpp_array {
    void*    m_class;
    // ReSharper disable once CppDeclaratorNeverUsed
    void*    m_monitor;
    // ReSharper disable once CppDeclaratorNeverUsed
    void*    m_bounds;
    // ReSharper disable once CppDeclaratorNeverUsed
    uint32_t m_length;
};

inline char* data_start(patch_handle_t array) {
    return static_cast<char*>(array) + sizeof(il2cpp_array);
}

inline const char* data_start_const(patch_handle_t array) {
    return static_cast<const char*>(array) + sizeof(il2cpp_array);
}

/* 元素大小 = byte_length / length */
size_t get_element_size(patch_handle_t array) {
    const uint32_t byte_length = il2cpp_array_get_byte_length(array);
    const uint32_t length = il2cpp_array_length(array);
    if (length == 0) return 0;

    const size_t size = byte_length / length;
    debug("Array element size: {} (len={}, bytes={})", size, length, byte_length);
    return size;
}

/* 1MB 上限保护（防止长度*大小溢出 / 误当数组） */
bool is_element_size_valid(size_t size) {
    if (size == 0) {
        error("Invalid element size: 0");
        return false;
    }
    if (size > 1024u * 1024u) {
        error("Element size too large: {} bytes (max 1MB)", size);
        return false;
    }
    return true;
}

} // namespace

extern "C" {

/* ============================================================================
 * 元素大小
 * ============================================================================ */

size_t patchlib_array_element_size(patch_handle_t array) {
    if (!patchlib_is_valid(array)) return 0;
    return get_element_size(array);
}

/* ============================================================================
 * 读写
 * ============================================================================ */

bool patchlib_array_at(patch_handle_t array, size_t index, void* out_value) {
    if (!patchlib_is_valid(array)) {
        error("Invalid array handle");
        return false;
    }
    if (!out_value) {
        error("Output value pointer is NULL");
        return false;
    }

    if (const uint32_t length = il2cpp_array_length(array); index >= length) {
        error("Index {} out of bounds (length={})", index, length);
        return false;
    }

    const size_t element_size = get_element_size(array);
    if (!is_element_size_valid(element_size)) return false;

    std::memcpy(out_value, data_start_const(array) + index * element_size, element_size);
    return true;
}

bool patchlib_array_set(patch_handle_t array, size_t index, void* new_value) {
    if (!patchlib_is_valid(array)) {
        error("Invalid array handle");
        return false;
    }
    if (!new_value) {
        error("New value pointer is NULL");
        return false;
    }

    if (const uint32_t length = il2cpp_array_length(array); index >= length) {
        error("Index {} out of bounds (length={})", index, length);
        return false;
    }

    const size_t element_size = get_element_size(array);
    if (!is_element_size_valid(element_size)) return false;

    std::memcpy(data_start(array) + index * element_size, new_value, element_size);
    return true;
}

/* ============================================================================
 * 填充
 *
 * 优化：1 字节 → memset；4/8 字节且值为 0 → memset 0；其余逐元素 memcpy。
 * ============================================================================ */

bool patchlib_array_fill(patch_handle_t array, void* value) {
    if (!patchlib_is_valid(array)) {
        error("Invalid array handle");
        return false;
    }
    if (!value) {
        error("Value pointer is NULL");
        return false;
    }

    const uint32_t length = il2cpp_array_length(array);
    const size_t element_size = get_element_size(array);
    if (!is_element_size_valid(element_size)) return false;

    char* data = data_start(array);

    if (element_size == sizeof(uint8_t)) {
        const uint8_t v = *static_cast<uint8_t*>(value);
        std::memset(data, v, length);
    } else if ((element_size == sizeof(uint32_t) &&
                *static_cast<uint32_t*>(value) == 0) ||
               (element_size == sizeof(uint64_t) &&
                *static_cast<uint64_t*>(value) == 0)) {
        std::memset(data, 0, length * element_size);
    } else {
        for (size_t i = 0; i < length; ++i)
            std::memcpy(data + i * element_size, value, element_size);
    }
    return true;
}

/* ============================================================================
 * 复制
 * ============================================================================ */

bool patchlib_array_copy_from_c(patch_handle_t dest, const void* src, size_t count) {
    if (!patchlib_is_valid(dest)) {
        error("Invalid destination array handle");
        return false;
    }
    if (!src) {
        error("Source C array is NULL");
        return false;
    }

    const uint32_t dest_len = il2cpp_array_length(dest);
    if (count > dest_len) {
        error("Count {} exceeds destination length {}", count, dest_len);
        return false;
    }

    const size_t element_size = get_element_size(dest);
    if (!is_element_size_valid(element_size)) return false;

    std::memcpy(data_start(dest), src, count * element_size);
    return true;
}

bool patchlib_array_copy_to_c(void* dest, patch_handle_t src, size_t count) {
    if (!dest) {
        error("Destination C array is NULL");
        return false;
    }
    if (!patchlib_is_valid(src)) {
        error("Invalid source array handle");
        return false;
    }

    if (const uint32_t src_len = il2cpp_array_length(src); count > src_len) {
        error("Count {} exceeds source length {}", count, src_len);
        return false;
    }

    const size_t element_size = get_element_size(src);
    if (!is_element_size_valid(element_size)) return false;

    std::memcpy(dest, data_start_const(src), count * element_size);
    return true;
}

bool patchlib_array_copy(patch_handle_t dest, patch_handle_t src, size_t count) {
    if (!patchlib_is_valid(dest)) {
        error("Invalid destination array handle");
        return false;
    }
    if (!patchlib_is_valid(src)) {
        error("Invalid source array handle");
        return false;
    }
    if (count == 0) return true;

    const uint32_t dest_len = il2cpp_array_length(dest);
    const uint32_t src_len = il2cpp_array_length(src);
    if (count > dest_len) {
        error("Count {} exceeds destination length {}", count, dest_len);
        return false;
    }
    if (count > src_len) {
        error("Count {} exceeds source length {}", count, src_len);
        return false;
    }

    const size_t dest_size = get_element_size(dest);
    const size_t src_size = get_element_size(src);
    if (!is_element_size_valid(dest_size) || !is_element_size_valid(src_size)) return false;
    if (dest_size != src_size) {
        error("Element size mismatch: dest={} src={}", dest_size, src_size);
        return false;
    }

    std::memcpy(data_start(dest), data_start_const(src), count * dest_size);
    return true;
}

/* ============================================================================
 * 扩容
 * ============================================================================ */

patch_handle_t patchlib_array_resize(patch_handle_t src, const size_t nsize, void* dvalue) {
    if (!src) {
        error("Source array handle is NULL");
        return nullptr;
    }

    const uint32_t osize = il2cpp_array_length(src);

    /* 数组类 = Il2CppArray 首字段 */
    patch_handle_t array_class =
        static_cast<il2cpp_array*>(src)->m_class;
    if (!array_class) {
        error("Failed to get array class");
        return nullptr;
    }

    const int element_size = il2cpp_array_element_size(array_class);
    if (element_size <= 0 || element_size > 1024 * 1024) {
        error("Invalid element size: {}", element_size);
        return nullptr;
    }

    patch_handle_t narray = il2cpp_array_new_specific(array_class, nsize);
    if (!narray) {
        error("Failed to create new array");
        return nullptr;
    }

    if (dvalue) patchlib_array_fill(narray, dvalue);

    if (osize > 0) {
        std::memcpy(data_start(narray), data_start_const(src),
                    static_cast<size_t>(osize) * element_size);
    }

    debug("Array resized: {} -> {} (element_size={})", osize, nsize, element_size);
    return narray;
}

}