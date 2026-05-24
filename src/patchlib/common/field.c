/*******************************************************************************
 * tefkernel - field
 * Copyright (C) 2026 eternalfuture-e38299
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
 * Created: 2026/5/24
 *******************************************************************************/


#include "internal/log.h"
#include "../il2cpp_api.h"
#include "patchlib/field.h"

const char* patchlib_field_get_name(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("Invalid field handle");
        return NULL;
    }

    const char* result = il2cpp_field_get_name(field);
    TEKLOG_DEBUG("Field name: %s", result ? result : "NULL");
    return result;
}

bool patchlib_field_is_instance(patch_handle_t field) {
    const bool result = !patchlib_field_is_static(field);
    TEKLOG_DEBUG("Field is instance: %s", result ? "true" : "false");
    return result;
}

patch_type_t patchlib_field_get_type(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("Invalid field handle");
        return PATCH_VOID;
    }

    patch_type_t result;
    const int field_type = il2cpp_type_get_type(il2cpp_field_get_type(field));

    switch (field_type) {
        case IL2CPP_TYPE_VOID:    result = PATCH_VOID; break;
        case IL2CPP_TYPE_BOOLEAN: result = PATCH_BOOL; break;     // bool
        case IL2CPP_TYPE_CHAR:    result = PATCH_CHAR; break;     // char
        case IL2CPP_TYPE_I1:      result = PATCH_INT8; break;     // sbyte
        case IL2CPP_TYPE_U1:      result = PATCH_UINT8; break;    // byte
        case IL2CPP_TYPE_I2:      result = PATCH_INT16; break;    // short
        case IL2CPP_TYPE_U2:      result = PATCH_UINT16; break;   // ushort
        case IL2CPP_TYPE_I4:      result = PATCH_INT32; break;    // int
        case IL2CPP_TYPE_U4:      result = PATCH_UINT32; break;   // uint
        case IL2CPP_TYPE_I8:      result = PATCH_INT64; break;    // long
        case IL2CPP_TYPE_U8:      result = PATCH_UINT64; break;   // ulong
        case IL2CPP_TYPE_R4:      result = PATCH_FLOAT; break;    // float
        case IL2CPP_TYPE_R8:      result = PATCH_DOUBLE; break;   // double
        case IL2CPP_TYPE_STRING:  result = PATCH_OBJECT; break;   // string
        case IL2CPP_TYPE_PTR:       // pointer
        case IL2CPP_TYPE_I:         // IntPtr
        case IL2CPP_TYPE_U:       result = PATCH_POINTER; break;  // UIntPtr
        default:
            result = PATCH_OBJECT;
            break;
    }

    return result;
}

size_t patchlib_field_get_size(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("Invalid field handle");
        return 0;
    }

    const int field_type = il2cpp_type_get_type(il2cpp_field_get_type(field));
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