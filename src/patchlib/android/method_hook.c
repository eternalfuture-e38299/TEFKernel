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
#include <errno.h>
#include <string.h>

#include "shadowhook.h"

#include "private.h"
#include "internal/log.h"
#include "patchlib/il2cpp_api.h"
#include "patchlib/method.h"
#include "tefstd/hashmap.h"

typedef struct patchlib_hook_handle_t {
    void *method;              ///< 当前挂钩的函数
    void *original;            ///< 原始函数
    void *stub;                ///< ShadowHook 返回的 hook 凭据，unhook 时用
    ffi_cif cif;               ///< FFI 调用接口
    ffi_closure *closure;
    void *trampoline_func;     ///< 跳板函数
    patch_method_signature_t method_signature; ///< 函数签名
    tefstd_vector_t prefixes;  ///< 前置补丁函数(patchlib_hook_cb_t)
    tefstd_vector_t postfixes; ///< 后置补丁函数(patchlib_hook_cb_t)
    bool is_hooked;            ///< 是否已挂钩
    ffi_type **arg_types;      ///< 保存 arg_types 数组
    size_t arg_types_count;    ///< 数组长度
    ffi_type *return_ffi_type; ///< 返回类型 ffi_type
} patchlib_hook_handle_t;

typedef struct patchlib_hook_cb_t {
    void *fn;       ///< 回调函数指针（*_callback_t 或 *_callback_ex_t）
    void *user;     ///< 传给 *_ex 回调的上下文；普通回调为 NULL
    bool is_ex;     ///< true 表示 fn 是 *_ex 版本（第一个参数为 user）
} patchlib_hook_cb_t;

typedef struct patchlib_hook_node_t {
    patch_hook_id_t id;
    patchlib_hook_handle_t *handle;
    patchlib_hook_cb_t prefix;
    patchlib_hook_cb_t postfix;
} patchlib_hook_node_t;

static struct {
    bool initialized;
    tefstd_vector_t hook_handles;            ///< <patchlib_hook_handle_t*>
    tefstd_hashmap_t hooks;                  ///< <patch_hook_id_t, patchlib_hook_node_t*>
    tefstd_hashmap_t method_to_handle;       ///< <void*, patchlib_hook_handle_t*>
    patch_hook_id_t next_hook_id;
} g_hooks;

/* typed_hook 的 method → stub 映射，供 uninstall_typed_hook 使用 */
static struct {
    bool initialized;
    tefstd_hashmap_t method_to_stub;         ///< <void* method, void* stub>
} g_typed_hooks;

static void init_g_hooks(void) {
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

    tefstd_vector_init(&g_hooks.hook_handles, sizeof(patchlib_hook_handle_t *));
    tefstd_hashmap_init(&g_hooks.hooks, sizeof(patch_hook_id_t), sizeof(patchlib_hook_node_t *));
    tefstd_hashmap_init(&g_hooks.method_to_handle, sizeof(void *), sizeof(patchlib_hook_handle_t *));
    g_hooks.next_hook_id = 0;

    g_hooks.initialized = true;
    TEKLOG_INFO("Hook system initialized successfully");
}

static void init_g_typed_hooks(void) {
    if (g_typed_hooks.initialized) return;
    tefstd_hashmap_init(&g_typed_hooks.method_to_stub, sizeof(void *), sizeof(void *));
    g_typed_hooks.initialized = true;
}

static patchlib_hook_handle_t *find_or_create_hook_handle(void *method) {
    if (!g_hooks.initialized) {
        TEKLOG_DEBUG("Hook system not initialized, initializing now");
        init_g_hooks();
    }

    TEKLOG_DEBUG("Looking for hook handle for method: %p", method);

    patchlib_hook_handle_t **handle_ptr = tefstd_hashmap_get(&g_hooks.method_to_handle, &method);
    if (handle_ptr && *handle_ptr) {
        TEKLOG_DEBUG("Found existing hook handle for method: %p", method);
        return *handle_ptr;
    }

    TEKLOG_INFO("Creating new hook handle for method: %p", method);

    patchlib_hook_handle_t *handle = malloc(sizeof(patchlib_hook_handle_t));
    if (!handle) {
        TEKLOG_ERROR("Failed to allocate memory for hook handle");
        return NULL;
    }

    handle->method = method;
    handle->original = NULL;
    handle->stub = NULL;
    handle->closure = NULL;
    handle->trampoline_func = NULL;
    handle->is_hooked = false;
    handle->arg_types = NULL;
    handle->arg_types_count = 0;
    handle->return_ffi_type = NULL;

    tefstd_vector_init(&handle->prefixes, sizeof(patchlib_hook_cb_t));
    tefstd_vector_init(&handle->postfixes, sizeof(patchlib_hook_cb_t));

    if (!patchlib_method_get_signature(method, &handle->method_signature)) {
        TEKLOG_ERROR("Failed to get method signature for method: %p", method);
        tefstd_vector_destroy(&handle->prefixes);
        tefstd_vector_destroy(&handle->postfixes);
        free(handle);
        return NULL;
    }

    tefstd_hashmap_put(&g_hooks.method_to_handle, &method, &handle);
    tefstd_vector_push_back(&g_hooks.hook_handles, &handle);

    TEKLOG_DEBUG("Successfully created hook handle for method: %p", method);
    return handle;
}

static void *allocate_executable_memory(const size_t size) {
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        TEKLOG_ERROR("Failed to get page size: %s", strerror(errno));
        return NULL;
    }

    const size_t mapped_size = (size + page_size - 1) & ~(page_size - 1);

    size_t alignment;
    const char *arch_name;

#if defined(__aarch64__)
    alignment = 16;
    arch_name = "ARM64";
#elif defined(__arm__)
    alignment = 8;
    arch_name = "ARM32";
#else
    alignment = 16;
    arch_name = "Unknown";
#endif

    TEKLOG_DEBUG("Allocating executable memory for %s: size=%zu, page_size=%ld, alignment=%zu",
                 arch_name, size, page_size, alignment);

    const size_t total_size = mapped_size + alignment - 1;

    void *ptr = mmap(NULL, total_size,
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (ptr == MAP_FAILED) {
        TEKLOG_ERROR("mmap failed: %s (errno=%d)", strerror(errno), errno);
        return NULL;
    }

    const uintptr_t aligned_addr = ((uintptr_t) ptr + alignment - 1) & ~(alignment - 1);
    void *aligned_ptr = (void *) aligned_addr;

    if (((uintptr_t) aligned_ptr & (alignment - 1)) != 0) {
        TEKLOG_ERROR("Memory not properly aligned for %s: %p (required alignment: %zu)",
                     arch_name, aligned_ptr, alignment);
        munmap(ptr, total_size);
        return NULL;
    }

    TEKLOG_DEBUG("Allocated executable memory: raw=%p aligned=%p size=%zu",
                 ptr, aligned_ptr, total_size);
    return aligned_ptr;
}

static void free_executable_memory(void *ptr, const size_t size) {
    if (ptr) {
        const long page_size = sysconf(_SC_PAGESIZE);
        const size_t mapped_size = (size + page_size - 1) & ~(page_size - 1);
        munmap(ptr, mapped_size);
        TEKLOG_INFO("Release executable memory: %p", ptr);
    }
}

/// 调用一个 Prefix 回调；命中 *_ex 版本时把 user 放在第一个参数。
static inline bool invoke_prefix(const patchlib_hook_cb_t *cb, patch_handle_t instance,
                                 void **args, const patch_method_signature_t *sig, void *ret) {
    if (!cb || !cb->fn)
        return false;
    if (cb->is_ex) {
        const prefix_callback_ex_t fn = (prefix_callback_ex_t) cb->fn;
        return fn(cb->user, instance, args, sig, ret);
    }
    const prefix_callback_t fn = (prefix_callback_t) cb->fn;
    return fn(instance, args, sig, ret);
}

/// 调用一个 Postfix 回调；命中 *_ex 版本时把 user 放在第一个参数。
static inline void invoke_postfix(const patchlib_hook_cb_t *cb, patch_handle_t instance,
                                  void **args, void *ret, const patch_method_signature_t *sig) {
    if (!cb || !cb->fn)
        return;
    if (cb->is_ex) {
        const postfix_callback_ex_t fn = (postfix_callback_ex_t) cb->fn;
        fn(cb->user, instance, args, ret, sig);
        return;
    }
    const postfix_callback_t fn = (postfix_callback_t) cb->fn;
    fn(instance, args, ret, sig);
}

static void hook_dispatcher_static(ffi_cif *cif, void *ret, void **args, void *user_data) {
    const patchlib_hook_handle_t *handle = user_data;

    bool call_original = true;

    const size_t prefix_count = tefstd_vector_size(&handle->prefixes);
    for (size_t i = 0; i < prefix_count; ++i) {
        const patchlib_hook_cb_t *cb = tefstd_vector_at(&handle->prefixes, i);
        if (invoke_prefix(cb, NULL, args, &handle->method_signature, ret)) {
            call_original = false;
            break;
        }
    }

    if (call_original) {
        ffi_call(cif, FFI_FN(handle->original), ret, args);
    }

    const size_t postfix_count = tefstd_vector_size(&handle->postfixes);
    for (size_t i = 0; i < postfix_count; ++i) {
        const patchlib_hook_cb_t *cb = tefstd_vector_at(&handle->postfixes, i);
        invoke_postfix(cb, NULL, args, ret, &handle->method_signature);
    }
}

static void hook_dispatcher(ffi_cif *cif, void *ret, void **args, void *user_data) {
    const patchlib_hook_handle_t *handle = user_data;

    void *instance = *(void **) args[0];
    void **actual_args = args + 1;

    bool call_original = true;

    const size_t prefix_count = tefstd_vector_size(&handle->prefixes);
    for (size_t i = 0; i < prefix_count; ++i) {
        const patchlib_hook_cb_t *cb = tefstd_vector_at(&handle->prefixes, i);
        if (invoke_prefix(cb, instance, actual_args, &handle->method_signature, ret)) {
            call_original = false;
            break;
        }
    }

    if (call_original) {
        ffi_call(cif, FFI_FN(handle->original), ret, args);
    }

    const size_t postfix_count = tefstd_vector_size(&handle->postfixes);
    for (size_t i = 0; i < postfix_count; ++i) {
        const patchlib_hook_cb_t *cb = tefstd_vector_at(&handle->postfixes, i);
        invoke_postfix(cb, instance, actual_args, ret, &handle->method_signature);
    }
}

static bool create_closure_from_signature(patchlib_hook_handle_t *handle) {
    void (*hook_callback)(ffi_cif *, void *, void **, void *);
    if (handle->method_signature.is_instance)
        hook_callback = (void (*)(ffi_cif *, void *, void **, void *)) hook_dispatcher;
    else
        hook_callback = (void (*)(ffi_cif *, void *, void **, void *)) hook_dispatcher_static;

    ffi_type *re_type = patch_type_to_ffi_type(handle->method_signature.return_type,
                                               handle->method_signature.return_type_handle);

    const size_t args_count = tefstd_vector_size(&handle->method_signature.arg_types);
    const size_t total_args = args_count
                              + (handle->method_signature.is_instance ? 1 : 0)
                              + (il2cpp_method_is_generic(handle->method) ? 1 : 0);

    ffi_type **arg_types = malloc(total_args * sizeof(ffi_type *));
    if (!arg_types) {
        TEKLOG_ERROR("Failed to allocate memory for argument types");
        return false;
    }

    size_t arg_index = 0;
    if (handle->method_signature.is_instance) {
        arg_types[arg_index++] = &ffi_type_pointer;
    }

    for (size_t i = 0; i < args_count; ++i) {
        patch_type_t *type = tefstd_vector_at(&handle->method_signature.arg_types, i);
        patch_handle_t *type_handle = tefstd_vector_at(&handle->method_signature.arg_type_handles, i);

        if (!type) {
            TEKLOG_ERROR("Failed to get argument type at index %zu", i);
            free(arg_types);
            return false;
        }
        arg_types[arg_index++] = patch_type_to_ffi_type(*type, *type_handle);
    }

    if (il2cpp_method_is_generic(handle->method))
        arg_types[arg_index++] = &ffi_type_pointer;

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
    void *exec_mem = allocate_executable_memory(closure_size);
    if (!exec_mem) {
        TEKLOG_ERROR("Failed to allocate executable memory");
        free(arg_types);
        return false;
    }

    handle->closure = exec_mem;
    handle->trampoline_func = exec_mem;
    handle->arg_types = arg_types;
    handle->arg_types_count = total_args;
    handle->return_ffi_type = re_type;

    status = ffi_prep_closure_loc(handle->closure, &handle->cif, hook_callback, handle,
                                  handle->trampoline_func);
    if (status != FFI_OK) {
        TEKLOG_ERROR("ffi_prep_closure_loc failed with status: %d", status);
        free_executable_memory(exec_mem, closure_size);
        free(arg_types);
        handle->closure = NULL;
        handle->trampoline_func = NULL;
        handle->arg_types = NULL;
        handle->arg_types_count = 0;
        handle->return_ffi_type = NULL;
        return false;
    }

    TEKLOG_DEBUG("Successfully created closure for method: %p", handle->method);
    return true;
}

/// 从向量中移除一个回调条目；按 (fn, user, is_ex) 精确匹配，
/// 因此同一函数用不同 user 安装多次时也能各自卸载。
static bool remove_callback(tefstd_vector_t *vec, const patchlib_hook_cb_t *cb) {
    const size_t count = tefstd_vector_size(vec);
    for (size_t i = 0; i < count; ++i) {
        const patchlib_hook_cb_t *cur = tefstd_vector_at(vec, i);
        if (cur && cur->fn == cb->fn && cur->user == cb->user && cur->is_ex == cb->is_ex) {
            tefstd_vector_erase(vec, i, NULL);
            return true;
        }
    }
    return false;
}

static patch_hook_id_t install_prepost_internal(patch_handle_t method, void *prefix, void *postfix,
                                                void *user, bool is_ex) {
    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Cannot install hook: method is NULL");
        return PATCH_HOOK_INVALID_ID;
    }

    if (!prefix && !postfix) {
        TEKLOG_ERROR("Cannot install hook: both prefix and postfix are NULL");
        return PATCH_HOOK_INVALID_ID;
    }

    patchlib_hook_handle_t *handle = find_or_create_hook_handle(method);
    if (!handle) {
        TEKLOG_ERROR("Failed to create hook handle for method: %p", method);
        return PATCH_HOOK_INVALID_ID;
    }

    if (!handle->is_hooked) {
        if (!create_closure_from_signature(handle)) {
            TEKLOG_ERROR("Failed to create closure for method: %p", method);
            return PATCH_HOOK_INVALID_ID;
        }

        void *target = patchlib_method_get_pointer(method);
        if (!target) {
            TEKLOG_ERROR("Method pointer is NULL for method: %p", method);
            return PATCH_HOOK_INVALID_ID;
        }

        void *stub = shadowhook_hook_func_addr(target, handle->trampoline_func,
                                               &handle->original);
        if (!stub) {
            TEKLOG_ERROR("shadowhook_hook_func_addr failed: %s",
                         shadowhook_to_errmsg(shadowhook_get_errno()));
            return PATCH_HOOK_INVALID_ID;
        }

        handle->stub = stub;
        handle->is_hooked = true;
        TEKLOG_DEBUG("ShadowHook installed: target=%p stub=%p original=%p",
                     target, stub, handle->original);
    }

    patchlib_hook_node_t *node = malloc(sizeof(patchlib_hook_node_t));
    if (!node) {
        TEKLOG_ERROR("Failed to allocate memory for hook node");
        return PATCH_HOOK_INVALID_ID;
    }

    node->id = g_hooks.next_hook_id++;
    node->handle = handle;
    node->prefix = (patchlib_hook_cb_t) { .fn = prefix, .user = user, .is_ex = is_ex };
    node->postfix = (patchlib_hook_cb_t) { .fn = postfix, .user = user, .is_ex = is_ex };

    TEKLOG_DEBUG("Created hook node ID: %d (ex=%d)", node->id, is_ex);

    if (prefix) {
        tefstd_vector_push_back(&handle->prefixes, &node->prefix);
        TEKLOG_DEBUG("Added prefix callback: %p", prefix);
    }
    if (postfix) {
        tefstd_vector_push_back(&handle->postfixes, &node->postfix);
        TEKLOG_DEBUG("Added postfix callback: %p", postfix);
    }

    tefstd_hashmap_put(&g_hooks.hooks, &node->id, &node);
    TEKLOG_INFO("Hook installed successfully with ID: %d for method: %p", node->id, method);

    return node->id;
}

patch_hook_id_t patchlib_install_prepost_hook(patch_handle_t method, prefix_callback_t prefix,
                                              postfix_callback_t postfix) {
    return install_prepost_internal(method, (void *) prefix, (void *) postfix, NULL, false);
}

patch_hook_id_t patchlib_install_prepost_hook_ex(patch_handle_t method, prefix_callback_ex_t prefix,
                                                 postfix_callback_ex_t postfix, void *user) {
    return install_prepost_internal(method, (void *) prefix, (void *) postfix, user, true);
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

    patchlib_hook_node_t **node_ptr = tefstd_hashmap_get(&g_hooks.hooks, &hook_id);
    if (!node_ptr || !*node_ptr) {
        TEKLOG_ERROR("Hook node not found for ID: %d", hook_id);
        return false;
    }

    patchlib_hook_node_t *node = *node_ptr;
    patchlib_hook_handle_t *handle = node->handle;

    TEKLOG_DEBUG("Found hook node for method: %p", handle->method);

    if (node->prefix.fn) {
        const bool removed = remove_callback(&handle->prefixes, &node->prefix);
        TEKLOG_DEBUG("Removed prefix callback: %p (success: %s)", node->prefix.fn,
                     removed ? "true" : "false");
    }
    if (node->postfix.fn) {
        const bool removed = remove_callback(&handle->postfixes, &node->postfix);
        TEKLOG_DEBUG("Removed postfix callback: %p (success: %s)", node->postfix.fn,
                     removed ? "true" : "false");
    }

    const size_t prefix_count = tefstd_vector_size(&handle->prefixes);
    const size_t postfix_count = tefstd_vector_size(&handle->postfixes);

    if (prefix_count == 0 && postfix_count == 0 && handle->is_hooked) {
        TEKLOG_INFO("No more hooks for method %p, cleaning up", handle->method);

        if (handle->stub) {
            int rc = shadowhook_unhook(handle->stub);
            if (rc != 0) {
                TEKLOG_ERROR("shadowhook_unhook failed: %s", shadowhook_to_errmsg(rc));
            }
            handle->stub = NULL;
        }

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

    tefstd_hashmap_del(&g_hooks.hooks, &hook_id);
    TEKLOG_DEBUG("Removed hook node from global table");

    if (tefstd_vector_size(&handle->prefixes) == 0 &&
        tefstd_vector_size(&handle->postfixes) == 0) {

        TEKLOG_INFO("Handle is empty, cleaning up completely");

        tefstd_hashmap_del(&g_hooks.method_to_handle, &handle->method);

        for (size_t i = 0; i < tefstd_vector_size(&g_hooks.hook_handles); i++) {
            patchlib_hook_handle_t **handle_ptr = tefstd_vector_at(&g_hooks.hook_handles, i);
            if (handle_ptr && *handle_ptr == handle) {
                tefstd_vector_erase(&g_hooks.hook_handles, i, NULL);
                TEKLOG_DEBUG("Removed handle from hook_handles vector");
                break;
            }
        }

        for (size_t i = 0; i < handle->arg_types_count; i++) {
            patch_handle_t *h = tefstd_vector_at(&handle->method_signature.arg_type_handles, i);
            if (handle->arg_types && h) {
                patch_ffi_type_free(handle->arg_types[i], *h);
            }
        }

        if (handle->return_ffi_type) {
            patch_ffi_type_free(handle->return_ffi_type,
                                handle->method_signature.return_type_handle);
        }

        free(handle->arg_types);
        handle->arg_types = NULL;

        patchlib_method_signature_free(&handle->method_signature);

        tefstd_vector_destroy(&handle->prefixes);
        tefstd_vector_destroy(&handle->postfixes);
        free(handle);
        TEKLOG_INFO("Handle memory freed");
    }

    free(node);
    TEKLOG_INFO("Hook uninstalled successfully: %d", hook_id);
    return true;
}

bool patchlib_install_typed_hook(patch_handle_t method, void *detour, void **original) {
    if (!patchlib_is_valid(method) || !detour || !original) {
        TEKLOG_ERROR("install_typed_hook: invalid argument (method=%p detour=%p original=%p)",
                     (void *) method, detour, (void *) original);
        return false;
    }

    void *address = patchlib_method_get_pointer(method);
    if (!address) {
        TEKLOG_ERROR("install_typed_hook: method pointer is NULL");
        return false;
    }

    init_g_typed_hooks();

    void *stub = shadowhook_hook_func_addr(address, detour, original);
    if (!stub) {
        TEKLOG_ERROR("install_typed_hook: shadowhook_hook_func_addr failed: %s",
                     shadowhook_to_errmsg(shadowhook_get_errno()));
        return false;
    }

    tefstd_hashmap_put(&g_typed_hooks.method_to_stub, &method, &stub);
    TEKLOG_INFO("install_typed_hook: installed at %p (stub=%p original=%p)",
                address, stub, *original);
    return true;
}

bool patchlib_uninstall_typed_hook(patch_handle_t method) {
    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("uninstall_typed_hook: invalid method");
        return false;
    }

    if (!g_typed_hooks.initialized) {
        TEKLOG_ERROR("uninstall_typed_hook: typed hook system not initialized");
        return false;
    }

    void **stub_ptr = tefstd_hashmap_get(&g_typed_hooks.method_to_stub, &method);
    if (!stub_ptr || !*stub_ptr) {
        TEKLOG_ERROR("uninstall_typed_hook: no typed hook registered for method %p",
                     (void *) method);
        return false;
    }

    void *stub = *stub_ptr;
    int rc = shadowhook_unhook(stub);
    if (rc != 0) {
        TEKLOG_ERROR("uninstall_typed_hook: shadowhook_unhook failed: %s",
                     shadowhook_to_errmsg(rc));
        return false;
    }

    tefstd_hashmap_del(&g_typed_hooks.method_to_stub, &method);
    TEKLOG_INFO("uninstall_typed_hook: uninstalled for method %p", (void *) method);
    return true;
}