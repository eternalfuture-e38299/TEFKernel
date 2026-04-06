/*******************************************************************************
 * tefkernel - dictionary
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

#include "patchlib/struct/dictionary.h"
#include "internal/log.h"

#include "patchlib/method.h"
#include "../il2cpp_api.h"

static patch_handle_t dictionary_class = NULL;

static void init_dictionary_class() {
    if (!dictionary_class) {
        TEKLOG_DEBUG("Initializing dictionary class");
        dictionary_class = patchlib_type_get_type("System.Collections.Generic", "Dictionary`2");
        if (dictionary_class) {
            TEKLOG_DEBUG("Dictionary class initialized: %p", dictionary_class);
        } else {
            TEKLOG_ERROR("Failed to initialize dictionary class");
        }
    }
}

patch_handle_t patchlib_dictionary_create(patch_handle_t key_type, patch_handle_t value_type, size_t capacity) {
    TEKLOG_DEBUG("patchlib_dictionary_create called: key_type=%p, value_type=%p, capacity=%zu",
                 key_type, value_type, capacity);

    if (!patchlib_is_valid(key_type) || !patchlib_is_valid(value_type)) {
        TEKLOG_ERROR("Invalid key_type or value_type");
        return PATCH_NULL;
    }

    init_dictionary_class();

    // 创建泛型参数列表
    tefstd_vector_t v;
    tefstd_vector_init(&v, sizeof(patch_handle_t));
    tefstd_vector_push_back(&v, &key_type);
    tefstd_vector_push_back(&v, &value_type);

    TEKLOG_DEBUG("Creating generic dictionary with %zu type arguments", tefstd_vector_size(&v));

    patch_handle_t generic_type = patchlib_type_make_generic_type(dictionary_class, &v);
    if (!generic_type) {
        TEKLOG_ERROR("Failed to create generic dictionary type");
        tefstd_vector_destroy(&v);
        return PATCH_NULL;
    }
    TEKLOG_DEBUG("Generic dictionary type created: %p", generic_type);

    const char* ctor_args_name[1] = { "capacity" };
    patch_handle_t ctor = patchlib_type_get_method_by_param_names(generic_type, ".ctor", 1, ctor_args_name);
    if (!ctor) {
        TEKLOG_ERROR("Failed to find dictionary constructor");
        tefstd_vector_destroy(&v);
        return PATCH_NULL;
    }
    TEKLOG_DEBUG("Dictionary constructor found: %p", ctor);

    patch_handle_t instance = patchlib_type_new_instance(generic_type);
    if (!instance) {
        TEKLOG_ERROR("Failed to create dictionary instance");
        tefstd_vector_destroy(&v);
        return PATCH_NULL;
    }
    TEKLOG_DEBUG("Dictionary instance created: %p", instance);

    // 调用构造函数
    void* ctor_ptr = patchlib_method_get_pointer(ctor);
    if (ctor_ptr) {
        TEKLOG_DEBUG("Calling dictionary constructor with capacity: %d", (int)capacity);
        ((void(*)(void*, int))ctor_ptr)(instance, (int)capacity);
        TEKLOG_INFO("Dictionary created successfully: %p (capacity: %zu)", instance, capacity);
    } else {
        TEKLOG_ERROR("Failed to get constructor pointer");
    }

    tefstd_vector_destroy(&v);
    return instance;
}

bool patchlib_dictionary_add(patch_handle_t dictionary, void* key, void* value) {
    TEKLOG_DEBUG("patchlib_dictionary_add called: dictionary=%p, key=%p, value=%p",
                 dictionary, key, value);

    if (!patchlib_is_valid(dictionary) || !key || !value) {
        TEKLOG_ERROR("Invalid dictionary, key or value");
        return false;
    }

    init_dictionary_class();

    patch_handle_t method_add = patchlib_type_get_method_by_param_count(il2cpp_object_get_class(dictionary), "Add", 2);
    if (!method_add) {
        TEKLOG_ERROR("Failed to find Add method");
        return false;
    }
    TEKLOG_DEBUG("Add method found: %p", method_add);

    void* args[2] = {key, value};
    bool result = patchlib_method_invoke_args(method_add, dictionary, NULL, args);
    TEKLOG_DEBUG("Dictionary add operation %s", result ? "succeeded" : "failed");
    return result;
}

bool patchlib_dictionary_get_value(patch_handle_t dictionary, void* key, void* out_value) {
    TEKLOG_DEBUG("patchlib_dictionary_get_value called: dictionary=%p, key=%p, out_value=%p",
                 dictionary, key, out_value);

    if (!patchlib_is_valid(dictionary) || !key || !out_value) {
        TEKLOG_ERROR("Invalid dictionary, key or output value");
        return false;
    }

    init_dictionary_class();

    patch_handle_t method_get_item = patchlib_type_get_method_by_param_count(il2cpp_object_get_class(dictionary), "get_Item", 1);
    if (!method_get_item) {
        TEKLOG_ERROR("Failed to find get_Item method");
        return false;
    }
    TEKLOG_DEBUG("get_Item method found: %p", method_get_item);

    void* args[1] = {key};
    bool result = patchlib_method_invoke_args(method_get_item, dictionary, out_value, args);
    TEKLOG_DEBUG("Dictionary get_value operation %s", result ? "succeeded" : "failed");
    return result;
}

bool patchlib_dictionary_set_value(patch_handle_t dictionary, void* key, void* value) {
    TEKLOG_DEBUG("patchlib_dictionary_set_value called: dictionary=%p, key=%p, value=%p",
                 dictionary, key, value);

    if (!patchlib_is_valid(dictionary) || !key || !value) {
        TEKLOG_ERROR("Invalid dictionary, key or value");
        return false;
    }

    init_dictionary_class();

    patch_handle_t method_set_item = patchlib_type_get_method_by_param_count(il2cpp_object_get_class(dictionary), "set_Item", 2);
    if (!method_set_item) {
        TEKLOG_ERROR("Failed to find set_Item method");
        return false;
    }
    TEKLOG_DEBUG("set_Item method found: %p", method_set_item);

    void* args[2] = {key, value};
    bool result = patchlib_method_invoke_args(method_set_item, dictionary, NULL, args);
    TEKLOG_DEBUG("Dictionary set_value operation %s", result ? "succeeded" : "failed");
    return result;
}

bool patchlib_dictionary_clear(patch_handle_t dictionary) {
    TEKLOG_DEBUG("patchlib_dictionary_clear called: dictionary=%p", dictionary);

    if (!patchlib_is_valid(dictionary)) {
        TEKLOG_ERROR("Invalid dictionary");
        return false;
    }

    init_dictionary_class();

    patch_handle_t method_clear = patchlib_type_get_method_by_param_count(il2cpp_object_get_class(dictionary), "Clear", 0);
    if (!method_clear) {
        TEKLOG_ERROR("Failed to find Clear method");
        return false;
    }
    TEKLOG_DEBUG("Clear method found: %p", method_clear);

    void* clear_ptr = patchlib_method_get_pointer(method_clear);
    if (clear_ptr) {
        TEKLOG_DEBUG("Calling Clear method");
        ((void(*)(void*))clear_ptr)(dictionary);
        TEKLOG_DEBUG("Dictionary cleared successfully");
        return true;
    } else {
        TEKLOG_ERROR("Failed to get Clear method pointer");
        return false;
    }
}

size_t patchlib_dictionary_length(patch_handle_t dictionary) {
    TEKLOG_DEBUG("patchlib_dictionary_length called: dictionary=%p", dictionary);

    if (!patchlib_is_valid(dictionary)) {
        TEKLOG_ERROR("Invalid dictionary");
        return 0;
    }

    init_dictionary_class();

    patch_handle_t method_get_count = patchlib_type_get_method_by_param_count(il2cpp_object_get_class(dictionary), "get_Count", 0);
    if (!method_get_count) {
        TEKLOG_ERROR("Failed to find get_Count method");
        return 0;
    }
    TEKLOG_DEBUG("get_Count method found: %p", method_get_count);

    int count = 0;
    bool result = patchlib_method_invoke_args(method_get_count, dictionary, &count, NULL);
    if (result) {
        TEKLOG_DEBUG("Dictionary length: %d", count);
        return (size_t)count;
    } else {
        TEKLOG_ERROR("Failed to get dictionary length");
        return 0;
    }
}

bool patchlib_dictionary_remove(patch_handle_t dictionary, void* key) {
    TEKLOG_DEBUG("patchlib_dictionary_remove called: dictionary=%p, key=%p", dictionary, key);

    if (!patchlib_is_valid(dictionary) || !key) {
        TEKLOG_ERROR("Invalid dictionary or key");
        return false;
    }

    init_dictionary_class();

    patch_handle_t method_remove = patchlib_type_get_method_by_param_count(il2cpp_object_get_class(dictionary), "Remove", 1);
    if (!method_remove) {
        TEKLOG_ERROR("Failed to find Remove method");
        return false;
    }
    TEKLOG_DEBUG("Remove method found: %p", method_remove);

    void* args[1] = {key};
    bool result = patchlib_method_invoke_args(method_remove, dictionary, NULL, args);
    TEKLOG_DEBUG("Dictionary remove operation %s", result ? "succeeded" : "failed");
    return result;
}