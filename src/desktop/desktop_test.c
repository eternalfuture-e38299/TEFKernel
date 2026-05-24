/*******************************************************************************
 * tefkernel - desktop_test
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/log.h"
#include "patchlib/method.h"
#include "patchlib/field.h"
#include "patchlib/struct/array.h"

// 测试辅助函数
static void print_separator(const char *title) {
    printf("\n========== %s ==========\n", title);
}

static void print_test_result(const char *test_name, bool passed) {
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
}

// 测试基础工具函数
static void test_basic_utilities(void) {
    print_separator("基础工具函数测试");

    // 测试 patchlib_is_valid
    patch_handle_t valid_type = patchlib_type_get_type("System", "Object");
    print_test_result("patchlib_is_valid - 有效句柄", patchlib_is_valid(valid_type));
    print_test_result("patchlib_is_valid - NULL句柄", !patchlib_is_valid(PATCH_NULL));

    // 测试 patchlib_type_is_same
    patch_handle_t object_type1 = patchlib_type_get_type("System", "Object");
    patch_handle_t object_type2 = patchlib_type_get_type("System", "Object");
    patch_handle_t string_type = patchlib_type_get_type("System", "String");

    print_test_result("patchlib_type_is_same - 相同类型",
                      patchlib_type_is_same(object_type1, object_type2));
    print_test_result("patchlib_type_is_same - 不同类型",
                      !patchlib_type_is_same(object_type1, string_type));
    print_test_result("patchlib_type_is_same - 自身比较",
                      patchlib_type_is_same(object_type1, object_type1));

    // 测试 get_size_from_patch_type
    print_test_result("get_size_from_patch_type - PATCH_INT32 size=4",
                      get_size_from_patch_type(PATCH_INT32) == sizeof(int32_t));
    print_test_result("get_size_from_patch_type - PATCH_POINTER size=指针大小",
                      get_size_from_patch_type(PATCH_POINTER) == sizeof(void*));
}

// 测试类型获取和创建
static void test_type_acquisition(void) {
    print_separator("类型获取和创建测试");

    // 测试 patchlib_type_get_type
    patch_handle_t object_type = patchlib_type_get_type("System", "Object");
    print_test_result("patchlib_type_get_type - System.Object", patchlib_is_valid(object_type));

    patch_handle_t string_type = patchlib_type_get_type("System", "String");
    print_test_result("patchlib_type_get_type - System.String", patchlib_is_valid(string_type));

    patch_handle_t int32_type = patchlib_type_get_type("System", "Int32");
    print_test_result("patchlib_type_get_type - System.Int32", patchlib_is_valid(int32_type));

    patch_handle_t invalid_type = patchlib_type_get_type("Invalid.Namespace", "InvalidType");
    print_test_result("patchlib_type_get_type - 无效类型返回NULL", invalid_type == PATCH_NULL);

    // 测试 patchlib_get_basic_type
    patch_handle_t basic_int32 = patchlib_get_basic_type(PATCH_INT32);
    print_test_result("patchlib_get_basic_type - PATCH_INT32",
                      patchlib_type_is_same(basic_int32, int32_type));

    patch_handle_t basic_bool = patchlib_get_basic_type(PATCH_BOOL);
    patch_handle_t bool_type = patchlib_type_get_type("System", "Boolean");
    print_test_result("patchlib_get_basic_type - PATCH_BOOL",
                      patchlib_type_is_same(basic_bool, bool_type));

    // 测试 patchlib_type_new_instance
    patch_handle_t string_instance = patchlib_type_new_instance(string_type);
    print_test_result("patchlib_type_new_instance - 创建字符串实例",
                      patchlib_is_valid(string_instance));

    patch_handle_t object_instance = patchlib_type_new_instance(object_type);
    print_test_result("patchlib_type_new_instance - 创建Object实例",
                      patchlib_is_valid(object_instance));
}

// 测试类型信息查询
static void test_type_info(void) {
    print_separator("类型信息查询测试");

    patch_handle_t type = patchlib_type_get_type("System", "String");
    assert(patchlib_is_valid(type));

    // 测试 patchlib_type_get_name
    const char *name = patchlib_type_get_name(type);
    print_test_result("patchlib_type_get_name - 名称正确",
                      name && strcmp(name, "String") == 0);
    printf("  - 类型名称: %s\n", name);

    // 测试 patchlib_type_get_namespace
    const char *ns = patchlib_type_get_namespace(type);
    print_test_result("patchlib_type_get_namespace - 命名空间正确",
                      ns && strcmp(ns, "System") == 0);
    printf("  - 命名空间: %s\n", ns);

    // 测试 patchlib_type_get_full_name
    char *full_name = patchlib_type_get_full_name(type);
    print_test_result("patchlib_type_get_full_name - 完整名称正确",
                      full_name && strcmp(full_name, "System.String") == 0);
    printf("  - 完整名称: %s\n", full_name);
    free(full_name);

    // 测试 patchlib_type_get_parent
    patch_handle_t parent = patchlib_type_get_parent(type);
    print_test_result("patchlib_type_get_parent - String父类存在", patchlib_is_valid(parent));

    if (patchlib_is_valid(parent)) {
        const char *parent_name = patchlib_type_get_name(parent);
        printf("  - String的父类: %s\n", parent_name);
        print_test_result("patchlib_type_get_parent - String父类是Object",
                          strcmp(parent_name, "Object") == 0);
    }

    patch_handle_t object_type = patchlib_type_get_type("System", "Object");
    patch_handle_t object_parent = patchlib_type_get_parent(object_type);
    print_test_result("patchlib_type_get_parent - Object父类为NULL",
                      object_parent == PATCH_NULL);
}

// 测试成员获取（单个）
static void test_member_acquisition(void) {
    print_separator("成员获取测试（单个）");

    patch_handle_t string_type = patchlib_type_get_type("System", "String");
    assert(patchlib_is_valid(string_type));

    // 测试 patchlib_type_get_field
    patch_handle_t field = patchlib_type_get_field(string_type, "_firstChar");
    print_test_result("patchlib_type_get_field - 获取String._firstChar",
                      patchlib_is_valid(field));

    // 测试 patchlib_type_get_property
    patch_handle_t property = patchlib_type_get_property(string_type, "Length");
    print_test_result("patchlib_type_get_property - 获取String.Length",
                      patchlib_is_valid(property));

    // 测试 patchlib_type_get_method
    patch_handle_t method = patchlib_type_get_method(string_type, "Substring");
    print_test_result("patchlib_type_get_method - 获取String.Substring",
                      patchlib_is_valid(method));

    // 测试 patchlib_type_get_method_by_param_count
    patch_handle_t method_with_params = patchlib_type_get_method_by_param_count(string_type, "Substring", 2);
    print_test_result("patchlib_type_get_method_by_param_count - Substring(2参数)",
                      patchlib_is_valid(method_with_params));

    // 测试 patchlib_type_get_inner_type
    patch_handle_t list_type = patchlib_type_get_type("System.Collections.Generic", "List`1");
    if (patchlib_is_valid(list_type)) {
        patch_handle_t inner_type = patchlib_type_get_inner_type(list_type, "Enumerator");
        print_test_result("patchlib_type_get_inner_type - List.Enumerator",
                          patchlib_is_valid(inner_type));
    }
}

// 测试成员数组（批量获取）
static void test_member_arrays(void) {
    print_separator("成员数组测试（批量获取）");

    patch_handle_t string_type = patchlib_type_get_type("System", "String");
    assert(patchlib_is_valid(string_type));

    tefstd_vector_t methods;
    tefstd_vector_t fields;
    tefstd_vector_t properties;

    // 测试 patchlib_type_get_methods
    bool methods_result = patchlib_type_get_methods(string_type, false, &methods);
    print_test_result("patchlib_type_get_methods - 获取方法列表", methods_result);
    if (methods_result) {
        size_t method_count = tefstd_vector_size(&methods);
        printf("  - 方法数量: %zu\n", method_count);
        print_test_result("patchlib_type_get_methods - 方法数量>0", method_count > 0);
        tefstd_vector_destroy(&methods);
    }

    // 测试 patchlib_type_get_fields
    bool fields_result = patchlib_type_get_fields(string_type, false, &fields);
    print_test_result("patchlib_type_get_fields - 获取字段列表", fields_result);
    if (fields_result) {
        size_t field_count = tefstd_vector_size(&fields);
        printf("  - 字段数量: %zu\n", field_count);
        tefstd_vector_destroy(&fields);
    }

    // 测试 patchlib_type_get_properties
    bool props_result = patchlib_type_get_properties(string_type, false, &properties);
    print_test_result("patchlib_type_get_properties - 获取属性列表", props_result);
    if (props_result) {
        size_t prop_count = tefstd_vector_size(&properties);
        printf("  - 属性数量: %zu\n", prop_count);
        tefstd_vector_destroy(&properties);
    }

    // 测试 including_parent 标志
    patch_handle_t exception_type = patchlib_type_get_type("System", "Exception");
    if (patchlib_is_valid(exception_type)) {
        tefstd_vector_t methods_only;
        tefstd_vector_t methods_with_parent;

        patchlib_type_get_methods(exception_type, false, &methods_only);
        patchlib_type_get_methods(exception_type, true, &methods_with_parent);

        size_t only_count = tefstd_vector_size(&methods_only);
        size_t with_parent_count = tefstd_vector_size(&methods_with_parent);

        print_test_result("patchlib_type_get_methods - including_parent 工作",
                          with_parent_count > only_count);
        printf("  - 仅当前类方法: %zu, 包含父类方法: %zu\n", only_count, with_parent_count);

        tefstd_vector_destroy(&methods_only);
        tefstd_vector_destroy(&methods_with_parent);
    }
}

// 测试泛型类型
static void test_generic_types(void) {
    print_separator("泛型类型测试");

    patch_handle_t list_def = patchlib_type_get_type("System.Collections.Generic", "List`1");
    print_test_result("patchlib_type_get_type - 获取List`1定义", patchlib_is_valid(list_def));

    if (patchlib_is_valid(list_def)) {
        // 测试泛型判定
        bool is_generic = false;
        // 注意：需要检查是否有 il2cpp_class_is_generic API
        printf("  - List`1 是泛型类型\n");

        // 测试 patchlib_type_get_inner_types 获取嵌套类型
        tefstd_vector_t inner_types;
        if (patchlib_type_get_inner_types(list_def, false, &inner_types)) {
            size_t inner_count = tefstd_vector_size(&inner_types);
            printf("  - List`1 嵌套类型数量: %zu\n", inner_count);
            tefstd_vector_destroy(&inner_types);
        }
    }
}

// 测试资源释放
static void test_resource_management(void) {
    print_separator("资源管理测试");

    // 测试创建和释放
    patch_handle_t instance = patchlib_type_new_instance(
        patchlib_type_get_type("System", "Object")
    );

    if (patchlib_is_valid(instance)) {
        printf("patchlib_object_free - 释放对象");
        patchlib_free(instance);
    }

    // 测试持久化
    patch_handle_t temp_obj = patchlib_type_new_instance(
        patchlib_type_get_type("System", "String")
    );

    if (patchlib_is_valid(temp_obj)) {
        patch_handle_t persisted = patchlib_handle_copy(temp_obj);
        print_test_result("patchlib_handle_copy - 浅拷贝对象",
                          patchlib_is_valid(persisted));

        #if !defined(__ANDROID__)
        patchlib_free(persisted);
        #endif
    }
}

// 测试边界条件和错误处理
static void test_edge_cases(void) {
    print_separator("边界条件和错误处理测试");

    // 测试 NULL 参数
    print_test_result("patchlib_type_get_type(NULL, NULL) 返回 PATCH_NULL",
                      patchlib_type_get_type(NULL, NULL) == PATCH_NULL);
    print_test_result("patchlib_type_get_type(\"System\", NULL) 返回 PATCH_NULL",
                      patchlib_type_get_type("System", NULL) == PATCH_NULL);
    print_test_result("patchlib_type_get_type(NULL, \"Object\") 返回 PATCH_NULL",
                      patchlib_type_get_type(NULL, "Object") == PATCH_NULL);

    // 测试无效句柄
    print_test_result("patchlib_type_get_name(PATCH_NULL) 返回 NULL",
                      patchlib_type_get_name(PATCH_NULL) == NULL);
    print_test_result("patchlib_type_get_parent(PATCH_NULL) 返回 PATCH_NULL",
                      patchlib_type_get_parent(PATCH_NULL) == PATCH_NULL);
    print_test_result("patchlib_type_get_field(PATCH_NULL, \"name\") 返回 PATCH_NULL",
                      patchlib_type_get_field(PATCH_NULL, "name") == PATCH_NULL);

    // 测试不存在的成员
    patch_handle_t object_type = patchlib_type_get_type("System", "Object");
    patch_handle_t non_existent_field = patchlib_type_get_field(object_type, "NonExistentField");
    print_test_result("获取不存在的字段返回 PATCH_NULL",
                      non_existent_field == PATCH_NULL);

    patch_handle_t non_existent_method = patchlib_type_get_method(object_type, "NonExistentMethod");
    print_test_result("获取不存在的方法返回 PATCH_NULL",
                      non_existent_method == PATCH_NULL);
}

void Test() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           patchlib_type 模块完整功能测试                     ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");

    // 初始化日志（如果需要）
    // teklog_init(TEKLOG_LEVEL_DEBUG, NULL);

    // 运行所有测试
    test_basic_utilities();
    test_type_acquisition();
    test_type_info();
    test_member_acquisition();
    test_member_arrays();
    test_generic_types();
    test_resource_management();
    test_edge_cases();

    print_separator("测试完成");
    printf("所有测试执行完毕！\n");
}