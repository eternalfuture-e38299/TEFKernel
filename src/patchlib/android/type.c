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

#include "patchlib/method.h"

#include <stdlib.h>

#include "private.h"
#include "internal/log.h"
#include "../il2cpp_api.h"

patch_handle_t patchlib_type_make_generic_type(patch_handle_t generic_type_def, const tefstd_vector_t *type_args) {
    TEKLOG_DEBUG("patchlib_type_make_generic_type called: generic_type_def=%p, type_args_count=%zu",
                 generic_type_def, type_args ? tefstd_vector_size(type_args) : 0);

    if (!patchlib_is_valid(generic_type_def)) {
        TEKLOG_ERROR("Invalid generic type definition");
        return PATCH_NULL;
    }

    patch_handle_t c_mono_type = patchlib_type_get_mono_type(generic_type_def);
    TEKLOG_DEBUG("Mono type: %p", c_mono_type);

    void *type_array = create_type_array_from_vector(type_args, il2cpp_class_from_name(il2cpp_get_corlib(), "System", "Type"));
    TEKLOG_DEBUG("Type array created: %p", type_array);

    void *generic_type = ((void*(*)(void *, void *)) patchlib_method_get_pointer(patchlib_MakeGenericType))(
        c_mono_type, type_array);

    TEKLOG_DEBUG("Generic type created: %p", generic_type);

    patch_handle_t result = il2cpp_class_from_system_type(generic_type);
    TEKLOG_DEBUG("Final generic type: %p", result);

    return result;
}

patch_handle_t patchlib_type_get_mono_type(patch_handle_t type) {
    TEKLOG_DEBUG("patchlib_type_get_mono_type called: type=%p", type);

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return PATCH_NULL;
    }

    patch_handle_t result = il2cpp_type_get_object((char *) type + sizeof(void *) * 4);
    TEKLOG_DEBUG("Mono type result: %p", result);
    return result;
}

bool patchlib_type_is_same(patch_handle_t t1, patch_handle_t t2) {
    return t1 == t2;
}