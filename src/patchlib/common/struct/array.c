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

#include <string.h>

#include "internal/log.h"
#include "../../il2cpp_api.h"

patch_handle_t patchlib_array_create(const size_t size, patch_handle_t type) {
    TEKLOG_DEBUG("patchlib_array_create called: size=%zu, type=%p", size, type);

    if (size <= 0) {
        TEKLOG_ERROR("Invalid array size: %zu", size);
        return PATCH_NULL;
    }

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle: %p", type);
        return PATCH_NULL;
    }

    TEKLOG_DEBUG("Creating array via il2cpp_array_new");
    patch_handle_t result = il2cpp_array_new(type, size);
    TEKLOG_DEBUG("Array created: %p, length should be: %zu", result, size);

    return result;
}

bool patchlib_array_empty(patch_handle_t array) {
    if (!patchlib_is_valid(array)) {
        TEKLOG_WARN("Invalid array handle, considering as empty");
        return true;
    }

    const bool result = il2cpp_array_length(array) == 0;
    return result;
}

size_t patchlib_array_length(patch_handle_t array) {
    TEKLOG_DEBUG("patchlib_array_length called: array=%p", array);

    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return 0;
    }

    const size_t result = il2cpp_array_length(array);

    TEKLOG_DEBUG("Array length: %zu", result);
    return result;
}

/* 数组元素是否是引用类型。名字形如 "Single[]" / "System.Int32[]"；
 * 命中托管值类型名则返回 false，其余按引用处理（与旧行为一致）。 */
bool patchlib_array_element_is_reference(patch_handle_t array) {
    if (!patchlib_is_valid(array)) return false;

    void *klass = il2cpp_object_get_class(array);
    if (!klass) return true;

    const char *name = il2cpp_class_get_name(klass);
    if (!name || !*name) return true;

    static const char *const value_type_names[] = {
        "Boolean[]", "Byte[]", "SByte[]", "Int16[]", "UInt16[]",
        "Int32[]", "UInt32[]", "Int64[]", "UInt64[]",
        "IntPtr[]", "UIntPtr[]", "Single[]", "Double[]",
        "Char[]", "Decimal[]",
    };

    for (size_t i = 0; i < sizeof(value_type_names) / sizeof(value_type_names[0]); ++i) {
        if (strstr(name, value_type_names[i]) != NULL) {
            return false;
        }
    }
    return true;
}