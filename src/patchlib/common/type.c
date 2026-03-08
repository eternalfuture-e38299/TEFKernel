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
#include "patchlib/common_private.h"
#include "patchlib/method.h"

bool patchlib_is_valid(patch_handle_t h) {
    if (h == PATCH_NULL) {
        TEKLOG_WARN("patch_handle is null");
        return false;
    }
    TEKLOG_TRACE("patch_handle is valid: " PATCH_HANDLE_FMT, h);
    return true;
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
        case PATCH_UINT8:  result = sizeof(uint8_t); break;
        case PATCH_UINT16: result = sizeof(uint16_t); break;
        case PATCH_UINT32: result = sizeof(uint32_t); break;
        case PATCH_UINT64: result = sizeof(uint64_t); break;
        case PATCH_INT8:   result = sizeof(int8_t); break;
        case PATCH_INT16:  result = sizeof(int16_t); break;
        case PATCH_INT32:  result = sizeof(int32_t); break;
        case PATCH_INT64:  result = sizeof(int64_t); break;
        case PATCH_FLOAT:  result = sizeof(float); break;
        case PATCH_DOUBLE: result = sizeof(double); break;
        case PATCH_BOOL:   result = sizeof(bool); break;
        case PATCH_POINTER:
        case PATCH_OBJECT: result = OBJECT_SIZE; break; // 对象通常是引用，大小为指针
        case PATCH_CHAR:   result = sizeof(char); break;
        default:           result = 0; break; // 未知类型
    }

#undef OBJECT_SIZE

    TEKLOG_DEBUG("Type %d size: %zu bytes", type, result);
    return result;
}

patch_handle_t patchlib_get_basic_type(const patch_type_t basic_type) {
    TEKLOG_DEBUG("patchlib_get_basic_type called: basic_type=%d", basic_type);

    const char* type_name;
    patch_handle_t result = PATCH_NULL;

    switch (basic_type) {
        case PATCH_VOID: type_name = "Void"; result = patchlib_type_get_type("System", "Void"); break;
        case PATCH_INT8: type_name = "SByte"; result = patchlib_type_get_type("System", "SByte"); break;
        case PATCH_INT16: type_name = "Int16"; result = patchlib_type_get_type("System", "Int16"); break;
        case PATCH_INT32: type_name = "Int32"; result = patchlib_type_get_type("System", "Int32"); break;
        case PATCH_INT64: type_name = "Int64"; result = patchlib_type_get_type("System", "Int64"); break;
        case PATCH_UINT8: type_name = "Byte"; result = patchlib_type_get_type("System", "Byte"); break;
        case PATCH_UINT16: type_name = "UInt16"; result = patchlib_type_get_type("System", "UInt16"); break;
        case PATCH_UINT32: type_name = "UInt32"; result = patchlib_type_get_type("System", "UInt32"); break;
        case PATCH_UINT64: type_name = "UInt64"; result = patchlib_type_get_type("System", "UInt64"); break;
        case PATCH_BOOL: type_name = "Boolean"; result = patchlib_type_get_type("System", "Boolean"); break;
        case PATCH_FLOAT: type_name = "Single"; result = patchlib_type_get_type("System", "Single"); break;
        case PATCH_DOUBLE: type_name = "Double"; result = patchlib_type_get_type("System", "Double"); break;
        case PATCH_POINTER: type_name = "IntPtr"; result = patchlib_type_get_type("System", "IntPtr"); break;
        case PATCH_OBJECT: type_name = "Object"; result = patchlib_type_get_type("System", "Object"); break;
        case PATCH_CHAR: type_name = "Char"; result = patchlib_type_get_type("System", "Char"); break;
        default: type_name = "Unknown"; result = PATCH_NULL; break;
    }


    TEKLOG_DEBUG("Basic type %d (%s) result: " PATCH_HANDLE_FMT,
                 basic_type, type_name, result);

    return result;
}

char* patchlib_type_get_full_name(patch_handle_t type) {
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

    char* full_name = malloc(total_len);
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
    TEKLOG_DEBUG("patchlib_type_get_inner_type called: parent=" PATCH_HANDLE_FMT " name='%s'", parent, name ? name : "NULL");

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
            patch_handle_t* type = tefstd_vector_at(&inner_types, i);
            const char* nested_name = patchlib_type_get_name(*type);

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
            const char* method_name = patchlib_method_get_name(*method);
            TEKLOG_DEBUG("Method %d: %p, name: '%s'", i, method, method_name ? method_name : "NULL");

            if (method_name && strcmp(method_name, name) == 0) {
                result = *method;
                TEKLOG_INFO("Found method '%s' at " PATCH_HANDLE_FMT, name, result);
                break;
            }
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
                const int args_count, const patch_handle_t* args_types) {
    TEKLOG_DEBUG("patchlib_type_get_method_by_param_types called: type=" PATCH_HANDLE_FMT ", name='%s', args_count=%d",
                 type, name ? name : "NULL", args_count);

    patch_handle_t result = patchlib_type_find_method(type, name, args_count, args_types, NULL);
    TEKLOG_DEBUG("Method by param types result: " PATCH_HANDLE_FMT, result);
    return result;
}

patch_handle_t patchlib_type_get_method_by_signature(patch_handle_t type, const char *name,
                const int args_count, const patch_handle_t *args_types, const char **args_names
) {
    TEKLOG_DEBUG("patchlib_type_get_method_by_signature called: type=" PATCH_HANDLE_FMT ", name='%s', args_count=%d",
                 type, name ? name : "NULL", args_count);

    patch_handle_t result = patchlib_type_find_method(type, name, args_count, args_types, args_names);
    TEKLOG_DEBUG("Method by signature result: " PATCH_HANDLE_FMT, result);
    return result;
}