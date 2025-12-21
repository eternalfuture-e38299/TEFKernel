/*******************************************************************************
 * tefkernel - test_mod
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
 * Created: 2025/12/20
 *******************************************************************************/

#include <stdio.h>
#include "test_mod_api.h"
#include "../include/tefstd/vector.h"  // 包含vector头文件

// 测试Mod的信息
static const mod_info_t test_mod_info = {
    .mod_id = "test.example_mod",
    .name = "Example Test Mod",
    .author = "eternalfuture-e38299",
    .version = "1.0.0",
    .description = "A simple test mod for demonstration",
    .api_version = MOD_API_VERSION
};

// 测试vector功能的函数
static void test_vector_functionality(void) {
    printf("[TestMod] Testing vector functionality...\n");

    tef_vector_t int_vec;

    // 1. 初始化vector
    if (!tefstd_vector_init(&int_vec, sizeof(int))) {
        printf("[TestMod] Failed to initialize vector\n");
        return;
    }
    printf("[TestMod] Vector initialized successfully\n");

    // 2. 添加一些整数
    for (int i = 0; i < 5; i++) {
        if (!tefstd_vector_push_back(&int_vec, &i)) {
            printf("[TestMod] Failed to push back element %d\n", i);
            tefstd_vector_destroy(&int_vec);
            return;
        }
        printf("[TestMod] Pushed element: %d\n", i);
    }

    // 3. 获取vector大小和容量
    size_t size = tefstd_vector_size(&int_vec);
    size_t capacity = tefstd_vector_capacity(&int_vec);
    printf("[TestMod] Vector size: %zu, capacity: %zu\n", size, capacity);

    // 4. 遍历并打印所有元素
    printf("[TestMod] Vector elements: ");
    for (size_t i = 0; i < size; i++) {
        int* element = (int*)tefstd_vector_at(&int_vec, i);
        if (element) {
            printf("%d ", *element);
        }
    }
    printf("\n");

    // 5. 测试修改元素
    int* first_element = (int*)tefstd_vector_at(&int_vec, 0);
    if (first_element) {
        *first_element = 100;  // 修改第一个元素
        printf("[TestMod] Modified first element to: %d\n", *first_element);
    }

    // 6. 测试删除元素
    int popped_value;
    if (tefstd_vector_pop_back(&int_vec, &popped_value)) {
        printf("[TestMod] Popped element: %d\n", popped_value);
    }

    // 7. 测试插入和删除中间元素
    int new_value = 50;
    if (tefstd_vector_push_back(&int_vec, &new_value)) {
        printf("[TestMod] Added new element: %d\n", new_value);
    }

    // 打印最终状态
    size = tefstd_vector_size(&int_vec);
    printf("[TestMod] Final vector elements: ");
    for (size_t i = 0; i < size; i++) {
        int* element = (int*)tefstd_vector_at(&int_vec, i);
        if (element) {
            printf("%d ", *element);
        }
    }
    printf("\n");

    // 8. 清理vector
    tefstd_vector_destroy(&int_vec);
    printf("[TestMod] Vector test completed successfully\n");
}

// 测试字符串vector
static void test_string_vector(void) {
    printf("[TestMod] Testing string vector functionality...\n");

    tef_vector_t str_vec;

    // 初始化字符串vector（存储char*）
    if (!tefstd_vector_init(&str_vec, sizeof(char*))) {
        printf("[TestMod] Failed to initialize string vector\n");
        return;
    }

    // 添加一些字符串
    const char* strings[] = {"Hello", "World", "From", "Test", "Mod"};
    for (int i = 0; i < 5; i++) {
        if (!tefstd_vector_push_back(&str_vec, &strings[i])) {
            printf("[TestMod] Failed to push string: %s\n", strings[i]);
            tefstd_vector_destroy(&str_vec);
            return;
        }
        printf("[TestMod] Pushed string: %s\n", strings[i]);
    }

    // 打印所有字符串
    size_t size = tefstd_vector_size(&str_vec);
    printf("[TestMod] String vector contents: ");
    for (size_t i = 0; i < size; i++) {
        char** str_ptr = (char**)tefstd_vector_at(&str_vec, i);
        if (str_ptr && *str_ptr) {
            printf("%s ", *str_ptr);
        }
    }
    printf("\n");

    tefstd_vector_destroy(&str_vec);
    printf("[TestMod] String vector test completed\n");
}

// 测试Mod的操作函数
static int test_mod_initialize(void) {
    printf("[TestMod] Initializing test mod...\n");
    printf("=== Test Mod Loaded Successfully! ===\n");
    printf("Mod ID: %s\n", test_mod_info.mod_id);
    printf("Name: %s\n", test_mod_info.name);
    printf("Author: %s\n", test_mod_info.author);
    printf("Version: %s\n", test_mod_info.version);
    printf("Description: %s\n", test_mod_info.description);
    printf("=====================================\n");

    // 测试vector功能
    printf("\n[TestMod] Starting vector functionality tests...\n");
    test_vector_functionality();
    printf("\n");
    test_string_vector();
    printf("[TestMod] All vector tests completed!\n");

    return 0; // 返回0表示成功
}

static void test_mod_shutdown(void) {
    printf("[TestMod] Shutting down test mod...\n");
    printf("=== Test Mod Unloaded ===\n");
}

static void test_mod_update(float delta_time) {
    // 简单的更新逻辑，每5秒打印一次
    static float timer = 0.0f;
    static int update_count = 0;
    timer += delta_time;

    if (timer >= 5.0f) {
        printf("[TestMod] Update #%d - Delta time: %.3f\n", ++update_count, delta_time);

        // 在更新时也可以进行简单的vector操作测试
        if (update_count % 2 == 0) {
            // 每2次更新测试一次简单的vector操作
            tef_vector_t temp_vec;
            if (tefstd_vector_init(&temp_vec, sizeof(int))) {
                int temp_value = update_count * 10;
                tefstd_vector_push_back(&temp_vec, &temp_value);
                printf("[TestMod] Quick vector test - added value: %d\n", temp_value);
                tefstd_vector_destroy(&temp_vec);
            }
        }

        timer = 0.0f;
    }
}

static const mod_info_t* test_mod_get_info(void) {
    return &test_mod_info;
}

// Mod操作函数表
static mod_ops_t test_mod_ops = {
    .initialize = test_mod_initialize,
    .shutdown = test_mod_shutdown,
    .update = test_mod_update,
    .get_info = test_mod_get_info
};

// Mod主函数
mod_ops_t* mod_main(void) {
    return &test_mod_ops;
}