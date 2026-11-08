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
#include "patchlib/field.h"
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
 * List<T> 类句柄缓存（懒加载 + 析构释放）
 * ============================================================================ */

struct list_class_cache {
    patch_handle_t handle = nullptr;

    ~list_class_cache() {
        if (handle) patchlib_free(handle);
    }

    patch_handle_t get() {
        if (handle) return handle;

        handle = patchlib_type_get_type("System.Collections.Generic", "List`1");
        if (!handle) {
            error("List`1 class not found");
            return nullptr;
        }
        debug("List class initialized: {}", fmt::ptr(handle));
        return handle;
    }
};

list_class_cache& list_class() {
    static list_class_cache c;
    return c;
}

/* ============================================================================
 * RAII 句柄守卫
 * ============================================================================ */

class handle_guard {
public:
    handle_guard() = default;
    explicit handle_guard(patch_handle_t h) : h_(h) {}
    ~handle_guard() { if (h_) patchlib_free(h_); }

    handle_guard(const handle_guard&) = delete;
    handle_guard& operator=(const handle_guard&) = delete;

    handle_guard(handle_guard&& o) noexcept : h_(o.h_) { o.h_ = nullptr; }
    handle_guard& operator=(handle_guard&& o) noexcept {
        if (this != &o) { if (h_) patchlib_free(h_); h_ = o.h_; o.h_ = nullptr; }
        return *this;
    }

private:
    patch_handle_t h_ = nullptr;
};

/* ============================================================================
 * 内部辅助：在具体 List<T> 类型上找方法并调用
 * ============================================================================ */

bool invoke_list_method(patch_handle_t list, const char* name, int argc,
                      void* ret, void** args) {
    patch_handle_t cls = il2cpp_object_get_class(list);
    if (!cls) {
        error("Failed to get list class");
        return false;
    }
    handle_guard cls_guard(cls);

    patch_handle_t method = patchlib_type_get_method_by_param_count(cls, name, argc);
    if (!method) {
        error("Failed to find method '{}' (argc={})", name, argc);
        return false;
    }
    handle_guard method_guard(method);

    return patchlib_method_invoke_args(method, list, ret, args);
}

/* 内部辅助：读 / 写 _items 字段 */
patch_handle_t get_items_field(patch_handle_t list) {
    patch_handle_t cls = il2cpp_object_get_class(list);
    if (!cls) return nullptr;
    handle_guard cls_guard(cls);

    patch_handle_t field = patchlib_type_get_field(cls, "_items");
    if (!field) {
        error("Failed to find _items field");
        return nullptr;
    }
    return field;
}

} // namespace

extern "C" {

/* ============================================================================
 * 创建
 * ============================================================================ */

patch_handle_t patchlib_list_create(size_t capacity, patch_handle_t type) {
    debug("patchlib_list_create: cap={} type={}", capacity, fmt::ptr(type));

    if (!patchlib_is_valid(type)) {
        error("Invalid type handle");
        return nullptr;
    }

    patch_handle_t list_cls = list_class().get();
    if (!list_cls) return nullptr;

    tefstd_vector_t types{};
    tefstd_vector_init(&types, sizeof(patch_handle_t));
    tefstd_vector_push_back(&types, &type);

    patch_handle_t generic_type = patchlib_type_make_generic_type(list_cls, &types);
    tefstd_vector_destroy(&types);

    if (!generic_type) {
        error("Failed to create generic List<T>");
        return nullptr;
    }
    handle_guard generic_guard(generic_type);
    debug("Generic list type: {}", fmt::ptr(generic_type));

    const char* ctor_param_names[1] = { "capacity" };
    patch_handle_t ctor = patchlib_type_get_method_by_param_names(
        generic_type, ".ctor", 1, ctor_param_names);
    if (!ctor) {
        error("Failed to find List<T> .ctor(capacity)");
        return nullptr;
    }
    handle_guard ctor_guard(ctor);

    int capacity_arg = static_cast<int>(capacity);
    void* args[] = { &capacity_arg };

    patch_handle_t instance = nullptr;
    if (!patchlib_constructor_invoke(ctor, &instance, args)) {
        error("Failed to create List instance");
        return nullptr;
    }

    debug("List instance: {}", fmt::ptr(instance));
    return instance;
}

/* ============================================================================
 * 复制（直接替换 _items 字段）
 *
 * 语义：用传入的 array 替换 List 内部的 _items 数组。
 * 调用方需保证 array 的元素类型和 List 元素类型一致。
 * ============================================================================ */

bool patchlib_list_copy_from(patch_handle_t list, patch_handle_t array) {
    if (!patchlib_is_valid(list) || !patchlib_is_valid(array)) {
        error("Invalid list or array handle");
        return false;
    }

    patch_handle_t field_items = get_items_field(list);
    if (!field_items) return false;
    handle_guard field_guard(field_items);

    patchlib_field_set_value(field_items, list, &array);
    return true;
}

/* ============================================================================
 * Add / Remove / RemoveAt / Clear
 * ============================================================================ */

bool patchlib_list_add(patch_handle_t list, void* value) {
    if (!patchlib_is_valid(list) || !value) {
        error("Invalid list or value");
        return false;
    }
    void* args[] = { value };
    return invoke_list_method(list, "Add", 1, nullptr, args);
}

bool patchlib_list_remove(patch_handle_t list, void* value) {
    if (!patchlib_is_valid(list) || !value) {
        error("Invalid list or value");
        return false;
    }
    void* args[] = { value };
    return invoke_list_method(list, "Remove", 1, nullptr, args);
}

bool patchlib_list_remove_at(patch_handle_t list, size_t index) {
    if (!patchlib_is_valid(list)) {
        error("Invalid list handle");
        return false;
    }

    int idx = static_cast<int>(index);
    void* args[] = { &idx };
    return invoke_list_method(list, "RemoveAt", 1, nullptr, args);
}

bool patchlib_list_clear(patch_handle_t list) {
    if (!patchlib_is_valid(list)) {
        error("Invalid list handle");
        return false;
    }
    return invoke_list_method(list, "Clear", 0, nullptr, nullptr);
}

/* ============================================================================
 * 获取内部 _items 数组
 * ============================================================================ */

patch_handle_t patchlib_list_get_array(patch_handle_t list) {
    if (!patchlib_is_valid(list)) {
        error("Invalid list handle");
        return nullptr;
    }

    patch_handle_t field_items = get_items_field(list);
    if (!field_items) return nullptr;
    handle_guard field_guard(field_items);

    patch_handle_t array = nullptr;
    patchlib_field_get_value(field_items, list, &array);
    return array;
}

}