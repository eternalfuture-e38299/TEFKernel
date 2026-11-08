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
 * Created: 2026/5/24
 *******************************************************************************/

#include "patchlib/struct/array.h"

#include <algorithm>

#include "patchlib/type.h"

#include <cstring>

#include "tef/core/log.hpp"

extern "C" {
#include "../../il2cpp_api.h"
}

using tef::log::debug;
using tef::log::error;
using tef::log::warn;

namespace {
    /* 值类型数组名 —— 命中则元素按值处理，其余按引用。
     * 名字形如 "Single[]" / "System.Int32[]"（子串匹配，与旧行为一致）。 */
    constexpr const char *k_value_type_array_names[] = {
        "Boolean[]", "Byte[]", "SByte[]", "Int16[]", "UInt16[]",
        "Int32[]", "UInt32[]", "Int64[]", "UInt64[]",
        "IntPtr[]", "UIntPtr[]", "Single[]", "Double[]",
        "Char[]", "Decimal[]",
    };
} // namespace

extern "C" {
/* ============================================================================
 * 创建 / 长度
 * ============================================================================ */

patch_handle_t patchlib_array_create(size_t size, patch_handle_t type) {
    debug("patchlib_array_create: size={} type={}", size, fmt::ptr(type));

    if (size == 0) {
        error("Invalid array size: 0");
        return nullptr;
    }
    if (!patchlib_is_valid(type)) {
        error("Invalid type handle: {}", fmt::ptr(type));
        return nullptr;
    }

    patch_handle_t result = il2cpp_array_new(type, size);
    if (result) debug("Array created: {} (len={})", fmt::ptr(result), size);
    else error("il2cpp_array_new failed");
    return result;
}

size_t patchlib_array_length(patch_handle_t array) {
    if (!patchlib_is_valid(array)) {
        error("Invalid array handle");
        return 0;
    }
    return il2cpp_array_length(array);
}

bool patchlib_array_empty(patch_handle_t array) {
    if (!patchlib_is_valid(array)) {
        warn("Invalid array handle, considering as empty");
        return true;
    }
    return il2cpp_array_length(array) == 0;
}

/* ============================================================================
 * 元素引用判定
 * ============================================================================ */

bool patchlib_array_element_is_reference(patch_handle_t array) {
    if (!patchlib_is_valid(array)) return false;

    void *klass = il2cpp_object_get_class(array);
    if (!klass) return true;

    const char *name = il2cpp_class_get_name(klass);
    patchlib_free(klass);
    if (!name || !*name) return true;

    return std::ranges::all_of(k_value_type_array_names,
                               [name](const char *v) { return std::strstr(name, v) == nullptr; });
}
}
