/*******************************************************************************
 * tefkernel - method
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
 * along with this program. NoCMakeLists.txtyet, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2025/11/23
 *******************************************************************************/

#include "patchlib/method.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "internal/log.h"
#include "patchlib/il2cpp_api.h"

bool patchlib_method_invoke_args(patch_handle_t method, patch_handle_t instance,
                                 void *return_value, void** args) {
    TEKLOG_DEBUG("patchlib_method_invoke_args (Desktop): method=%p, instance=%p, return_value=%p",
                 method, instance, return_value);

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle");
        return false;
    }

    bool is_constructor = false;
    const char* method_name = il2cpp_method_get_name(method);
    if (method_name != NULL && strcmp(method_name, ".ctor") == 0) {
        is_constructor = true;
        TEKLOG_DEBUG("Method is constructor, skipping instance validation");
    }

    // 验证实例方法调用（构造函数除外）
    if (!is_constructor && !patchlib_method_is_static(method) && !patchlib_is_valid(instance)) {
        TEKLOG_ERROR("Instance method requires valid instance");
        return false;
    }

    // 直接调用 il2cpp_runtime_invoke
    const bool result = il2cpp_method_invoke(method, instance, args, return_value);

    if (!result) {
        TEKLOG_ERROR("il2cpp_runtime_invoke failed");
        return false;
    }

    TEKLOG_DEBUG("Method invocation completed successfully");
    return true;
}

bool patchlib_constructor_invoke(patch_handle_t constructor,
                patch_handle_t *return_instance, void **args) {
    TEKLOG_DEBUG("patchlib_constructor_invoke: constructor=%p, return_instance=%p",
                 constructor, return_instance);

    if (!patchlib_is_valid(constructor)) {
        TEKLOG_ERROR("Invalid constructor handle");
        return false;
    }

    if (return_instance == NULL) {
        TEKLOG_ERROR("return_instance is NULL");
        return false;
    }

    *return_instance = PATCH_NULL;

    const bool result = patchlib_method_invoke_args(constructor, PATCH_NULL, return_instance, args);

    if (!result) {
        TEKLOG_ERROR("Constructor invocation failed");
        return false;
    }

    TEKLOG_DEBUG("Constructor invoked successfully, instance=%p", *return_instance);
    return true;
}

patch_handle_t patchlib_method_make_generic_instance(patch_handle_t method, const tefstd_vector_t *template_types) {
    TEKLOG_DEBUG("patchlib_method_make_generic_instance (Desktop): method=%p", method);

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle");
        return PATCH_NULL;
    }

    if (!template_types || tefstd_vector_size(template_types) == 0) {
        TEKLOG_ERROR("Invalid template types");
        return PATCH_NULL;
    }

    const size_t type_count = tefstd_vector_size(template_types);
    void** type_array = malloc(sizeof(void*) * type_count);
    if (!type_array) {
        TEKLOG_ERROR("Failed to allocate type array");
        return PATCH_NULL;
    }

    for (size_t i = 0; i < type_count; i++) {
        patch_handle_t* type_handle = tefstd_vector_at(template_types, i);
        if (type_handle && patchlib_is_valid(*type_handle)) {
            type_array[i] = *type_handle;
        } else {
            TEKLOG_ERROR("Invalid type at index %zu", i);
            free(type_array);
            return PATCH_NULL;
        }
    }

    patch_handle_t result = il2cpp_method_make_generic(method, type_array, (int)type_count);

    free(type_array);

    if (!patchlib_is_valid(result)) {
        TEKLOG_ERROR("il2cpp_method_make_generic failed");
        return PATCH_NULL;
    }

    TEKLOG_DEBUG("Generic method created successfully: %p", result);
    return result;
}

/* ---------------------------------------------------------------------------
 * 桌面端 patchlib_install_prepost_hook_ex
 *
 * tefloader 的 Harmony 只认普通回调（不带 user）。这里为每个 _ex Hook 生成
 * 一小段机器码跳板：把 (instance,args,sig/result) 重排成
 * (user,instance,args,...,result/sig)，再尾跳真正的 _ex 回调。
 *
 * 这样 LuaLoader 用「单分发函数 + user 槽位」的设计在 PC 上也能工作，
 * 不需要回到 1024 个静态函数的旧方案，也没有数量上限。
 *
 * 支持：x86_64 SysV(Linux)、x86_64 Win64、aarch64 AAPCS64。
 * 其它架构返回 INVALID（不影响库加载）。
 * ------------------------------------------------------------------------- */
typedef struct {
    patch_hook_id_t id;
    void *pre;
    void *post;
    size_t size;
} ex_tramp_t;

static ex_tramp_t *g_ex_tramps = NULL;
static size_t g_ex_count = 0;
static size_t g_ex_cap = 0;

static void *tef_alloc_exec(size_t size) {
#if defined(_WIN32) || defined(_WIN64)
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE,
                        PAGE_EXECUTE_READWRITE);
#else
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : p;
#endif
}

static void tef_free_exec(void *p, size_t size) {
    if (!p) return;
#if defined(_WIN32) || defined(_WIN64)
    (void) size;
    VirtualFree(p, 0, MEM_RELEASE);
#else
    munmap(p, size);
#endif
}

static void put_u64(unsigned char *p, uint64_t v) {
    memcpy(p, &v, sizeof(v));
}

static void *make_ex_tramp(const void *fn, void *user) {
    if (!fn) return NULL;
#if defined(__x86_64__) || defined(_M_X64)
    /* 两种 x86_64 ABI 的普通/带 user 参数顺序刚好都能用同一套重排：
     *   SysV : rdi,rsi,rdx,rcx        -> rdi=user,rsi,rdx,rcx,r8
     *   Win64: rcx,rdx,r8,r9(+stack)  -> rcx=user,rdx,r8,r9,5th=stack
     */
    const size_t size = 96;
    unsigned char *code = (unsigned char *) tef_alloc_exec(size);
    if (!code) return NULL;
    size_t n = 0;
#if defined(_WIN32) || defined(_WIN64)
    /* sub rsp, 0x38 */
    code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xEC; code[n++] = 0x38;
    /* mov [rsp+0x20], r9  (result/sig 放第 5 个参数) */
    code[n++] = 0x4C; code[n++] = 0x89; code[n++] = 0x4C; code[n++] = 0x24; code[n++] = 0x20;
    /* mov r9, r8 */
    code[n++] = 0x4D; code[n++] = 0x89; code[n++] = 0xC1;
    /* mov r8, rdx */
    code[n++] = 0x49; code[n++] = 0x89; code[n++] = 0xD0;
    /* mov rdx, rcx */
    code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0xCA;
    /* movabs rcx, user */
    code[n++] = 0x48; code[n++] = 0xB9; put_u64(code + n, (uint64_t) (uintptr_t) user); n += 8;
    /* movabs rax, fn */
    code[n++] = 0x48; code[n++] = 0xB8; put_u64(code + n, (uint64_t) (uintptr_t) fn); n += 8;
    /* call rax */
    code[n++] = 0xFF; code[n++] = 0xD0;
    /* add rsp, 0x38; ret */
    code[n++] = 0x48; code[n++] = 0x83; code[n++] = 0xC4; code[n++] = 0x38;
    code[n++] = 0xC3;
#else
    /* SysV: r8=rcx; rcx=rdx; rdx=rsi; rsi=rdi; rdi=user; jmp fn */
    code[n++] = 0x49; code[n++] = 0x89; code[n++] = 0xC8;
    code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0xD1;
    code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0xF2;
    code[n++] = 0x48; code[n++] = 0x89; code[n++] = 0xFE;
    code[n++] = 0x48; code[n++] = 0xBF; put_u64(code + n, (uint64_t) (uintptr_t) user); n += 8;
    code[n++] = 0x48; code[n++] = 0xB8; put_u64(code + n, (uint64_t) (uintptr_t) fn); n += 8;
    code[n++] = 0xFF; code[n++] = 0xE0;
#endif
    __builtin___clear_cache((char *) code, (char *) code + n);
    return code;
#elif defined(__aarch64__)
    /* x4=x3; x3=x2; x2=x1; x1=x0; x0=user; x9=fn; br x9 */
    const size_t size = 96;
    uint32_t *code = (uint32_t *) tef_alloc_exec(size);
    if (!code) return NULL;
    size_t n = 0;
    code[n++] = 0xAA0303E4u;  /* mov x4, x3 */
    code[n++] = 0xAA0203E3u;  /* mov x3, x2 */
    code[n++] = 0xAA0103E2u;  /* mov x2, x1 */
    code[n++] = 0xAA0003E1u;  /* mov x1, x0 */
    const uint64_t u = (uint64_t) (uintptr_t) user;
    const uint64_t f = (uint64_t) (uintptr_t) fn;
    code[n++] = 0xD2800000u | ((uint32_t) (u & 0xFFFF) << 5);
    code[n++] = 0xF2A00000u | ((uint32_t) ((u >> 16) & 0xFFFF) << 5);
    code[n++] = 0xF2C00000u | ((uint32_t) ((u >> 32) & 0xFFFF) << 5);
    code[n++] = 0xF2E00000u | ((uint32_t) ((u >> 48) & 0xFFFF) << 5);
    code[n++] = 0xD2800009u | ((uint32_t) (f & 0xFFFF) << 5);
    code[n++] = 0xF2A00009u | ((uint32_t) ((f >> 16) & 0xFFFF) << 5);
    code[n++] = 0xF2C00009u | ((uint32_t) ((f >> 32) & 0xFFFF) << 5);
    code[n++] = 0xF2E00009u | ((uint32_t) ((f >> 48) & 0xFFFF) << 5);
    code[n++] = 0xD61F0120u;  /* br x9 */
    __builtin___clear_cache((char *) code, (char *) code + n * 4);
    return code;
#else
    (void) fn; (void) user;
    return NULL;
#endif
}

static int remember_tramp(patch_hook_id_t id, void *pre, void *post, size_t size) {
    if (g_ex_count == g_ex_cap) {
        const size_t cap = g_ex_cap ? g_ex_cap * 2 : 16;
        ex_tramp_t *next = (ex_tramp_t *) realloc(g_ex_tramps, cap * sizeof(*next));
        if (!next) return 0;
        g_ex_tramps = next;
        g_ex_cap = cap;
    }
    g_ex_tramps[g_ex_count++] = (ex_tramp_t){ id, pre, post, size };
    return 1;
}

static void forget_tramp(patch_hook_id_t id) {
    for (size_t i = 0; i < g_ex_count; ++i) {
        if (g_ex_tramps[i].id != id) continue;
        tef_free_exec(g_ex_tramps[i].pre, g_ex_tramps[i].size);
        tef_free_exec(g_ex_tramps[i].post, g_ex_tramps[i].size);
        g_ex_tramps[i] = g_ex_tramps[--g_ex_count];
        return;
    }
}

patch_hook_id_t patchlib_install_prepost_hook_ex(patch_handle_t method,
        prefix_callback_ex_t prefix, postfix_callback_ex_t postfix, void *user) {
    if (!patchlib_is_valid(method)) return PATCH_HOOK_INVALID_ID;
    if (!prefix && !postfix) return PATCH_HOOK_INVALID_ID;

    void *pre = prefix ? make_ex_tramp((const void *) prefix, user) : NULL;
    void *post = postfix ? make_ex_tramp((const void *) postfix, user) : NULL;
    if ((prefix && !pre) || (postfix && !post)) {
        tef_free_exec(pre, 96);
        tef_free_exec(post, 96);
        TEKLOG_WARN("patchlib_install_prepost_hook_ex: unsupported arch or OOM");
        return PATCH_HOOK_INVALID_ID;
    }

    const patch_hook_id_t id = patchlib_install_prepost_hook(
            method, (prefix_callback_t) pre, (postfix_callback_t) post);
    if (id == PATCH_HOOK_INVALID_ID) {
        tef_free_exec(pre, 96);
        tef_free_exec(post, 96);
        return PATCH_HOOK_INVALID_ID;
    }
    if (!remember_tramp(id, pre, post, 96)) {
        TEKLOG_WARN("patchlib_install_prepost_hook_ex: track table OOM (hook works, tramp leaks)");
    }
    TEKLOG_DEBUG("patchlib_install_prepost_hook_ex: hook=%d pre=%p post=%p user=%p",
                 (int) id, pre, post, user);
    return id;
}

patch_hook_id_t patchlib_install_prepost_hook(patch_handle_t method, const prefix_callback_t prefix, const postfix_callback_t postfix) {
    TEKLOG_DEBUG("=== [START] patchlib_install_prepost_hook ===");
    TEKLOG_DEBUG("Parameters:");
    TEKLOG_DEBUG("  method: %p", method);
    TEKLOG_DEBUG("  prefix: %p", prefix);
    TEKLOG_DEBUG("  postfix: %p", postfix);

    // 记录函数指针的详细信息
    if (prefix) {
        TEKLOG_DEBUG("  prefix is valid function pointer at %p", prefix);
    } else {
        TEKLOG_DEBUG("  prefix is NULL");
    }

    if (postfix) {
        TEKLOG_DEBUG("  postfix is valid function pointer at %p", postfix);
     } else {
        TEKLOG_DEBUG("  postfix is NULL");
    }

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle: %p", method);
        TEKLOG_DEBUG("=== [END] patchlib_install_prepost_hook (invalid method) ===");
        return PATCH_HOOK_INVALID_ID;
    }

    if (prefix == NULL && postfix == NULL) {
        TEKLOG_ERROR("Both prefix and postfix callbacks are NULL");
        TEKLOG_DEBUG("=== [END] patchlib_install_prepost_hook (both callbacks NULL) ===");
        return PATCH_HOOK_INVALID_ID;
    }

    const bool is_hooked = il2cpp_is_method_hooked(method);
    TEKLOG_DEBUG("Method %p is already hooked: %s", method, is_hooked ? "true" : "false");

    patch_method_signature_t* sig = NULL;
    bool newly_allocated_sig = false;

    if (!is_hooked) {
        TEKLOG_DEBUG("Getting method signature for method %p (first time hook)", method);
        sig = malloc(sizeof(patch_method_signature_t));
        if (!sig) {
            TEKLOG_ERROR("Failed to allocate memory for signature");
            TEKLOG_DEBUG("=== [END] patchlib_install_prepost_hook (malloc failed) ===");
            return PATCH_HOOK_INVALID_ID;
        }
        newly_allocated_sig = true;

        TEKLOG_DEBUG("Allocated signature memory at %p, size: %zu bytes",
                     sig, sizeof(patch_method_signature_t));

        // 初始化签名结构体
        memset(sig, 0, sizeof(patch_method_signature_t));
        TEKLOG_DEBUG("Initialized signature struct to zeros");

        TEKLOG_DEBUG("Calling patchlib_method_get_signature for method %p", method);
        if (!patchlib_method_get_signature(method, sig)) {
            TEKLOG_ERROR("Failed to get method signature for method %p", method);
            patchlib_method_signature_free(sig);
            free(sig);
            TEKLOG_DEBUG("=== [END] patchlib_install_prepost_hook (get signature failed) ===");
            return PATCH_HOOK_INVALID_ID;
        }

        TEKLOG_DEBUG("Method signature obtained successfully:");
        TEKLOG_DEBUG("  - Signature pointer: %p", sig);
        TEKLOG_DEBUG("  - Return type: %d", sig->return_type);
        TEKLOG_DEBUG("  - Argument count: %zu", tefstd_vector_size(&sig->arg_types));

        // 打印参数类型
        for (size_t i = 0; i < tefstd_vector_size(&sig->arg_types); i++) {
            patch_handle_t* type_ptr = tefstd_vector_at(&sig->arg_types, i);
            if (type_ptr) {
                TEKLOG_DEBUG("  - Arg[%zu]: type handle %p", i, *type_ptr);
            } else {
                TEKLOG_DEBUG("  - Arg[%zu]: NULL", i);
            }
        }

        TEKLOG_DEBUG("Signature will be persisted for C# layer");
    } else {
        TEKLOG_DEBUG("Method %p is already hooked, retrieving existing signature", method);
        sig = il2cpp_get_hooked_method_sig(method);
        if (sig == NULL) {
            TEKLOG_ERROR("Failed to get hooked method signature for method %p", method);
            TEKLOG_DEBUG("=== [END] patchlib_install_prepost_hook (get cached sig failed) ===");
            return PATCH_HOOK_INVALID_ID;
        }
        TEKLOG_DEBUG("Retrieved existing signature for method %p: sig=%p", method, sig);
    }

    TEKLOG_DEBUG("Preparing to call il2cpp_hook_method:");
    TEKLOG_DEBUG("  - Method handle: %p", method);
    TEKLOG_DEBUG("  - Signature: %p", sig);
    TEKLOG_DEBUG("  - Prefix: %p", prefix);
    TEKLOG_DEBUG("  - Postfix: %p", postfix);

    const patch_hook_id_t hook_id = il2cpp_hook_method(method, sig, prefix, postfix);
    TEKLOG_DEBUG("il2cpp_hook_method returned: hook_id=%d", hook_id);

    if (hook_id == PATCH_HOOK_INVALID_ID) {
        TEKLOG_ERROR("il2cpp_hook_method failed for method %p", method);
        // 如果是第一次 Hook 且失败了，需要释放新分配的签名
        if (newly_allocated_sig && sig != NULL) {
            TEKLOG_DEBUG("Freeing newly allocated signature at %p (hook failed)", sig);
            patchlib_method_signature_free(sig);
            free(sig);
        }
        TEKLOG_DEBUG("=== [END] patchlib_install_prepost_hook (hook failed) ===");
        return PATCH_HOOK_INVALID_ID;
    }

    TEKLOG_DEBUG("Hook installed successfully:");
    TEKLOG_DEBUG("  - Method: %p", method);
    TEKLOG_DEBUG("  - Hook ID: %d", hook_id);
    TEKLOG_DEBUG("  - Signature stored at: %p", sig);
    TEKLOG_DEBUG("  - Newly allocated signature: %s", newly_allocated_sig ? "yes" : "no");

    TEKLOG_DEBUG("=== [END] patchlib_install_prepost_hook (success) ===");
    return hook_id;
}

bool patchlib_uninstall_hook(const patch_hook_id_t hook_id) {
    TEKLOG_DEBUG("patchlib_uninstall_hook called: hook_id=%d", hook_id);

    if (hook_id == PATCH_HOOK_INVALID_ID) {
        TEKLOG_ERROR("Invalid hook_id");
        return false;
    }

    // 获取方法句柄
    patch_handle_t method = il2cpp_get_method_by_hook_node(hook_id);
    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Failed to get method for hook_id %d", hook_id);
        return false;
    }

    // 检查是否只有一个 Hook 节点
    const bool single_node = il2cpp_has_single_hook_node(method);
    TEKLOG_DEBUG("Method %p has single hook node: %s", method, single_node ? "true" : "false");

    // 如果是最后一个 Hook，需要释放签名
    if (single_node) {
        patch_method_signature_t* sig = il2cpp_get_hooked_method_sig(method);
        if (sig != NULL) {
            TEKLOG_DEBUG("Freeing method signature at %p for method %p", sig, method);
            patchlib_method_signature_free(sig);
            free(sig);
        }
    }

    // 卸载 Hook
    const bool result = il2cpp_unhook_method(hook_id);

    if (result) {
        forget_tramp(hook_id);
        TEKLOG_DEBUG("Hook uninstalled successfully: hook_id=%d", hook_id);
    } else {
        TEKLOG_ERROR("Failed to uninstall hook: hook_id=%d", hook_id);
    }

    return result;
}