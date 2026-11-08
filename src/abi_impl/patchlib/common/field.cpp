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


#include "patchlib/field.h"
#include "patchlib/type.h"

#include <cstring>

#include "tef/core/log.hpp"

extern "C" {
#include "../il2cpp_api.h"
}

using tef::log::warn;

extern "C" {
/* ============================================================================
 * 基本信息
 * ============================================================================ */

const char *patchlib_field_get_name(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        warn("Invalid field handle");
        return nullptr;
    }
    return il2cpp_field_get_name(field);
}

bool patchlib_field_is_instance(patch_handle_t field) {
    return !patchlib_field_is_static(field);
}

/* ============================================================================
 * 类型
 * ============================================================================ */

patch_type_t patchlib_field_get_type(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        warn("Invalid field handle");
        return PATCH_VOID;
    }

    patch_handle_t type = il2cpp_field_get_type(field);
    const int field_type = il2cpp_type_get_type(type);

    patch_type_t result;

    switch (field_type) {
        case IL2CPP_TYPE_VOID: result = PATCH_VOID;
            break;
        case IL2CPP_TYPE_BOOLEAN: result = PATCH_BOOL;
            break;
        case IL2CPP_TYPE_CHAR: result = PATCH_CHAR;
            break;
        case IL2CPP_TYPE_I1: result = PATCH_INT8;
            break;
        case IL2CPP_TYPE_U1: result = PATCH_UINT8;
            break;
        case IL2CPP_TYPE_I2: result = PATCH_INT16;
            break;
        case IL2CPP_TYPE_U2: result = PATCH_UINT16;
            break;
        case IL2CPP_TYPE_I4: result = PATCH_INT32;
            break;
        case IL2CPP_TYPE_U4: result = PATCH_UINT32;
            break;
        case IL2CPP_TYPE_I8: result = PATCH_INT64;
            break;
        case IL2CPP_TYPE_U8: result = PATCH_UINT64;
            break;
        case IL2CPP_TYPE_R4: result = PATCH_FLOAT;
            break;
        case IL2CPP_TYPE_R8: result = PATCH_DOUBLE;
            break;
        case IL2CPP_TYPE_STRING: result = PATCH_OBJECT;
            break;
        case IL2CPP_TYPE_PTR:
        case IL2CPP_TYPE_I:
        case IL2CPP_TYPE_U: result = PATCH_POINTER;
            break;
        case IL2CPP_TYPE_ENUM: result = PATCH_INT32;
            break;

        case IL2CPP_TYPE_VALUETYPE: {
            patch_handle_t kclass = il2cpp_class_from_type(type);
            patch_handle_t parent = il2cpp_class_get_parent(kclass);
            const char *parent_name = il2cpp_class_get_name(parent);
            patchlib_free(parent);

            if (parent_name && std::strcmp(parent_name, "Enum") == 0) {
                result = PATCH_INT32;
                /* 桌面端 il2cpp_class_from_type 是恒等宏 → kclass == type，
                 * 释放会让调用者 double-free。只有真正转换出新句柄时才释放。 */
                if (kclass != type) patchlib_free(kclass);
                break;
            }

            const char *class_name = il2cpp_class_get_name(kclass);
            const char *namespace_name = il2cpp_class_get_namespace(kclass);
            if (kclass != type) patchlib_free(kclass);

            if (namespace_name && std::strcmp(namespace_name, "System") == 0) {
                if (std::strcmp(class_name, "Int32") == 0) {
                    result = PATCH_INT32;
                    break;
                }
                if (std::strcmp(class_name, "UInt32") == 0) {
                    result = PATCH_UINT32;
                    break;
                }
                if (std::strcmp(class_name, "Int64") == 0) {
                    result = PATCH_INT64;
                    break;
                }
                if (std::strcmp(class_name, "UInt64") == 0) {
                    result = PATCH_UINT64;
                    break;
                }
                if (std::strcmp(class_name, "Single") == 0) {
                    result = PATCH_FLOAT;
                    break;
                }
                if (std::strcmp(class_name, "Double") == 0) {
                    result = PATCH_DOUBLE;
                    break;
                }
                if (std::strcmp(class_name, "Boolean") == 0) {
                    result = PATCH_BOOL;
                    break;
                }
                if (std::strcmp(class_name, "Char") == 0) {
                    result = PATCH_CHAR;
                    break;
                }
                if (std::strcmp(class_name, "Byte") == 0) {
                    result = PATCH_UINT8;
                    break;
                }
                if (std::strcmp(class_name, "SByte") == 0) {
                    result = PATCH_INT8;
                    break;
                }
                if (std::strcmp(class_name, "Int16") == 0) {
                    result = PATCH_INT16;
                    break;
                }
                if (std::strcmp(class_name, "UInt16") == 0) {
                    result = PATCH_UINT16;
                    break;
                }
                if (std::strcmp(class_name, "IntPtr") == 0 ||
                    std::strcmp(class_name, "UIntPtr") == 0) {
                    result = PATCH_POINTER;
                    break;
                }
            }

            /* 结构体 → 按指针处理 */
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

patch_handle_t patchlib_field_get_type_handle(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        warn("Invalid field handle");
        return nullptr;
    }

    patch_handle_t type = il2cpp_field_get_type(field);
    if (!patchlib_is_valid(type)) return nullptr;

    /* Android: field_get_type 返回 Il2CppType*，需转 Il2CppClass*。
     * 桌面端 il2cpp_class_from_type 是恒等宏。 */
    return il2cpp_class_from_type(type);
}

/* ============================================================================
 * 大小
 * ============================================================================ */

size_t patchlib_field_get_size(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        warn("Invalid field handle");
        return 0;
    }

    patch_handle_t field_type = il2cpp_field_get_type(field);
    const int kind = il2cpp_type_get_type(field_type);

    size_t result;
    switch (kind) {
        case IL2CPP_TYPE_BOOLEAN:
        case IL2CPP_TYPE_I1:
        case IL2CPP_TYPE_U1:
            result = 1;
            break;
        case IL2CPP_TYPE_I2:
        case IL2CPP_TYPE_U2:
            result = 2;
            break;
        case IL2CPP_TYPE_CHAR:
        case IL2CPP_TYPE_I4:
        case IL2CPP_TYPE_R4:
        case IL2CPP_TYPE_U4:
            result = 4;
            break;
        case IL2CPP_TYPE_I8:
        case IL2CPP_TYPE_U8:
        case IL2CPP_TYPE_R8:
            result = 8;
            break;
        case IL2CPP_TYPE_VALUETYPE:
            result = static_cast<size_t>(
                il2cpp_class_value_size(
                    il2cpp_class_from_il2cpp_type(field_type), nullptr));
            break;
        default:
            result = sizeof(void *);
            break;
    }

    patchlib_free(field_type);
    return result;
}
}
