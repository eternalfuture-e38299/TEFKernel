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
        patchlib_method_signature_free(&signature);
        return false;
    }

    void* method_ptr = patchlib_method_get_pointer(method);
    if (!method_ptr) {
        TEKLOG_ERROR("Failed to get method pointer");
        patchlib_method_signature_free(&signature);
        return false;
    }

    // 计算总参数个数
    size_t explicit_arg_count = tefstd_vector_size(&signature.arg_types);
    const int total_arg_count = (int)explicit_arg_count + (signature.is_instance ? 1 : 0);

    TEKLOG_DEBUG("Method invocation: ptr=%p, total_args=%d, is_instance=%s",
                 method_ptr, total_arg_count, signature.is_instance ? "true" : "false");

    // 分配FFI参数数组（使用 RAII 风格，先分配再检查）
    ffi_type** arg_types = malloc(sizeof(ffi_type*) * total_arg_count);
    void** arg_values = malloc(sizeof(void*) * total_arg_count);

    if (!arg_types || !arg_values) {
        TEKLOG_ERROR("Memory allocation failed for FFI arguments");
        free(arg_types);   // free(NULL) 是安全的
        free(arg_values);
        patchlib_method_signature_free(&signature);
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

    // 设置方法参数 - 添加边界检查
    if (args) {
        for (size_t i = 0; i < explicit_arg_count; i++) {
            const patch_type_t* arg_type = tefstd_vector_at(&signature.arg_types, i);
            if (!arg_type) {
                TEKLOG_ERROR("Failed to get argument type at index %zu", i);
                free(arg_values);
                free(arg_types);
                patchlib_method_signature_free(&signature);
                return false;
            }

            arg_types[arg_index] = patch_type_to_ffi_type(*arg_type);
            arg_values[arg_index] = args[i];  // args 应该有 explicit_arg_count 个元素
            TEKLOG_DEBUG("Argument %d: type=%d, value=%p", arg_index, *arg_type, arg_values[arg_index]);
            arg_index++;
        }
    } else {
        // args 为 NULL，所有参数传 NULL
        for (size_t i = 0; i < explicit_arg_count; i++) {
            const patch_type_t* arg_type = tefstd_vector_at(&signature.arg_types, i);
            arg_types[arg_index] = patch_type_to_ffi_type(*arg_type);
            arg_values[arg_index] = NULL;
            TEKLOG_DEBUG("Argument %d: type=%d, value=NULL", arg_index, *arg_type);
            arg_index++;
        }
    }

    // 准备FFI调用
    ffi_cif cif;
    ffi_type* return_ffi_type = patch_type_to_ffi_type(signature.return_type);
    if (!return_ffi_type) {
        TEKLOG_ERROR("Invalid return type");
        free(arg_values);
        free(arg_types);
        patchlib_method_signature_free(&signature);
        return false;
    }

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

    ffi_status status = ffi_prep_cif(&cif, abi, (unsigned int)total_arg_count, return_ffi_type, arg_types);
    if (status != FFI_OK) {
        TEKLOG_ERROR("FFI CIF preparation failed: %d", status);
        free(arg_values);
        free(arg_types);
        patchlib_method_signature_free(&signature);
        return false;
    }

    TEKLOG_DEBUG("Calling method via FFI");

    // 调用方法
    ffi_call(&cif, FFI_FN(method_ptr), return_value, arg_values);

    TEKLOG_DEBUG("Method invocation completed successfully");

    // 清理资源
    if (arg_values) free(arg_values);
    if (arg_types) free(arg_types);
    patchlib_method_signature_free(&signature);
    return true;
}

bool patchlib_constructor_invoke(patch_handle_t constructor,
                patch_handle_t *return_instance, void **args) {
    patch_handle_t instance = patchlib_type_new_instance(il2cpp_method_get_class(constructor));
    *return_instance = instance;
    return patchlib_method_invoke_args(constructor,
        instance,
        NULL, args);
}

// ReSharper disable once CppUseInternalLinkage
typedef struct Il2CppObject
{
    void*/*Il2CppClass*/ *klass;
    void*/*MonitorData*/ *monitor;
} Il2CppObject;

// ReSharper disable once CppUseInternalLinkage
typedef struct Il2CppReflectionMethod
{
    Il2CppObject object;
    const void/*MethodInfo*/ *method;
    void*/*Il2CppString*/ *name;
    void*/*Il2CppReflectionType*/ *reftype;
} Il2CppReflectionMethod;


patch_handle_t patchlib_method_make_generic_instance(patch_handle_t method, const tefstd_vector_t *template_types) {
    TEKLOG_DEBUG("Manual generic method instantiation without symbols");

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle");
        return PATCH_NULL;
    }

    Il2CppReflectionMethod reflection_method;
    reflection_method.method = method;

    void* type_array = create_type_array_from_vector(template_types, il2cpp_class_from_name(il2cpp_get_corlib(), "System", "Type"));

    const Il2CppReflectionMethod* result_obj = ((void*(*)(void*, void*))patchlib_method_get_pointer(patchlib_MakeGenericMethod_impl))(&reflection_method, type_array);

    patch_handle_t result_method = (patch_handle_t)result_obj->method;
    if (!result_method) {
        TEKLOG_ERROR("Failed to convert reflection result to MethodInfo");
        return PATCH_NULL;
    }

    TEKLOG_DEBUG("Generic method created successfully: %p", result_method);
    return result_method;
}
