/*******************************************************************************
 * tefkernel - string
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

#include "patchlib/struct/string.h"
#include "patchlib/type.h"

#include <cstdint>

#include "tef/core/log.hpp"

extern "C" {
#include "../../il2cpp_api.h"
}

using tef::log::debug;
using tef::log::error;
using tef::log::warn;

extern "C" {

patch_handle_t patchlib_string_create(const char* str) {
    debug("patchlib_string_create: str={}", str ? str : "NULL");

    if (!str) {
        error("Input string is NULL");
        return nullptr;
    }

    patch_handle_t result = il2cpp_string_new(str);
    if (!result) {
        error("Failed to create string");
        return nullptr;
    }

    debug("String created: {}", fmt::ptr(result));
    return result;
}

bool patchlib_string_empty(patch_handle_t str) {
    if (!patchlib_is_valid(str)) {
        warn("Invalid string handle, treating as empty");
        return true;
    }
    return il2cpp_string_length(str) == 0;
}

size_t patchlib_string_length(patch_handle_t str) {
    if (!patchlib_is_valid(str)) {
        error("Invalid string handle");
        return 0;
    }
    return static_cast<size_t>(il2cpp_string_length(str));
}

}