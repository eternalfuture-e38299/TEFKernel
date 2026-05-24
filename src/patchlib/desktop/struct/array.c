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

#include <stdint.h>

#include "internal/log.h"
#include <string.h>
#include "../../il2cpp_api.h"

bool patchlib_array_at(patch_handle_t array, const size_t index, void* out_value) {
    TEKLOG_DEBUG("patchlib_array_at called: array=%p, index=%zu, out_value=%p",
                 array, index, out_value);

    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return false;
    }

    il2cpp_array_at(array, index, out_value);

    TEKLOG_DEBUG("Array element retrieved successfully");
    return true;
}

bool patchlib_array_set(patch_handle_t array, const size_t index, void* new_value) {
    TEKLOG_DEBUG("patchlib_array_set called: array=%p, index=%zu, new_value=%p",
                 array, index, new_value);

    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return false;
    }

    il2cpp_array_set(array, index, new_value);

    TEKLOG_DEBUG("Array element set successfully");
    return true;
}

bool patchlib_array_fill(patch_handle_t array, void* value) {
    TEKLOG_DEBUG("patchlib_array_fill called: array=%p, value=%p",
                 array, value);

    if (!patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid array handle");
        return false;
    }

    il2cpp_array_fill(array, value);

    TEKLOG_DEBUG("Array filled successfully");
    return true;
}

bool patchlib_array_copy_from_c(patch_handle_t dest, const void* src, const size_t count) {
    TEKLOG_DEBUG("patchlib_array_copy_from_c called: dest=%p, src=%p, count=%zu", dest, src, count);

    if (!patchlib_is_valid(dest)) {
        TEKLOG_ERROR("Invalid destination array handle");
        return false;
    }

    if (!src) {
        TEKLOG_ERROR("Source C array is NULL");
        return false;
    }

    il2cpp_array_copy_from_c(dest, src, count);

    return true;
}

bool patchlib_array_copy_to_c(void* dest, patch_handle_t src, const size_t count) {
    TEKLOG_DEBUG("patchlib_array_copy_to_c called: dest=%p, src=%p, count=%zu", dest, src, count);

    if (!dest) {
        TEKLOG_ERROR("Destination C array is NULL");
        return false;
    }

    if (!patchlib_is_valid(src)) {
        TEKLOG_ERROR("Invalid source array handle");
        return false;
    }

    il2cpp_array_copy_to_c(dest, src, count);

    return true;
}