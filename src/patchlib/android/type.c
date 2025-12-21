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

bool patchlib_is_valid(patch_handle_t h) {
    if (h == PATCH_NULL) {
        TEKLOG_WARN("patch_handle is null");
        return false;
    }
    return true;
}

patch_handle_t patchlib_type_get_type(const char *ns, const char *name) {
    const void *domain = il2cpp_domain_get(); // 获取il2cpp domain
    if (!domain)
        return PATCH_NULL;

    size_t assemblies_count = 0;
    void **assemblies = il2cpp_domain_get_assemblies(domain, &assemblies_count); // 获取所有程序集
    if (!assemblies || assemblies_count == 0)
        return PATCH_NULL;

    for (size_t i = 0; i < assemblies_count; ++i) {
        const void *assembly = assemblies[i];
        const void *image = il2cpp_assembly_get_image(assembly); //获取image

        if (!image)
            continue;

        void *found_class = il2cpp_class_from_name(image, ns, name); // 从名称获取类
        if (found_class)
            return found_class;
    }

    return PATCH_NULL;
}

patch_handle_t patchlib_get_basic_type(const patch_type_t basic_type) {
    switch (basic_type) {
        case PATCH_VOID: return patchlib_type_get_type("System", "Void");
        case PATCH_INT8: return patchlib_type_get_type("System", "SByte");
        case PATCH_INT16: return patchlib_type_get_type("System", "Int16");
        case PATCH_INT32: return patchlib_type_get_type("System", "Int32");
        case PATCH_INT64: return patchlib_type_get_type("System", "Int64");
        case PATCH_UINT8: return patchlib_type_get_type("System", "Byte");
        case PATCH_UINT16: return patchlib_type_get_type("System", "UInt16");
        case PATCH_UINT32: return patchlib_type_get_type("System", "UInt32");
        case PATCH_UINT64: return patchlib_type_get_type("System", "UInt64");
        case PATCH_BOOL: return patchlib_type_get_type("System", "Boolean");
        case PATCH_FLOAT: return patchlib_type_get_type("System", "Single");
        case PATCH_DOUBLE: return patchlib_type_get_type("System", "Double");
        case PATCH_POINTER: return patchlib_type_get_type("System", "IntPtr");
        case PATCH_OBJECT: return patchlib_type_get_type("System", "Object");
        case PATCH_CHAR: return patchlib_type_get_type("System", "Char");
        default: return NULL;
    }
}

patch_handle_t patchlib_type_new_instance(patch_handle_t type) {
    if (!patchlib_is_valid(type))
        return PATCH_NULL;

    return il2cpp_object_new(type);
}

patch_handle_t patchlib_type_make_generic_type(patch_handle_t generic_type_def, const tef_vector_t *type_args) {
    patch_handle_t c_mono_type = patchlib_type_get_mono_type(generic_type_def);
    void *type_array = create_type_array_from_vector(type_args);

    void *generic_type = ((void*(*)(void *, void *)) patchlib_method_get_pointer(patchlib_MakeGenericType))(
        c_mono_type, type_array);

    return il2cpp_class_from_system_type(generic_type);
}

patch_handle_t patchlib_type_get_mono_type(patch_handle_t type) {
    return il2cpp_type_get_object((char *) type + sizeof(void *) * 4);
}

const char* patchlib_type_get_name(patch_handle_t type) {
    if (!patchlib_is_valid(type)) return false;
    return il2cpp_class_get_name(type);
}

const char* patchlib_type_get_namespace(patch_handle_t type) {
    if (!patchlib_is_valid(type)) return false;
    return il2cpp_class_get_namespace(type);
}

const char* patchlib_type_get_full_name(patch_handle_t type) {
    if (!patchlib_is_valid(type)) {
        return NULL; // 无效句柄，直接返回 NULL
    }


    const char *namespace_name = il2cpp_class_get_namespace(type);
    const char *class_name = il2cpp_class_get_name(type);

    if (namespace_name == NULL) {
        namespace_name = "";
    }
    if (class_name == NULL) {
        class_name = ""; // 理论上类名不应该为 NULL，但以防万一
    }

    size_t namespace_len = strlen(namespace_name);
    size_t class_name_len = strlen(class_name);
    size_t total_len = class_name_len; // 至少需要容纳类名

    if (namespace_len > 0) {
        total_len += namespace_len + 1; // +1 是为了 '.' 分隔符
    }

    total_len += 1;

    char* full_name = malloc(total_len);
    if (full_name == NULL) {
        // 内存分配失败
        return NULL;
    }

    full_name[0] = '\0';
    if (namespace_len > 0) {
        strcat(full_name, namespace_name); // 复制命名空间
        strcat(full_name, ".");           // 添加点号分隔符
    }

    strcat(full_name, class_name);

    return full_name;
}

patch_handle_t patchlib_type_get_parent(patch_handle_t type) {
    return il2cpp_class_get_parent(type);
}

patch_handle_t patchlib_type_get_inner_type(patch_handle_t parent, const char *name) {
    patch_handle_t result = PATCH_NULL;

    tef_vector_t inner_types = {};
    patchlib_type_get_inner_types(parent, false, &inner_types);

    if (tefstd_vector_size(&inner_types) > 0) {
        for (int i = 0; i < tefstd_vector_size(&inner_types); ++i) {
            void *type = *(void **) tefstd_vector_at(&inner_types, i);
            const char* nested_name = patchlib_type_get_name(type);

            if (name && strcmp(nested_name, name) == 0) {
                result = type;
                break;
            }
        }
    }

    tefstd_vector_destroy(&inner_types);

    return result;
}

patch_handle_t patchlib_type_get_field(patch_handle_t type, const char *name) {
    if (!patchlib_is_valid(type))
        return false;

    return il2cpp_class_get_field_from_name(type, name);
}

patch_handle_t patchlib_type_get_property(patch_handle_t type, const char *name) {
    if (!patchlib_is_valid(type))
        return false;

    return il2cpp_class_get_property_from_name(type, name);
}

patch_handle_t patchlib_type_get_method(patch_handle_t type, const char *name) {
    patch_handle_t result = PATCH_NULL;

    tef_vector_t methods = {};
    patchlib_type_get_methods(type, false, &methods);

    if (tefstd_vector_size(&methods) > 0) {
        for (int i = 0; i < tefstd_vector_size(&methods); ++i) {
            void *method = *(void **) tefstd_vector_at(&methods, i);
            const char* method_name = patchlib_method_get_name(method);
            if (name && strcmp(method_name, name) == 0) {
                result = type;
                break;
            }
        }
    }

    tefstd_vector_destroy(&methods);

    return result;
}

patch_handle_t patchlib_type_get_method_by_param_count(patch_handle_t type, const char *name,
                                                       const int args_count) {
    if (!patchlib_is_valid(type))
        return false;

    return il2cpp_class_get_method_from_name(type, name, args_count);
}

// 内部 helper：判断一个 method 是否匹配给定的参数约束
static bool method_matches_args(
    patch_handle_t method,
    const int args_count,
    const patch_handle_t* args_types,      // 可为 NULL
    const char** args_names               // 可为 NULL
) {
    if (il2cpp_method_get_param_count(method) != args_count) {
        return false;
    }

    for (int i = 0; i < args_count; ++i) {
        // 检查参数类型（如果提供）
        if (args_types) {
            const void* param_type_obj = il2cpp_method_get_param(method, i);
            if (!param_type_obj) return false;

            patch_handle_t param_class = il2cpp_class_from_type(param_type_obj);
            if (args_types[i] != param_class) {
                return false;
            }
        }

        // 检查参数名称（如果提供）
        if (args_names && args_names[i]) {
            const char* param_name = il2cpp_method_get_param_name(method, i);
            if (!param_name || strcmp(param_name, args_names[i]) != 0) {
                return false;
            }
        }
    }

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
    if (!type || !name)
        return PATCH_NULL;

    tef_vector_t methods = {};
    patchlib_type_get_methods(type, false, &methods);

    for (int i = 0; i < tefstd_vector_size(&methods); ++i) {
        void* method = *(void**)tefstd_vector_at(&methods, i);
        const char* method_name = il2cpp_method_get_name(method);
        if (method_name && strcmp(method_name, name) == 0) {
            if (method_matches_args(method, args_count, args_types, args_names)) {
                return method; // 找到即返回
            }
        }
    }

    tefstd_vector_destroy(&methods);

    return PATCH_NULL;
}

patch_handle_t patchlib_type_get_method_by_param_names(patch_handle_t type, const char *name,
                const int args_count, const char **args_names) {
    return patchlib_type_find_method(type, name, args_count, NULL, args_names);
}


patch_handle_t patchlib_type_get_method_by_param_types(patch_handle_t type, const char *name,
                const int args_count, const patch_handle_t* args_types) {
    return patchlib_type_find_method(type, name, args_count, args_types, NULL);
}

patch_handle_t patchlib_type_get_method_by_signature(patch_handle_t type, const char *name,
                const int args_count, const patch_handle_t *args_types, const char **args_names
) {
    return patchlib_type_find_method(type, name, args_count, args_types, args_names);
}

typedef void * (*il2cpp_class_iterator_fn)(void *klass, void **iter);
static bool patchlib_collect_from_type_hierarchy(
    void *start_type,
    const bool including_parent,
    tef_vector_t *array,
    const il2cpp_class_iterator_fn iterator_fn) {
    if (!start_type || !array || !iterator_fn)
        return false;

    void *current_type = start_type;
    do {
        void *iter = NULL;
        const void *item;

        while ((item = iterator_fn(current_type, &iter)) != NULL) {
            tefstd_vector_push_back(array, (void *) item); // vector 存 void*
        }

        if (!including_parent)
            break;
    } while ((current_type = il2cpp_class_get_parent(current_type)) != NULL);

    return true;
}

bool patchlib_type_get_inner_types(patch_handle_t type, const bool including_parent, tef_vector_t *array) {
    if (!patchlib_is_valid(type))
        return false;
    return patchlib_collect_from_type_hierarchy(
        type, including_parent, array, il2cpp_class_get_nested_types);
}

bool patchlib_type_get_methods(patch_handle_t type, const bool including_parent, tef_vector_t *array) {
    if (!patchlib_is_valid(type))
        return false;
    return patchlib_collect_from_type_hierarchy(
        type, including_parent, array, il2cpp_class_get_methods);
}

bool patchlib_type_get_fields(patch_handle_t type, const bool including_parent, tef_vector_t *array) {
    if (!patchlib_is_valid(type))
        return false;
    return patchlib_collect_from_type_hierarchy(
        type, including_parent, array, il2cpp_class_get_fields);
}

bool patchlib_type_get_properties(patch_handle_t type, const bool including_parent, tef_vector_t *array) {
    if (!patchlib_is_valid(type))
        return false;
    return patchlib_collect_from_type_hierarchy(
        type, including_parent, array, il2cpp_class_get_properties);
}

bool patchlib_type_free(patch_handle_t type) {
    return true;
}