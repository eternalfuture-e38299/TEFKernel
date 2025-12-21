/*******************************************************************************
 * tefkernel - method
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

#include "patchlib/method.h"

#include <ffi.h>

#include "il2cpp_api.h"
#include "private.h"
#include "internal/log.h"

const char *patchlib_method_get_name(patch_handle_t method) {
    if (!patchlib_is_valid(method))
        return false;

    return il2cpp_method_get_name(method);
}

patch_handle_t patchlib_method_get_return_type(patch_handle_t method) {
    if (!patchlib_is_valid(method))
        return PATCH_NULL;

    return il2cpp_class_from_il2cpp_type(il2cpp_method_get_return_type(method));
}

int patchlib_method_get_param_count(patch_handle_t method) {
    if (!patchlib_is_valid(method))
        return -1;

    return (int) il2cpp_method_get_param_count(method);
}

patch_handle_t patchlib_method_get_param_type(patch_handle_t method, const int index) {
    if (!patchlib_is_valid(method))
        return PATCH_NULL;

    return il2cpp_class_from_il2cpp_type(il2cpp_method_get_param(method, index));
}

bool patchlib_method_is_instance(patch_handle_t method) {
    if (!patchlib_is_valid(method))
        return false;

    return il2cpp_method_is_instance(method);
}

bool patchlib_method_is_static(patch_handle_t method) {
    return !patchlib_method_is_instance(method);
}

patch_handle_t patchlib_method_make_generic_instance(patch_handle_t method, const tef_vector_t *template_types) {
    if (!patchlib_is_valid(method))
        return false;

    void *ref_method = il2cpp_method_get_object(method, il2cpp_method_get_declaring_type(method));
    void *type_array = create_type_array_from_vector(template_types);

    const void *generic_method = ((void*(*)(void *, void *)) patchlib_method_get_pointer(
        patchlib_MakeGenericMethod_impl))(
        ref_method, type_array);

    return il2cpp_method_get_from_reflection(generic_method);
}

void *patchlib_method_get_pointer(patch_handle_t method) {
    if (!patchlib_is_valid(method))
        return NULL;

    return *(void **) method;
}

static patch_type_t il2cpp_type_to_patch_type(patch_handle_t type) {
    if (!type) {
        TEKLOG_DEBUG("NULL type provided, returning PATCH_VOID");
        return PATCH_VOID;
    }

    patch_type_t result;
    switch (il2cpp_type_get_type(type)) {
        case IL2CPP_TYPE_VOID:     result = PATCH_VOID; break;
        case IL2CPP_TYPE_BOOLEAN:  result = PATCH_BOOL; break;
        case IL2CPP_TYPE_I1:       result = PATCH_INT8; break;
        case IL2CPP_TYPE_U1:       result = PATCH_UINT8; break;
        case IL2CPP_TYPE_I2:       result = PATCH_INT16; break;
        case IL2CPP_TYPE_U2:       result = PATCH_UINT16; break;
        case IL2CPP_TYPE_I4:       result = PATCH_INT32; break;
        case IL2CPP_TYPE_R4:       result = PATCH_FLOAT; break;
        case IL2CPP_TYPE_U4:       result = PATCH_UINT32; break;
        case IL2CPP_TYPE_I8:       result = PATCH_INT64; break;
        case IL2CPP_TYPE_U8:       result = PATCH_UINT64; break;
        case IL2CPP_TYPE_R8:       result = PATCH_DOUBLE; break;
        default:                   result = PATCH_POINTER; break;
    }

    TEKLOG_TRACE("Converted IL2CPP type %p to patch type: %d", type, result);
    return result;
}

bool patchlib_method_get_signature(patch_handle_t method, patch_method_signature_t* signature) {
    signature->return_type = il2cpp_type_to_patch_type(patchlib_method_get_return_type(method));
    signature->is_instance = patchlib_method_is_instance(method);
    tefstd_vector_init(&signature->arg_types, sizeof(patch_type_t));

    for (int i = 0; i < patchlib_method_get_param_count(method); ++i) {
        patch_type_t t = il2cpp_type_to_patch_type(patchlib_method_get_param_type(method, i));
        tefstd_vector_push_back(&signature->arg_types, &t);
    }

    return true;
}

bool patchlib_method_free(patch_handle_t method) { return true; }