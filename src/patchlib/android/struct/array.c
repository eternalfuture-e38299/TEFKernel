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

#include <stdint.h>

#include "internal/log.h"
#include <string.h>
#include "../../il2cpp_api.h"

// /*
// typedef struct il2cpp_array_t {
//     void *m_class;
//     void *m_monitor;
//     void *m_bounds;
//     uint32_t m_length;
//     // T *m_values;
// } il2cpp_array_t;*/

bool patchlib_array_at(patch_handle_t array, const size_t index, void* out_value) {
    TEKLOG_DEBUG("patchlib_array_at called: array=%p, index=%zu, out_value=%p",
                 array, index, out_value);

    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return false;
    }

    if (out_value == NULL) {
        TEKLOG_ERROR("Output value pointer is NULL");
        return false;
    }

    const size_t element_size = il2cpp_array_element_size(array);
    const char* data_start = array + sizeof(void*) * 3 + sizeof(uint32_t);
    const char* element_ptr = data_start + (index * element_size);

    TEKLOG_DEBUG("Memory copy: from=%p, to=%p, size=%zu", element_ptr, out_value, element_size);
    memcpy(out_value, element_ptr, element_size);

    TEKLOG_DEBUG("Array element retrieved successfully");
    return true;
}

bool patchlib_array_set(patch_handle_t array, const size_t index, void* new_value) {
    TEKLOG_DEBUG("patchlib_array_set called: array=%p, index=%zu, new_value=%p",
                 array, index, new_value);

    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return false;
    }

    if (!new_value) {
        TEKLOG_ERROR("New value pointer is NULL");
        return false;
    }

    const size_t element_size = il2cpp_array_element_size(array);
    const char* data_start = array + sizeof(void*) * 3 + sizeof(uint32_t);
    const char* element_ptr = data_start + (index * element_size);

    TEKLOG_DEBUG("Memory copy: from=%p, to=%p, size=%zu", new_value, element_ptr, element_size);
    memcpy((void*)element_ptr, new_value, element_size);

    TEKLOG_DEBUG("Array element set successfully");
    return true;
}

bool patchlib_array_fill(patch_handle_t array, void* value) {
    TEKLOG_DEBUG("patchlib_array_fill called: array=%p, value=%p",
                 array, value);

    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return false;
    }

    if (!value) {
        TEKLOG_ERROR("Value pointer is NULL");
        return false;
    }

    const size_t data_length = il2cpp_array_length(array);
    const size_t element_size = il2cpp_array_element_size(array);
    void* data_start = array + sizeof(void*) * 3 + sizeof(uint32_t);

    for (int32_t i = 0; i < data_length; ++i) {
        char* element_ptr = data_start + i * element_size;
        memcpy(element_ptr, value, element_size);
    }

    TEKLOG_DEBUG("Array filled successfully");
    return true;
}

bool patchlib_array_copy_from_c(patch_handle_t dest, const void* src, size_t count) {
    TEKLOG_DEBUG("patchlib_array_copy_from_c called: dest=%p, src=%p, count=%zu", dest, src, count);

    if (!patchlib_is_valid(dest)) {
        TEKLOG_ERROR("Invalid destination array handle");
        return false;
    }

    if (!src) {
        TEKLOG_ERROR("Source C array is NULL");
        return false;
    }

    const size_t dest_len = patchlib_array_length(dest);
    if (count > dest_len) {
        TEKLOG_ERROR("Count %zu exceeds destination length %zu", count, dest_len);
        return false;
    }

    const size_t element_size = il2cpp_array_element_size(dest);
    if (element_size == 0) {
        TEKLOG_ERROR("Failed to get element size");
        return false;
    }

    void* dest_data = dest + sizeof(void*) * 3 + sizeof(uint32_t);
    if (!dest_data) {
        TEKLOG_ERROR("Failed to get destination data pointer");
        return false;
    }

    memcpy(dest_data, src, count * element_size);
    TEKLOG_DEBUG("Copied %zu elements from C to IL2CPP array", count);
    return true;
}

bool patchlib_array_copy_to_c(void* dest, patch_handle_t src, size_t count) {
    TEKLOG_DEBUG("patchlib_array_copy_to_c called: dest=%p, src=%p, count=%zu", dest, src, count);

    if (!dest) {
        TEKLOG_ERROR("Destination C array is NULL");
        return false;
    }

    if (!patchlib_is_valid(src)) {
        TEKLOG_ERROR("Invalid source array handle");
        return false;
    }

    const size_t src_len = patchlib_array_length(src);
    if (count > src_len) {
        TEKLOG_ERROR("Count %zu exceeds source length %zu", count, src_len);
        return false;
    }

    const size_t element_size = il2cpp_array_element_size(src);
    if (element_size == 0) {
        TEKLOG_ERROR("Failed to get element size");
        return false;
    }

    void* src_data = src + sizeof(void*) * 3 + sizeof(uint32_t);
    if (!src_data) {
        TEKLOG_ERROR("Failed to get source data pointer");
        return false;
    }

    memcpy(dest, src_data, count * element_size);
    TEKLOG_DEBUG("Copied %zu elements from IL2CPP to C array", count);
    return true;
}