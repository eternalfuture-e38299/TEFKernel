/*******************************************************************************
 * tefkernel - field
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
 * Created: 2025/11/23
 *******************************************************************************/

#include "patchlib/field.h"

#include <string.h>

#include "il2cpp_api.h"
#include "internal/log.h"

const char* patchlib_field_get_name(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("Invalid field handle");
        return NULL;
    }

    const char* result = il2cpp_field_get_name(field);
    TEKLOG_DEBUG("Field name: %s", result ? result : "NULL");
    return result;
}

bool patchlib_field_is_static(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("Invalid field handle");
        return false;
    }

    const bool result = (*(int*)((uintptr_t)il2cpp_field_get_type(field) + sizeof(void*)) & 0x0010) != 0;
    TEKLOG_DEBUG("Field is static: %s", result ? "true" : "false");
    return result;
}

bool patchlib_field_is_instance(patch_handle_t field) {
    const bool result = !patchlib_field_is_static(field);
    TEKLOG_DEBUG("Field is instance: %s", result ? "true" : "false");
    return result;
}

bool patchlib_field_is_const(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("Invalid field handle");
        return false;
    }

    const bool result = (*(int*)((uintptr_t)il2cpp_field_get_type(field) + sizeof(void*)) & 0x0040) != 0;
    TEKLOG_DEBUG("Field is const: %s", result ? "true" : "false");
    return result;
}

bool patchlib_field_is_thread_static(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("Invalid field handle");
        return false;
    }

    const bool result = il2cpp_field_get_offset(field) == -1;
    TEKLOG_DEBUG("Field is thread static: %s", result ? "true" : "false");
    return result;
}

void patchlib_field_get_value(patch_handle_t field, patch_handle_t instance, void *value) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_ERROR("Invalid field handle");
        return;
    }

    if (!value) {
        TEKLOG_ERROR("Value pointer is NULL");
        return;
    }

    const size_t field_size = patchlib_field_get_size(field);
    void* field_pointer = patchlib_field_get_pointer(field, instance);

    if (!field_pointer) {
        TEKLOG_ERROR("Failed to get field pointer");
        return;
    }

    TEKLOG_DEBUG("Getting field value: field=%p, instance=%p, size=%zu",
                 field, instance, field_size);

    memcpy(value, field_pointer, field_size);
    TEKLOG_DEBUG("Field value retrieved successfully");
}

void patchlib_field_set_value(patch_handle_t field, patch_handle_t instance, const void *value) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_ERROR("Invalid field handle");
        return;
    }

    if (!value) {
        TEKLOG_ERROR("Value pointer is NULL");
        return;
    }

    const size_t field_size = patchlib_field_get_size(field);
    void* field_pointer = patchlib_field_get_pointer(field, instance);

    if (!field_pointer) {
        TEKLOG_ERROR("Failed to get field pointer");
        return;
    }

    TEKLOG_DEBUG("Setting field value: field=%p, instance=%p, size=%zu",
                 field, instance, field_size);

    memcpy(field_pointer, value, field_size);
    TEKLOG_DEBUG("Field value set successfully");
}

size_t patchlib_field_get_size(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("Invalid field handle");
        return 0;
    }

    const int field_type = il2cpp_type_get_type(il2cpp_field_get_parent(field));
    size_t result = 0;

    switch (field_type) {
        case IL2CPP_TYPE_BOOLEAN:   // bool
        case IL2CPP_TYPE_I1:        // sbyte
        case IL2CPP_TYPE_U1:      result = 1; break;  // byte
        case IL2CPP_TYPE_I2:        // short
        case IL2CPP_TYPE_U2:      result = 2; break; // ushort
        case IL2CPP_TYPE_I4:        // int
        case IL2CPP_TYPE_R4:        // float
        case IL2CPP_TYPE_U4:      result = 4; break; // uint
        case IL2CPP_TYPE_I8:        // long
        case IL2CPP_TYPE_U8:        // ulong
        case IL2CPP_TYPE_R8:      result = 8; break;  // double
        default:
            result = sizeof(void*);
            break;
    }

    TEKLOG_DEBUG("Field size: type=%d, size=%zu", field_type, result);
    return result;
}

void * patchlib_field_get_pointer(patch_handle_t field, void *instance) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_ERROR("Invalid field handle");
        return NULL;
    }

    if (patchlib_field_is_const(field)) {
        TEKLOG_WARN("Cannot get pointer to const field");
        return NULL;
    }

    if (patchlib_field_is_thread_static(field)) {
        TEKLOG_WARN("Thread static fields not supported");
        return NULL;
    }

    const size_t offset = il2cpp_field_get_offset(field);
    const void* p_class = il2cpp_field_get_parent(field);

    TEKLOG_DEBUG("Field offset: %zu, class: %p", offset, p_class);

    if (patchlib_field_is_static(field)) {
        void* staticData = il2cpp_class_get_static_field_data(p_class);
        if (!staticData) {
            TEKLOG_ERROR("Failed to get static field data");
            return NULL;
        }

        void* result = (void*)((uintptr_t)staticData + offset);
        TEKLOG_DEBUG("Static field pointer: %p (base=%p, offset=%zu)", result, staticData, offset);
        return result;
    }

    const bool isValueType = il2cpp_type_get_type(p_class) == IL2CPP_TYPE_VALUETYPE;
    void* result = (void*)((uintptr_t)instance + offset - (isValueType ? sizeof(void*) : 0x0));

    TEKLOG_DEBUG("Instance field pointer: %p (instance=%p, offset=%zu, isValueType=%s)",
                 result, instance, offset, isValueType ? "true" : "false");
    return result;
}

bool patchlib_field_free(patch_handle_t field) {
    TEKLOG_DEBUG("Field freed: %p", field);
    return true;
}