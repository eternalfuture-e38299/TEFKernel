/*******************************************************************************
 * tefkernel - modloader_core
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

#include <stdlib.h>
#include <string.h>

#include "internal/log.h"
#include "internal/modloader/modloader_core_imp.h"
#include "internal/tefplugin/tef_core_imp.h"
#include "internal/kernel_state.h"
#include "memdl/memdl.h"
#include "tefpackage/tefpkg.h"

tefstd_vector_t g_ml_list; ///< 句柄(ml_handle_t*)
bool g_ml_list_initialized = false; ///< 初始化状态

/**
 * @brief 清理无引用的插件
 * @param ml_info 要卸载的ModLoader的信息
 * @note 这个函数会在ModLoader卸载后调用
 */
static void cleanup_unreferenced_plugins(const ml_info_t* ml_info) {
    if (!ml_info || !ml_info->plugin_dependencies || ml_info->plugin_dependencies_sizes <= 0) {
        TEKLOG_DEBUG("No plugin dependencies to cleanup");
        return;
    }

    TEKLOG_INFO("Checking for unreferenced plugins after modloader %s unload",
                ml_info->pkg_id);

    // 遍历ModLoader的所有依赖
    for (int i = 0; i < ml_info->plugin_dependencies_sizes; ++i) {
        const char* dep_pkg_id = ml_info->plugin_dependencies[i];
        if (!dep_pkg_id) continue;

        TEKLOG_DEBUG("Checking plugin dependency: %s", dep_pkg_id);

        // 检查这个插件是否还被其他ModLoader依赖
        bool still_referenced = false;
        const size_t ml_count = tefstd_vector_size(&g_ml_list);

        for (size_t j = 0; j < ml_count; ++j) {
            ml_handle_t** ml_ptr = tefstd_vector_at(&g_ml_list, j);
            if (!ml_ptr || !*ml_ptr) continue;

            const ml_handle_t* other_ml = *ml_ptr;
            if (!other_ml->ml_entry || !other_ml->ml_entry->info) continue;

            const ml_info_t* other_info = other_ml->ml_entry->info;

            // 检查其他ModLoader是否依赖这个插件
            if (other_info->plugin_dependencies && other_info->plugin_dependencies_sizes > 0) {
                for (int k = 0; k < other_info->plugin_dependencies_sizes; ++k) {
                    if (other_info->plugin_dependencies[k] &&
                        strcmp(other_info->plugin_dependencies[k], dep_pkg_id) == 0) {
                        still_referenced = true;
                        TEKLOG_DEBUG("Plugin %s is still referenced by modloader %s",
                                   dep_pkg_id, other_info->pkg_id);
                        break;
                    }
                }
            }
            if (still_referenced) break;
        }

        // 如果插件不再被任何ModLoader引用，清理它
        if (!still_referenced) {
            plugin_handle_t* plugin = tpf_get_plugin_by_id(dep_pkg_id);
            if (plugin) {
                TEKLOG_INFO("Cleaning up unreferenced plugin: %s (handle: %p)",
                           dep_pkg_id, (void*)plugin);
                tpf_cleanup_plugin(plugin);
            } else {
                TEKLOG_DEBUG("Plugin %s not found or already cleaned up", dep_pkg_id);
            }
        } else {
            TEKLOG_DEBUG("Plugin %s is still referenced, keeping it", dep_pkg_id);
        }
    }
}

/**
 * @brief 内部清理函数
 */
static void free_modloader(ml_handle_t* ml_handle) {
    if (!ml_handle) return;

    TEKLOG_DEBUG("Freeing modloader handle %p (index: %zu)", (void*)ml_handle, ml_handle->index);

    // 在清理之前保存信息用于插件引用检查
    const ml_info_t* ml_info = NULL;
    if (ml_handle->ml_entry) {
        ml_info = ml_handle->ml_entry->info;
    }

    // 清理ModLoader入口
    if (ml_handle->ml_entry) {
        // 注意：不释放info结构，由开发者在其cleanup操作中管理

        // 释放目录字符串
        if (ml_handle->ml_entry->private_dir) {
            free((void*)ml_handle->ml_entry->private_dir);
        }

        if (ml_handle->ml_entry->logs_dir) {
            free((void*)ml_handle->ml_entry->logs_dir);
        }

        // 如果有cleanup_ml操作，调用它
        if (ml_handle->ml_entry->ops && ml_handle->ml_entry->ops->cleanup_ml) {
            ml_handle->ml_entry->ops->cleanup_ml(ml_handle->ml_entry);
        }

        // 释放入口结构
        free(ml_handle->ml_entry);
        ml_handle->ml_entry = NULL;
    }

    // 关闭动态库
    if (ml_handle->handle) {
        TEKLOG_DEBUG("Closing dynamic library handle");
        memdl_close(ml_handle->handle);
        ml_handle->handle = NULL;
    }

    // 关闭包句柄
    // ReSharper disable once CppDFANullDereference
    if (ml_handle->ml_entry->pkg_handle) {
        TEKLOG_DEBUG("Closing package handle");
        // ReSharper disable once CppDFANullDereference
        tefpkg_close(ml_handle->ml_entry->pkg_handle);
        // ReSharper disable once CppDFANullDereference
        ml_handle->ml_entry->pkg_handle = NULL;
    }

    // 释放句柄自身
    free(ml_handle);

    // 清理无引用的插件
    if (ml_info) {
        cleanup_unreferenced_plugins(ml_info);
    }
}

/**
 * @brief 基于pkg_id生成目录路径
 */
static char* generate_private_dir(const char* pkg_id) {
    if (!pkg_id) return NULL;

    const size_t len = snprintf(NULL, 0, "%s/modloader/private/%s", tefkernel_working_dir, pkg_id);
    char* dir = malloc(len + 1);
    if (dir) {
        snprintf(dir, len + 1, "%s/modloader/private/%s", tefkernel_working_dir, pkg_id);
    }
    return dir;
}

static char* generate_logs_dir(const char* pkg_id) {
    if (!pkg_id) return NULL;

    const size_t len = snprintf(NULL, 0, "%s/modloader/logs/%s", tefkernel_working_dir, pkg_id);
    char* dir = malloc(len + 1);
    if (dir) {
        snprintf(dir, len + 1, "%s/modloader/logs/%s", tefkernel_working_dir, pkg_id);
    }
    return dir;
}

/**
 * @brief 加载ModLoader
 */
bool tefkernel_load_ml(void* handle, tefpkg_t* pkg_handle,
                       ml_handle_t** out_ml) {
    if (!handle) {
        TEKLOG_ERROR("Invalid parameters: handle=%p, out_ml=%p", handle, (void*)out_ml);
        return false;
    }

    // 初始化全局列表
    if (!g_ml_list_initialized) {
        TEKLOG_DEBUG("Initializing modloader handles vector");
        if (!tefstd_vector_init(&g_ml_list, sizeof(ml_handle_t*))) {
            TEKLOG_ERROR("Failed to initialize vector");
            return false;
        }
        g_ml_list_initialized = true;
    }

    // 查找ml_create函数
    const ml_ops_t* (*ml_create)() = memdl_sym(handle, "ml_create");
    if (!ml_create) {
        TEKLOG_ERROR("Failed to find ml_create symbol in modloader library");
        TEKLOG_ERROR("memdl_error=%s", memdl_error());
        memdl_close(handle);
        return false;
    }

    // 注册共享库
    tpf_register_shared_plugin_library(handle);

    // 创建ModLoader实例
    const ml_ops_t* ml_ops = ml_create();
    if (!ml_ops) {
        TEKLOG_ERROR("Failed to create modloader instance: ml_create returned NULL");
        memdl_close(handle);
        return false;
    }

    // 检查是否支持get_info函数
    if (!ml_ops->get_info) {
        TEKLOG_ERROR("ModLoader does not support get_info operation");
        memdl_close(handle);
        return false;
    }

    // 获取Mod信息
    const ml_info_t* mod_info = ml_ops->get_info();
    if (!mod_info) {
        TEKLOG_ERROR("Failed to get mod info: get_info returned NULL");
        memdl_close(handle);
        return false;
    }

    if (!mod_info->pkg_id) {
        TEKLOG_ERROR("Mod info has NULL pkg_id");
        memdl_close(handle);
        return false;
    }

    TEKLOG_INFO("Loading modloader: pkg_id=%s, version=%s, api_version=%d",
                mod_info->pkg_id,
                mod_info->version ? mod_info->version : "NULL",
                mod_info->api_version);

    // 分配句柄内存
    ml_handle_t* new_ml = malloc(sizeof(ml_handle_t));
    if (!new_ml) {
        TEKLOG_ERROR("Failed to allocate memory for modloader handle");
        memdl_close(handle);
        return false;
    }
    memset(new_ml, 0, sizeof(ml_handle_t));

    // 分配入口内存
    ml_entry_t* new_ml_entry = malloc(sizeof(ml_entry_t));
    if (!new_ml_entry) {
        TEKLOG_ERROR("Failed to allocate memory for modloader entry");
        free(new_ml);
        memdl_close(handle);
        return false;
    }
    memset(new_ml_entry, 0, sizeof(ml_entry_t));

    // 注意：这里不再深拷贝ml_info，直接使用指针
    new_ml_entry->info = (ml_info_t*)mod_info;  // 去掉const限定符

    // 生成目录路径
    new_ml_entry->private_dir = generate_private_dir(mod_info->pkg_id);
    if (!new_ml_entry->private_dir) {
        TEKLOG_ERROR("Failed to generate private directory for pkg_id: %s", mod_info->pkg_id);
        free(new_ml_entry);
        free(new_ml);
        memdl_close(handle);
        return false;
    }

    new_ml_entry->logs_dir = generate_logs_dir(mod_info->pkg_id);
    if (!new_ml_entry->logs_dir) {
        TEKLOG_ERROR("Failed to generate logs directory for pkg_id: %s", mod_info->pkg_id);
        if (new_ml_entry->private_dir) free((void*)new_ml_entry->private_dir);
        free(new_ml_entry);
        free(new_ml);
        memdl_close(handle);
        return false;
    }

    // 初始化ModLoader句柄
    new_ml->ml_entry = new_ml_entry;
    new_ml->handle = handle;
    new_ml->ml_entry->pkg_handle = pkg_handle;
    new_ml->index = tefstd_vector_size(&g_ml_list);

    // 设置操作函数表
    new_ml_entry->ops = (ml_ops_t*)ml_ops;  // 去掉const限定符
    new_ml_entry->pkg_handle = pkg_handle;

    // 添加到全局列表
    if (!tefstd_vector_push_back(&g_ml_list, &new_ml)) {
        TEKLOG_ERROR("Failed to add modloader to global list");
        if (new_ml_entry->private_dir) free((void*)new_ml_entry->private_dir);
        if (new_ml_entry->logs_dir) free((void*)new_ml_entry->logs_dir);
        free(new_ml_entry);
        free(new_ml);
        memdl_close(handle);
        return false;
    }

    // 如果有init_ml操作，调用它
    if (new_ml_entry->ops->init_ml) {
        const ml_result_t result = new_ml_entry->ops->init_ml(new_ml_entry);
        if (result != ML_SUCCESS) {
            TEKLOG_ERROR("Failed to initialize modloader: result=%d", result);
            // 从列表中移除
            tefstd_vector_pop_back(&g_ml_list, NULL);
            if (new_ml_entry->private_dir) free((void*)new_ml_entry->private_dir);
            if (new_ml_entry->logs_dir) free((void*)new_ml_entry->logs_dir);
            free(new_ml_entry);
            free(new_ml);
            memdl_close(handle);
            return false;
        }
    }

    if (out_ml) *out_ml = new_ml;
    TEKLOG_INFO("Successfully loaded modloader: pkg_id=%s, index=%zu, handle=%p",
                mod_info->pkg_id, new_ml->index, (void*)new_ml);
    return true;
}

/**
 * @brief 卸载单个ModLoader
 */
void tefkernel_cleanup_ml(ml_handle_t* ml_handle) {
    if (!ml_handle) {
        TEKLOG_WARN("Attempted to cleanup NULL modloader handle");
        return;
    }

    TEKLOG_INFO("Cleaning up modloader %p (index: %zu, pkg_id: %s)",
                (void*)ml_handle, ml_handle->index,
                ml_handle->ml_entry && ml_handle->ml_entry->info ?
                ml_handle->ml_entry->info->pkg_id : "NULL");

    if (!g_ml_list_initialized) {
        TEKLOG_DEBUG("Modloader list not initialized, directly freeing handle");
        free_modloader(ml_handle);
        return;
    }

    // 从全局列表中查找并移除
    bool found = false;
    const size_t ml_count = tefstd_vector_size(&g_ml_list);
    TEKLOG_DEBUG("Searching for modloader in global list of %zu entries", ml_count);

    for (size_t i = 0; i < ml_count; ++i) {
        ml_handle_t** ml_ptr = (ml_handle_t**)tefstd_vector_at(&g_ml_list, i);
        if (ml_ptr && *ml_ptr == ml_handle) {
            TEKLOG_DEBUG("Removing modloader from global list at index %zu", i);

            // 移除元素
            if (!tefstd_vector_erase(&g_ml_list, i, NULL)) {
                TEKLOG_ERROR("Failed to remove modloader from vector at index %zu", i);
            } else {
                found = true;
            }

            // 更新后续元素的索引
            for (size_t j = i; j < tefstd_vector_size(&g_ml_list); ++j) {
                ml_handle_t** subsequent_ml = tefstd_vector_at(&g_ml_list, j);
                if (subsequent_ml && *subsequent_ml) {
                    (*subsequent_ml)->index = j;
                }
            }
            break;
        }
    }

    if (!found) {
        TEKLOG_WARN("Modloader handle %p not found in global list", (void*)ml_handle);
    }

    // 执行实际的清理工作
    free_modloader(ml_handle);
    TEKLOG_INFO("Modloader cleanup completed successfully");
}

/**
 * @brief 卸载所有ModLoader
 */
void tefkernel_cleanup_all_ml(void) {
    if (!g_ml_list_initialized) {
        TEKLOG_DEBUG("Modloader list not initialized, nothing to cleanup");
        return;
    }

    const size_t ml_count = tefstd_vector_size(&g_ml_list);
    TEKLOG_INFO("Cleaning up all %zu modloaders", ml_count);

    // 首先收集所有插件依赖
    tefstd_vector_t all_plugin_deps;
    tefstd_vector_init(&all_plugin_deps, sizeof(const char*));

    for (size_t i = 0; i < ml_count; ++i) {
        ml_handle_t** ml_ptr = (ml_handle_t**)tefstd_vector_at(&g_ml_list, i);
        if (!ml_ptr || !*ml_ptr || !(*ml_ptr)->ml_entry || !(*ml_ptr)->ml_entry->info) continue;

        const ml_info_t* info = (*ml_ptr)->ml_entry->info;
        if (info->plugin_dependencies && info->plugin_dependencies_sizes > 0) {
            for (int j = 0; j < info->plugin_dependencies_sizes; ++j) {
                if (info->plugin_dependencies[j]) {
                    tefstd_vector_push_back(&all_plugin_deps, &info->plugin_dependencies[j]);
                }
            }
        }
    }

    // 反向遍历清理ModLoader
    for (size_t i = ml_count; i > 0; --i) {
        ml_handle_t** ml_ptr = (ml_handle_t**)tefstd_vector_at(&g_ml_list, i - 1);
        if (ml_ptr && *ml_ptr) {
            TEKLOG_DEBUG("Cleaning up modloader at index %zu", i - 1);
            ml_handle_t* ml_handle = *ml_ptr;

            // 从向量中移除
            if (!tefstd_vector_erase(&g_ml_list, i - 1, NULL)) {
                TEKLOG_ERROR("Failed to remove modloader from vector at index %zu", i - 1);
            }

            // 清理资源
            free_modloader(ml_handle);
        }
    }

    // 清空向量
    tefstd_vector_clear(&g_ml_list);

    // 清理所有不再被引用的插件
    const size_t deps_count = tefstd_vector_size(&all_plugin_deps);
    for (size_t i = 0; i < deps_count; ++i) {
        const char** dep_ptr = tefstd_vector_at(&all_plugin_deps, i);
        if (dep_ptr && *dep_ptr) {
            plugin_handle_t* plugin = tpf_get_plugin_by_id(*dep_ptr);
            if (plugin) {
                TEKLOG_INFO("Cleaning up plugin after all modloaders unloaded: %s", *dep_ptr);
                tpf_cleanup_plugin(plugin);
            }
        }
    }

    tefstd_vector_destroy(&all_plugin_deps);

    TEKLOG_INFO("All modloaders and their plugins cleaned up successfully");
}

/**
 * @brief 通过索引获取ModLoader
 */
ml_handle_t* tefkernel_get_ml_by_index(const size_t index) {
    if (!g_ml_list_initialized) {
        TEKLOG_DEBUG("Modloader list not initialized");
        return NULL;
    }

    if (index >= tefstd_vector_size(&g_ml_list)) {
        TEKLOG_WARN("Modloader index %zu out of bounds (total: %zu)",
                   index, tefstd_vector_size(&g_ml_list));
        return NULL;
    }

    ml_handle_t** ml_ptr = tefstd_vector_at(&g_ml_list, index);
    if (!ml_ptr) {
        TEKLOG_ERROR("Failed to get modloader at index %zu", index);
        return NULL;
    }

    TEKLOG_DEBUG("Retrieved modloader %p at index %zu", (void*)(*ml_ptr), index);
    return *ml_ptr;
}

/**
 * @brief 获取ModLoader数量
 */
size_t tefkernel_get_ml_count(void) {
    if (!g_ml_list_initialized) {
        return 0;
    }

    const size_t count = tefstd_vector_size(&g_ml_list);
    TEKLOG_DEBUG("Current modloader count: %zu", count);
    return count;
}

/**
 * @brief 获取ModLoader的info
 */
const ml_info_t* tefkernel_get_ml_info(ml_handle_t* ml_handle) {
    if (!ml_handle || !ml_handle->ml_entry) {
        TEKLOG_ERROR("Invalid modloader handle or entry");
        return NULL;
    }

    return ml_handle->ml_entry->info;
}

/**
 * @brief 执行Mod加载操作
 */
ml_result_t tefkernel_ml_load_mod(ml_handle_t* ml_handle, mod_manifest_t* manifest) {
    if (!ml_handle || !ml_handle->ml_entry || !ml_handle->ml_entry->ops) {
        TEKLOG_ERROR("Invalid modloader handle or entry");
        return ML_ERROR;
    }

    if (!manifest) {
        TEKLOG_ERROR("Invalid mod manifest provided");
        return ML_ERROR;
    }

    const ml_ops_t* ops = ml_handle->ml_entry->ops;
    if (!ops->load_mod) {
        TEKLOG_ERROR("ModLoader does not support load_mod operation");
        return ML_ERROR;
    }

    TEKLOG_INFO("Loading mod: %s (path: %s)",
                manifest->mod_id ? manifest->mod_id : "NULL",
                manifest->path ? manifest->path : "NULL");

    return ops->load_mod(manifest);
}
