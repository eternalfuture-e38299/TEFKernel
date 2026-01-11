/*******************************************************************************
 * tefkernel - test_modloader
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
#include <stdlib.h>

#include "../include/modloader/modloader_core.h"
#include "../include/memdl/memdl.h"
#include "test_mod_api.h"
#include "../include/tefplugin/tpf_core.h"

// 已加载的Mod列表
static struct {
    void** mod_handles;        // 动态库句柄数组
    mod_ops_t** mod_ops;       // Mod操作函数表数组
    size_t count;              // 已加载的Mod数量
    size_t capacity;           // 数组容量
} g_loaded_mods = {0};

static void test_ml_unload_mods(ml_entry_t *loader) {
    printf("[TestModLoader] Unloading %zu mod(s)...\n", g_loaded_mods.count);

    for (size_t i = 0; i < g_loaded_mods.count; i++) {
        if (g_loaded_mods.mod_ops[i] && g_loaded_mods.mod_ops[i]->shutdown) {
            printf("[TestModLoader] Shutting down mod %zu\n", i);
            g_loaded_mods.mod_ops[i]->shutdown();
        }

        if (g_loaded_mods.mod_handles[i]) {
            memdl_close(g_loaded_mods.mod_handles[i]);
            g_loaded_mods.mod_handles[i] = NULL;
        }

        g_loaded_mods.mod_ops[i] = NULL;
    }

    g_loaded_mods.count = 0;
    printf("[TestModLoader] All mods unloaded\n");
}


// 初始化ModLoader
static ml_result_t test_ml_initialize(ml_entry_t *loader) {
    printf("[TestModLoader] Initializing test modloader...\n");

    // 初始化Mod列表
    g_loaded_mods.capacity = 10;
    g_loaded_mods.mod_handles = malloc(sizeof(void*) * g_loaded_mods.capacity);
    g_loaded_mods.mod_ops = malloc(sizeof(mod_ops_t*) * g_loaded_mods.capacity);

    if (!g_loaded_mods.mod_handles || !g_loaded_mods.mod_ops) {
        printf("[TestModLoader] Failed to allocate memory for mod list\n");
        return ML_ERROR;
    }

    g_loaded_mods.count = 0;

    printf("[TestModLoader] Test modloader initialized successfully\n");
    return ML_SUCCESS;
}

// 关闭ModLoader
static void test_ml_shutdown(ml_entry_t *loader) {
    printf("[TestModLoader] Shutting down test modloader...\n");

    // 先卸载所有Mod
    if (g_loaded_mods.count > 0) {
        test_ml_unload_mods(loader);
    }

    // 清理Mod列表内存
    if (g_loaded_mods.mod_handles) {
        free(g_loaded_mods.mod_handles);
        g_loaded_mods.mod_handles = NULL;
    }

    if (g_loaded_mods.mod_ops) {
        free(g_loaded_mods.mod_ops);
        g_loaded_mods.mod_ops = NULL;
    }

    g_loaded_mods.count = 0;
    g_loaded_mods.capacity = 0;

    printf("[TestModLoader] Test modloader shutdown completed\n");
}

// 加载Mods
static ml_result_t test_ml_load_mods(ml_entry_t *loader) {
    printf("[TestModLoader] Loading mods...\n");

    if (g_loaded_mods.count >= g_loaded_mods.capacity) {
        printf("[TestModLoader] Mod list is full, cannot load more mods\n");
        return ML_ERROR;
    }

    // 使用memdl加载测试Mod
    printf("[TestModLoader] Loading test mod...\n");

    // 使用相对路径或可配置的路径
    const char* mod_path = "./libtest_mod.so";  // 或者从配置读取
    void* mod_handle = memdl_open_file(mod_path, MEMDL_LAZY);

    if (!mod_handle) {
        printf("[TestModLoader] Failed to load mod from %s: %s\n",
               mod_path, memdl_error());
        return ML_ERROR;
    }

    // 获取mod_main函数 - 修复函数指针类型
    typedef mod_ops_t* (*mod_main_func_t)(void);
    mod_main_func_t mod_main_func = memdl_sym(mod_handle, "mod_main");

    if (!mod_main_func) {
        printf("[TestModLoader] Failed to find mod_main symbol: %s\n", memdl_error());
        memdl_close(mod_handle);
        return ML_ERROR;
    }

    // 调用mod_main获取操作函数表
    mod_ops_t* mod_ops = mod_main_func();
    if (!mod_ops) {
        printf("[TestModLoader] mod_main returned NULL\n");
        memdl_close(mod_handle);
        return ML_ERROR;
    }

    // 验证必要的函数
    if (!mod_ops->initialize || !mod_ops->shutdown || !mod_ops->get_info) {
        printf("[TestModLoader] Mod missing required functions\n");
        memdl_close(mod_handle);
        return ML_ERROR;
    }

    tpf_register_shared_plugin_library(mod_handle);

    // 添加到加载列表
    g_loaded_mods.mod_handles[g_loaded_mods.count] = mod_handle;
    g_loaded_mods.mod_ops[g_loaded_mods.count] = mod_ops;
    g_loaded_mods.count++;

    printf("[TestModLoader] Successfully loaded mod: %s\n",
           mod_ops->get_info()->name);
    printf("[TestModLoader] Successfully loaded %zu mod(s)\n", g_loaded_mods.count);
    return ML_SUCCESS;
}

// 初始化Mods
static ml_result_t test_ml_initialize_mods(ml_entry_t *loader) {
    printf("[TestModLoader] Initializing %zu mod(s)...\n", g_loaded_mods.count);

    for (size_t i = 0; i < g_loaded_mods.count; i++) {
        if (g_loaded_mods.mod_ops[i] && g_loaded_mods.mod_ops[i]->initialize) {
            const mod_info_t* info = g_loaded_mods.mod_ops[i]->get_info();
            printf("[TestModLoader] Initializing mod %zu: %s\n", i, info->name);

            int result = g_loaded_mods.mod_ops[i]->initialize();
            if (result != 0) {
                printf("[TestModLoader] Failed to initialize mod %zu (error: %d)\n", i, result);
                return ML_ERROR;
            }
            printf("[TestModLoader] Mod %zu initialized successfully\n", i);
        }
    }

    printf("[TestModLoader] All mods initialized successfully\n");
    return ML_SUCCESS;
}

// 更新所有Mods（可选功能）
static void test_ml_update_mods(float delta_time) {
    for (size_t i = 0; i < g_loaded_mods.count; i++) {
        if (g_loaded_mods.mod_ops[i] && g_loaded_mods.mod_ops[i]->update) {
            g_loaded_mods.mod_ops[i]->update(delta_time);
        }
    }
}

// 使用便捷宏声明ModLoader
ML_CREATE(
    "test.modloader",           // pkg_id
    1,                         // version_code
    "1.0.0",                   // version
    1,                         // api_version
    NULL, 0,                   // 无依赖
    test_ml_initialize,        // 初始化函数
    test_ml_shutdown,          // 关闭函数
    test_ml_load_mods,         // 加载Mods函数
    test_ml_unload_mods,       // 卸载Mods函数
    test_ml_initialize_mods    // 初始化Mods函数
)