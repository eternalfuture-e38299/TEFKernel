/*******************************************************************************
 * tefkernel - type
 * Copyright (C) 2026 eternalfuture-e38299
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
 * Created: 2026/1/3
 *******************************************************************************/

#include "patchlib/type.h"

#include <stdlib.h>
#include <string.h>


#include "internal/log.h"
#include "patchlib/method.h"
#include "../il2cpp_api.h"

static bool method_matches_args(
    patch_handle_t method,
    int args_count,
    const patch_handle_t *args_types, // 可为 NULL
    const char **args_names // 可为 NULL
);

static patch_handle_t patchlib_type_find_method(
    patch_handle_t type,
    const char *name,
    int args_count,
    const patch_handle_t *args_types, // nullable
    const char **args_names // nullable
);

// 内部 helper：判断一个 method 是否匹配给定的参数约束
bool method_matches_args(
    patch_handle_t method,
    const int args_count,
    const patch_handle_t *args_types, // 可为 NULL
    const char **args_names // 可为 NULL
) {
    TEKLOG_DEBUG("method_matches_args called: method=%p, args_count=%d", method, args_count);

    const uint32_t actual_param_count = il2cpp_method_get_param_count(method);
    TEKLOG_DEBUG("Method actual parameter count: %d", actual_param_count);

    if (actual_param_count != args_count) {
        TEKLOG_DEBUG("Parameter count mismatch: expected %d, got %d", args_count, actual_param_count);
        return false;
    }

    for (int i = 0; i < args_count; ++i) {
        TEKLOG_DEBUG("Checking parameter %d", i);

        // 检查参数类型（如果提供）
        if (args_types) {
            void *param_type_obj = il2cpp_method_get_param(method, i);
            if (!param_type_obj) {
                TEKLOG_ERROR("Failed to get parameter %d type", i);
                return false;
            }

            patch_handle_t param_class = il2cpp_class_from_type(param_type_obj);
            TEKLOG_DEBUG("Parameter %d type: expected=%p, actual=%p", i, args_types[i], param_class);

            // 使用 patchlib_type_is_same 进行安全比较
            if (!patchlib_type_is_same(args_types[i], param_class)) {
                TEKLOG_DEBUG("Parameter %d type mismatch", i);
                return false;
            }
        }

        // 检查参数名称（如果提供）
        if (args_names && args_names[i]) {
            const char *param_name = il2cpp_method_get_param_name(method, i);
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
    const char *name,
    const int args_count,
    const patch_handle_t *args_types, // nullable
    const char **args_names // nullable
) {
    TEKLOG_DEBUG("patchlib_type_find_method: type=%p, name='%s', args_count=%d",
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
    int matched_count = 0;

    for (int i = 0; i < method_count; ++i) {
        void *method = *(void **) tefstd_vector_at(&methods, i);
        const char *method_name = il2cpp_method_get_name(method);

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

bool patchlib_is_valid(patch_handle_t h) {
    if (h == PATCH_NULL) {
        TEKLOG_WARN("patch_handle is null");
        return false;
    }
    return true;
}

patch_handle_t patchlib_type_get_type(const char *ns, const char *name) {
    TEKLOG_DEBUG("patchlib_type_get_type called: namespace='%s', name='%s'", ns ? ns : "NULL", name ? name : "NULL");

    if (!ns || !name) {
        TEKLOG_ERROR("Namespace or name is NULL");
        return PATCH_NULL;
    }

    void *domain = il2cpp_domain_get(); // 获取il2cpp domain
    if (!domain) {
        TEKLOG_ERROR("Failed to get il2cpp domain");
        return PATCH_NULL;
    }

    size_t assemblies_count = 0;
    void **assemblies = il2cpp_domain_get_assemblies(domain, &assemblies_count); // 获取所有程序集
    if (!assemblies || assemblies_count == 0) {
        TEKLOG_ERROR("No assemblies found in domain");
        il2cpp_free(domain);
        return PATCH_NULL;
    }
    il2cpp_free(domain);

    TEKLOG_DEBUG("Searching in %zu assemblies", assemblies_count);

    for (size_t i = 0; i < assemblies_count; ++i) {
        void *assembly = assemblies[i];
        void *image = il2cpp_assembly_get_image(assembly); //获取image

        if (!image) {
            TEKLOG_DEBUG("Assembly %zu has no image", i);
            continue;
        }

        TEKLOG_DEBUG("Searching in assembly %zu, image: %p", i, image);
        void *found_class = il2cpp_class_from_name(image, ns, name); // 从名称获取类
        il2cpp_free(image);
        if (found_class) {
#if !defined(__ANDROID__)
            free(assemblies);
#endif
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
    const char *type_name = patchlib_type_get_name(type);
    const char *type_namespace = patchlib_type_get_namespace(type);

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

size_t get_size_from_patch_type(const patch_type_t type) {
    TEKLOG_DEBUG("get_size_from_patch_type called: type=%d", type);

#if defined(__ANDROID__)
#define OBJECT_SIZE sizeof(void*)
#else
#define OBJECT_SIZE sizeof(int32_t)
#endif

    size_t result = 0;
    switch (type) {
        case PATCH_UINT8: result = sizeof(uint8_t);
            break;
        case PATCH_UINT16: result = sizeof(uint16_t);
            break;
        case PATCH_UINT32: result = sizeof(uint32_t);
            break;
        case PATCH_UINT64: result = sizeof(uint64_t);
            break;
        case PATCH_INT8: result = sizeof(int8_t);
            break;
        case PATCH_INT16: result = sizeof(int16_t);
            break;
        case PATCH_INT32: result = sizeof(int32_t);
            break;
        case PATCH_INT64: result = sizeof(int64_t);
            break;
        case PATCH_FLOAT: result = sizeof(float);
            break;
        case PATCH_DOUBLE: result = sizeof(double);
            break;
        case PATCH_BOOL: result = sizeof(bool);
            break;
        case PATCH_POINTER:
        case PATCH_OBJECT: result = OBJECT_SIZE;
            break; // 对象通常是引用，大小为指针
        case PATCH_CHAR: result = sizeof(char);
            break;
        default: result = 0;
            break; // 未知类型
    }

#undef OBJECT_SIZE

    TEKLOG_DEBUG("Type %d size: %zu bytes", type, result);
    return result;
}

patch_handle_t patchlib_get_basic_type(const patch_type_t basic_type) {
    TEKLOG_DEBUG("patchlib_get_basic_type called: basic_type=%d", basic_type);

    const char *type_name;
    patch_handle_t result = PATCH_NULL;

    switch (basic_type) {
        case PATCH_VOID: type_name = "Void";
            result = patchlib_type_get_type("System", "Void");
            break;
        case PATCH_INT8: type_name = "SByte";
            result = patchlib_type_get_type("System", "SByte");
            break;
        case PATCH_INT16: type_name = "Int16";
            result = patchlib_type_get_type("System", "Int16");
            break;
        case PATCH_INT32: type_name = "Int32";
            result = patchlib_type_get_type("System", "Int32");
            break;
        case PATCH_INT64: type_name = "Int64";
            result = patchlib_type_get_type("System", "Int64");
            break;
        case PATCH_UINT8: type_name = "Byte";
            result = patchlib_type_get_type("System", "Byte");
            break;
        case PATCH_UINT16: type_name = "UInt16";
            result = patchlib_type_get_type("System", "UInt16");
            break;
        case PATCH_UINT32: type_name = "UInt32";
            result = patchlib_type_get_type("System", "UInt32");
            break;
        case PATCH_UINT64: type_name = "UInt64";
            result = patchlib_type_get_type("System", "UInt64");
            break;
        case PATCH_BOOL: type_name = "Boolean";
            result = patchlib_type_get_type("System", "Boolean");
            break;
        case PATCH_FLOAT: type_name = "Single";
            result = patchlib_type_get_type("System", "Single");
            break;
        case PATCH_DOUBLE: type_name = "Double";
            result = patchlib_type_get_type("System", "Double");
            break;
        case PATCH_POINTER: type_name = "IntPtr";
            result = patchlib_type_get_type("System", "IntPtr");
            break;
        case PATCH_OBJECT: type_name = "Object";
            result = patchlib_type_get_type("System", "Object");
            break;
        case PATCH_CHAR: type_name = "Char";
            result = patchlib_type_get_type("System", "Char");
            break;
        default: type_name = "Unknown";
            result = PATCH_NULL;
            break;
    }


    TEKLOG_DEBUG("Basic type %d (%s) result: " PATCH_HANDLE_FMT,
                 basic_type, type_name, result);

    return result;
}

char *patchlib_type_get_full_name(patch_handle_t type) {
    TEKLOG_DEBUG("patchlib_type_get_full_name called: type=" PATCH_HANDLE_FMT, type);

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return NULL;
    }

    const char *namespace_name = patchlib_type_get_namespace(type);
    const char *class_name = patchlib_type_get_name(type);

    TEKLOG_DEBUG("Raw names - namespace: '%s', class: '%s'",
                 namespace_name ? namespace_name : "NULL",
                 class_name ? class_name : "NULL");

    if (namespace_name == NULL) {
        namespace_name = "";
    }
    if (class_name == NULL) {
        class_name = "";
    }

    const size_t namespace_len = strlen(namespace_name);
    const size_t class_name_len = strlen(class_name);
    size_t total_len = class_name_len;

    if (namespace_len > 0) {
        total_len += namespace_len + 1;
    }

    total_len += 1;

    TEKLOG_DEBUG("Calculated total length: %zu", total_len);

    char *full_name = malloc(total_len);
    if (full_name == NULL) {
        TEKLOG_ERROR("Memory allocation failed for full name");
        return NULL;
    }

    full_name[0] = '\0';
    if (namespace_len > 0) {
        strcat(full_name, namespace_name);
        strcat(full_name, ".");
    }

    strcat(full_name, class_name);

    TEKLOG_DEBUG("Full name: '%s'", full_name);
    return full_name;
}

patch_handle_t patchlib_type_get_inner_type(patch_handle_t parent, const char *name) {
    TEKLOG_DEBUG("patchlib_type_get_inner_type called: parent=" PATCH_HANDLE_FMT " name='%s'", parent,
                 name ? name : "NULL");

    if (!patchlib_is_valid(parent) || !name) {
        TEKLOG_ERROR("Invalid parent or name");
        return PATCH_NULL;
    }

    patch_handle_t result = PATCH_NULL;

    tefstd_vector_t inner_types = {};
    patchlib_type_get_inner_types(parent, false, &inner_types);

    const size_t inner_count = tefstd_vector_size(&inner_types);
    TEKLOG_DEBUG("Found %zu inner types", inner_count);

    if (inner_count > 0) {
        for (int i = 0; i < inner_count; ++i) {
            patch_handle_t *type = tefstd_vector_at(&inner_types, i);
            const char *nested_name = patchlib_type_get_name(*type);

            TEKLOG_DEBUG("Inner type %d: %p, name: '%s'", i, type, nested_name ? nested_name : "NULL");

            if (nested_name && strcmp(nested_name, name) == 0) {
                result = *type;
                TEKLOG_INFO("Found inner type: %s at " PATCH_HANDLE_FMT, name, result);
                break;
            }
        }
    } else {
        TEKLOG_DEBUG("No inner types found");
    }

    tefstd_vector_destroy(&inner_types);

    if (result == PATCH_NULL) {
        TEKLOG_WARN("Inner type not found: %s", name);
    }

    return result;
}

const char *patchlib_type_get_name(patch_handle_t type) {
    TEKLOG_DEBUG("patchlib_type_get_name called: type=%p", type);

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return NULL;
    }

    const char *result = il2cpp_class_get_name(type);
    TEKLOG_DEBUG("Type name: %s", result ? result : "NULL");
    return result;
}

const char *patchlib_type_get_namespace(patch_handle_t type) {
    TEKLOG_DEBUG("patchlib_type_get_namespace called: type=%p", type);

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return NULL;
    }

    const char *result = il2cpp_class_get_namespace(type);
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

patch_handle_t patchlib_type_get_method(patch_handle_t type, const char *name) {
    TEKLOG_DEBUG("patchlib_type_get_method called: type=" PATCH_HANDLE_FMT ", name='%s'", type, name ? name : "NULL");

    if (!patchlib_is_valid(type)) {
        TEKLOG_ERROR("Invalid type handle");
        return PATCH_NULL;
    }

    if (!name) {
        TEKLOG_ERROR("Method name is NULL");
        return PATCH_NULL;
    }

    patch_handle_t result = PATCH_NULL;
    tefstd_vector_t methods = {};
    patchlib_type_get_methods(type, false, &methods);

    const size_t method_count = tefstd_vector_size(&methods);
    TEKLOG_DEBUG("Found %zu methods in type", method_count);

    if (method_count > 0) {
        for (int i = 0; i < method_count; ++i) {
            patch_handle_t *method = tefstd_vector_at(&methods, i);
            const char *method_name = patchlib_method_get_name(*method);
            TEKLOG_DEBUG("Method %d: %p, name: '%s'", i, method, method_name ? method_name : "NULL");

            if (method_name && strcmp(method_name, name) == 0) {
                result = *method;
                TEKLOG_INFO("Found method '%s' at " PATCH_HANDLE_FMT, name, result);
                break;
            }
            il2cpp_free(*method);
        }
    } else {
        TEKLOG_DEBUG("No methods found in type");
    }

    tefstd_vector_destroy(&methods);

    if (result == PATCH_NULL) {
        TEKLOG_WARN("Method not found: %s", name);
    }

    return result;
}

patch_handle_t patchlib_type_get_method_by_param_names(patch_handle_t type, const char *name,
                                                       const int args_count, const char **args_names) {
    TEKLOG_DEBUG("patchlib_type_get_method_by_param_names called: type=" PATCH_HANDLE_FMT ", name='%s', args_count=%d",
                 type, name ? name : "NULL", args_count);

    patch_handle_t result = patchlib_type_find_method(type, name, args_count, NULL, args_names);
    TEKLOG_DEBUG("Method by param names result: " PATCH_HANDLE_FMT, result);
    return result;
}

patch_handle_t patchlib_type_get_method_by_param_types(patch_handle_t type, const char *name,
                                                       const int args_count, const patch_handle_t *args_types) {
    TEKLOG_DEBUG("patchlib_type_get_method_by_param_types called: type=" PATCH_HANDLE_FMT ", name='%s', args_count=%d",
                 type, name ? name : "NULL", args_count);

    patch_handle_t result = patchlib_type_find_method(type, name, args_count, args_types, NULL);
    TEKLOG_DEBUG("Method by param types result: " PATCH_HANDLE_FMT, result);
    return result;
}

patch_handle_t patchlib_type_get_method_by_signature(patch_handle_t type, const char *name,
                                                     const int args_count, const patch_handle_t *args_types,
                                                     const char **args_names
) {
    TEKLOG_DEBUG("patchlib_type_get_method_by_signature called: type=" PATCH_HANDLE_FMT ", name='%s', args_count=%d",
                 type, name ? name : "NULL", args_count);

    patch_handle_t result = patchlib_type_find_method(type, name, args_count, args_types, args_names);
    TEKLOG_DEBUG("Method by signature result: " PATCH_HANDLE_FMT, result);
    return result;
}

#if defined(__ANDROID__)
typedef void * (*il2cpp_class_iterator_fn)(void *klass, void **iter);
#else
typedef void ** (*il2cpp_class_iterator_fn)(void *klass, int *iter);
#endif
#if defined(__ANDROID__)
// Android 模式：使用迭代器方式，逐个返回嵌套类型
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
    size_t total_collected = 0;
    bool collection_success = true;

    do {
        void *iter = NULL; // 必须初始化为 nullptr
        void *item;

        // 逐个获取嵌套类型，直到返回 NULL
        while ((item = iterator_fn(current_type, &iter)) != NULL) {
            if (tefstd_vector_push_back(array, &item)) {
                total_collected++;
            } else {
                TEKLOG_ERROR("Failed to add item to vector: %p", item);
                collection_success = false;
                break;
            }
        }

        if (!collection_success)
            break;

        if (!including_parent)
            break;

        // 获取父类继续遍历
        current_type = il2cpp_class_get_parent(current_type);
    } while (current_type != NULL);

    TEKLOG_DEBUG("Collection completed: total_collected=%zu, final_vector_size=%zu, success=%s",
                 total_collected, tefstd_vector_size(array), collection_success ? "true" : "false");

    return collection_success;
}

#else
static bool patchlib_collect_from_type_hierarchy(
    void *start_type,
    const bool including_parent,
    tefstd_vector_t *array,
    il2cpp_class_iterator_fn iterator_fn) {

    TEKLOG_DEBUG("patchlib_collect_from_type_hierarchy: start_type=%p", start_type);

    if (!array || !iterator_fn || !start_type) {
        TEKLOG_ERROR("Invalid parameters");
        return false;
    }

    tefstd_vector_init(array, sizeof(patch_handle_t));

    void *current_type = start_type;

    do {
        int size = 0;
        void **members = NULL;

        // 调用 Il2Cpp API 获取成员数组
        members = iterator_fn(current_type, &size);

        if (members != NULL && size > 0) {
            TEKLOG_DEBUG("Found %d members in type %p", size, current_type);

            // 添加所有成员
            for (int i = 0; i < size; i++) {
                if (members[i] != NULL) {
                    if (!tefstd_vector_push_back(array, &members[i])) {
                        TEKLOG_ERROR("Failed to add member %d", i);
                        free(members);
                        return false;
                    }
                }
            }

            // 重要：释放 Il2Cpp 返回的数组
            free(members);
            members = NULL;
        }

        if (!including_parent) {
            break;
        }

        current_type = il2cpp_class_get_parent(current_type);
    } while (current_type != NULL);

    TEKLOG_DEBUG("Collection completed: %zu items", tefstd_vector_size(array));
    return true;
}
#endif


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
