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
#include <string.h>

#include "il2cpp_api.h"
#include "private.h"
#include "internal/log.h"
#include "../common_private.h"

patch_handle_t patchlib_type_get_type(const char *ns, const char *name) {
    TEKLOG_DEBUG("patchlib_type_get_type called: namespace='%s', name='%s'", ns ? ns : "NULL", name ? name : "NULL");

    if (!ns || !name) {
        TEKLOG_ERROR("Namespace or name is NULL");
        return PATCH_NULL;
    }

    const void *domain = il2cpp_domain_get(); // 获取il2cpp domain
    if (!domain) {
        TEKLOG_ERROR("Failed to get il2cpp domain");
        return PATCH_NULL;
    }

    size_t assemblies_count = 0;
    void **assemblies = il2cpp_domain_get_assemblies(domain, &assemblies_count); // 获取所有程序集
    if (!assemblies || assemblies_count == 0) {
        TEKLOG_ERROR("No assemblies found in domain");
        return PATCH_NULL;
    }

    TEKLOG_DEBUG("Searching in %zu assemblies", assemblies_count);

    for (size_t i = 0; i < assemblies_count; ++i) {
        const void *assembly = assemblies[i];
        const void *image = il2cpp_assembly_get_image(assembly); //获取image

        if (!image) {
            TEKLOG_DEBUG("Assembly %zu has no image", i);
            continue;
        }

        TEKLOG_DEBUG("Searching in assembly %zu, image: %p", i, image);
        void *found_class = il2cpp_class_from_name(image, ns, name); // 从名称获取类
        if (found_class) {
            TEKLOG_INFO("Found type: %s.%s at %p", ns, name, found_class);
            return found_class;
        }
    }

    TEKLOG_WARN("Type not found: %s.%s", ns, name);
    return PATCH_NULL;
}

patch_handle_t patchlib_type_new_instance(patch_handle_t type) {
    TEKLOG_DEBUG("patchlib_type_new_instance called: type=%p", type);

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return PATCH_NULL;
    }

    // 获取类型信息用于日志
    const char* type_name = patchlib_type_get_name(type);
    const char* type_namespace = patchlib_type_get_namespace(type);

    TEKLOG_DEBUG("Creating instance of type: %s.%s",
                 type_namespace ? type_namespace : "NULL",
                 type_name ? type_name : "NULL");

    patch_handle_t result = il2cpp_object_new(type);

    if (result) {
        TEKLOG_DEBUG("Successfully created new instance: %p for type %s.%s",
                    result,
                    type_namespace ? type_namespace : "NULL",
                    type_name ? type_name : "NULL");
    } else {
        TEKLOG_ERROR("Failed to create instance for type: %p (%s.%s)",
                     type,
                     type_namespace ? type_namespace : "NULL",
                     type_name ? type_name : "NULL");
    }

    return result;
}

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

const char* patchlib_type_get_name(patch_handle_t type) {
    TEKLOG_DEBUG("patchlib_type_get_name called: type=%p", type);

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return NULL;
    }

    const char* result = il2cpp_class_get_name(type);
    TEKLOG_DEBUG("Type name: %s", result ? result : "NULL");
    return result;
}

const char* patchlib_type_get_namespace(patch_handle_t type) {
    TEKLOG_DEBUG("patchlib_type_get_namespace called: type=%p", type);

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return NULL;
    }

    const char* result = il2cpp_class_get_namespace(type);
    TEKLOG_DEBUG("Type namespace: %s", result ? result : "NULL");
    return result;
}

patch_handle_t patchlib_type_get_parent(patch_handle_t type) {
    TEKLOG_DEBUG("patchlib_type_get_parent called: type=%p", type);

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return PATCH_NULL;
    }

    patch_handle_t result = il2cpp_class_get_parent(type);
    TEKLOG_DEBUG("Parent type: %p", result);
    return result;
}

patch_handle_t patchlib_type_get_field(patch_handle_t type, const char *name) {
    TEKLOG_DEBUG("patchlib_type_get_field called: type=%p, name='%s'", type, name ? name : "NULL");

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return PATCH_NULL;
    }

    if (!name) {
        TEKLOG_ERROR("Field name is NULL");
        return PATCH_NULL;
    }

    patch_handle_t result = il2cpp_class_get_field_from_name(type, name);
    TEKLOG_DEBUG("Field '%s' result: %p", name, result);
    return result;
}

patch_handle_t patchlib_type_get_property(patch_handle_t type, const char *name) {
    TEKLOG_DEBUG("patchlib_type_get_property called: type=%p, name='%s'", type, name ? name : "NULL");

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return PATCH_NULL;
    }

    if (!name) {
        TEKLOG_ERROR("Property name is NULL");
        return PATCH_NULL;
    }

    patch_handle_t result = il2cpp_class_get_property_from_name(type, name);
    TEKLOG_DEBUG("Property '%s' result: %p", name, result);
    return result;
}

patch_handle_t patchlib_type_get_method_by_param_count(patch_handle_t type, const char *name, const int args_count) {
    TEKLOG_DEBUG("patchlib_type_get_method_by_param_count called: type=%p, name='%s', args_count=%d",
                 type, name ? name : "NULL", args_count);

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return PATCH_NULL;
    }

    if (!name) {
        TEKLOG_ERROR("Method name is NULL");
        return PATCH_NULL;
    }

    patch_handle_t result = il2cpp_class_get_method_from_name(type, name, args_count);
    TEKLOG_DEBUG("Method '%s' with %d parameters result: %p", name, args_count, result);
    return result;
}

// 内部 helper：判断一个 method 是否匹配给定的参数约束
static bool method_matches_args(
    patch_handle_t method,
    const int args_count,
    const patch_handle_t* args_types,      // 可为 NULL
    const char** args_names               // 可为 NULL
) {
    TEKLOG_DEBUG("method_matches_args called: method=%p, args_count=%d", method, args_count);

    int actual_param_count = il2cpp_method_get_param_count(method);
    TEKLOG_DEBUG("Method actual parameter count: %d", actual_param_count);

    if (actual_param_count != args_count) {
        TEKLOG_DEBUG("Parameter count mismatch: expected %d, got %d", args_count, actual_param_count);
        return false;
    }

    for (int i = 0; i < args_count; ++i) {
        TEKLOG_DEBUG("Checking parameter %d", i);

        // 检查参数类型（如果提供）
        if (args_types) {
            const void* param_type_obj = il2cpp_method_get_param(method, i);
            if (!param_type_obj) {
                TEKLOG_ERROR("Failed to get parameter %d type", i);
                return false;
            }

            patch_handle_t param_class = il2cpp_class_from_type(param_type_obj);
            TEKLOG_DEBUG("Parameter %d type: expected=%p, actual=%p", i, args_types[i], param_class);

            if (args_types[i] != param_class) {
                TEKLOG_DEBUG("Parameter %d type mismatch", i);
                return false;
            }
        }

        // 检查参数名称（如果提供）
        if (args_names && args_names[i]) {
            const char* param_name = il2cpp_method_get_param_name(method, i);
            TEKLOG_DEBUG("Parameter %d name: expected='%s', actual='%s'",
                         i, args_names[i], param_name ? param_name : "NULL");

            if (!param_name || strcmp(param_name, args_names[i]) != 0) {
                TEKLOG_DEBUG("Parameter %d name mismatch", i);
                return false;
            }
        }
    }

    TEKLOG_DEBUG("Method matches all arguments");
    return true;
}

// 主查找函数：支持类型 + 名称的任意组合
patch_handle_t patchlib_type_find_method(
    patch_handle_t type,
    const char* name,
    const int args_count,
    const patch_handle_t* args_types,   // nullable
    const char** args_names             // nullable
) {
    TEKLOG_DEBUG("patchlib_type_find_method: type=%p, name='%s', args_count=%d",
                 type, name ? name : "NULL", args_count);

    if (!type || !name) {
        TEKLOG_ERROR("Type or name is NULL");
        return PATCH_NULL;
    }

    tefstd_vector_t methods = {};
    patchlib_type_get_methods(type, false, &methods);

    size_t method_count = tefstd_vector_size(&methods);
    TEKLOG_DEBUG("Searching in %zu methods", method_count);

    patch_handle_t result = PATCH_NULL;
    int matched_count = 0;

    for (int i = 0; i < method_count; ++i) {
        void* method = *(void**)tefstd_vector_at(&methods, i);
        const char* method_name = il2cpp_method_get_name(method);

        if (method_name && strcmp(method_name, name) == 0) {
            matched_count++;
            if (method_matches_args(method, args_count, args_types, args_names)) {
                result = method;
                TEKLOG_INFO("Found matching method: %p (matches: %d)", result, matched_count);
                break;
            }
        }
    }

    tefstd_vector_destroy(&methods);

    if (result == PATCH_NULL) {
        TEKLOG_WARN("No matching method found: %s with %d parameters (checked %d/%zu)",
                   name, args_count, matched_count, method_count);
    }

    return result;
}

typedef void * (*il2cpp_class_iterator_fn)(void *klass, void **iter);
static bool patchlib_collect_from_type_hierarchy(
    void *start_type,
    const bool including_parent,
    tefstd_vector_t *array,
    const il2cpp_class_iterator_fn iterator_fn) {
    TEKLOG_DEBUG("patchlib_collect_from_type_hierarchy: start_type=%p, including_parent=%s",
                 start_type, including_parent ? "true" : "false");

    if (!array || !iterator_fn) {
        TEKLOG_ERROR("Invalid array or iterator function");
        return false;
    }

    tefstd_vector_init(array, sizeof(patch_handle_t));

    void *current_type = start_type;
    int hierarchy_level = 0;
    size_t total_collected = 0;
    bool collection_success = true;

    do {
        void *iter = NULL;
        const void *item;
        int level_item_count = 0;

        while ((item = iterator_fn(current_type, &iter)) != NULL) {
            if (tefstd_vector_push_back(array, &item)) {
                level_item_count++;
                total_collected++;
            } else {
                TEKLOG_ERROR("Failed to add item to vector: %p", item);
                collection_success = false;
                break;
            }
        }

        if (!including_parent)
            break;

        hierarchy_level++;
        current_type = il2cpp_class_get_parent(current_type);
    } while (current_type != NULL);

    TEKLOG_DEBUG("Collection completed: total_collected=%zu, final_vector_size=%zu, success=%s",
                total_collected, tefstd_vector_size(array), collection_success ? "true" : "false");

    return collection_success;
}

bool patchlib_type_get_inner_types(patch_handle_t type, const bool including_parent, tefstd_vector_t *array) {
    TEKLOG_DEBUG("patchlib_type_get_inner_types called: type=%p, including_parent=%s",
                 type, including_parent ? "true" : "false");

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return false;
    }

    bool result = patchlib_collect_from_type_hierarchy(type, including_parent, array, il2cpp_class_get_nested_types);
    TEKLOG_DEBUG("Get inner types result: %s, count: %zu", result ? "success" : "failed", tefstd_vector_size(array));
    return result;
}

bool patchlib_type_get_methods(patch_handle_t type, const bool including_parent, tefstd_vector_t *array) {
    TEKLOG_DEBUG("patchlib_type_get_methods called: type=%p, including_parent=%s",
                 type, including_parent ? "true" : "false");

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return false;
    }

    bool result = patchlib_collect_from_type_hierarchy(type, including_parent, array, il2cpp_class_get_methods);
    TEKLOG_DEBUG("Get methods result: %s, count: %zu", result ? "success" : "failed", tefstd_vector_size(array));
    return result;
}

bool patchlib_type_get_fields(patch_handle_t type, const bool including_parent, tefstd_vector_t *array) {
    TEKLOG_DEBUG("patchlib_type_get_fields called: type=%p, including_parent=%s",
                 type, including_parent ? "true" : "false");

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return false;
    }

    bool result = patchlib_collect_from_type_hierarchy(type, including_parent, array, il2cpp_class_get_fields);
    TEKLOG_DEBUG("Get fields result: %s, count: %zu", result ? "success" : "failed", tefstd_vector_size(array));
    return result;
}

bool patchlib_type_get_properties(patch_handle_t type, const bool including_parent, tefstd_vector_t *array) {
    TEKLOG_DEBUG("patchlib_type_get_properties called: type=%p, including_parent=%s",
                 type, including_parent ? "true" : "false");

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return false;
    }

    bool result = patchlib_collect_from_type_hierarchy(type, including_parent, array, il2cpp_class_get_properties);
    TEKLOG_DEBUG("Get properties result: %s, count: %zu", result ? "success" : "failed", tefstd_vector_size(array));
    return result;
}