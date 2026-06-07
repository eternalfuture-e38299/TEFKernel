/*******************************************************************************
 * tefkernel - method_hook
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
 * Created: 2025/12/21
 *******************************************************************************/

#include <ffi.h>
#include <bits/sysconf.h>
#include <sys/mman.h>
#include <malloc.h>
#include <dobby.h>
#include <errno.h>
#include <string.h>

#include "private.h"
#include "internal/log.h"
#include "patchlib/method.h"
#include "tefstd/hashmap.h"

typedef struct patchlib_hook_handle_t {
    void *method; ///< 当前挂钩的函数
    void *original; ///< 原始函数
    ffi_cif cif; ///< FFI调用接口（直接存储，不动态分配）
    ffi_closure* closure;
    void *trampoline_func; ///< 跳板函数
    patch_method_signature_t method_signature; ///< 函数签名
    tefstd_vector_t prefixes; ///< 前置补丁函数(void*)
    tefstd_vector_t postfixes; ///< 后置补丁函数(void*)
    bool is_hooked; ///< 是否已挂钩
} patchlib_hook_handle_t;

typedef struct patchlib_hook_node_t {
    patch_hook_id_t id; ///< Hook ID
    patchlib_hook_handle_t* handle; ///< 节点指向的句柄
    void* prefix;            ///< 前置补丁函数指针
    void* postfix;           ///< 后置补丁函数指针
} patchlib_hook_node_t;

static struct {
    bool initialized; ///< 初始化状态
    tefstd_vector_t hook_handles;    ///< Hook句柄 <patchlib_hook_handle_t*>
    tefstd_hashmap_t hooks; ///< Hook节点 <patch_hook_id_t, patchlib_hook_node_t*>
    tefstd_hashmap_t method_to_handle;  ///< 方法到句柄的映射 <void*, patchlib_hook_handle_t*>
    patch_hook_id_t next_hook_id; ///< 下一个可用的Hook ID
} g_hooks;

static void init_g_hooks() {
    if (g_hooks.initialized) {
        TEKLOG_DEBUG("Hook system already initialized");
        return;
    }

#if defined(__aarch64__)
    TEKLOG_INFO("Initializing hook system for ARM64");
#elif defined(__arm__)
    TEKLOG_INFO("Initializing hook system for ARM32");
#else
    TEKLOG_INFO("Initializing hook system");
#endif

    tefstd_vector_init(&g_hooks.hook_handles, sizeof(patchlib_hook_handle_t*));
    tefstd_hashmap_init(&g_hooks.hooks, sizeof(patch_hook_id_t), sizeof(patchlib_hook_node_t*));
    tefstd_hashmap_init(&g_hooks.method_to_handle, sizeof(void*), sizeof(patchlib_hook_handle_t*));
    g_hooks.next_hook_id = 0;

    g_hooks.initialized = true;
    TEKLOG_INFO("Hook system initialized successfully");
}

static patchlib_hook_handle_t* find_or_create_hook_handle(void* method) {
    if (!g_hooks.initialized) {
        TEKLOG_DEBUG("Hook system not initialized, initializing now");
        init_g_hooks();
    }

    TEKLOG_DEBUG("Looking for hook handle for method: %p", method);

    patchlib_hook_handle_t** handle_ptr = tefstd_hashmap_get(&g_hooks.method_to_handle, &method);
    if (handle_ptr && *handle_ptr) {
        TEKLOG_DEBUG("Found existing hook handle for method: %p", method);
        return *handle_ptr;
    }

    TEKLOG_INFO("Creating new hook handle for method: %p", method);

    // 创建新句柄
    patchlib_hook_handle_t* handle = malloc(sizeof(patchlib_hook_handle_t));
    if (!handle) {
        TEKLOG_ERROR("Failed to allocate memory for hook handle");
        return NULL;
    }

    handle->method = method;
    handle->is_hooked = false;


    // 初始化向量
    tefstd_vector_init(&handle->prefixes, sizeof(void*));
    tefstd_vector_init(&handle->postfixes, sizeof(void*));

    // 获取方法签名
    if (!patchlib_method_get_signature(method, &handle->method_signature)) {
        TEKLOG_ERROR("Failed to get method signature for method: %p", method);
        tefstd_vector_destroy(&handle->prefixes);
        tefstd_vector_destroy(&handle->postfixes);
        free(handle);
        return NULL;
    }

    // 注册到全局表
    tefstd_hashmap_put(&g_hooks.method_to_handle, &method, &handle);
    tefstd_vector_push_back(&g_hooks.hook_handles, &handle);

    TEKLOG_DEBUG("Successfully created hook handle for method: %p", method);
    return handle;
}

static void* allocate_executable_memory(const size_t size) {
    // 获取系统页大小
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        TEKLOG_ERROR("Failed to get page size: %s", strerror(errno));
        return NULL;
    }

    // 计算需要映射的页数
    size_t mapped_size = (size + page_size - 1) & ~(page_size - 1);

    // 根据架构确定对齐要求
    size_t alignment;
    const char* arch_name;

#if defined(__aarch64__)
    alignment = 16;  // ARM64要求16字节对齐
    arch_name = "ARM64";
#elif defined(__arm__)
    alignment = 8;   // ARM32要求8字节对齐
    arch_name = "ARM32";
#else
    alignment = 16;  // 其他架构使用安全值
    arch_name = "Unknown";
#endif

    TEKLOG_DEBUG("Allocating executable memory for %s: size=%zu, page_size=%ld, alignment=%zu",
                 arch_name, size, page_size, alignment);

    // 分配额外空间用于对齐
    size_t total_size = mapped_size + alignment - 1;

    void* ptr = mmap(NULL, total_size,
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (ptr == MAP_FAILED) {
        TEKLOG_ERROR("mmap failed: %s (errno=%d)", strerror(errno), errno);
        return NULL;
    }

    // 计算对齐的指针
    uintptr_t aligned_addr = ((uintptr_t)ptr + alignment - 1) & ~(alignment - 1);
    void* aligned_ptr = (void*)aligned_addr;

    // 验证对齐
    if (((uintptr_t)aligned_ptr & (alignment - 1)) != 0) {
        TEKLOG_ERROR("Memory not properly aligned for %s: %p (required alignment: %zu)",
                     arch_name, aligned_ptr, alignment);
        munmap(ptr, total_size);
        return NULL;
    }

    TEKLOG_DEBUG("Successfully allocated executable memory for %s:", arch_name);
    TEKLOG_DEBUG("  Raw pointer: %p", ptr);
    TEKLOG_DEBUG("  Aligned pointer: %p", aligned_ptr);
    TEKLOG_DEBUG("  Total size: %zu, Usable size: %zu", total_size, size);
    TEKLOG_DEBUG("  Alignment check: %p & %zu = 0 (OK)",
                 aligned_ptr, alignment - 1);

    return aligned_ptr;
}

static void free_executable_memory(void* ptr, const size_t size) {
    if (ptr) {
        const long page_size = sysconf(_SC_PAGESIZE);
        const size_t mapped_size = (size + page_size - 1) & ~(page_size - 1);
        munmap(ptr, mapped_size);
        TEKLOG_INFO("Release executable memory: %p", ptr);
    }
}

static void hook_dispatcher_static(ffi_cif* cif, void* ret, void** args, void* user_data) {
    const patchlib_hook_handle_t* handle = user_data;

    bool call_original = true;

    const size_t prefix_count = tefstd_vector_size(&handle->prefixes);
    if (prefix_count > 0) {
        for (size_t i = 0; i < prefix_count; ++i) {
            void** prefix_ptr = tefstd_vector_at(&handle->prefixes, i);
            if (prefix_ptr && *prefix_ptr) {
                const prefix_callback_t prefix = *prefix_ptr;
                if (prefix(NULL, args, &handle->method_signature, ret)) {
                    call_original = false;
                    break;
                }
            }
        }
    }

    if (call_original) {
        ffi_call(cif, handle->original, ret, args);
    }

    const size_t postfix_count = tefstd_vector_size(&handle->postfixes);
    if (postfix_count > 0) {
        for (size_t i = 0; i < postfix_count; ++i) {
            void** postfix_ptr = tefstd_vector_at(&handle->postfixes, i);
            if (postfix_ptr && *postfix_ptr) {
                const postfix_callback_t postfix = *postfix_ptr;
                postfix(NULL, args, ret, &handle->method_signature);
            }
        }
    }
}

static void hook_dispatcher(ffi_cif* cif, void* ret, void** args, void* user_data) {
    const patchlib_hook_handle_t* handle = user_data;

    void* instance = *(void**)args[0];
    void** actual_args = args + 1;

    bool call_original = true;

    const size_t prefix_count = tefstd_vector_size(&handle->prefixes);
    if (prefix_count > 0) {
        for (size_t i = 0; i < prefix_count; ++i) {
            void** prefix_ptr = tefstd_vector_at(&handle->prefixes, i);
            if (prefix_ptr && *prefix_ptr) {
                const prefix_callback_t prefix = *prefix_ptr;
                if (prefix(instance, actual_args, &handle->method_signature, ret)) {
                    call_original = false;
                    break;
                }
            }
        }
    }

    if (call_original) {
        ffi_call(cif, handle->original, ret, args);
    }

    const size_t postfix_count = tefstd_vector_size(&handle->postfixes);
    if (postfix_count > 0) {
        for (size_t i = 0; i < postfix_count; ++i) {
            void** postfix_ptr = tefstd_vector_at(&handle->postfixes, i);
            if (postfix_ptr && *postfix_ptr) {
                const postfix_callback_t postfix = *postfix_ptr;
                postfix(instance, actual_args, ret, &handle->method_signature);
            }
        }
    }
}

static bool create_closure_from_signature(patchlib_hook_handle_t* handle) {
    void(*hook_callback)(ffi_cif *, void *, void **, void *) = NULL;
    if (handle->method_signature.is_instance)
        hook_callback = (void*)hook_dispatcher;
    else
        hook_callback = (void*)hook_dispatcher_static;

    ffi_type* re_type = patch_type_to_ffi_type(handle->method_signature.return_type);

    const size_t args_count = tefstd_vector_size(&handle->method_signature.arg_types);
    const size_t total_args = args_count + (handle->method_signature.is_instance ? 1 : 0);

    ffi_type** arg_types = malloc(total_args * sizeof(ffi_type*));
    if (!arg_types) {
        TEKLOG_ERROR("Failed to allocate memory for argument types");
        return false;
    }

    size_t arg_index = 0;
    if (handle->method_signature.is_instance) {
        arg_types[arg_index++] = &ffi_type_pointer; // this 指针
    }

    for (size_t i = 0; i < args_count; ++i) {
        patch_type_t* type = tefstd_vector_at(&handle->method_signature.arg_types, i);
        if (!type) {
            TEKLOG_ERROR("Failed to get argument type at index %zu", i);
            free(arg_types);
            return false;
        }
        arg_types[arg_index++] = patch_type_to_ffi_type(*type);
    }



    ffi_abi abi = FFI_DEFAULT_ABI;
#if defined(__aarch64__)
    abi = FFI_SYSV;
    TEKLOG_DEBUG("Using FFI_SYSV ABI for ARM64");
#elif defined(__arm__)
    abi = FFI_SYSV;
    TEKLOG_DEBUG("Using FFI_SYSV ABI for ARM32");
#else
    TEKLOG_DEBUG("Using FFI_DEFAULT_ABI");
#endif

    ffi_status status = ffi_prep_cif(&handle->cif, abi, total_args, re_type, arg_types);
    if (status != FFI_OK) {
        TEKLOG_ERROR("ffi_prep_cif failed with status: %d", status);
        free(arg_types);
        return false;
    }

    const size_t closure_size = sizeof(ffi_closure);
    void* exec_mem = allocate_executable_memory(closure_size);
    if (!exec_mem) {
        TEKLOG_ERROR("Failed to allocate executable memory");
        free(arg_types);
        return false;
    }

    handle->closure = exec_mem;
    handle->trampoline_func = exec_mem;

    status = ffi_prep_closure_loc(handle->closure, &handle->cif, hook_callback, handle, handle->trampoline_func);
    if (status != FFI_OK) {
        TEKLOG_ERROR("ffi_prep_closure_loc failed with status: %d", status);
        free_executable_memory(exec_mem, closure_size);
        free(arg_types);
        return false;
    }

    TEKLOG_DEBUG("Successfully created closure for method: %p", handle->method);
    return true;
}

patch_hook_id_t patchlib_install_prepost_hook(patch_handle_t method, prefix_callback_t prefix, postfix_callback_t postfix) {
    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Cannot install hook: method is NULL");
        return PATCH_HOOK_INVALID_ID;
    }

    if (!prefix && !postfix) {
        TEKLOG_ERROR("Cannot install hook: both prefix and postfix are NULL");
        return PATCH_HOOK_INVALID_ID;
    }


    patchlib_hook_handle_t* handle = find_or_create_hook_handle(method);
    if (!handle->is_hooked) {
        patchlib_method_get_signature(method, &handle->method_signature);
        create_closure_from_signature(handle);
        DobbyHook(patchlib_method_get_pointer(method), handle->trampoline_func,  &handle->original);
        handle->is_hooked = true;
    }

    // 创建Hook节点
    patchlib_hook_node_t* node = malloc(sizeof(patchlib_hook_node_t));
    if (!node) {
        TEKLOG_ERROR("Failed to allocate memory for hook node");
        return PATCH_HOOK_INVALID_ID;
    }

    node->id = g_hooks.next_hook_id++;
    node->handle = handle;
    node->prefix = prefix;
    node->postfix = postfix;

    TEKLOG_DEBUG("Created hook node ID: %d", node->id);

    // 添加回调到句柄
    if (prefix) {
        tefstd_vector_push_back(&handle->prefixes, &prefix);
        TEKLOG_DEBUG("Added prefix callback: %p", prefix);
    }
    if (postfix) {
        tefstd_vector_push_back(&handle->postfixes, &postfix);
        TEKLOG_DEBUG("Added postfix callback: %p", postfix);
    }

    tefstd_hashmap_put(&g_hooks.hooks, &node->id, &node);
    TEKLOG_INFO("Hook installed successfully with ID: %d for method: %p", node->id, method);

    return node->id;
}

bool patchlib_uninstall_hook(patch_hook_id_t hook_id) {
    if (!g_hooks.initialized) {
        TEKLOG_WARN("Hook system not initialized, cannot uninstall hook: %d", hook_id);
        return false;
    }

    if (hook_id == PATCH_HOOK_INVALID_ID) {
        TEKLOG_WARN("Attempted to uninstall invalid hook ID");
        return false;
    }

    TEKLOG_INFO("Uninstalling hook ID: %d", hook_id);

    patchlib_hook_node_t** node_ptr = tefstd_hashmap_get(&g_hooks.hooks, &hook_id);
    if (!node_ptr || !*node_ptr) {
        TEKLOG_ERROR("Hook node not found for ID: %d", hook_id);
        return false;
    }

    patchlib_hook_node_t* node = *node_ptr;
    patchlib_hook_handle_t* handle = node->handle;

    TEKLOG_DEBUG("Found hook node for method: %p", handle->method);

    // 从handle中移除回调
    if (node->prefix) {
        const bool removed = tefstd_vector_remove_value(&handle->prefixes, &node->prefix);
        TEKLOG_DEBUG("Removed prefix callback: %p (success: %s)", node->prefix, removed ? "true" : "false");
    }
    if (node->postfix) {
        const bool removed = tefstd_vector_remove_value(&handle->postfixes, &node->postfix);
        TEKLOG_DEBUG("Removed postfix callback: %p (success: %s)", node->postfix, removed ? "true" : "false");
    }

    // 如果没有其他hook，清理整个handle
    const size_t prefix_count = tefstd_vector_size(&handle->prefixes);
    const size_t postfix_count = tefstd_vector_size(&handle->postfixes);

    if (prefix_count == 0 && postfix_count == 0 && handle->is_hooked) {
        TEKLOG_INFO("No more hooks for method %p, cleaning up", handle->method);

        // 恢复原始函数
        void* method_ptr = patchlib_method_get_pointer(handle->method);
        if (method_ptr && handle->original) {
            TEKLOG_DEBUG("Restoring original function for method: %p", method_ptr);
            DobbyDestroy(method_ptr);
        }

        // 释放FFI资源
        if (handle->closure) {
            TEKLOG_DEBUG("Freeing executable closure");
            free_executable_memory(handle->closure, sizeof(ffi_closure));
            handle->closure = NULL;
        }

        handle->is_hooked = false;
        handle->trampoline_func = NULL;
        handle->original = NULL;
        TEKLOG_INFO("Hook resources cleaned up for method: %p", handle->method);
    } else {
        TEKLOG_DEBUG("Method %p still has %zu prefixes and %zu postfixes",
                    handle->method, prefix_count, postfix_count);
    }

    // 从全局表中移除节点
    tefstd_hashmap_del(&g_hooks.hooks, &hook_id);
    TEKLOG_DEBUG("Removed hook node from global table");

    // 如果handle完全空了，也清理它
    if (tefstd_vector_size(&handle->prefixes) == 0 &&
        tefstd_vector_size(&handle->postfixes) == 0) {

        TEKLOG_INFO("Handle is empty, cleaning up completely");

        tefstd_hashmap_del(&g_hooks.method_to_handle, &handle->method);

        // 从hook_handles中移除
        for (size_t i = 0; i < tefstd_vector_size(&g_hooks.hook_handles); i++) {
            patchlib_hook_handle_t** handle_ptr = tefstd_vector_at(&g_hooks.hook_handles, i);
            if (handle_ptr && *handle_ptr == handle) {
                tefstd_vector_erase(&g_hooks.hook_handles, i, NULL);
                TEKLOG_DEBUG("Removed handle from hook_handles vector");
                break;
            }
        }

        // 清理签名资源
        tefstd_vector_destroy(&handle->method_signature.arg_types);
        tefstd_vector_destroy(&handle->prefixes);
        tefstd_vector_destroy(&handle->postfixes);
        free(handle);
        TEKLOG_INFO("Handle memory freed");
    }

    free(node);
    TEKLOG_INFO("Hook uninstalled successfully: %d", hook_id);
    return true;
}
