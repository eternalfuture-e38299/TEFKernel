/*******************************************************************************
 * tefkernel - runtime
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
 * Created: 2026/3/8
 *******************************************************************************/

#include "internal/runtime.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "internal/platform_threads.h"

#include "internal/kernel_state.h"
#include "internal/log.h"
#include "internal/mod_core.h"
#include "tefpackage/tefpkg.h"
#include "tefstd/vector.h"
#include "tefstd/hashmap.h"
#include "internal/modloader/modloader_core_imp.h"
#include "internal/module/module_core_imp.h"
#include "internal/tefplugin/tef_core_imp.h"
#include "memdl/memdl.h"

// 快速字符串哈希函数 (FNV-1a)
static uint64_t fast_hash_str(const char* str) {
    uint64_t hash = 14695981039346656037ULL;
    while (*str) {
        hash = (hash ^ (uint8_t)(*str)) * 1099511628211ULL;
        str++;
    }
    return hash;
}

// 存储包信息的结构
typedef struct {
    uint64_t key_hash;     // 字符串哈希作为键
    tefpkg_t* pkg;         // 包指针
} runtime_pkg_entry_t;

static tefstd_hashmap_t plugin_pkgs;    ///< uint64_t, runtime_pkg_entry_t
static tefstd_hashmap_t modloader_pkgs; ///< uint64_t, runtime_pkg_entry_t
static tefstd_hashmap_t module_pkgs;    ///< uint64_t, runtime_pkg_entry_t

// 热重载线程相关
static thrd_t g_hotreload_thread;
static mtx_t g_hotreload_mutex;
static bool g_hotreload_running = false;
static bool g_hotreload_enabled = true;

/**
 * @brief 从enables.txt加载包ID列表
 */
static int load_enabled_ids(const char* base_dir, tefstd_vector_t* ids) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s/enables.txt",
             tefkernel_working_dir, base_dir);

    FILE* file = fopen(path, "r");
    if (!file) {
        // 文件不存在，直接返回-1表示失败
        return -1;
    }

    if (!tefstd_vector_init(ids, sizeof(char*))) {
        fclose(file);
        return -1;
    }

    int count = 0;
    char line[256];

    while (fgets(line, sizeof(line), file)) {
        // 清理行尾
        line[strcspn(line, "\r\n")] = '\0';

        // 跳过空行和注释
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // 复制字符串
        char* id_copy = strdup(line);
        if (!id_copy) continue;

        if (tefstd_vector_push_back(ids, &id_copy)) {
            count++;
        } else {
            free(id_copy);
        }
    }

    fclose(file);
    return count;
}

/**
 * @brief 加载单个目录的包
 */
static void load_single_pkg_type(const char* type_name, tefstd_hashmap_t* target) {
    TEKLOG_INFO("Loading %s packages", type_name);

    tefstd_vector_t ids;
    const int count = load_enabled_ids(type_name, &ids);

    if (count <= 0) {
        TEKLOG_DEBUG("No enabled %s packages (or enables.txt not found)", type_name);
        // if (ids.data) tefstd_vector_destroy(&ids);
        return;
    }

    TEKLOG_DEBUG("Found %d enabled %s packages", count, type_name);

    int loaded = 0;
    int failed = 0;
    char pkg_path[1024];

    for (size_t i = 0; i < tefstd_vector_size(&ids); i++) {
        char** id_ptr = tefstd_vector_at(&ids, i);
        if (!id_ptr || !*id_ptr) continue;

        const char* pkg_id = *id_ptr;

        // 构建包路径
        snprintf(pkg_path, sizeof(pkg_path), "%s/%s/pkg/%s.tefpkg",
                 tefkernel_working_dir, type_name, pkg_id);

        // 打开包
        tefpkg_t* pkg = NULL;
        const tefpkg_result_t result = tefpkg_open_readonly(pkg_path, &pkg);

        if (result != TEF_OK || !pkg) {
            TEKLOG_WARN("Failed to open %s: %s (result=%d)",
                       pkg_id, pkg_path, result);
            failed++;
            continue;
        }

        // 创建条目
        runtime_pkg_entry_t entry = {
            .key_hash = fast_hash_str(pkg_id),
            .pkg = pkg
        };

        // 存储到哈希表
        if (tefstd_hashmap_put(target, &entry.key_hash, &entry)) {
            TEKLOG_DEBUG("Loaded %s: %s", type_name, pkg_id);
            loaded++;
        } else {
            TEKLOG_WARN("Failed to store %s in hashmap: %s", type_name, pkg_id);
            tefpkg_close(pkg);
            failed++;
        }
    }

    // 清理ID列表
    for (size_t i = 0; i < tefstd_vector_size(&ids); i++) {
        char** ptr = tefstd_vector_at(&ids, i);
        if (ptr && *ptr) free(*ptr);
    }
    tefstd_vector_destroy(&ids);

    TEKLOG_INFO("%s packages: %d loaded, %d failed", type_name, loaded, failed);
}

/**
 * @brief 获取目录的修改时间
 */
static time_t get_directory_mtime(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

/**
 * @brief 检查工作目录是否被修改
 */
static bool check_working_dir_modified(time_t* last_check_time) {
    const time_t current_mtime = get_directory_mtime(tefkernel_working_dir);

    if (*last_check_time == 0) {
        *last_check_time = current_mtime;
        return false;
    }

    if (current_mtime != *last_check_time) {
        TEKLOG_DEBUG("Working directory modified: old=%ld, new=%ld",
                    *last_check_time, current_mtime);
        *last_check_time = current_mtime;
        return true;
    }

    return false;
}

/**
 * @brief 热重载线程函数
 */
static int hotreload_thread_func(void* arg) {
    TEKLOG_INFO("Hot reload thread started");

    time_t last_check_time = 0;

    while (g_hotreload_running) {
        const int check_interval_ms = 1000;
        // 休眠一段时间
        thrd_sleep(&(struct timespec){.tv_sec = 0, .tv_nsec = check_interval_ms * 1000000}, NULL);

        mtx_lock(&g_hotreload_mutex);

        if (g_hotreload_enabled && check_working_dir_modified(&last_check_time)) {
            TEKLOG_INFO("Detected working directory modification, performing hot reload");

            // 1. 调用所有ModLoader的reload_mod函数
            if (g_ml_list_initialized) {
                const size_t ml_count = tefstd_vector_size(&g_ml_list);
                TEKLOG_DEBUG("Calling reload_mod for %zu modloaders", ml_count);

                for (size_t i = 0; i < ml_count; ++i) {
                    ml_handle_t** ml_ptr = tefstd_vector_at(&g_ml_list, i);
                    if (!ml_ptr || !*ml_ptr || !(*ml_ptr)->ml_entry || !(*ml_ptr)->ml_entry->ops) continue;

                    const ml_entry_t* entry = (*ml_ptr)->ml_entry;

                    // 检查是否有reload_mod函数
                    if (entry->ops->reload_mod) {
                        TEKLOG_DEBUG("Calling reload_mod for modloader %zu", i);
                        // 注意：这里需要提供一个manifest参数，实际应用中需要从配置文件加载
                        mod_manifest_t dummy_manifest = {0};
                        entry->ops->reload_mod(&dummy_manifest);
                    }
                }
            }

            // 2. 调用所有模块的热重载函数
            if (g_module_list_initialized) {
                const size_t module_count = tefstd_vector_size(&g_module_list);
                TEKLOG_DEBUG("Calling hot_reload for %zu modules", module_count);

                for (size_t i = 0; i < module_count; ++i) {
                    module_handle_t** module_ptr = tefstd_vector_at(&g_module_list, i);
                    if (!module_ptr || !*module_ptr || !(*module_ptr)->module_entry || !(*module_ptr)->module_entry->ops) continue;

                    module_entry_t* entry = (*module_ptr)->module_entry;

                    if (entry->ops->hot_reload) {
                        TEKLOG_DEBUG("Calling hot_reload for module %zu", i);
                        entry->ops->hot_reload(entry);
                    }
                }
            }

            TEKLOG_INFO("Hot reload completed");
        }

        mtx_unlock(&g_hotreload_mutex);
    }

    TEKLOG_INFO("Hot reload thread stopped");
    return 0;
}

/**
 * @brief 启动热重载线程
 */
void tefkernel_start_hotreload_thread(void) {
    if (g_hotreload_running) {
        TEKLOG_WARN("Hot reload thread is already running");
        return;
    }

    if (mtx_init(&g_hotreload_mutex, mtx_plain) != thrd_success) {
        TEKLOG_ERROR("Failed to initialize hot reload mutex");
        return;
    }

    g_hotreload_running = true;
    g_hotreload_enabled = true;

    if (thrd_create(&g_hotreload_thread, hotreload_thread_func, NULL) != thrd_success) {
        TEKLOG_ERROR("Failed to create hot reload thread");
        g_hotreload_running = false;
        mtx_destroy(&g_hotreload_mutex);
        return;
    }

    TEKLOG_INFO("Hot reload thread started successfully");
}

/**
 * @brief 停止热重载线程
 */
void tefkernel_stop_hotreload_thread(void) {
    if (!g_hotreload_running) {
        TEKLOG_DEBUG("Hot reload thread is not running");
        return;
    }

    TEKLOG_INFO("Stopping hot reload thread");

    mtx_lock(&g_hotreload_mutex);
    g_hotreload_running = false;
    mtx_unlock(&g_hotreload_mutex);

    thrd_join(g_hotreload_thread, NULL);
    mtx_destroy(&g_hotreload_mutex);

    TEKLOG_INFO("Hot reload thread stopped");
}

/**
 * @brief 启用/禁用热重载
 */
void tefkernel_set_hotreload_enabled(const bool enabled) {
    mtx_lock(&g_hotreload_mutex);
    g_hotreload_enabled = enabled;
    mtx_unlock(&g_hotreload_mutex);

    TEKLOG_INFO("Hot reload %s", enabled ? "enabled" : "disabled");
}

/**
 * @brief 初始化运行时包系统
 */
void tefkernel_init(void) {
    TEKLOG_INFO("Initializing package runtime system");

    // 第一步：初始化内核依赖
    tpf_init_libtefkernel();
    TEKLOG_DEBUG("Initialized libtefkernel");

    // 第二步：初始化哈希表
    if (!tefstd_hashmap_init(&plugin_pkgs, sizeof(uint64_t), sizeof(runtime_pkg_entry_t)) ||
        !tefstd_hashmap_init(&modloader_pkgs, sizeof(uint64_t), sizeof(runtime_pkg_entry_t)) ||
        !tefstd_hashmap_init(&module_pkgs, sizeof(uint64_t), sizeof(runtime_pkg_entry_t))) {
        TEKLOG_ERROR("Failed to initialize package hash tables");
        return;
    }

    // 第三步：加载所有包类型
    load_single_pkg_type("modloader", &modloader_pkgs);
    load_single_pkg_type("plugin", &plugin_pkgs);
    load_single_pkg_type("module", &module_pkgs);

    // 统计总数
    const size_t total = tefstd_hashmap_len(&plugin_pkgs) +
                   tefstd_hashmap_len(&modloader_pkgs) +
                   tefstd_hashmap_len(&module_pkgs);

    TEKLOG_INFO("Package initialization complete: %zu total packages", total);
}

/**
 * @brief 加载所有插件
 */
static void load_all_plugins(void) {
    TEKLOG_INFO("Loading all plugins");

    tefstd_hashmap_iter_t iter = tefstd_hashmap_iter(&plugin_pkgs);
    uint64_t key_hash = 0;
    runtime_pkg_entry_t entry = {0};
    int loaded = 0;
    int failed = 0;

    while (tefstd_hashmap_next(&iter, &key_hash, &entry)) {
        if (entry.pkg) {
            uint8_t* dylib = NULL;
            uint32_t dylib_size = 0;
            const tefpkg_result_t result = tefpkg_extract_entry_to_memory(
                entry.pkg, TEFPKG_ID_DYLIB, &dylib, &dylib_size);

            if (result == TEF_OK && dylib && dylib_size > 0) {
                void* dylib_handle = memdl_open(dylib, dylib_size, MEMDL_LAZY);
                if (dylib_handle) {
                    if (tpf_load_plugin(dylib_handle, NULL)) {
                        loaded++;
                        TEKLOG_DEBUG("Loaded plugin from package");
                    } else {
                        failed++;
                        TEKLOG_WARN("Failed to load plugin");
                    }
                } else {
                    failed++;
                    TEKLOG_WARN("Failed to open dynamic library");
                }
            } else {
                failed++;
                TEKLOG_WARN("Failed to extract plugin dynamic library");
            }
        }
    }

    TEKLOG_INFO("Plugins: %d loaded, %d failed", loaded, failed);
}

/**
 * @brief 加载所有ModLoader
 */
static void load_all_modloaders(void) {
    TEKLOG_INFO("Loading all modloaders");

    tefstd_hashmap_iter_t iter = tefstd_hashmap_iter(&modloader_pkgs);
    uint64_t key_hash = 0;
    runtime_pkg_entry_t entry = {0};
    int loaded = 0;
    int failed = 0;

    while (tefstd_hashmap_next(&iter, &key_hash, &entry)) {
        if (entry.pkg) {
            uint8_t* dylib = NULL;
            uint32_t dylib_size = 0;
            const tefpkg_result_t result = tefpkg_extract_entry_to_memory(
                entry.pkg, TEFPKG_ID_DYLIB, &dylib, &dylib_size);

            if (result == TEF_OK && dylib && dylib_size > 0) {
                void* dylib_handle = memdl_open(dylib, dylib_size, MEMDL_LAZY);
                if (dylib_handle) {
                    if (tefkernel_load_ml(dylib_handle, entry.pkg, NULL)) {
                        loaded++;
                        TEKLOG_DEBUG("Loaded modloader from package");
                    } else {
                        failed++;
                        TEKLOG_WARN("Failed to load modloader");
                    }
                } else {
                    failed++;
                    TEKLOG_WARN("Failed to open dynamic library");
                }
            } else {
                failed++;
                TEKLOG_WARN("Failed to extract modloader dynamic library");
            }
        }
    }

    TEKLOG_INFO("ModLoaders: %d loaded, %d failed", loaded, failed);
}

/**
 * @brief 加载所有模块
 */
static void load_all_modules(void) {
    TEKLOG_INFO("Loading all modules");

    tefstd_hashmap_iter_t iter = tefstd_hashmap_iter(&module_pkgs);
    uint64_t key_hash = 0;
    runtime_pkg_entry_t entry = {0};
    int loaded = 0;
    int failed = 0;

    while (tefstd_hashmap_next(&iter, &key_hash, &entry)) {
        if (entry.pkg) {
            uint8_t* dylib = NULL;
            uint32_t dylib_size = 0;
            const tefpkg_result_t result = tefpkg_extract_entry_to_memory(
                entry.pkg, TEFPKG_ID_DYLIB, &dylib, &dylib_size);

            if (result == TEF_OK && dylib && dylib_size > 0) {
                void* dylib_handle = memdl_open(dylib, dylib_size, MEMDL_LAZY);
                if (dylib_handle) {
                    if (tefkernel_load_module(dylib_handle, entry.pkg, NULL)) {
                        loaded++;
                        TEKLOG_DEBUG("Loaded module from package");
                    } else {
                        failed++;
                        TEKLOG_WARN("Failed to load module");
                    }
                } else {
                    failed++;
                    TEKLOG_WARN("Failed to open dynamic library");
                }
            } else {
                failed++;
                TEKLOG_WARN("Failed to extract module dynamic library");
            }
        }
    }

    TEKLOG_INFO("Modules: %d loaded, %d failed", loaded, failed);
}

/**
 * @brief 加载所有Mod
 */
static void load_all_mods(void) {
    TEKLOG_INFO("Loading all mods");

    const int loaded_count = tefkernel_load_all_mods();
    if (loaded_count > 0) {
        TEKLOG_INFO("Loaded %d mods successfully", loaded_count);
    } else {
        TEKLOG_DEBUG("No mods loaded");
    }
}

/**
 * @brief 加载所有组件
 */
void tefkernel_load(void) {
    TEKLOG_INFO("Starting to load all components");

    // 按照要求的顺序：
    // 1. 先加载插件
    // 2. 然后调用tpf_initialize_all_plugins
    load_all_plugins();
    tpf_initialize_all_plugins();

    // 3. 加载ModLoader和模块
    load_all_modloaders();
    load_all_modules();

    // 4. 加载所有Mod
    load_all_mods();

    TEKLOG_INFO("All components loaded");
}

/**
 * @brief 初始化所有Mod
 */
static void init_all_mods(void) {
    TEKLOG_INFO("Initializing all mods");

    const int init_count = tefkernel_init_all_mods();
    if (init_count > 0) {
        TEKLOG_INFO("Initialized %d mods successfully", init_count);
    } else {
        TEKLOG_DEBUG("No mods initialized");
    }
}

/**
 * @brief 启动系统
 */
void tefkernel_start(void) {
    TEKLOG_INFO("Starting kernel runtime");

    // 1. 初始化所有模块
    TEKLOG_DEBUG("Initializing all modules");
    tefkernel_initialize_all_modules();

    // 2. 初始化所有ModLoader
    if (g_ml_list_initialized) {
        const size_t ml_count = tefstd_vector_size(&g_ml_list);
        TEKLOG_INFO("Ensuring %zu modloaders are initialized", ml_count);

        for (size_t i = 0; i < ml_count; ++i) {
            ml_handle_t** ml_ptr = tefstd_vector_at(&g_ml_list, i);
            if (!ml_ptr || !*ml_ptr || !(*ml_ptr)->ml_entry) continue;

            ml_entry_t* entry = (*ml_ptr)->ml_entry;
            if (entry->ops && entry->ops->init_ml) {
                TEKLOG_DEBUG("Initializing modloader %zu", i);
                entry->ops->init_ml(entry);
            }
        }
    }

    // 3. 初始化所有Mod
    init_all_mods();

    TEKLOG_INFO("Kernel runtime started successfully");
}

/**
 * @brief 热重载
 */
void tefkernel_hot_reload(void) {
    TEKLOG_INFO("Manual hot reload triggered");

    // 调用ModLoader的reload_mod函数
    if (g_ml_list_initialized) {
        const size_t ml_count = tefstd_vector_size(&g_ml_list);
        for (size_t i = 0; i < ml_count; ++i) {
            ml_handle_t** ml_ptr = tefstd_vector_at(&g_ml_list, i);
            if (!ml_ptr || !*ml_ptr || !(*ml_ptr)->ml_entry || !(*ml_ptr)->ml_entry->ops) continue;

            const ml_entry_t* entry = (*ml_ptr)->ml_entry;
            if (entry->ops->reload_mod) {
                mod_manifest_t dummy_manifest = {0};
                entry->ops->reload_mod(&dummy_manifest);
            }
        }
    }

    // 调用模块的热重载
    tefkernel_hot_reload_all_modules();

    TEKLOG_INFO("Manual hot reload completed");
}

/**
 * @brief 清理哈希表
 */
static void cleanup_table(tefstd_hashmap_t *table, const char* table_name) {
    TEKLOG_DEBUG("Cleaning up %s table", table_name);

    tefstd_hashmap_iter_t iter = tefstd_hashmap_iter(table);
    uint64_t key_hash = 0;
    runtime_pkg_entry_t entry = {0};
    int closed = 0;

    while (tefstd_hashmap_next(&iter, &key_hash, &entry)) {
        if (entry.pkg) {
            tefpkg_close(entry.pkg);
            closed++;
        }
    }

    tefstd_hashmap_free(table);
    TEKLOG_DEBUG("Cleaned up %s table: %d packages closed", table_name, closed);
}

/**
 * @brief 清理运行时包系统
 */
void tefkernel_cleanup(void) {
    TEKLOG_INFO("Cleaning up package runtime system");

    // 1. 停止热重载线程
    tefkernel_stop_hotreload_thread();

    // 2. 清理所有模块
    tefkernel_cleanup_all_modules();

    // 3. 清理所有ModLoader
    tefkernel_cleanup_all_ml();

    // 4. 清理包哈希表
    cleanup_table(&plugin_pkgs, "plugin");
    cleanup_table(&modloader_pkgs, "modloader");
    cleanup_table(&module_pkgs, "module");

    TEKLOG_INFO("Package runtime cleanup complete");
}
