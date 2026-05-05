/*******************************************************************************
 * tefkernel - mod_core
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
 * Created: 2026/3/14
 *******************************************************************************/

#include "internal/mod_core.h"
#include "internal/log.h"
#include "internal/kernel_state.h"
#include "internal/modloader/modloader_core_imp.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// 全局Mod列表
tefstd_vector_t g_mod_list;  ///< mod_handle_t*
bool g_mod_list_initialized = false; ///< 初始化状态

/**
 * @brief 获取ModLoader启用的Mod列表
 */
int tefkernel_get_enabled_mods_for_ml(const char* ml_pkg_id, tefstd_vector_t* enabled_mods) {
    if (!ml_pkg_id || !enabled_mods) {
        TEKLOG_ERROR("Invalid parameters: ml_pkg_id=%p, enabled_mods=%p",
                    (void*)ml_pkg_id, (void*)enabled_mods);
        return -1;
    }

    // 先初始化向量
    if (!tefstd_vector_init(enabled_mods, sizeof(char*))) {
        TEKLOG_ERROR("Failed to initialize vector for enabled mods");
        return -1;
    }

    // 构建enables.txt文件路径
    char enable_path[1024];
    snprintf(enable_path, sizeof(enable_path), "%s/mods/%s/enables.txt",
             tefkernel_working_dir, ml_pkg_id);

    TEKLOG_DEBUG("Checking enabled mods file: %s", enable_path);

    FILE* file = fopen(enable_path, "r");
    if (!file) {
        // 文件不存在，返回空向量
        TEKLOG_INFO("Enabled mods file not found: %s", enable_path);
        TEKLOG_DEBUG("No enabled mods for modloader: %s (file not found)", ml_pkg_id);
        return 0;
    }

    TEKLOG_INFO("Reading enabled mods from: %s", enable_path);

    int count = 0;
    char line[256];

    while (fgets(line, sizeof(line), file)) {
        // 清理行尾
        line[strcspn(line, "\r\n")] = '\0';

        // 跳过空行和注释
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
            TEKLOG_DEBUG("Skipping line: '%s' (empty or comment)", line);
            continue;
        }

        // 复制字符串
        char* mod_id_copy = strdup(line);
        if (!mod_id_copy) {
            TEKLOG_WARN("Failed to allocate memory for mod id: %s", line);
            continue;
        }

        if (tefstd_vector_push_back(enabled_mods, &mod_id_copy))
            count++;
        else
            free(mod_id_copy);
    }

    fclose(file);

    if (count > 0) {
        TEKLOG_INFO("Found %d enabled mod(s) for modloader: %s", count, ml_pkg_id);
    } else {
        TEKLOG_INFO("No enabled mods found for modloader: %s (empty file)", ml_pkg_id);
    }

    return count;
}

/**
 * @brief 创建Mod清单
 */
static mod_manifest_t* create_mod_manifest(const char* mod_path, const char* mod_id, ml_handle_t* owner_ml) {
    if (!mod_path || !mod_id || !owner_ml || !owner_ml->ml_entry) {
        TEKLOG_ERROR("Invalid parameters for creating mod manifest");
        return NULL;
    }

    mod_manifest_t* manifest = malloc(sizeof(mod_manifest_t));
    if (!manifest) {
        TEKLOG_ERROR("Failed to allocate memory for mod manifest");
        return NULL;
    }
    memset(manifest, 0, sizeof(mod_manifest_t));

    // 复制路径
    manifest->path = strdup(mod_path);
    if (!manifest->path) {
        TEKLOG_ERROR("Failed to duplicate mod path");
        free(manifest);
        return NULL;
    }

    // 复制mod_id
    manifest->mod_id = strdup(mod_id);
    if (!manifest->mod_id) {
        TEKLOG_ERROR("Failed to duplicate mod id");
        free((void*)manifest->path);
        free(manifest);
        return NULL;
    }

    // 获取ModLoader的包名
    const char* ml_pkg_id = owner_ml->ml_entry->info ? owner_ml->ml_entry->info->pkg_id : NULL;
    if (!ml_pkg_id) {
        TEKLOG_ERROR("ModLoader has no pkg_id");
        free((void*)manifest->path);
        free((void*)manifest->mod_id);
        free(manifest);
        return NULL;
    }

    // 构建私有目录路径: 工作目录/mods/<modloader包名>/private/<mod包名>
    char private_dir_path[1024];
    snprintf(private_dir_path, sizeof(private_dir_path), "%s/mods/%s/private/%s",
             tefkernel_working_dir, ml_pkg_id, mod_id);

    // 构建日志目录路径: 工作目录/mods/<modloader包名>/logs/<mod包名>
    char logs_dir_path[1024];
    snprintf(logs_dir_path, sizeof(logs_dir_path), "%s/mods/%s/logs/%s",
             tefkernel_working_dir, ml_pkg_id, mod_id);

    // 分配并复制目录路径字符串
    manifest->private_dir = strdup(private_dir_path);
    if (!manifest->private_dir) {
        TEKLOG_ERROR("Failed to duplicate private directory path");
        free((void*)manifest->path);
        free((void*)manifest->mod_id);
        free(manifest);
        return NULL;
    }

    manifest->logs_dir = strdup(logs_dir_path);
    if (!manifest->logs_dir) {
        TEKLOG_ERROR("Failed to duplicate logs directory path");
        free((void*)manifest->path);
        free((void*)manifest->mod_id);
        free((void*)manifest->private_dir);
        free(manifest);
        return NULL;
    }

    return manifest;
}

/**
 * @brief 释放Mod清单
 */
static void free_mod_manifest(mod_manifest_t* manifest) {
    if (!manifest) return;

    if (manifest->path) {
        free((void*)manifest->path);
    }
    if (manifest->mod_id) {
        free((void*)manifest->mod_id);
    }
    if (manifest->private_dir) {
        free((void*)manifest->private_dir);
    }
    if (manifest->logs_dir) {
        free((void*)manifest->logs_dir);
    }

    free(manifest);
}

/**
 * @brief 释放Mod句柄
 */
static void free_mod_handle(mod_handle_t* mod_handle) {
    if (!mod_handle) return;

    if (mod_handle->mod_id) {
        free((void*)mod_handle->mod_id);
    }
    if (mod_handle->mod_path) {
        free((void*)mod_handle->mod_path);
    }
    if (mod_handle->manifest) {
        free_mod_manifest(mod_handle->manifest);
    }
    // 注意：owner_ml 只是引用，不应该在这里释放

    free(mod_handle);
}

/**
 * @brief 为特定ModLoader加载其启用的Mod
 */
static int load_mods_for_ml(ml_handle_t* ml_handle) {
    if (!ml_handle || !ml_handle->ml_entry || !ml_handle->ml_entry->info) {
        TEKLOG_ERROR("Invalid modloader handle");
        return 0;
    }

    const char* ml_pkg_id = ml_handle->ml_entry->info->pkg_id;
    if (!ml_pkg_id) {
        TEKLOG_ERROR("ModLoader has no pkg_id");
        return 0;
    }

    // 获取这个ModLoader启用的Mod列表
    tefstd_vector_t enabled_mods;
    const int enabled_count = tefkernel_get_enabled_mods_for_ml(ml_pkg_id, &enabled_mods);
    if (enabled_count <= 0) {
        // 只有向量被初始化时才销毁
        if (enabled_mods.data) {
            tefstd_vector_destroy(&enabled_mods);
        }
        return 0;
    }

    int loaded_count = 0;

    // 检查ModLoader是否有load_mod函数
    if (!ml_handle->ml_entry->ops || !ml_handle->ml_entry->ops->load_mod) {
        // 清理启用的Mod列表
        for (size_t i = 0; i < tefstd_vector_size(&enabled_mods); i++) {
            char** mod_id_ptr = tefstd_vector_at(&enabled_mods, i);
            if (mod_id_ptr && *mod_id_ptr) {
                free(*mod_id_ptr);
            }
        }
        tefstd_vector_destroy(&enabled_mods);
        return 0;
    }

    // 遍历所有启用的Mod
    for (size_t i = 0; i < tefstd_vector_size(&enabled_mods); i++) {
        char** mod_id_ptr = tefstd_vector_at(&enabled_mods, i);
        if (!mod_id_ptr || !*mod_id_ptr) continue;

        const char* mod_id = *mod_id_ptr;

        // 构建Mod文件路径
        char mod_path[1024];
        snprintf(mod_path, sizeof(mod_path), "%s/mods/%s/mod/%s",
                 tefkernel_working_dir, ml_pkg_id, mod_id);

        // 创建Mod清单
        mod_manifest_t* manifest = create_mod_manifest(mod_path, mod_id, ml_handle);
        if (!manifest) {
            continue;
        }

        // 调用ModLoader的load_mod函数
        const ml_result_t load_result = ml_handle->ml_entry->ops->load_mod(manifest);

        if (load_result == ML_SUCCESS) {
            // 创建Mod句柄
            mod_handle_t* new_mod = malloc(sizeof(mod_handle_t));
            if (!new_mod) {
                free_mod_manifest(manifest);
                continue;
            }

            memset(new_mod, 0, sizeof(mod_handle_t));
            new_mod->mod_id = strdup(mod_id);
            new_mod->mod_path = strdup(mod_path);
            new_mod->manifest = manifest;
            new_mod->owner_ml = ml_handle;
            new_mod->state = MOD_STATE_LOADED;
            new_mod->load_time = time(NULL);

            if (!new_mod->mod_id || !new_mod->mod_path) {
                free_mod_handle(new_mod);
                free_mod_manifest(manifest);
                continue;
            }

            // 添加到全局列表
            if (!tefstd_vector_push_back(&g_mod_list, &new_mod)) {
                free_mod_handle(new_mod);
                free_mod_manifest(manifest);
                continue;
            }

            loaded_count++;
        } else {
            free_mod_manifest(manifest);
        }
    }

    // 清理启用的Mod列表
    for (size_t i = 0; i < tefstd_vector_size(&enabled_mods); i++) {
        char** mod_id_ptr = tefstd_vector_at(&enabled_mods, i);
        if (mod_id_ptr && *mod_id_ptr) {
            free(*mod_id_ptr);
        }
    }
    tefstd_vector_destroy(&enabled_mods);

    return loaded_count;
}

/**
 * @brief 加载所有Mod
 */
int tefkernel_load_all_mods(void) {
    // 初始化全局Mod列表
    if (!g_mod_list_initialized) {
        if (!tefstd_vector_init(&g_mod_list, sizeof(mod_handle_t*))) {
            TEKLOG_ERROR("Failed to initialize global mod list");
            return 0;
        }
        g_mod_list_initialized = true;
    } else {
        // 如果已经初始化，先清理之前加载的Mod
        tefkernel_cleanup_all_mods();
    }

    // 检查是否有已加载的ModLoader
    if (!g_ml_list_initialized) {
        return 0;
    }

    const size_t ml_count = tefstd_vector_size(&g_ml_list);
    if (ml_count == 0) {
        return 0;
    }

    int total_loaded = 0;

    // 为每个ModLoader加载其启用的Mod
    for (size_t i = 0; i < ml_count; ++i) {
        ml_handle_t** ml_ptr = tefstd_vector_at(&g_ml_list, i);
        if (!ml_ptr || !*ml_ptr || !(*ml_ptr)->ml_entry || !(*ml_ptr)->ml_entry->info) {
            continue;
        }

        ml_handle_t* ml_handle = *ml_ptr;
        int loaded_for_ml = load_mods_for_ml(ml_handle);
        total_loaded += loaded_for_ml;
    }

    return total_loaded;
}

/**
 * @brief 初始化所有已加载的Mod
 */
int tefkernel_init_all_mods(void) {
    if (!g_mod_list_initialized) {
        return 0;
    }

    const size_t mod_count = tefstd_vector_size(&g_mod_list);
    if (mod_count == 0) {
        return 0;
    }

    int initialized_count = 0;

    for (size_t i = 0; i < mod_count; ++i) {
        mod_handle_t** mod_ptr = tefstd_vector_at(&g_mod_list, i);
        if (!mod_ptr || !*mod_ptr) {
            continue;
        }

        mod_handle_t* mod_handle = *mod_ptr;

        // 检查状态
        if (mod_handle->state != MOD_STATE_LOADED) {
            continue;
        }

        // 检查ModLoader和操作函数
        if (!mod_handle->owner_ml || !mod_handle->owner_ml->ml_entry ||
            !mod_handle->owner_ml->ml_entry->ops) {
            mod_handle->state = MOD_STATE_ERROR;
            continue;
        }

        if (!mod_handle->owner_ml->ml_entry->ops->init_mod) {
            mod_handle->state = MOD_STATE_INITIALIZED; // 没有初始化函数，算作已初始化
            initialized_count++;
            continue;
        }

        // 调用ModLoader的init_mod函数
        ml_result_t init_result = mod_handle->owner_ml->ml_entry->ops->init_mod(mod_handle->manifest);

        if (init_result == ML_SUCCESS) {
            mod_handle->state = MOD_STATE_INITIALIZED;
            initialized_count++;
        } else {
            mod_handle->state = MOD_STATE_ERROR;
        }
    }

    return initialized_count;
}

/**
 * @brief 卸载所有Mod
 */
int tefkernel_cleanup_all_mods(void) {
    if (!g_mod_list_initialized) {
        return 0;
    }

    const size_t mod_count = tefstd_vector_size(&g_mod_list);
    if (mod_count == 0) {
        return 0;
    }

    int unloaded_count = 0;

    // 反向遍历清理Mod
    for (size_t i = mod_count; i > 0; --i) {
        mod_handle_t** mod_ptr = tefstd_vector_at(&g_mod_list, i - 1);
        if (!mod_ptr || !*mod_ptr) {
            continue;
        }

        mod_handle_t* mod_handle = *mod_ptr;

        // 检查是否需要卸载
        if (mod_handle->state != MOD_STATE_UNLOADED) {
            // 检查ModLoader是否有unload_mod函数
            if (mod_handle->owner_ml && mod_handle->owner_ml->ml_entry &&
                mod_handle->owner_ml->ml_entry->ops &&
                mod_handle->owner_ml->ml_entry->ops->unload_mod) {

                // 调用ModLoader的unload_mod函数
                mod_handle->owner_ml->ml_entry->ops->unload_mod(mod_handle->manifest);
            }
        }

        // 从列表中移除
        tefstd_vector_erase(&g_mod_list, i - 1, NULL);

        // 释放资源
        free_mod_handle(mod_handle);
        unloaded_count++;
    }

    // 清空向量但不释放，以便重用
    tefstd_vector_clear(&g_mod_list);

    return unloaded_count;
}

/**
 * @brief 获取Mod数量
 */
size_t tefkernel_get_mod_count(void) {
    if (!g_mod_list_initialized) {
        return 0;
    }
    return tefstd_vector_size(&g_mod_list);
}

/**
 * @brief 通过索引获取Mod句柄
 */
mod_handle_t* tefkernel_get_mod_by_index(size_t index) {
    if (!g_mod_list_initialized) {
        return NULL;
    }

    if (index >= tefstd_vector_size(&g_mod_list)) {
        return NULL;
    }

    mod_handle_t** mod_ptr = tefstd_vector_at(&g_mod_list, index);
    if (!mod_ptr) {
        return NULL;
    }

    return *mod_ptr;
}
