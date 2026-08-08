/*******************************************************************************
 * tefkernel - method
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
 * Created: 2026/1/3
 *******************************************************************************/

#include "patchlib/method.h"

#include <string.h>

#include "patchlib/type.h"

#include "internal/log.h"
#include "patchlib/il2cpp_api.h"

bool patchlib_method_is_static(patch_handle_t method) {
    const bool result = !patchlib_method_is_instance(method);
    TEKLOG_DEBUG("Method is static: %s", result ? "true" : "false");
    return result;
}

const char *patchlib_method_get_name(patch_handle_t method) {
    if (!patchlib_is_valid(method)) {
        TEKLOG_WARN("Invalid method handle");
        return NULL;
    }

    const char* result = il2cpp_method_get_name(method);
    TEKLOG_DEBUG("Method name: %s", result ? result : "NULL");
    return result;
}

int patchlib_method_get_param_count(patch_handle_t method) {
    if (!patchlib_is_valid(method)) {
        TEKLOG_WARN("Invalid method handle");
        return -1;
    }

    const int result = (int) il2cpp_method_get_param_count(method);
    TEKLOG_DEBUG("Method parameter count: %d", result);
    return result;
}

bool patchlib_method_is_instance(patch_handle_t method) {
    if (!patchlib_is_valid(method)) {
        TEKLOG_WARN("Invalid method handle");
        return false;
    }

    const bool result = il2cpp_method_is_instance(method);
    TEKLOG_DEBUG("Method is instance: %s", result ? "true" : "false");
    return result;
}

static patch_type_t il2cpp_type_to_patch_type(patch_handle_t type) {
    if (!type) {
        TEKLOG_DEBUG("NULL type provided, returning PATCH_VOID");
        return PATCH_VOID;
    }

    const int il2cpp_type = il2cpp_type_get_type(type);

    if (il2cpp_type_is_byref(type)) return PATCH_POINTER;

    patch_type_t result;
    switch (il2cpp_type) {
        case IL2CPP_TYPE_VOID:     result = PATCH_VOID; break;
        case IL2CPP_TYPE_BOOLEAN:  result = PATCH_BOOL; break;
        case IL2CPP_TYPE_I1:       result = PATCH_INT8; break;
        case IL2CPP_TYPE_U1:       result = PATCH_UINT8; break;
        case IL2CPP_TYPE_I2:       result = PATCH_INT16; break;
        case IL2CPP_TYPE_U2:       result = PATCH_UINT16; break;
        case IL2CPP_TYPE_I4:       result = PATCH_INT32; break;
        case IL2CPP_TYPE_R4:       result = PATCH_FLOAT; break;
        case IL2CPP_TYPE_U4:       result = PATCH_UINT32; break;
        case IL2CPP_TYPE_I8:       result = PATCH_INT64; break;
        case IL2CPP_TYPE_U8:       result = PATCH_UINT64; break;
        case IL2CPP_TYPE_R8:       result = PATCH_DOUBLE; break;
        case IL2CPP_TYPE_CHAR:     result = PATCH_CHAR; break;
        case IL2CPP_TYPE_PTR:      result = PATCH_POINTER; break;
        case IL2CPP_TYPE_ENUM:     result = PATCH_INT32; break;
        case IL2CPP_TYPE_VALUETYPE: {
            patch_handle_t kclass = il2cpp_class_from_type(type);
            patch_handle_t parent = il2cpp_class_get_parent(kclass);
            const char* parent_name = il2cpp_class_get_name(parent);
            patchlib_free(parent);
            if (parent_name && strcmp(parent_name, "Enum") == 0) {
                result = PATCH_INT32;
                patchlib_free(kclass);
                break;
            }


            const char* class_name = il2cpp_class_get_name(kclass);
            const char* namespace_name = il2cpp_class_get_namespace(kclass);
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
        default:                   result = PATCH_OBJECT; break;
    }

    TEKLOG_TRACE("Converted IL2CPP type %d to patch type: %d", il2cpp_type, result);
    return result;
}

int patchlib_method_get_token(patch_handle_t method) {
    if (!patchlib_is_valid(method)) return 0;
    return (int)il2cpp_method_get_token(method);
}

patch_type_t patchlib_method_get_return_type(patch_handle_t method) {
    return il2cpp_type_to_patch_type(il2cpp_method_get_return_type(method));
}

bool patchlib_method_get_signature(patch_handle_t method, patch_method_signature_t* signature) {
    TEKLOG_DEBUG("patchlib_method_get_signature called: method=%p", method);

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle");
        return false;
    }

    signature->method = method;
    signature->token = patchlib_method_get_token(signature->method);
    signature->return_type = patchlib_method_get_return_type(method);
    signature->is_instance = patchlib_method_is_instance(method);
    tefstd_vector_init(&signature->arg_types, sizeof(patch_type_t));
    tefstd_vector_init(&signature->arg_names, sizeof(const char*));

    const int param_count = patchlib_method_get_param_count(method);
    TEKLOG_DEBUG("Method signature: return_type=%d, is_instance=%s, param_count=%d",
                 signature->return_type, signature->is_instance ? "true" : "false", param_count);

    for (int i = 0; i < param_count; ++i) {
        patch_type_t t = il2cpp_type_to_patch_type(il2cpp_method_get_param(method, i));
        const char* n = il2cpp_method_get_param_name(method, i);
        tefstd_vector_push_back(&signature->arg_types, &t);
        tefstd_vector_push_back(&signature->arg_names, &n);
        TEKLOG_DEBUG("Parameter %d type: %d", i, t);
    }

    TEKLOG_DEBUG("Signature extraction completed: total_params=%zu",
                 tefstd_vector_size(&signature->arg_types));
    return true;
}

bool patchlib_method_signature_free(patch_method_signature_t* signature) {
    tefstd_vector_destroy(&signature->arg_types);
    tefstd_vector_destroy(&signature->arg_names);
    signature->method = PATCH_NULL;
    signature->is_instance = 0;
    signature->return_type = PATCH_VOID;

    return true;
}
