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

#include <string.h>

const char *patchlib_field_get_name(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("Invalid field handle");
        return NULL;
    }

    const char *result = il2cpp_field_get_name(field);
    return result;
}

bool patchlib_field_is_instance(patch_handle_t field) {
    const bool result = !patchlib_field_is_static(field);
    return result;
}

patch_type_t patchlib_field_get_type(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("Invalid field handle");
        return PATCH_VOID;
    }

    patch_type_t result;
    patch_handle_t type = il2cpp_field_get_type(field);
    const int field_type = il2cpp_type_get_type(type);

    switch (field_type) {
        case IL2CPP_TYPE_VOID: result = PATCH_VOID;
            break;
        case IL2CPP_TYPE_BOOLEAN: result = PATCH_BOOL;
            break; // bool
        case IL2CPP_TYPE_CHAR: result = PATCH_CHAR;
            break; // char
        case IL2CPP_TYPE_I1: result = PATCH_INT8;
            break; // sbyte
        case IL2CPP_TYPE_U1: result = PATCH_UINT8;
            break; // byte
        case IL2CPP_TYPE_I2: result = PATCH_INT16;
            break; // short
        case IL2CPP_TYPE_U2: result = PATCH_UINT16;
            break; // ushort
        case IL2CPP_TYPE_I4: result = PATCH_INT32;
            break; // int
        case IL2CPP_TYPE_U4: result = PATCH_UINT32;
            break; // uint
        case IL2CPP_TYPE_I8: result = PATCH_INT64;
            break; // long
        case IL2CPP_TYPE_U8: result = PATCH_UINT64;
            break; // ulong
        case IL2CPP_TYPE_R4: result = PATCH_FLOAT;
            break; // float
        case IL2CPP_TYPE_R8: result = PATCH_DOUBLE;
            break; // double
        case IL2CPP_TYPE_STRING: result = PATCH_OBJECT;
            break; // string
        case IL2CPP_TYPE_PTR: // pointer
        case IL2CPP_TYPE_I: // IntPtr
        case IL2CPP_TYPE_U: result = PATCH_POINTER;
            break; // UIntPtr
        case IL2CPP_TYPE_ENUM: result = PATCH_INT32;
            break;
        case IL2CPP_TYPE_VALUETYPE: {
            patch_handle_t kclass = il2cpp_class_from_type(type);
            patch_handle_t parent = il2cpp_class_get_parent(kclass);
            const char *parent_name = il2cpp_class_get_name(parent);
            patchlib_free(parent);
            if (parent_name && strcmp(parent_name, "Enum") == 0) {
                result = PATCH_INT32;
                patchlib_free(kclass);
                break;
            }


            const char *class_name = il2cpp_class_get_name(kclass);
            const char *namespace_name = il2cpp_class_get_namespace(kclass);
            patchlib_free(kclass);

            if (namespace_name && strcmp(namespace_name, "System") == 0) {
                // System 命名空间下的原始类型
                if (strcmp(class_name, "Int32") == 0) {
                    result = PATCH_INT32;
                    break;
                }
                if (strcmp(class_name, "UInt32") == 0) {
                    result = PATCH_UINT32;
                    break;
                }
                if (strcmp(class_name, "Int64") == 0) {
                    result = PATCH_INT64;
                    break;
                }
                if (strcmp(class_name, "UInt64") == 0) {
                    result = PATCH_UINT64;
                    break;
                }
                if (strcmp(class_name, "Single") == 0) {
                    result = PATCH_FLOAT;
                    break;
                }
                if (strcmp(class_name, "Double") == 0) {
                    result = PATCH_DOUBLE;
                    break;
                }
                if (strcmp(class_name, "Boolean") == 0) {
                    result = PATCH_BOOL;
                    break;
                }
                if (strcmp(class_name, "Char") == 0) {
                    result = PATCH_CHAR;
                    break;
                }
                if (strcmp(class_name, "Byte") == 0) {
                    result = PATCH_UINT8;
                    break;
                }
                if (strcmp(class_name, "SByte") == 0) {
                    result = PATCH_INT8;
                    break;
                }
                if (strcmp(class_name, "Int16") == 0) {
                    result = PATCH_INT16;
                    break;
                }
                if (strcmp(class_name, "UInt16") == 0) {
                    result = PATCH_UINT16;
                    break;
                }
                if (strcmp(class_name, "IntPtr") == 0 || strcmp(class_name, "UIntPtr") == 0) {
                    result = PATCH_POINTER;
                    break;
                }
            }

            // 结构体 - 需要展开或按指针处理
            // 对于复杂结构体，返回 POINTER 让调用者通过指针访问
            result = PATCH_POINTER;
            break;
        }
        
        default:
            result = PATCH_OBJECT;
            break;
    }

    patchlib_free(type);

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
        case IL2CPP_TYPE_BOOLEAN: // bool
        case IL2CPP_TYPE_I1: // sbyte
        case IL2CPP_TYPE_U1: result = 1;
            break; // byte
        case IL2CPP_TYPE_I2: // short
        case IL2CPP_TYPE_U2: result = 2;
            break; // ushort
        case IL2CPP_TYPE_CHAR: // char (Unicode UTF-16)
        case IL2CPP_TYPE_I4: // int
        case IL2CPP_TYPE_R4: // float
        case IL2CPP_TYPE_U4: result = 4;
            break; // uint
        case IL2CPP_TYPE_I8: // long
        case IL2CPP_TYPE_U8: // ulong
        case IL2CPP_TYPE_R8: result = 8;
            break; // double
        default:
            result = sizeof(void *);
            break;
    }

    return result;
}