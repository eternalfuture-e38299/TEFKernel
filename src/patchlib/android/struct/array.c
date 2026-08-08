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

#include <stdint.h>
#include <string.h>
#include "internal/log.h"
#include "../../il2cpp_api.h"

// 移除最大元素大小限制，支持任意大小的结构体
// #define MAX_ELEMENT_SIZE sizeof(void*)  // 已移除

// ReSharper disable once CppUseInternalLinkage
typedef struct il2cpp_array_t {
    // ReSharper disable once CppDeclaratorNeverUsed
    void *m_class;
    // ReSharper disable once CppDeclaratorNeverUsed
    void *m_monitor;
    // ReSharper disable once CppDeclaratorNeverUsed
    void *m_bounds;
    // ReSharper disable once CppDeclaratorNeverUsed
    uint32_t m_length;
    // T *m_values;
} il2cpp_array_t;


// 获取数组数据起始位置
#define ARRAY_DATA_START(array) ((char*)(array) + sizeof(il2cpp_array_t))

// 获取数组元素大小（通过字节长度和长度计算）
static size_t get_array_element_size(patch_handle_t array) {
    const uint32_t byte_length = il2cpp_array_get_byte_length(array);
    const uint32_t length = il2cpp_array_length(array);

    if (length == 0) {
        return 0;
    }

    const size_t element_size = byte_length / length;

    // 调试日志：输出元素大小
    TEKLOG_DEBUG("Array element size: %zu bytes (length=%u, byte_length=%u)",
                 element_size, length, byte_length);

    return element_size;
}

// 检查元素大小是否合法（移除最大限制）
static bool is_element_size_valid(size_t element_size) {
    if (element_size == 0) {
        TEKLOG_ERROR("Invalid element size: 0");
        return false;
    }
    // 只检查是否过大（防止溢出）
    if (element_size > 1024 * 1024) { // 1MB 上限保护
        TEKLOG_ERROR("Element size too large: %zu bytes (max: 1MB)", element_size);
        return false;
    }
    return true;
}

// 创建新数组（支持任意元素大小）
patch_handle_t patchlib_array_new(patch_handle_t element_type, const uint32_t length) {
    if (!element_type) {
        TEKLOG_ERROR("Element type is NULL");
        return NULL;
    }

    // 使用 IL2CPP API 创建数组
    patch_handle_t array = il2cpp_array_new(element_type, length);
    if (!array) {
        TEKLOG_ERROR("Failed to create array with length %u", length);
        return NULL;
    }

    TEKLOG_DEBUG("Created array: length=%u, element_size=%zu",
                 length, get_array_element_size(array));

    return array;
}

bool patchlib_array_at(patch_handle_t array, const size_t index, void* out_value) {
    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return false;
    }

    if (out_value == NULL) {
        TEKLOG_ERROR("Output value pointer is NULL");
        return false;
    }

    const uint32_t length = il2cpp_array_length(array);
    if (index >= length) {
        TEKLOG_ERROR("Index %zu out of bounds (length=%u)", index, length);
        return false;
    }

    const size_t element_size = get_array_element_size(array);
    if (!is_element_size_valid(element_size)) {
        return false;
    }

    const char* data_start = ARRAY_DATA_START(array);
    const char* element_ptr = data_start + (index * element_size);

    memcpy(out_value, element_ptr, element_size);
    return true;
}

bool patchlib_array_set(patch_handle_t array, const size_t index, void* new_value) {
    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return false;
    }

    if (!new_value) {
        TEKLOG_ERROR("New value pointer is NULL");
        return false;
    }

    const uint32_t length = il2cpp_array_length(array);
    if (index >= length) {
        TEKLOG_ERROR("Index %zu out of bounds (length=%u)", index, length);
        return false;
    }

    const size_t element_size = get_array_element_size(array);
    if (!is_element_size_valid(element_size)) {
        return false;
    }

    char* data_start = ARRAY_DATA_START(array);
    char* element_ptr = data_start + (index * element_size);

    memcpy(element_ptr, new_value, element_size);

    TEKLOG_DEBUG("Set array[%zu]: element_size=%zu", index, element_size);

    return true;
}

bool patchlib_array_fill(patch_handle_t array, void* value) {
    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return false;
    }

    if (!value) {
        TEKLOG_ERROR("Value pointer is NULL");
        return false;
    }

    const uint32_t length = il2cpp_array_length(array);
    const size_t element_size = get_array_element_size(array);

    if (!is_element_size_valid(element_size)) {
        return false;
    }

    char* data_start = ARRAY_DATA_START(array);

    // 优化：根据元素大小选择最优的填充方式
    if (element_size == sizeof(uint8_t)) {
        const uint8_t byte_value = *(uint8_t*)value;
        memset(data_start, byte_value, length);
    } else if (element_size == sizeof(uint64_t) && *(uint64_t*)value == 0 || element_size == sizeof(uint32_t) && *(uint32_t*)value == 0) {
        memset(data_start, 0, length * element_size);
    } else {
        for (size_t i = 0; i < length; ++i) {
            char* element_ptr = data_start + i * element_size;
            memcpy(element_ptr, value, element_size);
        }
    }

    return true;
}

bool patchlib_array_copy_from_c(patch_handle_t dest, const void* src, const size_t count) {
    if (!patchlib_is_valid(dest)) {
        TEKLOG_ERROR("Invalid destination array handle");
        return false;
    }

    if (!src) {
        TEKLOG_ERROR("Source C array is NULL");
        return false;
    }

    const uint32_t dest_len = il2cpp_array_length(dest);
    if (count > dest_len) {
        TEKLOG_ERROR("Count %zu exceeds destination length %u", count, dest_len);
        return false;
    }

    const size_t element_size = get_array_element_size(dest);
    if (!is_element_size_valid(element_size)) {
        return false;
    }

    char* dest_data = ARRAY_DATA_START(dest);
    memcpy(dest_data, src, count * element_size);

    TEKLOG_DEBUG("Copied %zu elements (size=%zu) to array", count, element_size);

    return true;
}

bool patchlib_array_copy_to_c(void* dest, patch_handle_t src, const size_t count) {
    if (!dest) {
        TEKLOG_ERROR("Destination C array is NULL");
        return false;
    }

    if (!patchlib_is_valid(src)) {
        TEKLOG_ERROR("Invalid source array handle");
        return false;
    }

    const uint32_t src_len = il2cpp_array_length(src);
    if (count > src_len) {
        TEKLOG_ERROR("Count %zu exceeds source length %u", count, src_len);
        return false;
    }

    const size_t element_size = get_array_element_size(src);
    if (!is_element_size_valid(element_size)) {
        return false;
    }

    const char* src_data = ARRAY_DATA_START(src);
    memcpy(dest, src_data, count * element_size);

    TEKLOG_DEBUG("Copied %zu elements (size=%zu) from array", count, element_size);

    return true;
}