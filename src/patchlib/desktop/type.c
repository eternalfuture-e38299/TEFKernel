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

#include "net_api.h"
#include "internal/log.h"
#include "patchlib/method.h"

patch_handle_t patchlib_type_get_type(const char *ns, const char *name) {
    return net_get_type(ns, name);
}

patch_handle_t patchlib_type_new_instance(const patch_handle_t type) {
    return net_new_instance(type);
}

patch_handle_t patchlib_type_make_generic_type(const patch_handle_t generic_type_def, const tefstd_vector_t *type_args) {
    return net_type_make_generic(generic_type_def, type_args->data, type_args->size);
}

patch_handle_t patchlib_type_get_mono_type(const patch_handle_t type) {
    return type;
}

const char *patchlib_type_get_name(const patch_handle_t type) {
    TEKLOG_DEBUG("patchlib_type_get_name called: type=%d", type);

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return NULL;
    }

    const char *result = net_type_get_name(type);
    TEKLOG_DEBUG("Type name: %s", result ? result : "NULL");
    return result;
}

const char *patchlib_type_get_namespace(const patch_handle_t type) {
    TEKLOG_DEBUG("patchlib_type_get_namespace called: type=%d", type);

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return NULL;
    }

    const char *result = net_type_get_namespace(type);
    TEKLOG_DEBUG("Type namespace: %s", result ? result : "NULL");
    return result;
}

patch_handle_t patchlib_type_get_parent(const patch_handle_t type) {
    TEKLOG_DEBUG("patchlib_type_get_parent called: type=%d", type);

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return PATCH_NULL;
    }

    const patch_handle_t result = net_type_get_parent(type);
    TEKLOG_DEBUG("Parent type: %d", result);
    return result;
}

patch_handle_t patchlib_type_get_field(const patch_handle_t type, const char *name) {
    TEKLOG_DEBUG("patchlib_type_get_field called: type=%d, name='%s'", type, name ? name : "NULL");

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return PATCH_NULL;
    }

    if (!name) {
        TEKLOG_ERROR("Field name is NULL");
        return PATCH_NULL;
    }

    const patch_handle_t result = net_type_get_field(type, name);
    TEKLOG_DEBUG("Field '%s' result: %d", name, result);
    return result;
}

patch_handle_t patchlib_type_get_property(const patch_handle_t type, const char *name) {
    TEKLOG_DEBUG("patchlib_type_get_property called: type=%d, name='%s'", type, name ? name : "NULL");

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return PATCH_NULL;
    }

    if (!name) {
        TEKLOG_ERROR("Property name is NULL");
        return PATCH_NULL;
    }

    const patch_handle_t result = net_type_get_property(type, name);
    TEKLOG_DEBUG("Property '%s' result: %d", name, result);
    return result;
}

patch_handle_t patchlib_type_get_method_by_param_count(const patch_handle_t type, const char *name,
                                                       const int args_count) {
    TEKLOG_DEBUG("patchlib_type_get_method_by_param_count called: type=%d, name='%s', args_count=%d",
                 type, name ? name : "NULL", args_count);

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return PATCH_NULL;
    }

    if (!name) {
        TEKLOG_ERROR("Method name is NULL");
        return PATCH_NULL;
    }

    const patch_handle_t result = net_type_get_method_from_args_count(type, name, args_count);
    TEKLOG_DEBUG("Method '%s' with %d parameters result: %d", name, args_count, result);
    return result;
}


static bool method_matches_signature(
    const patch_handle_t method,
    const int args_count,
    const patch_handle_t *args_types, // 可为 NULL
    const char **args_names // 可为 NULL
) {
    TEKLOG_DEBUG("method_matches_signature called: method=" PATCH_HANDLE_FMT ", args_count=%d",
                 method, args_count);

    if (args_count < 0) {
        TEKLOG_ERROR("Invalid args_count: %d", args_count);
        return false;
    }

    patch_method_signature_t signature = {0};
    if (!patchlib_method_get_signature(method, &signature)) {
        TEKLOG_ERROR("Failed to get method signature");
        patchlib_method_signature_free(&signature);
        return false;
    }

    const int actual_arg_count = (int) tefstd_vector_size(&signature.arg_types);
    if (actual_arg_count != args_count) {
        TEKLOG_DEBUG("Parameter count mismatch: expected %d, got %d",
                     args_count, actual_arg_count);
        patchlib_method_signature_free(&signature);
        return false;
    }

    if (args_types) {
        for (int i = 0; i < args_count; ++i) {
            const patch_type_t expected_type = *(patch_type_t *) tefstd_vector_at(&signature.arg_types, i);

            patch_type_t actual_type = PATCH_NULL;

            if (args_types[i])
                actual_type = net_type_to_patchlib_type(args_types[i]);

            TEKLOG_DEBUG("Parameter %d type: expected=%d, actual=%d",
                         i, expected_type, actual_type);
        }
    }

    if (args_names) {
        for (int i = 0; i < args_count; ++i) {
            if (args_names[i]) {
                const char *actual_name = *(const char **) tefstd_vector_at(&signature.arg_names, i);
                TEKLOG_DEBUG("Parameter %d name: expected='%s', actual='%s'",
                             i, args_names[i], actual_name ? actual_name : "NULL");

                if (!actual_name || strcmp(actual_name, args_names[i]) != 0) {
                    TEKLOG_DEBUG("Parameter %d name mismatch", i);
                    patchlib_method_signature_free(&signature);
                    return false;
                }
            }
        }
    }

    TEKLOG_DEBUG("Method matches signature");
    patchlib_method_signature_free(&signature);
    return true;
}

patch_handle_t patchlib_type_find_method(
    const patch_handle_t type,
    const char *name,
    const int args_count,
    const patch_handle_t *args_types, // nullable
    const char **args_names // nullable
) {
    TEKLOG_DEBUG("patchlib_type_find_method called: type=%d, name='%s', args_count=%d",
                 type, name ? name : "NULL", args_count);

    if (!type || !name) {
        TEKLOG_ERROR("Type or name is NULL");
        return PATCH_NULL;
    }

    tefstd_vector_t methods = {};
    patchlib_type_get_methods(type, false, &methods);

    const size_t method_count = tefstd_vector_size(&methods);
    TEKLOG_DEBUG("Searching in %zu methods", method_count);

    patch_handle_t result = PATCH_NULL;

    for (int i = 0; i < method_count; ++i) {
        const patch_handle_t *method = tefstd_vector_at(&methods, i);
        const char *method_name = patchlib_type_get_name(*method);

        TEKLOG_DEBUG("Checking method %d: %d, name: '%s'", i, *method, method_name ? method_name : "NULL");

        if (method_name && strcmp(method_name, name) == 0) {
            TEKLOG_DEBUG("Method name matches, checking arguments");
            if (method_matches_signature(*method, args_count, args_types, args_names)) {
                result = *method;
                TEKLOG_INFO("Found matching method: %d", result);
                break;
            }
        }
    }

    tefstd_vector_destroy(&methods);

    if (result == PATCH_NULL) {
        TEKLOG_WARN("No matching method found: %s with %d parameters", name, args_count);
    }

    return result;
}

bool patchlib_type_get_inner_types(const patch_handle_t type, const bool including_parent, tefstd_vector_t *array) {
    TEKLOG_DEBUG("patchlib_type_get_inner_types called: type=%d, including_parent=%s",
                 type, including_parent ? "true" : "false");

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return false;
    }

    patch_handle_t *inner_types;
    int count;
    net_type_get_inner_types(type, including_parent, &inner_types, &count);

    const bool result = tefstd_vector_init_from_array(array, sizeof(patch_handle_t), inner_types, count);

    TEKLOG_DEBUG("Get inner types result: %s, count: %zu", result ? "success" : "failed", tefstd_vector_size(array));
    return result;
}

bool patchlib_type_get_methods(const patch_handle_t type, const bool including_parent, tefstd_vector_t *array) {
    TEKLOG_DEBUG("patchlib_type_get_methods called: type=%d, including_parent=%s",
                 type, including_parent ? "true" : "false");

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return false;
    }

    patch_handle_t *methods;
    int count;
    net_type_get_methods(type, including_parent, &methods, &count);

    const bool result = tefstd_vector_init_from_array(array, sizeof(patch_handle_t), methods, count);
    TEKLOG_DEBUG("Get methods result: %s, count: %zu", result ? "success" : "failed", tefstd_vector_size(array));
    return result;
}

bool patchlib_type_get_fields(const patch_handle_t type, const bool including_parent, tefstd_vector_t *array) {
    TEKLOG_DEBUG("patchlib_type_get_fields called: type=%d, including_parent=%s",
                 type, including_parent ? "true" : "false");

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return false;
    }

    patch_handle_t *fields;
    int count;
    net_type_get_fields(type, including_parent, &fields, &count);

    const bool result = tefstd_vector_init_from_array(array, sizeof(patch_handle_t), fields, count);

    TEKLOG_DEBUG("Get fields result: %s, count: %zu", result ? "success" : "failed", tefstd_vector_size(array));
    return result;
}

bool patchlib_type_get_properties(const patch_handle_t type, const bool including_parent, tefstd_vector_t *array) {
    TEKLOG_DEBUG("patchlib_type_get_properties called: type=%d, including_parent=%s",
                 type, including_parent ? "true" : "false");

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return false;
    }

    patch_handle_t *properties;
    int count;
    net_type_get_properties(type, including_parent, &properties, &count);

    const bool result = tefstd_vector_init_from_array(array, sizeof(patch_handle_t), properties, count);

    TEKLOG_DEBUG("Get properties result: %s, count: %zu", result ? "success" : "failed", tefstd_vector_size(array));
    return result;
}

bool patchlib_type_free(const patch_handle_t type) {
    return net_type_free(type);
}

bool patchlib_object_free(const patch_handle_t object) {
    return net_object_free(object);
}

patch_handle_t patchlib_object_persist(patch_handle_t object) {
    if (!patchlib_is_valid(object))
        return PATCH_NULL;

    return net_object_persist(object);
}