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

#include "tef/core/log.hpp"

extern "C" {
#include "../il2cpp_api.h"
}

using tef::log::debug;
using tef::log::error;

extern "C" {
/* ============================================================================
 * 泛型实例化（桌面：il2cpp_class_make_generic + class_from_system_type）
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
    if (!type_args) return nullptr;

    /* 桌面端 il2cpp_class_make_generic 接受 void* types[]（元素是 Il2CppClass*） */
    patch_handle_t generic = il2cpp_class_make_generic(
        generic_type_def, type_args->data, static_cast<int>(type_args->size));
    if (!generic) {
        error("il2cpp_class_make_generic failed");
        return nullptr;
    }
    debug("Generic type created: {}", fmt::ptr(generic));

    patch_handle_t result = il2cpp_class_from_system_type(generic);
    debug("Final generic type: {}", fmt::ptr(result));
    return result;
}

/* ============================================================================
 * 类型比较
 * ============================================================================ */

bool patchlib_type_is_same(patch_handle_t t1, patch_handle_t t2) {
    return il2cpp_class_is_same(t1, t2);
}

/* ============================================================================
 * 资源释放
 *
 * loader 的 il2cpp_free 对 GCHandle.Zero 会抛 "GCHandle value cannot be zero"，
 * 从而中断当前 hook。这里先判空，保证 patchlib_free(nullptr) 安全。
 * ============================================================================ */

void patchlib_free(patch_handle_t type) {
    if (type == nullptr) return;
    il2cpp_free(type);
}

/* ============================================================================
 * 句柄复制（hook 临时句柄快照用）
 * ============================================================================ */

patch_handle_t patchlib_handle_copy(patch_handle_t handle) {
    if (!handle) return nullptr;
    return il2cpp_object_copy(handle);
}
}
