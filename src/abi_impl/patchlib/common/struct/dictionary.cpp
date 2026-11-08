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

#include <utility>

#include "patchlib/method.h"
#include "patchlib/type.h"

#include "tef/core/log.hpp"

extern "C" {
#include "../../il2cpp_api.h"
}

using tef::log::debug;
using tef::log::error;

namespace {
    /* ============================================================================
     * Dictionary 类句柄缓存（懒加载）
     * ============================================================================ */

    struct dictionary_class_cache {
        patch_handle_t handle = nullptr;

        ~dictionary_class_cache() {
            if (handle)
                patchlib_free(handle);
        }

        patch_handle_t get() {
            if (handle) return handle;

            handle = patchlib_type_get_type("System.Collections.Generic",
                                            "Dictionary`2");
            if (!handle) {
                error("Dictionary`2 class not found");
                return nullptr;
            }
            debug("Dictionary class initialized: {}", fmt::ptr(handle));
            return handle;
        }
    };

    dictionary_class_cache &dictionary_class() {
        static dictionary_class_cache c;
        return c;
    }

    /* ============================================================================
     * 类型句柄 RAII
     * ============================================================================ */

    class handle_guard {
    public:
        handle_guard() = default;

        explicit handle_guard(patch_handle_t h) : h_(h) {
        }

        ~handle_guard() {
            if (h_)
                patchlib_free(h_);
        }

        handle_guard(const handle_guard &) = delete;

        handle_guard &operator=(const handle_guard &) = delete;

        handle_guard(handle_guard &&o) noexcept : h_(o.h_) { o.h_ = nullptr; }

        handle_guard &operator=(handle_guard &&o) noexcept {
            if (this != &o) {
                if (h_)
                    patchlib_free(h_);
                h_ = o.h_;
                o.h_ = nullptr;
            }
            return *this;
        }

    private:
        patch_handle_t h_ = nullptr;
    };
} // namespace

extern "C" {
/* ============================================================================
 * 创建
 * ============================================================================ */

patch_handle_t patchlib_dictionary_create(patch_handle_t key_type,
                                          patch_handle_t value_type,
                                          size_t capacity) {
    debug("patchlib_dictionary_create: key={} value={} cap={}",
          fmt::ptr(key_type), fmt::ptr(value_type), capacity);

    if (!patchlib_is_valid(key_type) || !patchlib_is_valid(value_type)) {
        error("Invalid key_type or value_type");
        return nullptr;
    }

    patch_handle_t dict_cls = dictionary_class().get();
    if (!dict_cls) return nullptr;

    /* 构造泛型参数 vector */
    tefstd_vector_t types{};
    tefstd_vector_init(&types, sizeof(patch_handle_t));
    tefstd_vector_push_back(&types, &key_type);
    tefstd_vector_push_back(&types, &value_type);

    patch_handle_t generic_type = patchlib_type_make_generic_type(dict_cls, &types);
    tefstd_vector_destroy(&types);

    if (!generic_type) {
        error("Failed to create generic Dictionary<K,V>");
        return nullptr;
    }
    handle_guard generic_guard(generic_type);
    debug("Generic dictionary type: {}", fmt::ptr(generic_type));

    /* 找 .ctor(int capacity) */
    const char *ctor_param_names[1] = {"capacity"};
    patch_handle_t ctor = patchlib_type_get_method_by_param_names(
        generic_type, ".ctor", 1, ctor_param_names);
    if (!ctor) {
        error("Failed to find Dictionary .ctor(capacity)");
        return nullptr;
    }
    handle_guard ctor_guard(ctor);

    /* 调 ctor */
    patch_handle_t instance = nullptr;
    void *args[] = {&capacity};
    patchlib_constructor_invoke(ctor, &instance, args);
    if (!instance) {
        error("Failed to create Dictionary instance");
        return nullptr;
    }
    debug("Dictionary instance: {}", fmt::ptr(instance));
    return instance;
}

/* ============================================================================
 * 内部：按方法名调用（Add / get_Item / set_Item / Clear / get_Count / Remove）
 *
 * 每个公开 API 都遵循同一模式：
 *   1. il2cpp_object_get_class(dictionary) → 拿具体闭合泛型类
 *   2. patchlib_type_get_method_by_param_count(cls, name, argc)
 *   3. patchlib_method_invoke_args(method, dict, ret, args)
 *   4. 释放 method / cls
 * ============================================================================ */

namespace {
    /* 通用：在 dictionary 具体类型上找方法并调用 */
    bool invoke_dict_method(patch_handle_t dictionary, const char *name, int argc,
                            void *ret, void **args) {
        patch_handle_t cls = il2cpp_object_get_class(dictionary);
        if (!cls) {
            error("Failed to get dictionary class");
            return false;
        }
        handle_guard cls_guard(cls);

        patch_handle_t method = patchlib_type_get_method_by_param_count(cls, name, argc);
        if (!method) {
            error("Failed to find method '{}' (argc={})", name, argc);
            return false;
        }
        handle_guard method_guard(method);

        return patchlib_method_invoke_args(method, dictionary, ret, args);
    }
} // namespace

/* ============================================================================
 * Add
 * ============================================================================ */

bool patchlib_dictionary_add(patch_handle_t dictionary, void *key, void *value) {
    debug("patchlib_dictionary_add: dict={} key={} value={}",
          fmt::ptr(dictionary), fmt::ptr(key), fmt::ptr(value));

    if (!patchlib_is_valid(dictionary) || !key || !value) {
        error("Invalid dictionary, key or value");
        return false;
    }

    void *args[] = {key, value};
    return invoke_dict_method(dictionary, "Add", 2, nullptr, args);
}

/* ============================================================================
 * get_Item / set_Item
 * ============================================================================ */

bool patchlib_dictionary_get_value(patch_handle_t dictionary, void *key,
                                   void *out_value) {
    if (!patchlib_is_valid(dictionary) || !key || !out_value) {
        error("Invalid dictionary, key or out_value");
        return false;
    }

    void *args[] = {key};
    return invoke_dict_method(dictionary, "get_Item", 1, out_value, args);
}

bool patchlib_dictionary_set_value(patch_handle_t dictionary, void *key,
                                   void *value) {
    if (!patchlib_is_valid(dictionary) || !key || !value) {
        error("Invalid dictionary, key or value");
        return false;
    }

    void *args[] = {key, value};
    return invoke_dict_method(dictionary, "set_Item", 2, nullptr, args);
}

/* ============================================================================
 * set_Item（显式提供字典类型句柄，避免 il2cpp_object_get_class 不可用时崩）
 * ============================================================================ */

bool patchlib_dictionary_set_value_typed(patch_handle_t dictionary,
                                         patch_handle_t dictionary_type,
                                         void *key, void *value) {
    if (!patchlib_is_valid(dictionary) ||
        !patchlib_is_valid(dictionary_type) ||
        !key || !value) {
        error("Invalid dictionary, type, key or value");
        return false;
    }

    patch_handle_t method = patchlib_type_get_method_by_param_count(
        dictionary_type, "set_Item", 2);
    if (!method) {
        error("Failed to find set_Item on dictionary type");
        return false;
    }
    handle_guard method_guard(method);

    void *args[] = {key, value};
    return patchlib_method_invoke_args(method, dictionary, nullptr, args);
}

/* ============================================================================
 * Clear / Length / Remove
 * ============================================================================ */

bool patchlib_dictionary_clear(patch_handle_t dictionary) {
    if (!patchlib_is_valid(dictionary)) {
        error("Invalid dictionary");
        return false;
    }
    return invoke_dict_method(dictionary, "Clear", 0, nullptr, nullptr);
}

size_t patchlib_dictionary_length(patch_handle_t dictionary) {
    if (!patchlib_is_valid(dictionary)) {
        error("Invalid dictionary");
        return 0;
    }

    int count = 0;
    if (invoke_dict_method(dictionary, "get_Count", 0, &count, nullptr)) {
        return static_cast<size_t>(count);
    }
    return 0;
}

bool patchlib_dictionary_remove(patch_handle_t dictionary, void *key) {
    if (!patchlib_is_valid(dictionary) || !key) {
        error("Invalid dictionary or key");
        return false;
    }

    void *args[] = {key};
    return invoke_dict_method(dictionary, "Remove", 1, nullptr, args);
}
}
