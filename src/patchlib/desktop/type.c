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

#include <string.h>

#include "internal/log.h"
#include "../il2cpp_api.h"
#include "patchlib/method.h"

patch_handle_t patchlib_type_make_generic_type(patch_handle_t generic_type_def, const tefstd_vector_t *type_args) {
    TEKLOG_DEBUG("patchlib_type_make_generic_type called: generic_type_def=%p, type_args_count=%zu",
                 generic_type_def, type_args ? tefstd_vector_size(type_args) : 0);

    if (!patchlib_is_valid(generic_type_def)) {
        TEKLOG_ERROR("Invalid generic type definition");
        return PATCH_NULL;
    }


    patch_handle_t generic_type = il2cpp_class_make_generic(generic_type_def, type_args->data, type_args->size);

    TEKLOG_DEBUG("Generic type created: %p", generic_type);

    patch_handle_t result = il2cpp_class_from_system_type(generic_type);
    TEKLOG_DEBUG("Final generic type: %p", result);

    return result;
}

bool patchlib_type_is_same(patch_handle_t t1, patch_handle_t t2) {
    return il2cpp_class_is_same(t1, t2);
}

void patchlib_free(patch_handle_t type) {
    il2cpp_free(type);
}

patch_handle_t patchlib_handle_copy(patch_handle_t handle) {
    return il2cpp_object_copy(handle);
}