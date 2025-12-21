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

const char* patchlib_field_get_name(patch_handle_t field) {
    if (!patchlib_is_valid(field))
        return false;

    return il2cpp_field_get_name(field);
}

bool patchlib_field_is_static(patch_handle_t field) {
    if (!patchlib_is_valid(field))
        return false;

    return (*(int*)((uintptr_t)il2cpp_field_get_type(field) + sizeof(void*)) & 0x0010) != 0;
}

bool patchlib_field_is_instance(patch_handle_t field) {
    return !patchlib_field_is_static(field);
}

bool patchlib_field_is_const(patch_handle_t field) {
    if (!patchlib_is_valid(field))
        return false;

    return (*(int*)((uintptr_t)il2cpp_field_get_type(field) + sizeof(void*)) & 0x0040) != 0;
}

bool patchlib_field_is_thread_static(patch_handle_t field) {
    if (!patchlib_is_valid(field))
        return false;

    return il2cpp_field_get_offset(field) == -1;
}

void patchlib_field_get_value(patch_handle_t field, void *instance, void *value) {
    if (!patchlib_is_valid(field))
        return;

    memcpy(value, patchlib_field_get_pointer(field, instance), patchlib_field_get_size(field));
}

void patchlib_field_set_value(patch_handle_t field, void *instance, const void *value) {
    if (!patchlib_is_valid(field))
        return;

    memcpy(patchlib_field_get_pointer(field, instance), value, patchlib_field_get_size(field));
}

size_t patchlib_field_get_size(patch_handle_t field) {
    if (!patchlib_is_valid(field))
        return 0;

    switch (il2cpp_type_get_type(il2cpp_field_get_parent(field))) {
        case IL2CPP_TYPE_BOOLEAN:   // bool
        case IL2CPP_TYPE_I1:        // sbyte
        case IL2CPP_TYPE_U1:      return 1;  // byte
        case IL2CPP_TYPE_I2:        // short
        case IL2CPP_TYPE_U2:      return 2; // ushort
        case IL2CPP_TYPE_I4:        // int
        case IL2CPP_TYPE_R4:        // float
        case IL2CPP_TYPE_U4:      return 4; // uint
        case IL2CPP_TYPE_I8:        // long
        case IL2CPP_TYPE_U8:        // ulong
        case IL2CPP_TYPE_R8:      return 8;  // double
        default:
            return sizeof(void*);
    }
}

void * patchlib_field_get_pointer(patch_handle_t field, void *instance) {
    if (!patchlib_is_valid(field))
        return NULL;

    if (patchlib_field_is_const(field))
        return NULL;

    if (patchlib_field_is_thread_static(field))
        return NULL;

    const size_t offset = il2cpp_field_get_offset(field);
    const void* p_class = il2cpp_field_get_parent(field);

    if (patchlib_field_is_static(field)) {
        void* staticData = il2cpp_class_get_static_field_data(p_class);
        if (!staticData)
            return NULL;

        return (void*)((uintptr_t)staticData + offset);
    }

    const bool isValueType = il2cpp_type_get_type(p_class) == IL2CPP_TYPE_VALUETYPE;
    return (void*)((uintptr_t)instance + offset - (isValueType ? sizeof(void*) : 0x0));
}

bool patchlib_field_free(patch_handle_t field) { return true; }