/*******************************************************************************
 * tefkernel - array
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
 * Created: 2026/1/10
 *******************************************************************************/

#include "patchlib/struct/array.h"
#include "patchlib/type.h"

#include <cstddef>

#include "tef/core/log.hpp"

extern "C" {
#include "../../il2cpp_api.h"
}

using tef::log::debug;
using tef::log::error;

extern "C" {
/* ============================================================================
 * 元素大小
 * ============================================================================ */

size_t patchlib_array_element_size(patch_handle_t array) {
    if (!patchlib_is_valid(array)) return 0;

    const void *klass = il2cpp_object_get_class(array);
    const int size = il2cpp_array_element_size(klass);
    return size > 0 ? static_cast<size_t>(size) : 0;
}

/* ============================================================================
 * 读写 / 填充
 * ============================================================================ */

bool patchlib_array_at(patch_handle_t array, size_t index, void *out_value) {
    if (!patchlib_is_valid(array)) {
        error("Invalid array handle");
        return false;
    }

    const bool ok = il2cpp_array_at(array, index, out_value);
    if (!ok)
        /* 非 blittable 元素（Nullable<T> / 含引用字段的结构体）无法按值封送。
         * 这是预期跳过，不是错误——用 DEBUG 而不是 ERROR，避免逐元素刷日志。 */
        debug("Skipped array[{}] (unsupported element type)", index);
    return ok;
}

bool patchlib_array_set(patch_handle_t array, const size_t index, void *new_value) {
    if (!patchlib_is_valid(array)) {
        error("Invalid array handle");
        return false;
    }
    il2cpp_array_set(array, index, new_value);
    return true;
}

bool patchlib_array_fill(patch_handle_t array, void *value) {
    if (!patchlib_is_valid(array)) {
        error("Invalid array handle");
        return false;
    }
    il2cpp_array_fill(array, value);
    return true;
}

/* ============================================================================
 * 复制
 * ============================================================================ */

bool patchlib_array_copy_from_c(patch_handle_t dest, const void *src, const size_t count) {
    if (!patchlib_is_valid(dest)) {
        error("Invalid destination array handle");
        return false;
    }
    if (!src) {
        error("Source C array is NULL");
        return false;
    }
    il2cpp_array_copy_from_c(dest, src, count);
    return true;
}

bool patchlib_array_copy_to_c(void *dest, patch_handle_t src, const size_t count) {
    if (!dest) {
        error("Destination C array is NULL");
        return false;
    }
    if (!patchlib_is_valid(src)) {
        error("Invalid source array handle");
        return false;
    }
    il2cpp_array_copy_to_c(dest, src, count);
    return true;
}

bool patchlib_array_copy(patch_handle_t dest, patch_handle_t src, const size_t count) {
    if (!patchlib_is_valid(dest)) {
        error("Invalid destination array handle");
        return false;
    }
    if (!patchlib_is_valid(src)) {
        error("Invalid source array handle");
        return false;
    }
    /* C# 侧负责类型检查 / 自动适应 / 引用类型处理 */
    return il2cpp_array_copy(dest, src, count);
}

/* ============================================================================
 * 扩容
 * ============================================================================ */

patch_handle_t patchlib_array_resize(patch_handle_t src, const size_t nsize, void *dvalue) {
    if (!src) return nullptr;
    return il2cpp_array_resize(src, static_cast<int>(nsize), dvalue);
}
}
