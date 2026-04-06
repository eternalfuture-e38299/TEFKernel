/*******************************************************************************
 * tefkernel - private
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
 * Created: 2025/12/7
 *******************************************************************************/

#include "private.h"

#include "il2cpp_api.h"
#include "internal/log.h"

typedef struct il2cpp_array_t {
    void *m_class;
    void *m_monitor;
    void *m_bounds;
    int m_length;
    // void *m_values;
} il2cpp_array_t;

void *create_type_array_from_vector(const tefstd_vector_t *type_vector, void *elementTypeClass) {
    if (!elementTypeClass) {
        TEKLOG_ERROR("elementTypeClass is NULL");
        return NULL;
    }

    TEKLOG_DEBUG("create_type_array_from_vector called: type_vector=%p, size=%zu, elementTypeClass=%p",
                 type_vector, type_vector ? tefstd_vector_size(type_vector) : 0, elementTypeClass);

    if (!type_vector || tefstd_vector_size(type_vector) == 0) {
        TEKLOG_WARN("Invalid or empty type vector");
        return NULL;
    }

    const size_t count = tefstd_vector_size(type_vector);
    TEKLOG_DEBUG("Creating type array with %zu elements", count);

    void *array = il2cpp_array_new(elementTypeClass, (uintptr_t)count);
    if (!array) {
        TEKLOG_ERROR("Failed to create array");
        return NULL;
    }

    // 获取数组类型的 TypeInfo 并计算元素大小
    int elementSize = il2cpp_array_element_size(elementTypeClass);
    if (elementSize <= 0)
        // 对于引用类型数组，元素是指针
        elementSize = sizeof(void*);
    TEKLOG_DEBUG("Array element size: %d", elementSize);

    // 计算元素数据起始地址
    char *dataStart = (char *) array + sizeof(il2cpp_array_t);
    TEKLOG_DEBUG("Array data start: %p", dataStart);

    // 逐个复制指针
    int copied_count = 0;
    for (size_t i = 0; i < count; ++i) {
        void ** obj = tefstd_vector_at(type_vector, i);
        if (obj) {
            void **elemPtr = (void **) (dataStart + i * elementSize);
            *elemPtr = *obj;
            copied_count++;
            TEKLOG_TRACE("Copied type %zu: %p to array position %zu", i, *obj, i);
        } else {
            TEKLOG_WARN("Type at index %zu is NULL, skipping", i);
        }
    }

    TEKLOG_DEBUG("Type array created successfully: %p, copied %d/%zu elements",
                 array, copied_count, count);
    return array;
}

ffi_type* patch_type_to_ffi_type(const patch_type_t type) {
    TEKLOG_DEBUG("patch_type_to_ffi_type called: type=%d", type);

    ffi_type* result = NULL;
    const char* type_name = "UNKNOWN";

    switch (type) {
        case PATCH_VOID:    result = &ffi_type_void; type_name = "VOID"; break;
        case PATCH_BOOL:    result = &ffi_type_uint8; type_name = "BOOL"; break;
        case PATCH_INT8:    result = &ffi_type_sint8; type_name = "INT8"; break;
        case PATCH_INT16:   result = &ffi_type_sint16; type_name = "INT16"; break;
        case PATCH_INT32:   result = &ffi_type_sint32; type_name = "INT32"; break;
        case PATCH_INT64:   result = &ffi_type_sint64; type_name = "INT64"; break;
        case PATCH_UINT8:   result = &ffi_type_uint8; type_name = "UINT8"; break;
        case PATCH_UINT16:  result = &ffi_type_uint16; type_name = "UINT16"; break;
        case PATCH_UINT32:  result = &ffi_type_uint32; type_name = "UINT32"; break;
        case PATCH_UINT64:  result = &ffi_type_uint64; type_name = "UINT64"; break;
        case PATCH_FLOAT:   result = &ffi_type_float; type_name = "FLOAT"; break;
        case PATCH_DOUBLE:  result = &ffi_type_double; type_name = "DOUBLE"; break;
        case PATCH_CHAR:    result = &ffi_type_uint8; type_name = "CHAR"; break;
        default:            result = &ffi_type_pointer; type_name = "POINTER"; break;
    }

    TEKLOG_DEBUG("Converted patch type %d (%s) to FFI type: %p", type, type_name, result);
    return result;
}

// 全局变量定义
patch_handle_t patchlib_MakeGenericType = NULL;
patch_handle_t patchlib_MakeGenericMethod_impl = NULL;