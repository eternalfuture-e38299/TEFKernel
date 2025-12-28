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

#include "il2cpp_api.h"
#include "private.h"
#include "internal/log.h"

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

    int result = (int) il2cpp_method_get_param_count(method);
    TEKLOG_DEBUG("Method parameter count: %d", result);
    return result;
}

bool patchlib_method_is_instance(patch_handle_t method) {
    if (!patchlib_is_valid(method)) {
        TEKLOG_WARN("Invalid method handle");
        return false;
    }

    bool result = il2cpp_method_is_instance(method);
    TEKLOG_DEBUG("Method is instance: %s", result ? "true" : "false");
    return result;
}

bool patchlib_method_is_static(patch_handle_t method) {
    bool result = !patchlib_method_is_instance(method);
    TEKLOG_DEBUG("Method is static: %s", result ? "true" : "false");
    return result;
}

patch_handle_t patchlib_method_make_generic_instance(patch_handle_t method, const tef_vector_t *template_types) {
    TEKLOG_DEBUG("patchlib_method_make_generic_instance called: method=%p, template_types_count=%zu",
                 method, template_types ? tefstd_vector_size(template_types) : 0);

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle");
        return PATCH_NULL;
    }

    void *ref_method = il2cpp_method_get_object(method, il2cpp_method_get_declaring_type(method));
    void *type_array = create_type_array_from_vector(template_types);

    TEKLOG_DEBUG("Creating generic method: ref_method=%p, type_array=%p", ref_method, type_array);

    const void *generic_method = ((void*(*)(void *, void *)) patchlib_method_get_pointer(
        patchlib_MakeGenericMethod_impl))(
        ref_method, type_array);

    patch_handle_t result = il2cpp_method_get_from_reflection(generic_method);
    TEKLOG_DEBUG("Generic method created: %p", result);
    return result;
}

void *patchlib_method_get_pointer(patch_handle_t method) {
    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle");
        return NULL;
    }

    void* result = *(void **) method;
    TEKLOG_DEBUG("Method pointer: %p", result);
    return result;
}

static patch_type_t il2cpp_type_to_patch_type(patch_handle_t type) {
    if (!type) {
        TEKLOG_DEBUG("NULL type provided, returning PATCH_VOID");
        return PATCH_VOID;
    }

    const int il2cpp_type = il2cpp_type_get_type(type);

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
        default:                   result = PATCH_POINTER; break;
    }

    TEKLOG_TRACE("Converted IL2CPP type %d to patch type: %d", il2cpp_type, result);
    return result;
}

bool patchlib_method_get_signature(patch_handle_t method, patch_method_signature_t* signature) {
    TEKLOG_DEBUG("patchlib_method_get_signature called: method=%p", method);

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle");
        return false;
    }

    signature->return_type = il2cpp_type_to_patch_type(il2cpp_method_get_return_type(method));
    signature->is_instance = patchlib_method_is_instance(method);
    tefstd_vector_init(&signature->arg_types, sizeof(patch_type_t));

    int param_count = patchlib_method_get_param_count(method);
    TEKLOG_DEBUG("Method signature: return_type=%d, is_instance=%s, param_count=%d",
                 signature->return_type, signature->is_instance ? "true" : "false", param_count);

    for (int i = 0; i < param_count; ++i) {
        patch_type_t t = il2cpp_type_to_patch_type(il2cpp_method_get_param(method, i));
        tefstd_vector_push_back(&signature->arg_types, &t);
        TEKLOG_DEBUG("Parameter %d type: %d", i, t);
    }

    TEKLOG_DEBUG("Signature extraction completed: total_params=%zu",
                 tefstd_vector_size(&signature->arg_types));
    return true;
}

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
    int total_arg_count = (int)tefstd_vector_size(&signature.arg_types);
    if (signature.is_instance) {
        total_arg_count++; // 加上this指针
    }

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

    free(arg_values);
    free(arg_types);
    return true;
}

bool patchlib_method_invoke(patch_handle_t method, patch_handle_t instance,
                           void *return_value, ...) {
    TEKLOG_DEBUG("patchlib_method_invoke called: method=%p, instance=%p", method, instance);

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

    size_t arg_count = tefstd_vector_size(&signature.arg_types);
    TEKLOG_DEBUG("Varargs invocation: arg_count=%zu", arg_count);

    // 如果没有参数，直接调用基础版本
    if (arg_count == 0) {
        TEKLOG_DEBUG("No arguments, calling simple version");
        return patchlib_method_invoke_args(method, instance, return_value, NULL);
    }

    // 准备参数数组
    void** args = malloc(sizeof(void*) * arg_count);
    if (!args) {
        TEKLOG_ERROR("Memory allocation failed for arguments");
        return false;
    }

    va_list ap;
    va_start(ap, return_value);

    TEKLOG_DEBUG("Extracting %zu arguments from va_list", arg_count);

    // 从可变参数中提取参数值
    for (size_t i = 0; i < arg_count; i++) {
        const patch_type_t* arg_type = tefstd_vector_at(&signature.arg_types, i);

        // 根据类型提取参数
        switch (*arg_type) {
            case PATCH_BOOL:
                args[i] = malloc(sizeof(bool));
                *(bool*)args[i] = va_arg(ap, int);
                TEKLOG_DEBUG("Argument %zu: bool=%s", i, *(bool*)args[i] ? "true" : "false");
                break;
            case PATCH_INT8:
                args[i] = malloc(sizeof(int8_t));
                *(int8_t*)args[i] = va_arg(ap, int);
                TEKLOG_DEBUG("Argument %zu: int8=%d", i, *(int8_t*)args[i]);
                break;
            case PATCH_UINT8:
                args[i] = malloc(sizeof(uint8_t));
                *(uint8_t*)args[i] = va_arg(ap, unsigned int);
                TEKLOG_DEBUG("Argument %zu: uint8=%u", i, *(uint8_t*)args[i]);
                break;
            case PATCH_INT16:
                args[i] = malloc(sizeof(int16_t));
                *(int16_t*)args[i] = va_arg(ap, int);
                TEKLOG_DEBUG("Argument %zu: int16=%d", i, *(int16_t*)args[i]);
                break;
            case PATCH_UINT16:
                args[i] = malloc(sizeof(uint16_t));
                *(uint16_t*)args[i] = va_arg(ap, unsigned int);
                TEKLOG_DEBUG("Argument %zu: uint16=%u", i, *(uint16_t*)args[i]);
                break;
            case PATCH_INT32:
                args[i] = malloc(sizeof(int32_t));
                *(int32_t*)args[i] = va_arg(ap, int32_t);
                TEKLOG_DEBUG("Argument %zu: int32=%d", i, *(int32_t*)args[i]);
                break;
            case PATCH_UINT32:
                args[i] = malloc(sizeof(uint32_t));
                *(uint32_t*)args[i] = va_arg(ap, uint32_t);
                TEKLOG_DEBUG("Argument %zu: uint32=%u", i, *(uint32_t*)args[i]);
                break;
            case PATCH_INT64:
                args[i] = malloc(sizeof(int64_t));
                *(int64_t*)args[i] = va_arg(ap, int64_t);
                TEKLOG_DEBUG("Argument %zu: int64=%ld", i, *(int64_t*)args[i]);
                break;
            case PATCH_UINT64:
                args[i] = malloc(sizeof(uint64_t));
                *(uint64_t*)args[i] = va_arg(ap, uint64_t);
                TEKLOG_DEBUG("Argument %zu: uint64=%lu", i, *(uint64_t*)args[i]);
                break;
            case PATCH_FLOAT:
                args[i] = malloc(sizeof(float));
                *(float*)args[i] = (float)va_arg(ap, double);
                TEKLOG_DEBUG("Argument %zu: float=%f", i, *(float*)args[i]);
                break;
            case PATCH_DOUBLE:
                args[i] = malloc(sizeof(double));
                *(double*)args[i] = va_arg(ap, double);
                TEKLOG_DEBUG("Argument %zu: double=%f", i, *(double*)args[i]);
                break;
            default:
                args[i] = malloc(sizeof(void*));
                *(void**)args[i] = va_arg(ap, void*);
                TEKLOG_DEBUG("Argument %zu: pointer=%p", i, *(void**)args[i]);
                break;
        }
    }
    va_end(ap);

    // 调用基础版本
    TEKLOG_DEBUG("Calling method_invoke_args with extracted arguments");
    const bool result = patchlib_method_invoke_args(method, instance, return_value, args);

    // 清理参数内存
    for (size_t i = 0; i < arg_count; i++) {
        free(args[i]);
    }
    free(args);

    TEKLOG_DEBUG("Method invocation %s", result ? "succeeded" : "failed");
    return result;
}

bool patchlib_method_free(patch_handle_t method) {
    TEKLOG_DEBUG("Method freed: %p", method);
    return true;
}