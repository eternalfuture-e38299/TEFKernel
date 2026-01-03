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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "patchlib/type.h"

#include "internal/log.h"

bool patchlib_method_is_static(patch_handle_t method) {
    const bool result = !patchlib_method_is_instance(method);
    TEKLOG_DEBUG("Method is static: %s", result ? "true" : "false");
    return result;
}

bool patchlib_method_invoke(patch_handle_t method, patch_handle_t instance,
                           void *return_value, ...) {
    TEKLOG_DEBUG("patchlib_method_invoke called: method="PATCH_HANDLE_FMT", instance="PATCH_HANDLE_FMT, method, instance);

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

