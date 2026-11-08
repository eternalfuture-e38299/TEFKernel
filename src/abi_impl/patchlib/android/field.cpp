/*******************************************************************************
 * tefkernel - field
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

#include "patchlib/field.h"
#include "patchlib/type.h"

#include <cstdint>
#include <cstring>

#include "tef/core/log.hpp"

extern "C" {
#include "../il2cpp_api.h"
}

using tef::log::error;
using tef::log::warn;

namespace {
    /* Il2CppType::attrs 的两个位（II.23.1.15 FieldAttributes）：
     *   0x0010 = static
     *   0x0040 = literal（const）
     * il2cpp_api.h 的 Il2CppType 结构体不可见（未导出），用指针偏移读：
     *   Il2CppType 前 4 个指针（data / byref 等联合 / num_mods / byref / pinned / valuetype），
     *   attrs 在 offsetof(Il2CppType, attrs) = sizeof(void*)+sizeof(int) 附近……
     * 原代码用 offset = sizeof(void*)，即「data 之后紧跟 attrs」——与当前 il2cpp
     * 的 Il2CppType 布局一致（data 是 1 个指针，后面 attrs 是 int）。 */
    bool is_static_impl(patch_handle_t field) {
        const auto tp = reinterpret_cast<uintptr_t>(il2cpp_field_get_type(field));
        return (*reinterpret_cast<int *>(tp + sizeof(void *)) & 0x0010) != 0;
    }

    bool is_const_impl(patch_handle_t field) {
        const auto tp = reinterpret_cast<uintptr_t>(il2cpp_field_get_type(field));
        return (*reinterpret_cast<int *>(tp + sizeof(void *)) & 0x0040) != 0;
    }
} // namespace

extern "C" {
/* ============================================================================
 * 字段特征
 * ============================================================================ */

bool patchlib_field_is_thread_static(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        warn("Invalid field handle");
        return false;
    }
    return il2cpp_field_get_offset(field) == static_cast<size_t>(-1);
}

bool patchlib_field_is_static(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        warn("Invalid field handle");
        return false;
    }
    return is_static_impl(field);
}

bool patchlib_field_is_const(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        warn("Invalid field handle");
        return false;
    }
    return is_const_impl(field);
}

/* ============================================================================
 * 读写（Android：走指针 + memcpy）
 * ============================================================================ */

void patchlib_field_get_value(patch_handle_t field, patch_handle_t instance,
                              void *value) {
    if (!patchlib_is_valid(field)) {
        error("Invalid field handle");
        return;
    }
    if (!value) {
        error("Value pointer is NULL");
        return;
    }

    /* const / thread-static 只能走 il2cpp_field_static_get_value */
    if (patchlib_field_is_const(field) || patchlib_field_is_thread_static(field)) {
        il2cpp_field_static_get_value(field, value);
        return;
    }

    const size_t field_size = patchlib_field_get_size(field);
    void *p = patchlib_field_get_pointer(field, instance);
    if (!p) {
        error("Failed to get field pointer");
        return;
    }
    std::memcpy(value, p, field_size);
}

void patchlib_field_set_value(patch_handle_t field, patch_handle_t instance,
                              void *value) {
    if (!patchlib_is_valid(field)) {
        error("Invalid field handle");
        return;
    }
    if (!value) {
        error("Value pointer is NULL");
        return;
    }

    const size_t field_size = patchlib_field_get_size(field);
    void *p = patchlib_field_get_pointer(field, instance);
    if (!p) {
        error("Failed to get field pointer");
        return;
    }
    std::memcpy(p, value, field_size);
}

/* ============================================================================
 * 字段真实指针
 * ============================================================================ */

void *patchlib_field_get_pointer(patch_handle_t field, void *instance) {
    if (!patchlib_is_valid(field)) {
        error("Invalid field handle");
        return nullptr;
    }
    if (patchlib_field_is_const(field)) {
        warn("Cannot get pointer to const field");
        return nullptr;
    }
    if (patchlib_field_is_thread_static(field)) {
        warn("Thread static fields not supported");
        return nullptr;
    }

    const size_t offset = il2cpp_field_get_offset(field);
    const void *parent = il2cpp_field_get_parent(field);

    if (patchlib_field_is_static(field)) {
        void *static_data = il2cpp_class_get_static_field_data(parent);
        if (!static_data) {
            error("Failed to get static field data");
            return nullptr;
        }
        return reinterpret_cast<void *>(
            reinterpret_cast<uintptr_t>(static_data) + offset);
    }

    /* 实例字段必须给 instance。否则算出 NULL + offset（比如 0x12f），
     * 调用方再 memcpy 就是 SIGSEGV，且故障地址看起来像随机数。 */
    if (!instance) {
        const char *f_name = patchlib_field_get_name(field);
        error("Instance is NULL for non-static field '{}'", f_name ? f_name : "?");
        return nullptr;
    }

    /* 旧代码把 il2cpp_field_get_parent() 返回的 Il2CppClass* 直接传给
     * il2cpp_type_get_type()，读的是类对象里的 gc_desc 字节；约 1/256 的类
     * 会随机命中 IL2CPP_TYPE_VALUETYPE，导致该类的所有实例字段地址偏移 -16。
     * 内核拿到的 instance 都是类对象指针（不是装箱结构体），这里不做修正。 */
    return reinterpret_cast<void *>(
        reinterpret_cast<uintptr_t>(instance) + offset);
}
}
