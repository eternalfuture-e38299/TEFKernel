/*******************************************************************************
 * tefkernel - module_core
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

#include "internal/module/module_core_imp.h"
#include "internal/log.h"
#include "internal/kernel_state.h"
#include "memdl/memdl.h"
#include "tefpackage/tefpkg.h"
#include "internal/tefplugin/tef_core_imp.h"
#include "internal/modloader/modloader_core_imp.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tefstd/hashmap.h"

// 全局模块列表
tefstd_vector_t g_module_list; ///< 句柄(module_handle_t*)
bool g_module_list_initialized = false; ///< 初始化状态

/**
 * @brief 初始化插件引用计数系统
 */
static void init_plugin_refs(void) {
    if (!g_plugin_refs_initialized) {
        tefstd_hashmap_init(&g_plugin_refs, sizeof(const char*), sizeof(plugin_ref_entry_t));
        g_plugin_refs_initialized = true;
    }
}

/**
 * @brief 增加插件引用计数
 */
static void add_plugin_ref(const char* pkg_id) {
    if (!pkg_id) return;

    init_plugin_refs();

    plugin_ref_entry_t* entry = tefstd_hashmap_get(&g_plugin_refs, &pkg_id);
    if (entry) {
        entry->ref_count++;
        TEKLOG_DEBUG("Plugin %s ref count increased to %d", pkg_id, entry->ref_count);
    } else {
        const plugin_ref_entry_t new_entry = {
            .pkg_id = pkg_id,
            .ref_count = 1
        };
        tefstd_hashmap_put(&g_plugin_refs, &pkg_id, &new_entry);
        TEKLOG_DEBUG("Plugin %s ref count initialized to 1", pkg_id);
    }
}

/**
 * @brief 减少插件引用计数
 */
static void remove_plugin_ref(const char* pkg_id) {
    if (!pkg_id) return;

    if (!g_plugin_refs_initialized) {
        TEKLOG_DEBUG("Plugin refs not initialized, skipping remove for %s", pkg_id);
        return;
    }

    plugin_ref_entry_t* entry = tefstd_hashmap_get(&g_plugin_refs, &pkg_id);
    if (entry) {
        entry->ref_count--;
        TEKLOG_DEBUG("Plugin %s ref count decreased to %d", pkg_id, entry->ref_count);

        if (entry->ref_count <= 0) {
            tefstd_hashmap_del(&g_plugin_refs, &pkg_id);
            TEKLOG_DEBUG("Plugin %s removed from ref tracking (no references)", pkg_id);
        }
    } else {
        TEKLOG_WARN("Plugin %s not found in ref tracking", pkg_id);
    }
}

/**
 * @brief 获取插件引用计数
 */
static int get_plugin_ref_count(const char* pkg_id) {
    if (!pkg_id || !g_plugin_refs_initialized) return 0;

    const plugin_ref_entry_t* entry = tefstd_hashmap_get(&g_plugin_refs, &pkg_id);
    return entry ? entry->ref_count : 0;
}

/**
 * @brief 检查插件是否还有其他引用
 */
bool module_check_plugin_references(const char* pkg_id) {
    int ref_count = get_plugin_ref_count(pkg_id);

    // 检查其他模块的引用
    if (g_module_list_initialized) {
        const size_t module_count = tefstd_vector_size(&g_module_list);
        for (size_t i = 0; i < module_count; ++i) {
            module_handle_t** module_ptr = tefstd_vector_at(&g_module_list, i);
            if (!module_ptr || !*module_ptr || !(*module_ptr)->module_entry || !(*module_ptr)->module_entry->info) continue;

            const module_info_t* info = (*module_ptr)->module_entry->info;
            if (info->plugin_dependencies && info->plugin_dependencies_sizes > 0) {
                for (int j = 0; j < info->plugin_dependencies_sizes; ++j) {
                    if (info->plugin_dependencies[j] && strcmp(info->plugin_dependencies[j], pkg_id) == 0) {
                        ref_count++;
                        break;
                    }
                }
            }
        }
    }

    // 检查ModLoader的引用
    if (g_ml_list_initialized) {
        const size_t ml_count = tefstd_vector_size(&g_ml_list);
        for (size_t i = 0; i < ml_count; ++i) {
            ml_handle_t** ml_ptr = tefstd_vector_at(&g_ml_list, i);
            if (!ml_ptr || !*ml_ptr || !(*ml_ptr)->ml_entry || !(*ml_ptr)->ml_entry->info) continue;

            const ml_info_t* info = (*ml_ptr)->ml_entry->info;
            if (info->plugin_dependencies && info->plugin_dependencies_sizes > 0) {
                for (int j = 0; j < info->plugin_dependencies_sizes; ++j) {
                    if (info->plugin_dependencies[j] && strcmp(info->plugin_dependencies[j], pkg_id) == 0) {
                        ref_count++;
                        break;
                    }
                }
            }
        }
    }

    TEKLOG_DEBUG("Plugin %s has %d total references", pkg_id, ref_count);
    return ref_count > 0;
}

/**
 * @brief 清理无引用的插件
 */
static void cleanup_unreferenced_plugins(const module_info_t* module_info) {
    if (!module_info || !module_info->plugin_dependencies || module_info->plugin_dependencies_sizes <= 0) {
        TEKLOG_DEBUG("No plugin dependencies to cleanup");
        return;
    }

    TEKLOG_INFO("Checking for unreferenced plugins after module %s unload", module_info->pkg_id);

    for (int i = 0; i < module_info->plugin_dependencies_sizes; ++i) {
        const char* dep_pkg_id = module_info->plugin_dependencies[i];
        if (!dep_pkg_id) continue;

        TEKLOG_DEBUG("Checking plugin dependency: %s", dep_pkg_id);

        // 检查这个插件是否还被其他模块或ModLoader引用
        const bool still_referenced = module_check_plugin_references(dep_pkg_id);

        // 如果插件不再被任何模块或ModLoader引用，清理它
        if (!still_referenced) {
            plugin_handle_t* plugin = tpf_get_plugin_by_id(dep_pkg_id);
            if (plugin) {
                TEKLOG_INFO("Cleaning up unreferenced plugin: %s (handle: %p)",
                           dep_pkg_id, (void*)plugin);
                tpf_cleanup_plugin(plugin);

                // 从引用计数中移除
                remove_plugin_ref(dep_pkg_id);
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
static void free_module(module_handle_t* module_handle) {
    if (!module_handle) return;

    TEKLOG_DEBUG("Freeing module handle %p (index: %zu)", (void*)module_handle, module_handle->index);

    // 在清理之前保存信息用于插件引用检查
    const module_info_t* module_info = NULL;
    if (module_handle->module_entry) {
        module_info = module_handle->module_entry->info;
    }

    // 清理模块入口
    if (module_handle->module_entry) {
        // 注意：不释放info结构，由开发者在其cleanup操作中管理

        // 释放目录字符串
        if (module_handle->module_entry->private_dir) {
            free((void*)module_handle->module_entry->private_dir);
        }

        if (module_handle->module_entry->logs_dir) {
            free((void*)module_handle->module_entry->logs_dir);
        }

        // 如果有cleanup_module操作，调用它
        if (module_handle->module_entry->ops && module_handle->module_entry->ops->cleanup_module) {
            module_handle->module_entry->ops->cleanup_module(module_handle->module_entry);
        }

        // 释放入口结构
        free(module_handle->module_entry);
        module_handle->module_entry = NULL;
    }

    // 关闭动态库
    if (module_handle->handle) {
        TEKLOG_DEBUG("Closing dynamic library handle");
        memdl_close(module_handle->handle);
        module_handle->handle = NULL;
    }

    // 关闭包句柄
    if (module_handle->module_entry && module_handle->module_entry->pkg_handle) {
        TEKLOG_DEBUG("Closing package handle");
        tefpkg_close(module_handle->module_entry->pkg_handle);
        module_handle->module_entry->pkg_handle = NULL;
    }

    // 释放句柄自身
    free(module_handle);

    // 清理无引用的插件
    if (module_info) {
        cleanup_unreferenced_plugins(module_info);
    }
}

/**
 * @brief 基于pkg_id生成目录路径
 */
static char* generate_module_private_dir(const char* pkg_id) {
    if (!pkg_id) return NULL;

    const size_t len = snprintf(NULL, 0, "%s/module/private/%s", tefkernel_working_dir, pkg_id);
    char* dir = malloc(len + 1);
    if (dir) {
        snprintf(dir, len + 1, "%s/module/private/%s", tefkernel_working_dir, pkg_id);
    }
    return dir;
}

static char* generate_module_logs_dir(const char* pkg_id) {
    if (!pkg_id) return NULL;

    const size_t len = snprintf(NULL, 0, "%s/module/logs/%s", tefkernel_working_dir, pkg_id);
    char* dir = malloc(len + 1);
    if (dir) {
        snprintf(dir, len + 1, "%s/module/logs/%s", tefkernel_working_dir, pkg_id);
    }
    return dir;
}

/**
 * @brief 加载模块
 */
bool tefkernel_load_module(void* handle, tefpkg_t* pkg_handle, module_handle_t** out_module) {
    if (!handle) {
        TEKLOG_ERROR("Invalid parameters: handle=%p, out_module=%p", handle, (void*)out_module);
        return false;
    }

    // 初始化全局列表
    if (!g_module_list_initialized) {
        TEKLOG_DEBUG("Initializing module handles vector");
        if (!tefstd_vector_init(&g_module_list, sizeof(module_handle_t*))) {
            TEKLOG_ERROR("Failed to initialize vector");
            return false;
        }
        g_module_list_initialized = true;
    }

    // 查找module_create函数
    const module_ops_t* (*module_create)() = memdl_sym(handle, "module_create");
    if (!module_create) {
        TEKLOG_ERROR("Failed to find module_create symbol in module library");
        memdl_close(handle);
        return false;
    }

    // 创建模块实例
    const module_ops_t* module_ops = module_create();
    if (!module_ops) {
        TEKLOG_ERROR("Failed to create module instance: module_create returned NULL");
        memdl_close(handle);
        return false;
    }

    // 检查是否支持get_info函数
    if (!module_ops->get_info) {
        TEKLOG_ERROR("Module does not support get_info operation");
        memdl_close(handle);
        return false;
    }

    // 获取模块信息
    const module_info_t* mod_info = module_ops->get_info();
    if (!mod_info) {
        TEKLOG_ERROR("Failed to get module info: get_info returned NULL");
        memdl_close(handle);
        return false;
    }

    if (!mod_info->pkg_id) {
        TEKLOG_ERROR("Module info has NULL pkg_id");
        memdl_close(handle);
        return false;
    }

    TEKLOG_INFO("Loading module: pkg_id=%s, version=%s, api_version=%d",
                mod_info->pkg_id,
                mod_info->version ? mod_info->version : "NULL",
                mod_info->api_version);

    // 分配句柄内存
    module_handle_t* new_module = malloc(sizeof(module_handle_t));
    if (!new_module) {
        TEKLOG_ERROR("Failed to allocate memory for module handle");
        memdl_close(handle);
        return false;
    }
    memset(new_module, 0, sizeof(module_handle_t));

    // 分配入口内存
    module_entry_t* new_module_entry = malloc(sizeof(module_entry_t));
    if (!new_module_entry) {
        TEKLOG_ERROR("Failed to allocate memory for module entry");
        free(new_module);
        memdl_close(handle);
        return false;
    }
    memset(new_module_entry, 0, sizeof(module_entry_t));

    // 注意：这里不再深拷贝module_info，直接使用指针
    new_module_entry->info = (module_info_t*)mod_info;  // 去掉const限定符

    // 生成目录路径
    new_module_entry->private_dir = generate_module_private_dir(mod_info->pkg_id);
    if (!new_module_entry->private_dir) {
        TEKLOG_ERROR("Failed to generate private directory for pkg_id: %s", mod_info->pkg_id);
        free(new_module_entry);
        free(new_module);
        memdl_close(handle);
        return false;
    }

    new_module_entry->logs_dir = generate_module_logs_dir(mod_info->pkg_id);
    if (!new_module_entry->logs_dir) {
        TEKLOG_ERROR("Failed to generate logs directory for pkg_id: %s", mod_info->pkg_id);
        if (new_module_entry->private_dir) free((void*)new_module_entry->private_dir);
        free(new_module_entry);
        free(new_module);
        memdl_close(handle);
        return false;
    }

    // 初始化模块句柄
    new_module->handle = handle;
    new_module->module_entry = new_module_entry;
    new_module->index = tefstd_vector_size(&g_module_list);

    // 设置操作函数表
    new_module_entry->ops = (module_ops_t*)module_ops;  // 去掉const限定符
    new_module_entry->pkg_handle = pkg_handle;

    // 记录插件依赖
    if (mod_info->plugin_dependencies && mod_info->plugin_dependencies_sizes > 0) {
        for (int i = 0; i < mod_info->plugin_dependencies_sizes; ++i) {
            if (mod_info->plugin_dependencies[i]) {
                add_plugin_ref(mod_info->plugin_dependencies[i]);
            }
        }
    }

    // 添加到全局列表
    if (!tefstd_vector_push_back(&g_module_list, &new_module)) {
        TEKLOG_ERROR("Failed to add module to global list");
        if (new_module_entry->private_dir) free((void*)new_module_entry->private_dir);
        if (new_module_entry->logs_dir) free((void*)new_module_entry->logs_dir);
        free(new_module_entry);
        free(new_module);
        memdl_close(handle);
        return false;
    }

    // 如果有init_module操作，稍后在tefkernel_start中调用
    // 这里只注册共享库
    tpf_register_shared_plugin_library(handle);

    if (out_module) *out_module = new_module;
    TEKLOG_INFO("Successfully loaded module: pkg_id=%s, index=%zu, handle=%p",
                mod_info->pkg_id, new_module->index, (void*)new_module);
    return true;
}

/**
 * @brief 卸载单个模块
 */
void tefkernel_cleanup_module(module_handle_t* module_handle) {
    if (!module_handle) {
        TEKLOG_WARN("Attempted to cleanup NULL module handle");
        return;
    }

    TEKLOG_INFO("Cleaning up module %p (index: %zu, pkg_id: %s)",
                (void*)module_handle, module_handle->index,
                module_handle->module_entry && module_handle->module_entry->info ?
                module_handle->module_entry->info->pkg_id : "NULL");

    if (!g_module_list_initialized) {
        TEKLOG_DEBUG("Module list not initialized, directly freeing handle");
        free_module(module_handle);
        return;
    }

    // 从全局列表中查找并移除
    bool found = false;
    const size_t module_count = tefstd_vector_size(&g_module_list);
    TEKLOG_DEBUG("Searching for module in global list of %zu entries", module_count);

    for (size_t i = 0; i < module_count; ++i) {
        module_handle_t** module_ptr = (module_handle_t**)tefstd_vector_at(&g_module_list, i);
        if (module_ptr && *module_ptr == module_handle) {
            TEKLOG_DEBUG("Removing module from global list at index %zu", i);

            // 移除元素
            if (!tefstd_vector_erase(&g_module_list, i, NULL)) {
                TEKLOG_ERROR("Failed to remove module from vector at index %zu", i);
            } else {
                found = true;
            }

            // 更新后续元素的索引
            for (size_t j = i; j < tefstd_vector_size(&g_module_list); ++j) {
                module_handle_t** subsequent_module = tefstd_vector_at(&g_module_list, j);
                if (subsequent_module && *subsequent_module) {
                    (*subsequent_module)->index = j;
                }
            }
            break;
        }
    }

    if (!found) {
        TEKLOG_WARN("Module handle %p not found in global list", (void*)module_handle);
    }

    // 执行实际的清理工作
    free_module(module_handle);
    TEKLOG_INFO("Module cleanup completed successfully");
}

/**
 * @brief 卸载所有模块
 */
void tefkernel_cleanup_all_modules(void) {
    if (!g_module_list_initialized) {
        TEKLOG_DEBUG("Module list not initialized, nothing to cleanup");
        return;
    }

    const size_t module_count = tefstd_vector_size(&g_module_list);
    TEKLOG_INFO("Cleaning up all %zu modules", module_count);

    // 反向遍历清理模块
    for (size_t i = module_count; i > 0; --i) {
        module_handle_t** module_ptr = (module_handle_t**)tefstd_vector_at(&g_module_list, i - 1);
        if (module_ptr && *module_ptr) {
            TEKLOG_DEBUG("Cleaning up module at index %zu", i - 1);
            module_handle_t* module_handle = *module_ptr;

            // 从向量中移除
            if (!tefstd_vector_erase(&g_module_list, i - 1, NULL)) {
                TEKLOG_ERROR("Failed to remove module from vector at index %zu", i - 1);
            }

            // 清理资源
            free_module(module_handle);
        }
    }

    // 清空向量
    tefstd_vector_clear(&g_module_list);
    TEKLOG_INFO("All modules cleaned up successfully");
}

/**
 * @brief 通过索引获取模块
 */
module_handle_t* tefkernel_get_module_by_index(const size_t index) {
    if (!g_module_list_initialized) {
        TEKLOG_DEBUG("Module list not initialized");
        return NULL;
    }

    if (index >= tefstd_vector_size(&g_module_list)) {
        TEKLOG_WARN("Module index %zu out of bounds (total: %zu)",
                   index, tefstd_vector_size(&g_module_list));
        return NULL;
    }

    module_handle_t** module_ptr = tefstd_vector_at(&g_module_list, index);
    if (!module_ptr) {
        TEKLOG_ERROR("Failed to get module at index %zu", index);
        return NULL;
    }

    TEKLOG_DEBUG("Retrieved module %p at index %zu", (void*)(*module_ptr), index);
    return *module_ptr;
}

/**
 * @brief 获取模块数量
 */
size_t tefkernel_get_module_count(void) {
    if (!g_module_list_initialized) {
        return 0;
    }

    const size_t count = tefstd_vector_size(&g_module_list);
    TEKLOG_DEBUG("Current module count: %zu", count);
    return count;
}

/**
 * @brief 获取模块的info
 */
const module_info_t* tefkernel_get_module_info(module_handle_t* module_handle) {
    if (!module_handle || !module_handle->module_entry) {
        TEKLOG_ERROR("Invalid module handle or entry");
        return NULL;
    }

    return module_handle->module_entry->info;
}

/**
 * @brief 初始化所有已加载的模块
 */
bool tefkernel_initialize_all_modules(void) {
    if (!g_module_list_initialized) {
        TEKLOG_WARN("Module system not initialized");
        return true;
    }

    const size_t module_count = tefstd_vector_size(&g_module_list);
    if (module_count == 0) {
        TEKLOG_DEBUG("No modules to initialize");
        return true;
    }

    TEKLOG_INFO("Initializing all %zu modules", module_count);
    bool all_success = true;

    for (size_t i = 0; i < module_count; ++i) {
        module_handle_t** module_ptr = tefstd_vector_at(&g_module_list, i);
        if (!module_ptr || !*module_ptr || !(*module_ptr)->module_entry) {
            TEKLOG_WARN("Invalid module handle at index %zu", i);
            continue;
        }

        module_handle_t* module_handle = *module_ptr;
        module_entry_t* entry = module_handle->module_entry;

        if (entry->ops && entry->ops->init_module) {
            TEKLOG_DEBUG("Initializing module %zu: %s", i,
                        entry->info ? entry->info->pkg_id : "unknown");

            if (!entry->ops->init_module(entry)) {
                TEKLOG_ERROR("Module %zu initialization failed", i);
                all_success = false;
            } else {
                TEKLOG_DEBUG("Module %zu initialized successfully", i);
            }
        } else {
            TEKLOG_DEBUG("Module %zu has no init_module function", i);
        }
    }

    TEKLOG_INFO("All modules initialization completed: %s",
                all_success ? "success" : "with errors");
    return all_success;
}

/**
 * @brief 调用所有模块的热重载函数
 */
void tefkernel_hot_reload_all_modules(void) {
    if (!g_module_list_initialized) {
        TEKLOG_DEBUG("Module list not initialized, nothing to hot reload");
        return;
    }

    const size_t module_count = tefstd_vector_size(&g_module_list);
    if (module_count == 0) {
        TEKLOG_DEBUG("No modules to hot reload");
        return;
    }

    TEKLOG_INFO("Hot reloading all %zu modules", module_count);

    for (size_t i = 0; i < module_count; ++i) {
        module_handle_t** module_ptr = tefstd_vector_at(&g_module_list, i);
        if (!module_ptr || !*module_ptr || !(*module_ptr)->module_entry) {
            continue;
        }

        const module_handle_t* module_handle = *module_ptr;
        module_entry_t* entry = module_handle->module_entry;

        if (entry->ops && entry->ops->hot_reload) {
            TEKLOG_DEBUG("Calling hot_reload for module %zu: %s", i,
                        entry->info ? entry->info->pkg_id : "unknown");
            entry->ops->hot_reload(entry);
        }
    }

    TEKLOG_INFO("All modules hot reload completed");
}