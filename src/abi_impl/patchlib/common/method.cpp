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
#include "patchlib/type.h"

#include <cstring>

#include "tef/core/log.hpp"

extern "C" {
#include "../il2cpp_api.h"
}

using tef::log::debug;
using tef::log::error;
using tef::log::trace;
using tef::log::warn;

namespace {
    /* ============================================================================
     * IL2CPP 类型 → patch_type_t
     *
     * VALUETYPE 的处理（历史上踩过两个坑）：
     *   1. 枚举 → PATCH_INT32
     *   2. System 命名空间的原始类型 → 对应 PATCH_*
     *   3. 其余结构体 → PATCH_STRUCT（让 ffi 展开字段，处理 Vector2 等）
     * ============================================================================ */

    // ReSharper disable once CppInconsistentNaming
    patch_type_t il2cpp_type_to_patch_type(patch_handle_t type) {
        if (!type) return PATCH_VOID;

        if (il2cpp_type_is_byref(type)) return PATCH_POINTER;

        switch (il2cpp_type_get_type(type)) {
            case IL2CPP_TYPE_VOID: return PATCH_VOID;
            case IL2CPP_TYPE_BOOLEAN: return PATCH_BOOL;
            case IL2CPP_TYPE_I1: return PATCH_INT8;
            case IL2CPP_TYPE_U1: return PATCH_UINT8;
            case IL2CPP_TYPE_I2: return PATCH_INT16;
            case IL2CPP_TYPE_U2: return PATCH_UINT16;
            case IL2CPP_TYPE_I4: return PATCH_INT32;
            case IL2CPP_TYPE_U4: return PATCH_UINT32;
            case IL2CPP_TYPE_I8: return PATCH_INT64;
            case IL2CPP_TYPE_U8: return PATCH_UINT64;
            case IL2CPP_TYPE_R4: return PATCH_FLOAT;
            case IL2CPP_TYPE_R8: return PATCH_DOUBLE;
            case IL2CPP_TYPE_CHAR: return PATCH_CHAR;
            case IL2CPP_TYPE_PTR: return PATCH_POINTER;
            case IL2CPP_TYPE_ENUM: return PATCH_INT32;

            case IL2CPP_TYPE_VALUETYPE: {
                patch_handle_t kclass = il2cpp_class_from_type(type);
                patch_handle_t parent = il2cpp_class_get_parent(kclass);
                const char *parent_name = parent ? il2cpp_class_get_name(parent) : nullptr;
                patchlib_free(parent);

                /* 枚举：只有真正转换出新句柄时才释放 kclass。
                 * 桌面端 il2cpp_class_from_type 是恒等宏 → kclass == type，
                 * 释放它会 double-free（mono_gchandle_is_in_domain SIGSEGV）。 */
                if (parent_name && std::strcmp(parent_name, "Enum") == 0) {
                    if (kclass != type) patchlib_free(kclass);
                    return PATCH_INT32;
                }

                const char *class_name = il2cpp_class_get_name(kclass);
                const char *ns = il2cpp_class_get_namespace(kclass);
                if (kclass != type) patchlib_free(kclass);

                if (ns && std::strcmp(ns, "System") == 0) {
                    struct {
                        const char *n;
                        patch_type_t t;
                    } k_map[] = {
                        {.n = "Int32", .t = PATCH_INT32},
                        {.n = "UInt32", .t = PATCH_UINT32},
                        {.n = "Int64", .t = PATCH_INT64},
                        {.n = "UInt64", .t = PATCH_UINT64},
                        {.n = "Single", .t = PATCH_FLOAT},
                        {.n = "Double", .t = PATCH_DOUBLE},
                        {.n = "Boolean", .t = PATCH_BOOL},
                        {.n = "Char", .t = PATCH_CHAR},
                        {.n = "Byte", .t = PATCH_UINT8},
                        {.n = "SByte", .t = PATCH_INT8},
                        {.n = "Int16", .t = PATCH_INT16},
                        {.n = "UInt16", .t = PATCH_UINT16},
                        {.n = "IntPtr", .t = PATCH_POINTER},
                        {.n = "UIntPtr", .t = PATCH_POINTER},
                    };
                    for (auto &[name, pt]: k_map)
                        if (std::strcmp(class_name, name) == 0) return pt;
                }

                /* 复杂结构体：让 ffi 展开字段（不是按指针处理——PATCH_POINTER 会
                 * 让 libffi 用指针 ABI 调返回结构体的方法，Vector2 恒 (0,0)）。 */
                return PATCH_STRUCT;
            }

            default: return PATCH_OBJECT;
        }
    }
} // namespace

extern "C" {
/* ============================================================================
 * 特征
 * ============================================================================ */

bool patchlib_method_is_instance(patch_handle_t method) {
    if (!patchlib_is_valid(method)) {
        warn("Invalid method handle");
        return false;
    }
    return il2cpp_method_is_instance(method);
}

bool patchlib_method_is_static(patch_handle_t method) {
    return !patchlib_method_is_instance(method);
}

/* ============================================================================
 * 基本信息
 * ============================================================================ */

const char *patchlib_method_get_name(patch_handle_t method) {
    if (!patchlib_is_valid(method)) {
        warn("Invalid method handle");
        return nullptr;
    }
    return il2cpp_method_get_name(method);
}

int patchlib_method_get_param_count(patch_handle_t method) {
    if (!patchlib_is_valid(method)) {
        warn("Invalid method handle");
        return -1;
    }
    return static_cast<int>(il2cpp_method_get_param_count(method));
}

int patchlib_method_get_token(patch_handle_t method) {
    if (!patchlib_is_valid(method)) return 0;
    return static_cast<int>(il2cpp_method_get_token(method));
}

patch_type_t patchlib_method_get_return_type(patch_handle_t method) {
    patch_handle_t rt = il2cpp_method_get_return_type(method);
    patch_type_t result = il2cpp_type_to_patch_type(rt);
    patchlib_free(rt);
    return result;
}

/* ============================================================================
 * 签名
 * ============================================================================ */

bool patchlib_method_get_signature(patch_handle_t method,
                                   patch_method_signature_t *sig) {
    if (!patchlib_is_valid(method) || !sig) {
        error("Invalid method handle or sig");
        return false;
    }

    sig->method = method;
    sig->token = patchlib_method_get_token(method);
    sig->is_instance = patchlib_method_is_instance(method);

    /* return_type_handle 必须是 Class* 而不是 Type*：
     * patch_type_to_ffi_type 的 PATCH_STRUCT 分支会拿它调
     * il2cpp_class_get_fields，喂 Type* 会得到空结构体 → ffi 不写返回缓冲
     * → Vector2 返回值恒 (0,0)。
     *
     * Android 上必须有这个转换；桌面端保持原样——il2cpp_class_from_type
     * 在桌面是恒等宏，而 signature_free 会 free 它，提前 free 会把引擎
     * 持有��� GCHandle 作废（mono_gchandle_is_in_domain SIGSEGV）。 */
#if defined(__ANDROID__)
    sig->return_type_handle =
            il2cpp_class_from_type(il2cpp_method_get_return_type(method));
#else
    sig->return_type_handle = il2cpp_method_get_return_type(method);
#endif

    sig->return_type = patchlib_method_get_return_type(method);

    tefstd_vector_init(&sig->arg_types, sizeof(patch_type_t));
    tefstd_vector_init(&sig->arg_names, sizeof(const char *));
    tefstd_vector_init(&sig->arg_type_handles, sizeof(patch_handle_t));

    const int n = patchlib_method_get_param_count(method);
    for (int i = 0; i < n; ++i) {
        patch_handle_t pt = il2cpp_method_get_param(method, static_cast<uint32_t>(i));
        patch_handle_t handle = il2cpp_class_from_type(pt);
        patch_type_t t = il2cpp_type_to_patch_type(pt);
        const char *name = il2cpp_method_get_param_name(method, static_cast<uint32_t>(i));

        tefstd_vector_push_back(&sig->arg_types, &t);
        tefstd_vector_push_back(&sig->arg_names, &name);
        tefstd_vector_push_back(&sig->arg_type_handles, &handle);
    }

    return true;
}

bool patchlib_method_signature_free(patch_method_signature_t *sig) {
    if (!sig) return false;

    tefstd_vector_destroy(&sig->arg_types);
    tefstd_vector_destroy(&sig->arg_names);

#if !defined(__ANDROID__)
    /* 桌面端 arg_type_handles 里是独立 GCHandle，必须逐个释放 */
    for (size_t i = 0; i < tefstd_vector_size(&sig->arg_type_handles); ++i) {
        auto h = *static_cast<patch_handle_t *>(
            tefstd_vector_at(&sig->arg_type_handles, i));
        patchlib_free(h);
    }
#endif

    tefstd_vector_destroy(&sig->arg_type_handles);

    sig->method = nullptr;
    sig->is_instance = false;
    sig->return_type = PATCH_VOID;

    patchlib_free(sig->return_type_handle);
    sig->return_type_handle = nullptr;

    return true;
}
}
