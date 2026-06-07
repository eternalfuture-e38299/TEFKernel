/*******************************************************************************
 * tefkernel - method
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

#include "patchlib/method.h"

#include <ffi.h>
#include <stdlib.h>
#include <string.h>

#include "../il2cpp_api.h"
#include "private.h"
#include "internal/log.h"

void* patchlib_method_get_pointer(patch_handle_t method) {
    if (!patchlib_is_valid(method))
        return NULL;
    return *(void**)method;
}

static bool patchlib_is_primitive_type(patch_type_t type) {
    switch (type) {
        case PATCH_BOOL:
        case PATCH_INT8:
        case PATCH_UINT8:
        case PATCH_INT16:
        case PATCH_UINT16:
        case PATCH_INT32:
        case PATCH_UINT32:
        case PATCH_INT64:
        case PATCH_UINT64:
        case PATCH_FLOAT:
        case PATCH_DOUBLE:
        case PATCH_CHAR:
            return true;
        case PATCH_VOID:
        case PATCH_POINTER:
        case PATCH_OBJECT:
        default:
            return false;
    }
}

/*
bool patchlib_method_invoke_args(patch_handle_t method, patch_handle_t instance,
                                void *return_value, void** args) {
    TEKLOG_DEBUG("patchlib_method_invoke_args (Android): method=%p, instance=%p, return_value=%p",
                 method, instance, return_value);

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle");
        return false;
    }

    // 获取参数数量
    const int param_count = patchlib_method_get_param_count(method);
    const bool is_instance = patchlib_method_is_instance(method);

    TEKLOG_DEBUG("Method: param_count=%d, is_instance=%s", param_count, is_instance ? "true" : "false");

    // 验证实例：实例方法需要有效的 instance
    if (is_instance && !patchlib_is_valid(instance)) {
        TEKLOG_ERROR("Instance method requires valid instance");
        return false;
    }

    // 静态方法：instance 应该为 NULL
    if (!is_instance && instance != PATCH_NULL) {
        TEKLOG_WARN("Static method called with non-NULL instance, ignoring instance");
        instance = PATCH_NULL;
    }

    // 准备参数数组（只包含方法参数，不包括 this）
    void** invoke_args = NULL;
    if (param_count > 0) {
        invoke_args = (void**)malloc(sizeof(void*) * param_count);
        if (!invoke_args) {
            TEKLOG_ERROR("Failed to allocate arguments array");
            return false;
        }

        for (int i = 0; i < param_count; i++) {
            if (args && i < param_count) {
                invoke_args[i] = args[i];
                TEKLOG_DEBUG("Arg[%d] = %p", i, invoke_args[i]);
            } else {
                invoke_args[i] = NULL;
                TEKLOG_DEBUG("Arg[%d] = NULL", i);
            }
        }
    }

    // 异常对象
    void* exception = NULL;

    TEKLOG_DEBUG("Calling il2cpp_runtime_invoke with method=%p, obj=%p, params=%p",
                 method, instance, invoke_args);

    // 调用方法（obj 参数就是实例，不需要放入 params 数组）
    void* result = il2cpp_runtime_invoke(method, instance, invoke_args, &exception);

    // 检查异常
    if (exception != NULL) {
        TEKLOG_ERROR("Exception occurred during method invocation: exception=%p", exception);
        if (invoke_args) free(invoke_args);
        return false;
    }

    TEKLOG_DEBUG("il2cpp_runtime_invoke returned: %p", result);

    // 处理返回值
    if (return_value != NULL) {
        const patch_type_t return_type = patchlib_method_get_return_type(method);

        if (return_type != PATCH_VOID) {
            if (result == NULL) {
                // 返回 null
                if (return_type == PATCH_OBJECT || return_type == PATCH_POINTER) {
                    *(void**)return_value = NULL;
                } else {
                    // 值类型返回默认值 0
                    memset(return_value, 0, get_size_from_patch_type(return_type));
                }
                TEKLOG_DEBUG("Return value is NULL");
            } else if (return_type != PATCH_OBJECT && return_type != PATCH_POINTER) {
                // 值类型需要拆箱
                void* unboxed = il2cpp_object_unbox(result);
                if (unboxed) {
                    const size_t size = get_size_from_patch_type(return_type);
                    memcpy(return_value, unboxed, size);
                    TEKLOG_DEBUG("Return value (unboxed): type=%d, size=%zu, value=%p",
                                 return_type, size, unboxed);
                } else {
                    TEKLOG_ERROR("Failed to unbox return value");
                    if (invoke_args) free(invoke_args);
                    return false;
                }
            } else {
                // 引用类型：直接返回对象指针
                *(void**)return_value = result;
                TEKLOG_DEBUG("Return value (reference): %p", result);
            }
        }
    }

    TEKLOG_DEBUG("Method invocation completed successfully");

    // 清理
    if (invoke_args) {
        free(invoke_args);
    }

    return true;
}
*/

bool patchlib_method_invoke_args(patch_handle_t method, patch_handle_t instance,
                                void *return_value, void** args) {
    TEKLOG_DEBUG("patchlib_method_invoke_args called: method=%p, instance=%p, return_value=%p",
                 method, instance, return_value);

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle");
        return false;
    }

    // 获取方法签名
    patch_method_signature_t signature;
    if (!patchlib_method_get_signature(method, &signature)) {
        TEKLOG_ERROR("Failed to get method signature");
        return false;
    }

    // 验证实例方法调用
    if (signature.is_instance && !patchlib_is_valid(instance)) {
        TEKLOG_ERROR("Instance method requires valid instance");
        return false;
    }

    void* method_ptr = patchlib_method_get_pointer(method);
    if (!method_ptr) {
        TEKLOG_ERROR("Failed to get method pointer");
        return false;
    }

    // 计算总参数个数
    const int total_arg_count = (int)tefstd_vector_size(&signature.arg_types)
                        + (signature.is_instance ? 1 : 0);

    TEKLOG_DEBUG("Method invocation: ptr=%p, total_args=%d, is_instance=%s",
                 method_ptr, total_arg_count, signature.is_instance ? "true" : "false");

    // 准备ffi类型数组
    ffi_type** arg_types = malloc(sizeof(ffi_type*) * total_arg_count);
    void** arg_values = malloc(sizeof(void*) * total_arg_count);

    if (!arg_types || !arg_values) {
        TEKLOG_ERROR("Memory allocation failed for FFI arguments");
        free(arg_types);
        free(arg_values);
        return false;
    }

    int arg_index = 0;

    // 设置this指针（如果是实例方法）
    if (signature.is_instance) {
        arg_types[arg_index] = &ffi_type_pointer;
        arg_values[arg_index] = &instance;
        TEKLOG_DEBUG("Argument %d: this pointer=%p", arg_index, instance);
        arg_index++;
    }

    // 设置方法参数
    for (size_t i = 0; i < tefstd_vector_size(&signature.arg_types); i++) {
        const patch_type_t* arg_type = tefstd_vector_at(&signature.arg_types, i);
        arg_types[arg_index] = patch_type_to_ffi_type(*arg_type);
        arg_values[arg_index] = args ? args[i] : NULL;
        TEKLOG_DEBUG("Argument %d: type=%d, value=%p", arg_index, *arg_type, arg_values[arg_index]);
        arg_index++;
    }

    // 准备ffi调用
    ffi_cif cif;
    ffi_type* return_ffi_type = patch_type_to_ffi_type(signature.return_type);

    ffi_abi abi = FFI_DEFAULT_ABI;
#if defined(__aarch64__)
    abi = FFI_SYSV;
    TEKLOG_DEBUG("Using FFI_SYSV ABI for ARM64");
#elif defined(__arm__)
    abi = FFI_SYSV;
    TEKLOG_DEBUG("Using FFI_SYSV ABI for ARM32");
#else
    TEKLOG_DEBUG("Using FFI_DEFAULT_ABI");
#endif

    TEKLOG_DEBUG("Preparing FFI CIF: abi=%d, nargs=%d", abi, total_arg_count);

    if (ffi_prep_cif(&cif, abi, total_arg_count, return_ffi_type, arg_types) != FFI_OK) {
        TEKLOG_ERROR("FFI CIF preparation failed");
        free(arg_values);
        free(arg_types);
        return false;
    }

    TEKLOG_DEBUG("Calling method via FFI");

    // 调用方法
    ffi_call(&cif, FFI_FN(method_ptr), return_value, arg_values);

    TEKLOG_DEBUG("Method invocation completed successfully");

    patchlib_method_signature_free(&signature);
    free(arg_values);
    free(arg_types);
    return true;
}


patch_handle_t patchlib_method_make_generic_instance(patch_handle_t method, const tefstd_vector_t *template_types) {
    TEKLOG_DEBUG("Manual generic method instantiation without symbols");

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle");
        return PATCH_NULL;
    }

    patch_handle_t declaring_class = il2cpp_method_get_declaring_type(method);
    void* reflection_method = il2cpp_method_get_object(method, declaring_class);

    if (!reflection_method) {
        TEKLOG_ERROR("Failed to get reflection method object");
        return PATCH_NULL;
    }

    void* type_array = create_type_array_from_vector(template_types, il2cpp_class_from_name(il2cpp_get_corlib(), "System", "Type"));

    const void* result_obj = ((void*(*)(void*, void*))patchlib_method_get_pointer(patchlib_MakeGenericMethod_impl))(reflection_method, type_array);

    void* result_method = il2cpp_method_get_from_reflection(result_obj);
    if (!result_method) {
        TEKLOG_ERROR("Failed to convert reflection result to MethodInfo");
        return PATCH_NULL;
    }

    TEKLOG_DEBUG("Generic method created successfully: %p", result_method);
    return result_method;
}
