/*******************************************************************************
 * tefkernel - list
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
 * Created: 2025/12/28
 *******************************************************************************/

#include "patchlib/struct/list.h"
#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "../../il2cpp_api.h"

static patch_handle_t list_class = NULL;

static void init_list_class() {
    if (!list_class) {
        TEKLOG_DEBUG("Initializing list class");
        list_class = patchlib_type_get_type("System.Collections.Generic", "List`1");
        if (list_class) {
            TEKLOG_DEBUG("List class initialized: %p", list_class);
        } else {
            TEKLOG_ERROR("Failed to initialize list class");
        }
    }
}

patch_handle_t patchlib_list_create(size_t capacity, patch_handle_t type) {
    TEKLOG_DEBUG("patchlib_list_create called: capacity=%zu, type=%p", capacity, type);

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return PATCH_NULL;
    }

    init_list_class();

    // 创建泛型参数列表
    tefstd_vector_t v;
    tefstd_vector_init(&v, sizeof(patch_handle_t));
    tefstd_vector_push_back(&v, &type);

    TEKLOG_DEBUG("Creating generic list with type: %p", type);

    patch_handle_t generic_type = patchlib_type_make_generic_type(list_class, &v);
    if (!generic_type) {
        TEKLOG_ERROR("Failed to create generic list type");
        tefstd_vector_destroy(&v);
        return PATCH_NULL;
    }
    TEKLOG_DEBUG("Generic list type created: %p", generic_type);

    const char* ctor_args_name[1] = { "capacity" };
    patch_handle_t ctor = patchlib_type_get_method_by_param_names(generic_type, ".ctor", 1, ctor_args_name);
    if (!ctor) {
        TEKLOG_ERROR("Failed to find list constructor");
        tefstd_vector_destroy(&v);
        return PATCH_NULL;
    }
    TEKLOG_DEBUG("List constructor found: %p", ctor);

    patch_handle_t instance = patchlib_type_new_instance(generic_type);
    if (!instance) {
        TEKLOG_ERROR("Failed to create list instance");
        tefstd_vector_destroy(&v);
        return PATCH_NULL;
    }
    TEKLOG_DEBUG("List instance created: %p", instance);

    // 使用 patchlib_method_invoke_args 调用构造函数
    int capacity_arg = (int)capacity;
    void* args[1] = { &capacity_arg };

    bool result = patchlib_method_invoke_args(ctor, instance, NULL, args);
    if (result) {
        TEKLOG_INFO("List created successfully: %p (capacity: %zu)", instance, capacity);
    } else {
        TEKLOG_ERROR("Failed to call list constructor");
        il2cpp_free(instance);
        instance = PATCH_NULL;
    }

    tefstd_vector_destroy(&v);
    return instance;
}

bool patchlib_list_copy_from(patch_handle_t list, patch_handle_t array) {
    TEKLOG_DEBUG("patchlib_list_copy_from called: list=%p, array=%p", list, array);

    if (!patchlib_is_valid(list) || !patchlib_is_valid(array)) {
        TEKLOG_ERROR("Invalid list or array handle");
        return false;
    }

    init_list_class();

    patch_handle_t field_items = patchlib_type_get_field(il2cpp_object_get_class(list), "_items");
    if (!field_items) {
        TEKLOG_ERROR("Failed to find _items field");
        return false;
    }
    TEKLOG_DEBUG("Found _items field: %p", field_items);

    patchlib_field_set_value(field_items, list, &array);
    return true;
}

bool patchlib_list_add(patch_handle_t list, void* value) {
    TEKLOG_DEBUG("patchlib_list_add called: list=%p, value=%p", list, value);

    if (!patchlib_is_valid(list) || !value) {
        TEKLOG_ERROR("Invalid list or value");
        return false;
    }

    init_list_class();

    patch_handle_t method_add = patchlib_type_get_method_by_param_count(il2cpp_object_get_class(list), "Add", 1);
    if (!method_add) {
        TEKLOG_ERROR("Failed to find Add method");
        return false;
    }
    TEKLOG_DEBUG("Add method found: %p", method_add);

    void* args[1] = { value };
    bool result = patchlib_method_invoke_args(method_add, list, NULL, args);
    TEKLOG_DEBUG("List add operation %s", result ? "succeeded" : "failed");
    return result;
}

bool patchlib_list_remove(patch_handle_t list, void* value) {
    TEKLOG_DEBUG("patchlib_list_remove called: list=%p, value=%p", list, value);

    if (!patchlib_is_valid(list) || !value) {
        TEKLOG_ERROR("Invalid list or value");
        return false;
    }

    init_list_class();

    patch_handle_t method_remove = patchlib_type_get_method_by_param_count(il2cpp_object_get_class(list), "Remove", 1);
    if (!method_remove) {
        TEKLOG_ERROR("Failed to find Remove method");
        return false;
    }
    TEKLOG_DEBUG("Remove method found: %p", method_remove);

    void* args[1] = { value };
    bool result = patchlib_method_invoke_args(method_remove, list, NULL, args);
    TEKLOG_DEBUG("List remove operation %s", result ? "succeeded" : "failed");
    return result;
}

bool patchlib_list_remove_at(patch_handle_t list, const size_t index) {
    TEKLOG_DEBUG("patchlib_list_remove_at called: list=%p, index=%zu", list, index);

    if (!patchlib_is_valid(list)) {
        TEKLOG_ERROR("Invalid list handle");
        return false;
    }

    init_list_class();

    patch_handle_t method_remove_at = patchlib_type_get_method_by_param_count(il2cpp_object_get_class(list), "RemoveAt", 1);
    if (!method_remove_at) {
        TEKLOG_ERROR("Failed to find RemoveAt method");
        return false;
    }
    TEKLOG_DEBUG("RemoveAt method found: %p", method_remove_at);

    int index_arg = (int)index;
    void* args[1] = { &index_arg };

    bool result = patchlib_method_invoke_args(method_remove_at, list, NULL, args);
    TEKLOG_DEBUG("RemoveAt operation %s", result ? "succeeded" : "failed");
    return result;
}

bool patchlib_list_clear(patch_handle_t list) {
    TEKLOG_DEBUG("patchlib_list_clear called: list=%p", list);

    if (!patchlib_is_valid(list)) {
        TEKLOG_ERROR("Invalid list handle");
        return false;
    }

    init_list_class();

    patch_handle_t method_clear = patchlib_type_get_method_by_param_count(il2cpp_object_get_class(list), "Clear", 0);
    if (!method_clear) {
        TEKLOG_ERROR("Failed to find Clear method");
        return false;
    }
    TEKLOG_DEBUG("Clear method found: %p", method_clear);

    bool result = patchlib_method_invoke_args(method_clear, list, NULL, NULL);
    TEKLOG_DEBUG("List clear operation %s", result ? "succeeded" : "failed");
    return result;
}

patch_handle_t patchlib_list_get_array(patch_handle_t list) {
    TEKLOG_DEBUG("patchlib_list_get_array called: list=%p", list);

    if (!patchlib_is_valid(list)) {
        TEKLOG_ERROR("Invalid list handle");
        return PATCH_NULL;
    }

    patch_handle_t field_items = patchlib_type_get_field(il2cpp_object_get_class(list), "_items");
    if (!field_items) {
        TEKLOG_ERROR("Failed to find _items field");
        return PATCH_NULL;
    }
    TEKLOG_DEBUG("Found _items field: %p", field_items);

    patch_handle_t array = PATCH_NULL;
    patchlib_field_get_value(field_items, list, &array);
    return array;
}

size_t patchlib_list_get_count(patch_handle_t list) {
    TEKLOG_DEBUG("patchlib_list_get_count called: list=%p", list);

    if (!patchlib_is_valid(list)) {
        TEKLOG_ERROR("Invalid list handle");
        return 0;
    }

    patch_handle_t field_count = patchlib_type_get_field(il2cpp_object_get_class(list), "_size");
    if (!field_count) {
        TEKLOG_ERROR("Failed to find _size field");
        return 0;
    }
    TEKLOG_DEBUG("Found _size field: %p", field_count);

    size_t count = 0;
    patchlib_field_get_value(field_count, list, &count);
    TEKLOG_DEBUG("List count: %zu", count);
    return count;
}