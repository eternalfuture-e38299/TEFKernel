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
 *******************************************************************************/

#include "patchlib/struct/array.h"
#include "internal/log.h"
#include <string.h>
#include "../il2cpp_api.h"

typedef struct il2cpp_array_t {
    void *m_class;
    void *m_monitor;
    void *m_bounds;
    uint32_t m_length;
    // T *m_values;
} il2cpp_array_t;

patch_handle_t patchlib_array_create (const size_t size, patch_handle_t type) {
    TEKLOG_DEBUG("patchlib_array_create called: size=%zu, type=%p", size, type);

    if (size <= 0) {
        TEKLOG_ERROR("Invalid array size: %zu", size);
        return PATCH_NULL;
    }

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle: %p", type);
        return PATCH_NULL;
    }

    TEKLOG_DEBUG("Creating array via il2cpp_array_new");
    patch_handle_t result = il2cpp_array_new(type, size);
    TEKLOG_DEBUG("Array created: %p, length should be: %zu", result, size);

    return result;
}

bool patchlib_array_at(patch_handle_t array, const size_t index, void* out_value, const patch_type_t value_type) {
    TEKLOG_DEBUG("patchlib_array_at called: array=%p, index=%zu, out_value=%p, value_type=%d",
                 array, index, out_value, value_type);

    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return false;
    }

    if (out_value == NULL) {
        TEKLOG_ERROR("Output value pointer is NULL");
        return false;
    }

    il2cpp_array_t* il2cpp_array = array;
    TEKLOG_DEBUG("Array info: m_length=%d", il2cpp_array->m_length);

    if (index >= (size_t)il2cpp_array->m_length) {
        TEKLOG_ERROR("Index out of bounds: index=%zu, length=%d", index, il2cpp_array->m_length);
        return false;
    }

    // 获取元素大小
    const size_t element_size = get_size_from_patch_type(value_type);
    TEKLOG_DEBUG("Element size for type %d: %zu", value_type, element_size);

    if (element_size == 0) {
        TEKLOG_ERROR("Unknown or unsupported value type: %d", value_type);
        return false;
    }

    const char* base_ptr = (char*)il2cpp_array;
    const size_t array_header_size = offsetof(il2cpp_array_t, m_length) + sizeof(il2cpp_array->m_length);
    const char* data_start = base_ptr + array_header_size;
    const char* element_ptr = data_start + (index * element_size);

    TEKLOG_DEBUG("Memory copy: from=%p, to=%p, size=%zu", element_ptr, out_value, element_size);
    memcpy(out_value, element_ptr, element_size);

    TEKLOG_DEBUG("Array element retrieved successfully");
    return true;
}

bool patchlib_array_set(patch_handle_t array, const size_t index, void* new_value, const patch_type_t value_type) {
    TEKLOG_DEBUG("patchlib_array_set called: array=%p, index=%zu, new_value=%p, value_type=%d",
                 array, index, new_value, value_type);

    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return false;
    }

    if (!new_value) {
        TEKLOG_ERROR("New value pointer is NULL");
        return false;
    }

    il2cpp_array_t* il2cpp_array = array;
    TEKLOG_DEBUG("Array info: m_length=%d", il2cpp_array->m_length);

    if (index >= (size_t)il2cpp_array->m_length) {
        TEKLOG_ERROR("Index out of bounds: index=%zu, length=%d", index, il2cpp_array->m_length);
        return false;
    }

    const size_t element_size = get_size_from_patch_type(value_type);
    TEKLOG_DEBUG("Element size for type %d: %zu", value_type, element_size);

    if (element_size == 0) {
        TEKLOG_ERROR("Unknown or unsupported value type: %d", value_type);
        return false;
    }

    const char* base_ptr = (char*)il2cpp_array;
    const size_t array_header_size = offsetof(il2cpp_array_t, m_length) + sizeof(il2cpp_array->m_length);
    const char* data_start = base_ptr + array_header_size;
    const char* element_ptr = data_start + (index * element_size);

    TEKLOG_DEBUG("Memory copy: from=%p, to=%p, size=%zu", new_value, element_ptr, element_size);
    memcpy((void*)element_ptr, new_value, element_size);

    TEKLOG_DEBUG("Array element set successfully");
    return true;
}

bool patchlib_array_fill(patch_handle_t array, void* value, const patch_type_t value_type) {
    TEKLOG_DEBUG("patchlib_array_fill called: array=%p, value=%p, value_type=%d",
                 array, value, value_type);

    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return false;
    }

    if (!value) {
        TEKLOG_ERROR("Value pointer is NULL");
        return false;
    }

    il2cpp_array_t* il2cpp_array = array;
    TEKLOG_DEBUG("Array info: m_length=%d", il2cpp_array->m_length);

    if (il2cpp_array->m_length == 0) {
        TEKLOG_DEBUG("Array is empty, nothing to fill");
        return true;
    }

    const size_t element_size = get_size_from_patch_type(value_type);
    TEKLOG_DEBUG("Element size for type %d: %zu", value_type, element_size);

    if (element_size == 0) {
        TEKLOG_ERROR("Unknown or unsupported value type: %d", value_type);
        return false;
    }

    const char* base_ptr = (char*)il2cpp_array;
    const size_t array_header_size = offsetof(il2cpp_array_t, m_length) + sizeof(il2cpp_array->m_length);
    char* data_start = (char*)base_ptr + array_header_size;

    TEKLOG_DEBUG("Filling array with %d elements, each %zu bytes", il2cpp_array->m_length, element_size);

    for (int32_t i = 0; i < il2cpp_array->m_length; ++i) {
        char* element_ptr = data_start + (i * element_size);
        memcpy(element_ptr, value, element_size);
    }

    TEKLOG_DEBUG("Array filled successfully");
    return true;
}

bool patchlib_array_empty(patch_handle_t array) {
    TEKLOG_DEBUG("patchlib_array_empty called: array=%p", array);

    if (!patchlib_is_valid(array)) {
        TEKLOG_WARN("Invalid array handle, considering as empty");
        return true;
    }

    const il2cpp_array_t* il2cpp_array = array;
    const bool result = il2cpp_array->m_length == 0;

    TEKLOG_DEBUG("Array empty check: length=%d, empty=%s",
                 il2cpp_array->m_length, result ? "true" : "false");
    return result;
}

size_t patchlib_array_length(patch_handle_t array) {
    TEKLOG_DEBUG("patchlib_array_length called: array=%p", array);

    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return 0;
    }

    const il2cpp_array_t* il2cpp_array = array;
    const size_t result = il2cpp_array->m_length;

    TEKLOG_DEBUG("Array length: %zu", result);
    return result;
}

bool patchlib_array_clear(patch_handle_t array, const patch_type_t value_type) {
    TEKLOG_DEBUG("patchlib_array_clear called: array=%p, value_type=%d", array, value_type);

    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return false;
    }

    il2cpp_array_t* il2cpp_array = array;
    TEKLOG_DEBUG("Array info: m_length=%d", il2cpp_array->m_length);

    if (il2cpp_array->m_length == 0) {
        TEKLOG_DEBUG("Array is already empty");
        return true;
    }

    const size_t element_size = get_size_from_patch_type(value_type);
    TEKLOG_DEBUG("Element size for type %d: %zu", value_type, element_size);

    if (element_size == 0) {
        TEKLOG_ERROR("Unknown or unsupported value type: %d", value_type);
        return false;
    }

    const char* base_ptr = (char*)il2cpp_array;
    const size_t array_header_size = offsetof(il2cpp_array_t, m_length) + sizeof(il2cpp_array->m_length);
    char* data_start = (char*)base_ptr + array_header_size;
    const size_t total_data_size = (size_t)il2cpp_array->m_length * element_size;

    TEKLOG_DEBUG("Clearing array: total_size=%zu bytes", total_data_size);
    memset(data_start, 0, total_data_size);

    TEKLOG_DEBUG("Array cleared successfully");
    return true;
}