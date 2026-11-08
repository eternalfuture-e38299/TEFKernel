/*******************************************************************************
 * tefkernel - type
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

#include "patchlib/type.h"
#include "patchlib/method.h"

#include "tef/core/log.hpp"

extern "C" {
#include "../il2cpp_api.h"
#include "private.hpp"
}

using tef::log::debug;
using tef::log::error;

extern "C" {
/* ============================================================================
 * 泛型实例化（Android：MonoType + 反射 MakeGenericType）
 *
 * il2cpp_class_make_generic 在 Android 不可用，必须走反射：
 *   1. 取 generic_type_def 的 MonoType（Il2CppReflectionType*）
 *   2. 把 type_args 包成 Il2CppArray<System.Type>
 *   3. 调 patchlib_MakeGenericType(MonoType, Type[]) → 返回 Il2CppReflectionType*
 *   4. il2cpp_class_from_system_type 转回 Il2CppClass*
 * ============================================================================ */

patch_handle_t patchlib_type_make_generic_type(patch_handle_t generic_type_def,
                                               const tefstd_vector_t *type_args) {
    debug("patchlib_type_make_generic_type: def={} args={}",
          fmt::ptr(generic_type_def),
          type_args ? tefstd_vector_size(type_args) : 0);

    if (!patchlib_is_valid(generic_type_def)) {
        error("Invalid generic type definition");
        return nullptr;
    }

    patch_handle_t mono_type = patchlib_type_get_mono_type(generic_type_def);
    debug("Mono type: {}", fmt::ptr(mono_type));

    void *type_array = create_type_array_from_vector(
        type_args,
        il2cpp_class_from_name(il2cpp_get_corlib(), "System", "Type"));
    debug("Type array created: {}", type_array);

    /* 反射调用：MakeGenericType(MonoType, Type[]) */
    const auto fn = reinterpret_cast<void* (*)(void *, void *)>(
        patchlib_method_get_pointer(patchlib_make_generic_type));
    if (!fn) {
        error("patchlib_MakeGenericType not resolved");
        return nullptr;
    }

    void *generic_type = fn(mono_type, type_array);
    debug("Generic type created: {}", generic_type);

    patch_handle_t result = il2cpp_class_from_system_type(generic_type);
    debug("Final generic type: {}", fmt::ptr(result));
    return result;
}

/* ============================================================================
 * MonoType
 *
 * il2cpp_class_get_type 在 Android 不可用，用 Il2CppClass 内存偏移取 Il2CppType*
 * （offset = sizeof(void*) * 4 = 32 字节，Android 32/64 位均是 4 指针宽度）。
 * ============================================================================ */

patch_handle_t patchlib_type_get_mono_type(patch_handle_t type) {
    debug("patchlib_type_get_mono_type: type={}", fmt::ptr(type));

    if (!patchlib_is_valid(type)) {
        error("Invalid type handle");
        return nullptr;
    }

    patch_handle_t result = il2cpp_type_get_object(
        static_cast<char *>(type) + sizeof(void *) * 4);
    debug("Mono type result: {}", fmt::ptr(result));
    return result;
}

/* ============================================================================
 * GC 根（Android：裸对象指针需要显式保活）
 * ============================================================================ */

void *patchlib_object_root(patch_handle_t object) {
    if (!object) return nullptr;
    return il2cpp_gchandle_new(object, /*pinned=*/false);
}

void patchlib_object_unroot(void *root) {
    if (root) il2cpp_gchandle_free(root);
}

/* ============================================================================
 * 类型比较
 *
 * Android：句柄就是 Il2CppClass*，直接指针比较。
 * （桌面：il2cpp_class_is_same 走 GCHandle 相等）
 * ============================================================================ */

bool patchlib_type_is_same(patch_handle_t t1, patch_handle_t t2) {
    return t1 == t2;
}
}
