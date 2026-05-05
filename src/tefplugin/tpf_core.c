/*******************************************************************************
 * tefkernel - tpf_core
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
 * Created: 2025/12/12
 *******************************************************************************/

#include "tefplugin/tpf_core.h"
#include "internal/log.h"

#include <stdlib.h>
#include <string.h>

#include "internal/platform_threads.h"
#include "internal/modloader/modloader_core_imp.h"
#include "internal/module/module_core_imp.h"
#include "internal/tefplugin/tef_core_imp.h"
#include "memdl/memdl.h"

static struct {
    // 插件句柄，为malloc分配
    tefstd_vector_t plugin_handles;  // plugin_handle_t*
    bool initialized;
} g_tpf_symbols;

static struct {
    // 使用插件的动态库的句柄
    tefstd_vector_t handles; // void*
    bool initialized;
} shared_plugin_libraries;

// 全局插件引用计数表
tefstd_hashmap_t g_plugin_refs;
bool g_plugin_refs_initialized = false;

/**
 * @brief 初始化插件引用计数系统
 */
void tpf_init_plugin_refs(void) {
    if (!g_plugin_refs_initialized) {
        tefstd_hashmap_init(&g_plugin_refs, sizeof(const char*), sizeof(plugin_ref_entry_t));
        g_plugin_refs_initialized = true;
        TEKLOG_DEBUG("Plugin refs system initialized");
    }
}

/**
 * @brief 增加插件引用计数
 */
void tpf_add_plugin_ref(const char* pkg_id) {
    if (!pkg_id) return;

    tpf_init_plugin_refs();

    plugin_ref_entry_t* entry = tefstd_hashmap_get(&g_plugin_refs, &pkg_id);
    if (entry) {
        entry->ref_count++;
        TEKLOG_DEBUG("Plugin %s ref count increased to %d", pkg_id, entry->ref_count);
    } else {
        plugin_ref_entry_t new_entry = {
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
void tpf_remove_plugin_ref(const char* pkg_id) {
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
int tpf_get_plugin_ref_count(const char* pkg_id) {
    if (!pkg_id || !g_plugin_refs_initialized) return 0;

    plugin_ref_entry_t* entry = tefstd_hashmap_get(&g_plugin_refs, &pkg_id);
    return entry ? entry->ref_count : 0;
}

/**
 * @brief 检查插件是否还有其他引用
 */
bool tpf_check_plugin_references(const char* pkg_id) {
    int ref_count = tpf_get_plugin_ref_count(pkg_id);

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

static void free_plugin(plugin_handle_t* plugin) {
    if (!plugin) return;

    TEKLOG_DEBUG("Freeing plugin handle %p", (void*)plugin);

    if (plugin->ops && plugin->ops->cleanup) {
        plugin->ops->cleanup(plugin);
    }

    if (plugin->sym_addrs.data) {
        tefstd_vector_destroy(&plugin->sym_addrs);
    }

    if (plugin->sym_names.data) {
        tefstd_vector_destroy(&plugin->sym_names);
    }

    if (plugin->handle) {
        memdl_close(plugin->handle);
        plugin->handle = NULL;
    }

    plugin->ops = NULL;
    free(plugin);
}

bool tpf_plugin_exists(const char* pkg_id) {
    if (!pkg_id) {
        TEKLOG_ERROR("NULL pkg_id provided to tpf_plugin_exists");
        return false;
    }

    if (!g_tpf_symbols.initialized) {
        TEKLOG_DEBUG("Plugin system not initialized, no plugins exist");
        return false;
    }

    return tpf_get_plugin_by_id(pkg_id) != NULL;
}

plugin_handle_t* tpf_get_plugin_by_id(const char* pkg_id) {
    if (!pkg_id) {
        TEKLOG_ERROR("NULL pkg_id provided to tpf_get_plugin_by_id");
        return NULL;
    }

    if (!g_tpf_symbols.initialized) {
        TEKLOG_DEBUG("Plugin system not initialized");
        return NULL;
    }

    const size_t plugin_count = tefstd_vector_size(&g_tpf_symbols.plugin_handles);
    TEKLOG_DEBUG("Looking for plugin by id: pkg_id=%s, total plugins=%zu",
                pkg_id, plugin_count);

    for (size_t i = 0; i < plugin_count; ++i) {
        plugin_handle_t** plugin_ptr = tefstd_vector_at(&g_tpf_symbols.plugin_handles, i);
        if (!plugin_ptr || !*plugin_ptr) {
            continue;
        }

        plugin_handle_t* plugin = *plugin_ptr;

        // 获取插件信息
        if (plugin->ops && plugin->ops->get_info) {
            const tpf_plugin_info_t* info = plugin->ops->get_info();
            if (info && info->pkg_id && strcmp(info->pkg_id, pkg_id) == 0) {
                TEKLOG_DEBUG("Found plugin: pkg_id=%s, handle=%p, index=%zu",
                           pkg_id, (void*)plugin, i);
                return plugin;
            }
        }
    }

    TEKLOG_DEBUG("Plugin not found by id: pkg_id=%s", pkg_id);
    return NULL;
}

// 内核功能实现
bool tpf_load_plugin(void *handle, plugin_handle_t** out_plugin) {
    if (!handle) {
        TEKLOG_ERROR("Invalid handle provided to tpf_load_plugin");
        return false;
    }

    TEKLOG_INFO("Loading plugin from handle %p", handle);

    if (!g_tpf_symbols.initialized) {
        TEKLOG_DEBUG("Initializing plugin handles vector");
        tefstd_vector_init(&g_tpf_symbols.plugin_handles, sizeof(void*));
        g_tpf_symbols.initialized = true;
    }

    tpf_plugin_ops_t*(*create_method)() = memdl_sym(handle, "tpf_create_plugin");
    if (!create_method) {
        TEKLOG_ERROR("Failed to find tpf_create_plugin symbol in plugin");
        memdl_close(handle);
        return false;
    }

    plugin_handle_t* new_plugin = malloc(sizeof(plugin_handle_t));
    if (!new_plugin) {
        TEKLOG_ERROR("Failed to allocate memory for new plugin");
        memdl_close(handle);
        return false;
    }

    new_plugin->handle = handle;
    new_plugin->ops = create_method();

    if (!new_plugin->ops) {
        TEKLOG_ERROR("Plugin create_method returned NULL");
        free(new_plugin);
        memdl_close(handle);
        return false;
    }

    tefstd_vector_init(&new_plugin->sym_addrs, sizeof(void*));
    tefstd_vector_init(&new_plugin->sym_names, sizeof(const char*));
    new_plugin->index = g_tpf_symbols.plugin_handles.size;

    TEKLOG_DEBUG("Created plugin structure, index: %zu", new_plugin->index);

    if (tefstd_vector_push_back(&g_tpf_symbols.plugin_handles, &new_plugin)) {
        TEKLOG_DEBUG("Plugin added to global list, initializing...");

        // 初始化插件
        const bool init_result = new_plugin->ops->initialize(new_plugin);

        if (init_result) {
            // 添加插件引用计数
            if (new_plugin->ops->get_info) {
                const tpf_plugin_info_t* info = new_plugin->ops->get_info();
                if (info && info->pkg_id) {
                    tpf_add_plugin_ref(info->pkg_id);
                }
            }

            TEKLOG_INFO("Plugin initialized successfully, handle: %p", (void*)new_plugin);
            if (out_plugin) *out_plugin = new_plugin;
            return true;
        }
        TEKLOG_ERROR("Plugin initialization failed");
        // 从列表中移除失败的插件
        tefstd_vector_pop_back(&g_tpf_symbols.plugin_handles, NULL);
    } else {
        TEKLOG_ERROR("Failed to add plugin to global list");
    }

    // 注册符号
    *(void**)memdl_sym(handle, "tpf_register_symbol") = (void*)tpf_register_symbol;
    *(void**)memdl_sym(handle, "tpf_register_shared_plugin_library") = (void*)tpf_register_shared_plugin_library;

    // 如果失败则释放资源
    free_plugin(new_plugin);
    return false;
}

bool tpf_cleanup_plugin(plugin_handle_t *plugin) {
    if (!plugin) {
        TEKLOG_WARN("Attempted to cleanup NULL plugin");
        return false;
    }

    TEKLOG_INFO("Cleaning up plugin %p (index: %zu)", (void*)plugin, plugin->index);

    if (!shared_plugin_libraries.initialized) {
        TEKLOG_DEBUG("No shared libraries registered, directly freeing plugin");
        free_plugin(plugin);
        return true;
    }

    // 清理共享库中的符号引用
    const size_t lib_count = tefstd_vector_size(&shared_plugin_libraries.handles);
    TEKLOG_DEBUG("Cleaning up symbols from %zu shared libraries", lib_count);

    for (size_t i = 0; i < lib_count; ++i) {
        void** lib_ptr = tefstd_vector_at(&shared_plugin_libraries.handles, i);
        if (!lib_ptr || !*lib_ptr) continue;

        void* lib = *lib_ptr;
        const size_t sym_count = tefstd_vector_size(&plugin->sym_names);

        for (size_t ii = 0; ii < sym_count; ++ii) {
            const char** name_ptr = tefstd_vector_at(&plugin->sym_names, ii);
            if (!name_ptr || !*name_ptr) continue;

            const char* sym_name = *name_ptr;
            void** sym_addr_ptr = memdl_sym(lib, sym_name);

            if (sym_addr_ptr) {
                TEKLOG_DEBUG("Nullifying symbol '%s' in library %p", sym_name, lib);
                *sym_addr_ptr = NULL;  // 将符号指针设为NULL
            }
        }
    }

    // 从全局插件列表中移除
    if (g_tpf_symbols.initialized) {
        const size_t plugin_count = tefstd_vector_size(&g_tpf_symbols.plugin_handles);
        for (size_t i = 0; i < plugin_count; ++i) {
            plugin_handle_t** plugin_ptr = tefstd_vector_at(&g_tpf_symbols.plugin_handles, i);
            if (plugin_ptr && *plugin_ptr == plugin) {
                TEKLOG_DEBUG("Removing plugin from global list at index %zu", i);
                tefstd_vector_erase(&g_tpf_symbols.plugin_handles, i, NULL);
                break;
            }
        }
    }

    // 减少插件引用计数
    if (plugin->ops && plugin->ops->get_info) {
        const tpf_plugin_info_t* info = plugin->ops->get_info();
        if (info && info->pkg_id) {
            tpf_remove_plugin_ref(info->pkg_id);
        }
    }

    free_plugin(plugin);
    TEKLOG_INFO("Plugin cleanup completed successfully");
    return true;
}

// 线程参数结构
typedef struct {
    void* lib_handle;
    plugin_handle_t* plugin;
    bool success;
} tpf_thread_arg_t;

static int register_symbols_thread(void* arg) {
    tpf_thread_arg_t* args = arg;
    void* lib = args->lib_handle;
    const plugin_handle_t* plugin = args->plugin;
    args->success = true;

    TEKLOG_DEBUG("Thread started for library %p, plugin %p", lib, plugin);

    // 注册插件中的所有符号到该共享库
    const size_t sym_count = tefstd_vector_size(&plugin->sym_names);
    TEKLOG_DEBUG("Registering %zu symbols to library %p", sym_count, lib);

    for (size_t i = 0; i < sym_count; ++i) {
        const char** name_ptr = tefstd_vector_at(&plugin->sym_names, i);
        const void** addr_ptr = tefstd_vector_at(&plugin->sym_addrs, i);

        if (name_ptr && addr_ptr && *name_ptr && *addr_ptr) {
            const char* sym_name = *name_ptr;
            const void* sym_addr = *addr_ptr;

            // 在目标库中查找符号指针并设置值
            void** sym_ptr = memdl_sym(lib, sym_name);
            if (sym_ptr) {
                TEKLOG_TRACE("Registering symbol '%s' at %p to library %p",
                           sym_name, sym_addr, lib);
                *sym_ptr = (void*)sym_addr;
            }
        }
    }
    return 0;
}

bool tpf_register_plugin_symbols(plugin_handle_t* plugin) {
    if (!plugin) {
        TEKLOG_ERROR("Invalid plugin provided to tpf_register_plugin_symbols");
        return false;
    }

    if (!shared_plugin_libraries.initialized) {
        TEKLOG_WARN("Shared libraries not initialized, skipping symbol registration");
        return false;
    }

    const size_t lib_count = tefstd_vector_size(&shared_plugin_libraries.handles);
    TEKLOG_INFO("Registering symbols for plugin %p to %zu libraries",
               (void*)plugin, lib_count);

    if (lib_count == 0) {
        TEKLOG_DEBUG("No shared libraries to register symbols to");
        return true; // 没有共享库，直接返回成功
    }

    // 分配线程和参数数组
    thrd_t* threads = malloc(sizeof(thrd_t) * lib_count);
    tpf_thread_arg_t** args_array = malloc(sizeof(tpf_thread_arg_t*) * lib_count);

    if (!threads || !args_array) {
        TEKLOG_ERROR("Failed to allocate memory for thread registration");
        free(threads);
        free(args_array);
        return false;
    }

    // 创建所有线程
    size_t thread_count = 0;
    for (size_t i = 0; i < lib_count; ++i) {
        void** lib_ptr = tefstd_vector_at(&shared_plugin_libraries.handles, i);
        if (!lib_ptr || !*lib_ptr) continue;

        tpf_thread_arg_t* args = malloc(sizeof(tpf_thread_arg_t));
        if (!args) {
            TEKLOG_WARN("Failed to allocate thread arguments for library %zu", i);
            continue;
        }

        args->lib_handle = *lib_ptr;
        args->plugin = plugin;
        args->success = false;

        args_array[thread_count] = args;

        if (thrd_create(&threads[thread_count], register_symbols_thread, args) == thrd_success) {
            TEKLOG_DEBUG("Created symbol registration thread %zu for library %p",
                        thread_count, args->lib_handle);
            thread_count++;
        } else {
            TEKLOG_ERROR("Failed to create symbol registration thread for library %p",
                        args->lib_handle);
            free(args);
        }
    }

    TEKLOG_DEBUG("Created %zu symbol registration threads", thread_count);

    // 如果没有创建任何线程，返回成功
    if (thread_count == 0) {
        TEKLOG_DEBUG("No symbol registration threads were created");
        free(threads);
        free(args_array);
        return true;  // 没有线程创建，返回成功
    }

    // 等待所有线程完成
    bool any_success = false;
    size_t success_count = 0;
    for (size_t i = 0; i < thread_count; ++i) {
        int result;
        thrd_join(threads[i], &result);

        if (args_array[i]->success) {
            any_success = true;
            success_count++;
            TEKLOG_DEBUG("Symbol registration thread %zu for library %p completed successfully",
                        i, args_array[i]->lib_handle);
        }
        // 失败的线程不打印日志

        free(args_array[i]);
    }

    // 清理资源
    free(threads);
    free(args_array);

    // 记录总体结果
    if (any_success) {
        TEKLOG_INFO("Symbol registration for plugin %p completed: %zu/%zu libraries registered successfully",
                   (void*)plugin, success_count, thread_count);
    } else {
        TEKLOG_WARN("Symbol registration for plugin %p completed: 0/%zu libraries registered successfully",
                   (void*)plugin, thread_count);
    }

    return any_success;
}

bool tpf_register_shared_plugin_library(void *handle) {
    if (!handle) {
        TEKLOG_ERROR("Invalid handle provided to tpf_register_shared_plugin_library");
        return false;
    }

    TEKLOG_INFO("Registering shared plugin library with handle %p", handle);

    // 初始化共享库列表（如果未初始化）
    if (!shared_plugin_libraries.initialized) {
        TEKLOG_DEBUG("Initializing shared plugin libraries vector");
        if (!tefstd_vector_init(&shared_plugin_libraries.handles, sizeof(void*))) {
            TEKLOG_ERROR("Failed to initialize shared plugin libraries vector");
            return false;
        }
        shared_plugin_libraries.initialized = true;
    }

    // 检查是否已经注册过
    const size_t current_count = tefstd_vector_size(&shared_plugin_libraries.handles);
    for (size_t i = 0; i < current_count; ++i) {
        void** existing_ptr = tefstd_vector_at(&shared_plugin_libraries.handles, i);
        if (existing_ptr && *existing_ptr == handle) {
            TEKLOG_DEBUG("Library %p already registered, skipping", handle);
            return true;
        }
    }

    // 添加到列表
    if (!tefstd_vector_push_back(&shared_plugin_libraries.handles, &handle)) {
        TEKLOG_ERROR("Failed to add library %p to shared libraries list", handle);
        return false;
    }

    TEKLOG_DEBUG("Library %p added to shared libraries list (total: %zu)",
                handle, tefstd_vector_size(&shared_plugin_libraries.handles));

    // 立即将所有已加载插件的符号注册到这个新库
    TEKLOG_DEBUG("Registering all existing plugin symbols to new library %p", handle);
    const bool registration_result = tpf_register_all_plugin_symbols_to_library(handle);

    if (registration_result) {
        TEKLOG_INFO("Successfully registered existing plugin symbols to library %p", handle);
    } else {
        TEKLOG_WARN("No plugin symbols were registered to library %p (no plugins loaded yet)", handle);
    }

    return true;
}

// API函数
// 在符号注册时添加详细日志
bool tpf_register_symbol(plugin_handle_t* this_handle, const char *name, const void *addr) {
    if (!this_handle || !name || !addr) {
        TEKLOG_ERROR("Invalid parameters for symbol registration");
        return false;
    }

    const bool name_result = tefstd_vector_push_back(&this_handle->sym_names, &name);
    const bool addr_result = tefstd_vector_push_back(&this_handle->sym_addrs, &addr);

    if (name_result && addr_result) {
        // 立即将这个符号注册到所有已存在的共享库
        if (shared_plugin_libraries.initialized) {
            const size_t lib_count = tefstd_vector_size(&shared_plugin_libraries.handles);
            for (size_t i = 0; i < lib_count; ++i) {
                void** lib_ptr = tefstd_vector_at(&shared_plugin_libraries.handles, i);
                if (lib_ptr && *lib_ptr) {
                    void** sym_ptr = memdl_sym(*lib_ptr, name);
                    if (sym_ptr) {
                        *sym_ptr = (void*)addr;
                    }
                }
            }
        }
        return true;
    }

    TEKLOG_ERROR("Failed to register symbol '%s'", name);
    return false;
}

bool tpf_initialize_all_plugins() {
    // 先初始化内核
    tpf_init_libtefkernel();

    if (!g_tpf_symbols.initialized) {
        TEKLOG_WARN("No plugins loaded, nothing to initialize");
        return true;
    }

    const size_t plugin_count = tefstd_vector_size(&g_tpf_symbols.plugin_handles);
    if (plugin_count == 0) {
        TEKLOG_DEBUG("Plugin handles vector is empty");
        return true;
    }

    TEKLOG_INFO("Initializing all %zu plugins", plugin_count);

    bool all_success = true;

    // 第一步：初始化所有插件
    for (size_t i = 0; i < plugin_count; ++i) {
        plugin_handle_t** plugin_ptr = tefstd_vector_at(&g_tpf_symbols.plugin_handles, i);
        if (!plugin_ptr || !*plugin_ptr) {
            TEKLOG_WARN("Invalid plugin handle at index %zu", i);
            continue;
        }

        plugin_handle_t* plugin = *plugin_ptr;

        // 尝试初始化插件（如果有初始化函数）
        if (plugin->ops && plugin->ops->initialize) {
            TEKLOG_DEBUG("Initializing plugin %zu (handle: %p, pkg_id: %s)",
                        i, (void*)plugin,
                        plugin->ops->get_info ? plugin->ops->get_info()->pkg_id : "unknown");

            if (!plugin->ops->initialize(plugin)) {
                TEKLOG_ERROR("Plugin %zu initialization failed", i);
                all_success = false;
            } else {
                TEKLOG_DEBUG("Plugin %zu initialized successfully", i);
            }
        } else {
            TEKLOG_DEBUG("Plugin %zu has no initialize function (handle: %p, ops: %p)",
                        i, (void*)plugin, (void*)plugin->ops);
        }
    }

    // 第二步：将所有插件的符号注册到所有共享库
    if (!shared_plugin_libraries.initialized) {
        TEKLOG_WARN("Shared libraries not initialized, cannot register symbols");
        TEKLOG_INFO("All plugins initialization completed: %s",
                    all_success ? "success" : "with errors");
        return all_success;
    }

    const size_t lib_count = tefstd_vector_size(&shared_plugin_libraries.handles);
    if (lib_count == 0) {
        TEKLOG_DEBUG("No shared libraries to register symbols to");
        TEKLOG_INFO("All plugins initialization completed: %s",
                    all_success ? "success" : "with errors");
        return all_success;
    }

    TEKLOG_INFO("Registering all plugin symbols to %zu shared libraries", lib_count);

    // 为每个共享库注册所有插件的符号
    size_t total_symbols_registered = 0;
    size_t successful_libraries = 0;

    for (size_t i = 0; i < lib_count; ++i) {
        void** lib_ptr = tefstd_vector_at(&shared_plugin_libraries.handles, i);
        if (!lib_ptr || !*lib_ptr) {
            TEKLOG_WARN("Invalid library handle at index %zu", i);
            continue;
        }

        void* lib_handle = *lib_ptr;
        TEKLOG_DEBUG("Registering symbols to library %zu: %p", i, lib_handle);

        // 使用统一的函数注册所有插件符号到这个库
        if (tpf_register_all_plugin_symbols_to_library(lib_handle)) {
            successful_libraries++;
            // 获取注册的符号数量（可以通过修改 tpf_register_all_plugin_symbols_to_library 返回注册数量）
            TEKLOG_DEBUG("Successfully registered symbols to library %zu", i);
        } else {
            TEKLOG_WARN("Failed to register some symbols to library %zu", i);
            all_success = false;
        }
    }

    // 统计总符号数（可选：修改 tpf_register_all_plugin_symbols_to_library 返回注册数量）
    for (size_t i = 0; i < plugin_count; ++i) {
        plugin_handle_t** plugin_ptr = tefstd_vector_at(&g_tpf_symbols.plugin_handles, i);
        if (!plugin_ptr || !*plugin_ptr) continue;

        const plugin_handle_t* plugin = *plugin_ptr;
        total_symbols_registered += tefstd_vector_size(&plugin->sym_names);
    }

    TEKLOG_INFO("Symbol registration completed: %zu symbols from %zu plugins registered to %zu/%zu libraries",
               total_symbols_registered, plugin_count, successful_libraries, lib_count);
    TEKLOG_INFO("All plugins initialization completed: %s",
                all_success ? "success" : "with errors");

    return all_success;
}

bool tpf_register_all_plugin_symbols_to_library(void *handle) {
    if (!handle) {
        TEKLOG_ERROR("Invalid handle provided");
        return false;
    }

    if (!g_tpf_symbols.initialized) {
        TEKLOG_DEBUG("Plugin system not initialized, no symbols to register");
        return true;  // 没有插件，返回成功
    }

    const size_t plugin_count = tefstd_vector_size(&g_tpf_symbols.plugin_handles);
    if (plugin_count == 0) {
        TEKLOG_DEBUG("No plugins loaded to register symbols");
        return true;
    }

    TEKLOG_INFO("Registering all %zu plugins' symbols to new library %p", plugin_count, handle);

    size_t total_symbols_registered = 0;
    bool any_success = false;

    for (size_t i = 0; i < plugin_count; ++i) {
        plugin_handle_t** plugin_ptr = tefstd_vector_at(&g_tpf_symbols.plugin_handles, i);
        if (!plugin_ptr || !*plugin_ptr) {
            continue;
        }

        const plugin_handle_t* plugin = *plugin_ptr;
        const size_t sym_count = tefstd_vector_size(&plugin->sym_names);

        if (sym_count == 0) {
            continue;
        }

        size_t plugin_success_count = 0;

        for (size_t j = 0; j < sym_count; ++j) {
            const char** name_ptr = tefstd_vector_at(&plugin->sym_names, j);
            const void** addr_ptr = tefstd_vector_at(&plugin->sym_addrs, j);

            if (name_ptr && addr_ptr && *name_ptr && *addr_ptr) {
                const char* sym_name = *name_ptr;
                const void* sym_addr = *addr_ptr;

                // 在新库中查找符号指针并设置值
                void** sym_ptr = memdl_sym(handle, sym_name);
                if (sym_ptr) {
                    *sym_ptr = (void*)sym_addr;
                    plugin_success_count++;
                    total_symbols_registered++;
                }
            }
        }

        if (plugin_success_count > 0) {
            any_success = true;
            TEKLOG_DEBUG("Plugin %zu: %zu/%zu symbols registered successfully",
                        i, plugin_success_count, sym_count);
        } else {
            TEKLOG_WARN("Plugin %zu: 0/%zu symbols registered to library %p",
                       i, sym_count, handle);
        }
    }

    TEKLOG_INFO("Symbol registration completed: %zu symbols registered to library %p, success=%s",
               total_symbols_registered, handle, any_success ? "true" : "false");

    return any_success;
}

#define TPF_KERNEL_SYMBOL(func) \
tpf_register_symbol(kernel_plugin, #func, (const void *)(func))

#include "tefstd/hashmap.h"
#include "tefstd/skipmap.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/property.h"
#include "patchlib/type.h"
#include "patchlib/struct/array.h"
#include "patchlib/struct/dictionary.h"
#include "patchlib/struct/list.h"
#include "patchlib/struct/string.h"
#include "tefpackage/tefpkg.h"

void tpf_init_libtefkernel() {
    TEKLOG_INFO("Initializing libtefkernel - creating TPF instance");

    // 初始化全局数据结构
    if (!g_tpf_symbols.initialized) {
        TEKLOG_DEBUG("Initializing plugin symbols management");
        tefstd_vector_init(&g_tpf_symbols.plugin_handles, sizeof(plugin_handle_t*));
        g_tpf_symbols.initialized = true;
    }

    if (!shared_plugin_libraries.initialized) {
        TEKLOG_DEBUG("Initializing shared plugin libraries management");
        tefstd_vector_init(&shared_plugin_libraries.handles, sizeof(void*));
        shared_plugin_libraries.initialized = true;
    }

    // 初始化插件引用计数系统
    tpf_init_plugin_refs();

    // 创建内核自身的TPF实例
    plugin_handle_t* kernel_plugin = malloc(sizeof(plugin_handle_t));
    if (!kernel_plugin) {
        TEKLOG_ERROR("Failed to allocate memory for kernel plugin");
        return;
    }

    // 初始化内核插件结构
    kernel_plugin->handle = NULL;
    kernel_plugin->ops = NULL;
    tefstd_vector_init(&kernel_plugin->sym_names, sizeof(const char*));
    tefstd_vector_init(&kernel_plugin->sym_addrs, sizeof(void*));
    kernel_plugin->index = 0;

    // 注册内核提供的核心符号
    TEKLOG_DEBUG("Registering kernel symbols");

    // 注册memdl相关符号（所有平台都需要）
    TPF_KERNEL_SYMBOL(memdl_open_file);
    TPF_KERNEL_SYMBOL(memdl_open);
    TPF_KERNEL_SYMBOL(memdl_sym);
    TPF_KERNEL_SYMBOL(memdl_close);
    TPF_KERNEL_SYMBOL(memdl_error);
    TPF_KERNEL_SYMBOL(memdl_get_arch);
    TPF_KERNEL_SYMBOL(memdl_validate);
    TPF_KERNEL_SYMBOL(memdl_get_platform);

    // 注册std相关符号 - hashmap（所有平台都需要）
    TEKLOG_DEBUG("Registering hashmap symbols");
    TPF_KERNEL_SYMBOL(tefstd_hash_str);
    TPF_KERNEL_SYMBOL(tefstd_hash_mem);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_init);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_free);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_put);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_get);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_del);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_has);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_len);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_clear);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_iter);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_next);

    // 注册std相关符号 - skipmap（所有平台都需要）
    TEKLOG_DEBUG("Registering skipmap symbols");
    TPF_KERNEL_SYMBOL(tefstd_skipmap_init);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_free);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_put);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_get);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_del);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_min);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_max);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_range);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_next);

    // 注册std相关符号 - vector（所有平台都需要）
    TEKLOG_DEBUG("Registering vector symbols");
    TPF_KERNEL_SYMBOL(tefstd_vector_init);
    TPF_KERNEL_SYMBOL(tefstd_vector_destroy);
    TPF_KERNEL_SYMBOL(tefstd_vector_push_back);
    TPF_KERNEL_SYMBOL(tefstd_vector_pop_back);
    TPF_KERNEL_SYMBOL(tefstd_vector_at);
    TPF_KERNEL_SYMBOL(tefstd_vector_size);
    TPF_KERNEL_SYMBOL(tefstd_vector_capacity);
    TPF_KERNEL_SYMBOL(tefstd_vector_clear);
    TPF_KERNEL_SYMBOL(tefstd_vector_reserve);
    TPF_KERNEL_SYMBOL(tefstd_vector_erase);
    TPF_KERNEL_SYMBOL(tefstd_vector_remove_value);
    TPF_KERNEL_SYMBOL(tefstd_vector_init_from_array);

    // ==================== 注册TEFPKG包管理API ====================
    TEKLOG_DEBUG("Registering TEFPKG package management symbols");

    // 包生命周期管理API
    TPF_KERNEL_SYMBOL(tefpkg_create_reserved_from_file);
    TPF_KERNEL_SYMBOL(tefpkg_create_reserved_from_memory);
    TPF_KERNEL_SYMBOL(tefpkg_open_readonly);
    TPF_KERNEL_SYMBOL(tefpkg_open_from_memory);
    TPF_KERNEL_SYMBOL(tefpkg_save_file);
    TPF_KERNEL_SYMBOL(tefpkg_save_memory_file);
    TPF_KERNEL_SYMBOL(tefpkg_close);

    // 条目操作API
    TPF_KERNEL_SYMBOL(tefpkg_add_entry_from_memory);
    TPF_KERNEL_SYMBOL(tefpkg_add_entry_from_file);
    TPF_KERNEL_SYMBOL(tefpkg_extract_entry_to_memory);
    TPF_KERNEL_SYMBOL(tefpkg_extract_entry_to_file);
    TPF_KERNEL_SYMBOL(tefpkg_get_entry_info);
    TPF_KERNEL_SYMBOL(tefpkg_get_entries_count);
    TPF_KERNEL_SYMBOL(tefpkg_get_reserved_entries);

    // 完整性验证API
    TPF_KERNEL_SYMBOL(tefpkg_verify_entry);
    TPF_KERNEL_SYMBOL(tefpkg_verify_pkg);
    TPF_KERNEL_SYMBOL(tefpkg_verify_signature);
    TPF_KERNEL_SYMBOL(tefpkg_sign_package);

    // ==================== 注册所有patchlib核心API ====================
    TEKLOG_DEBUG("Registering patchlib core symbols");

    // 基础类型和工具函数
    TPF_KERNEL_SYMBOL(get_size_from_patch_type);
    TPF_KERNEL_SYMBOL(patchlib_is_valid);

    // 类型操作API
    TPF_KERNEL_SYMBOL(patchlib_type_get_type);
    TPF_KERNEL_SYMBOL(patchlib_get_basic_type);
    TPF_KERNEL_SYMBOL(patchlib_type_new_instance);
    TPF_KERNEL_SYMBOL(patchlib_type_make_generic_type);
    TPF_KERNEL_SYMBOL(patchlib_type_get_mono_type);
    TPF_KERNEL_SYMBOL(patchlib_type_get_name);
    TPF_KERNEL_SYMBOL(patchlib_type_get_namespace);
    TPF_KERNEL_SYMBOL(patchlib_type_get_full_name);
    TPF_KERNEL_SYMBOL(patchlib_type_get_parent);

    // 类型成员获取API
    TPF_KERNEL_SYMBOL(patchlib_type_get_inner_type);
    TPF_KERNEL_SYMBOL(patchlib_type_get_field);
    TPF_KERNEL_SYMBOL(patchlib_type_get_property);
    TPF_KERNEL_SYMBOL(patchlib_type_get_method);
    TPF_KERNEL_SYMBOL(patchlib_type_get_method_by_param_count);
    TPF_KERNEL_SYMBOL(patchlib_type_get_method_by_param_names);
    TPF_KERNEL_SYMBOL(patchlib_type_get_method_by_param_types);
    TPF_KERNEL_SYMBOL(patchlib_type_get_method_by_signature);

    // 类型批量成员获取API
    TPF_KERNEL_SYMBOL(patchlib_type_get_inner_types);
    TPF_KERNEL_SYMBOL(patchlib_type_get_methods);
    TPF_KERNEL_SYMBOL(patchlib_type_get_fields);
    TPF_KERNEL_SYMBOL(patchlib_type_get_properties);

    // 字段操作API
    TPF_KERNEL_SYMBOL(patchlib_field_get_name);
    TPF_KERNEL_SYMBOL(patchlib_field_is_static);
    TPF_KERNEL_SYMBOL(patchlib_field_is_instance);
    TPF_KERNEL_SYMBOL(patchlib_field_is_const);
    TPF_KERNEL_SYMBOL(patchlib_field_is_thread_static);
    TPF_KERNEL_SYMBOL(patchlib_field_get_value);
    TPF_KERNEL_SYMBOL(patchlib_field_set_value);
    TPF_KERNEL_SYMBOL(patchlib_field_get_type);

    // 方法操作API
    TPF_KERNEL_SYMBOL(patchlib_method_get_name);
    TPF_KERNEL_SYMBOL(patchlib_method_get_param_count);
    TPF_KERNEL_SYMBOL(patchlib_method_is_instance);
    TPF_KERNEL_SYMBOL(patchlib_method_is_static);
    TPF_KERNEL_SYMBOL(patchlib_method_make_generic_instance);
    TPF_KERNEL_SYMBOL(patchlib_method_invoke_args);
    TPF_KERNEL_SYMBOL(patchlib_method_invoke);
    TPF_KERNEL_SYMBOL(patchlib_method_get_token);
    TPF_KERNEL_SYMBOL(patchlib_method_get_signature);
    TPF_KERNEL_SYMBOL(patchlib_method_signature_free);
    TPF_KERNEL_SYMBOL(patchlib_install_prepost_hook);
    TPF_KERNEL_SYMBOL(patchlib_uninstall_hook);

    // 属性操作API
    TPF_KERNEL_SYMBOL(patchlib_property_get_name);
    TPF_KERNEL_SYMBOL(patchlib_property_get_get_method);
    TPF_KERNEL_SYMBOL(patchlib_property_get_set_method);

    // 集合操作API - Array
    TPF_KERNEL_SYMBOL(patchlib_array_create);
    TPF_KERNEL_SYMBOL(patchlib_array_at);
    TPF_KERNEL_SYMBOL(patchlib_array_set);
    TPF_KERNEL_SYMBOL(patchlib_array_fill);
    TPF_KERNEL_SYMBOL(patchlib_array_empty);
    TPF_KERNEL_SYMBOL(patchlib_array_length);
    TPF_KERNEL_SYMBOL(patchlib_array_clear);

    // 集合操作API - List
    TPF_KERNEL_SYMBOL(patchlib_list_create);
    TPF_KERNEL_SYMBOL(patchlib_list_copy_from);
    TPF_KERNEL_SYMBOL(patchlib_list_add);
    TPF_KERNEL_SYMBOL(patchlib_list_remove);
    TPF_KERNEL_SYMBOL(patchlib_list_remove_at);
    TPF_KERNEL_SYMBOL(patchlib_list_clear);
    TPF_KERNEL_SYMBOL(patchlib_list_get_array);

    // 集合操作API - Dictionary
    TPF_KERNEL_SYMBOL(patchlib_dictionary_create);
    TPF_KERNEL_SYMBOL(patchlib_dictionary_add);
    TPF_KERNEL_SYMBOL(patchlib_dictionary_get_value);
    TPF_KERNEL_SYMBOL(patchlib_dictionary_set_value);
    TPF_KERNEL_SYMBOL(patchlib_dictionary_clear);
    TPF_KERNEL_SYMBOL(patchlib_dictionary_length);
    TPF_KERNEL_SYMBOL(patchlib_dictionary_remove);

    // 字符串操作API
    TPF_KERNEL_SYMBOL(patchlib_string_create);
    TPF_KERNEL_SYMBOL(patchlib_string_cstr16);
    TPF_KERNEL_SYMBOL(patchlib_string_cstr);
    TPF_KERNEL_SYMBOL(patchlib_string_empty);
    TPF_KERNEL_SYMBOL(patchlib_string_length);

    // Android平台特定API
#if __ANDROID__
    TEKLOG_DEBUG("Registering Android-specific symbols");
    TPF_KERNEL_SYMBOL(patchlib_field_get_pointer);
    TPF_KERNEL_SYMBOL(patchlib_field_get_size);
    TPF_KERNEL_SYMBOL(patchlib_method_get_pointer);
#endif

    // 在Android平台，不需要注册资源管理相关的符号，因为它们被宏定义为空操作
#if !defined(__ANDROID__)
    // 非Android平台需要注册这些资源管理符号
    TEKLOG_DEBUG("Registering resource management symbols for non-Android platform");

    // 字段资源管理
    TPF_KERNEL_SYMBOL(patchlib_field_free);

    // 方法资源管理
    TPF_KERNEL_SYMBOL(patchlib_method_free);

    // 属性资源管理
    TPF_KERNEL_SYMBOL(patchlib_property_free);

    // 类型和对象资源管理
    TPF_KERNEL_SYMBOL(patchlib_type_free);
    TPF_KERNEL_SYMBOL(patchlib_object_free);
    TPF_KERNEL_SYMBOL(patchlib_object_persist);
#endif

    TPF_KERNEL_SYMBOL(tpf_register_shared_plugin_library);

    // 将内核插件添加到全局列表
    if (tefstd_vector_push_back(&g_tpf_symbols.plugin_handles, &kernel_plugin)) {
        TEKLOG_INFO("Kernel TPF instance created successfully with %zu symbols",
                   tefstd_vector_size(&kernel_plugin->sym_names));
    } else {
        TEKLOG_ERROR("Failed to add kernel plugin to global list");
        // 清理资源
        if (kernel_plugin->sym_names.data) tefstd_vector_destroy(&kernel_plugin->sym_names);
        if (kernel_plugin->sym_addrs.data) tefstd_vector_destroy(&kernel_plugin->sym_addrs);
        free(kernel_plugin);
    }
}