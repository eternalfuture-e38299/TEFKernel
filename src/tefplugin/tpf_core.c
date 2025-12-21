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
#include <threads.h>

#include "internal/tefplugin/tef_core_imp.h"
#include "memdl/memdl.h"

static struct {
    // 插件句柄，为malloc分配
    tef_vector_t plugin_handles;  // plugin_handle_t*
    bool initialized;
} g_tpf_symbols;

static struct {
    // 使用插件的动态库的句柄
    tef_vector_t handles; // void*
    bool initialized;
} shared_plugin_libraries;

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

// 内核功能实现
bool tpf_load_plugin(void *handle, plugin_handle_t** out_plugin) {
    if (!handle || !out_plugin) {
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
        const bool init_result = new_plugin->ops->initialize(new_plugin);

        if (init_result) {
            TEKLOG_INFO("Plugin initialized successfully, handle: %p", (void*)new_plugin);
            *out_plugin = new_plugin;
            return true;
        } else {
            TEKLOG_ERROR("Plugin initialization failed");
            // 从列表中移除失败的插件
            tefstd_vector_pop_back(&g_tpf_symbols.plugin_handles, NULL);
        }
    } else {
        TEKLOG_ERROR("Failed to add plugin to global list");
    }

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

// 线程函数：注册符号到单个共享库
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
            } else {
                TEKLOG_WARN("Symbol '%s' not found in library %p", sym_name, lib);
                args->success = false; // 符号查找失败
            }
        }
    }

    TEKLOG_DEBUG("Thread completed for library %p, success: %s",
                lib, args->success ? "true" : "false");
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

    // 等待所有线程完成
    bool all_success = true;
    for (size_t i = 0; i < thread_count; ++i) {
        int result;
        thrd_join(threads[i], &result);

        if (!args_array[i]->success) {
            TEKLOG_WARN("Symbol registration thread %zu completed with errors", i);
            all_success = false;
        } else {
            TEKLOG_DEBUG("Symbol registration thread %zu completed successfully", i);
        }

        free(args_array[i]);
    }

    // 清理资源
    free(threads);
    free(args_array);

    TEKLOG_INFO("Symbol registration for plugin %p completed: %s",
               (void*)plugin, all_success ? "success" : "with errors");
    return all_success;
}

bool tpf_register_shared_plugin_library(void *handle) {
    if (!shared_plugin_libraries.initialized)
        tefstd_vector_init(&shared_plugin_libraries.handles, sizeof(void*));
    return tefstd_vector_push_back(&shared_plugin_libraries.handles, &handle);
}

// API函数
bool tpf_register_symbol(plugin_handle_t* this_handle, const char *name, const void *addr) {
    if (!this_handle || !name || !addr) {
        TEKLOG_ERROR("Invalid parameters for symbol registration");
        return false;
    }

    TEKLOG_DEBUG("Registering symbol '%s' at %p for plugin %p",
                name, addr, (void*)this_handle);

    const bool name_result = tefstd_vector_push_back(&this_handle->sym_names, &name);
    const bool addr_result = tefstd_vector_push_back(&this_handle->sym_addrs, &addr);

    if (name_result && addr_result) {
        TEKLOG_TRACE("Symbol '%s' registered successfully", name);
        return true;
    }
    TEKLOG_ERROR("Failed to register symbol '%s'", name);
    return false;
}

#define TPF_KERNEL_SYMBOL(func) \
tpf_register_symbol(kernel_plugin, #func, (const void *)(func))

#include "tefstd/hashmap.h"
#include "tefstd/skipmap.h"

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

    // 创建内核自身的TPF实例（内核作为特殊插件）
    // 这里我们注册内核提供的核心符号

    // 首先创建内核插件句柄
    plugin_handle_t* kernel_plugin = malloc(sizeof(plugin_handle_t));
    if (!kernel_plugin) {
        TEKLOG_ERROR("Failed to allocate memory for kernel plugin");
        return;
    }

    // 初始化内核插件结构
    kernel_plugin->handle = NULL;  // 内核没有外部句柄
    kernel_plugin->ops = NULL;     // 内核不需要标准插件操作
    tefstd_vector_init(&kernel_plugin->sym_names, sizeof(const char*));
    tefstd_vector_init(&kernel_plugin->sym_addrs, sizeof(void*));
    kernel_plugin->index = 0;      // 内核是第一个插件

    // 注册内核提供的核心符号
    TEKLOG_DEBUG("Registering kernel symbols");

    // 注册memdl相关符号
    TPF_KERNEL_SYMBOL(memdl_open_file);
    TPF_KERNEL_SYMBOL(memdl_open);
    TPF_KERNEL_SYMBOL(memdl_sym);
    TPF_KERNEL_SYMBOL(memdl_close);
    TPF_KERNEL_SYMBOL(memdl_error);
    TPF_KERNEL_SYMBOL(memdl_get_arch);
    TPF_KERNEL_SYMBOL(memdl_validate);
    TPF_KERNEL_SYMBOL(memdl_get_platform);

    // 注册std相关符号 - hashmap
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

    // 注册std相关符号 - skipmap
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

    // 注册std相关符号 - vector
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

    // 将内核插件添加到全局列表
    if (tefstd_vector_push_back(&g_tpf_symbols.plugin_handles, &kernel_plugin)) {
        TEKLOG_INFO("Kernel TPF instance created successfully with %zu symbols",
                   tefstd_vector_size(&kernel_plugin->sym_names));

        // 立即将内核符号注册到所有已注册的共享库
        if (shared_plugin_libraries.initialized &&
            tefstd_vector_size(&shared_plugin_libraries.handles) > 0) {
            TEKLOG_DEBUG("Registering kernel symbols to shared libraries");
            tpf_register_plugin_symbols(kernel_plugin);
        }
    } else {
        TEKLOG_ERROR("Failed to add kernel plugin to global list");
        // 清理资源
        if (kernel_plugin->sym_names.data) tefstd_vector_destroy(&kernel_plugin->sym_names);
        if (kernel_plugin->sym_addrs.data) tefstd_vector_destroy(&kernel_plugin->sym_addrs);
        free(kernel_plugin);
    }
}