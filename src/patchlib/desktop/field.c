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

#include "net_api.h"
#include "internal/log.h"

const char* patchlib_field_get_name(const patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("Invalid field handle");
        return NULL;
    }

    const char* result = net_field_get_name(field);
    TEKLOG_DEBUG("Field name: %s", result ? result : "NULL");
    return result;
}

bool patchlib_field_is_static(const patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("Invalid field handle");
        return false;
    }

    const bool result = net_field_is_static(field);
    TEKLOG_DEBUG("Field is static: %s", result ? "true" : "false");
    return result;
}

bool patchlib_field_is_instance(const patch_handle_t field) {
    const bool result = !patchlib_field_is_static(field);
    TEKLOG_DEBUG("Field is instance: %s", result ? "true" : "false");
    return result;
}

bool patchlib_field_is_const(const patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("Invalid field handle");
        return false;
    }

    const bool result = net_field_is_const(field);
    TEKLOG_DEBUG("Field is const: %s", result ? "true" : "false");
    return result;
}

bool patchlib_field_is_thread_static(const patch_handle_t field) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("Invalid field handle");
        return false;
    }

    const bool result = net_field_is_thread_static(field);
    TEKLOG_DEBUG("Field is thread static: %s", result ? "true" : "false");
    return result;
}

void patchlib_field_get_value(const patch_handle_t field, const patch_handle_t instance, void *value) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_ERROR("Invalid field handle");
        return;
    }

    if (!value) {
        TEKLOG_ERROR("Value pointer is NULL");
        return;
    }

    if (net_field_get_value(field, instance, value)) {
        TEKLOG_DEBUG("Field value retrieved successfully");
    }
}

void patchlib_field_set_value(const patch_handle_t field, const patch_handle_t instance, void *value) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_ERROR("Invalid field handle");
        return;
    }

    if (!value) {
        TEKLOG_ERROR("Value pointer is NULL");
        return;
    }

    TEKLOG_DEBUG("Setting field value: field=%d, instance=%d",
                 field, instance);

    if (net_field_set_value(field, instance, value)) TEKLOG_DEBUG("Field value set successfully");
}

bool patchlib_field_free(const patch_handle_t field) {
    TEKLOG_DEBUG("Field freed: %d", field);
    net_field_free(field);
    return true;
}