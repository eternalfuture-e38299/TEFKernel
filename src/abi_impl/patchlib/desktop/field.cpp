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

#include "tef/core/log.hpp"

extern "C" {
#include "../il2cpp_api.h"
}

using tef::log::debug;
using tef::log::error;
using tef::log::warn;

namespace {
    /* 静态字段 / 静态线程字段的统一实例校验 */
    bool check_instance(patch_handle_t field, patch_handle_t instance, const char *op) {
        if (!patchlib_is_valid(field)) {
            error("Invalid field handle");
            return false;
        }
        if (!instance && !patchlib_field_is_static(field)) {
            const char *f_name = patchlib_field_get_name(field);
            error("Instance is NULL for non-static field '{}' ({})",
                  f_name ? f_name : "?", op);
            return false;
        }
        return true;
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
    return il2cpp_field_is_thread_static(field);
}

bool patchlib_field_is_static(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        warn("Invalid field handle");
        return false;
    }
    return il2cpp_field_is_static(field);
}

bool patchlib_field_is_const(patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        warn("Invalid field handle");
        return false;
    }
    return il2cpp_field_is_literal(field);
}

/* ============================================================================
 * 读写
 * ============================================================================ */

void patchlib_field_get_value(patch_handle_t field, patch_handle_t instance,
                              void *value) {
    if (!value) {
        error("Value pointer is NULL");
        return;
    }
    if (!check_instance(field, instance, "get")) return;

    debug("Getting field value: field={} instance={}",
          fmt::ptr(field), fmt::ptr(instance));
    il2cpp_field_get_value(field, instance, value);
}

void patchlib_field_set_value(patch_handle_t field, patch_handle_t instance,
                              void *value) {
    if (!value) {
        error("Value pointer is NULL");
        return;
    }
    if (!check_instance(field, instance, "set")) return;

    debug("Setting field value: field={} instance={}",
          fmt::ptr(field), fmt::ptr(instance));
    il2cpp_field_set_value(field, instance, value);
}
}
