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
#include <dobby.h>

#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>
#include <vector>

#include "patchlib/method.h"

#include "tef/core/log.hpp"
#include "private.hpp"

extern "C" {
#include "../il2cpp_api.h"
}

using tef::log::debug;
using tef::log::error;
using tef::log::info;
using tef::log::warn;

namespace {

/* ============================================================================
 * ffi_call 保护（原 ffi_guard.cpp 内联）
 *
 * 托管方法抛异常（Il2CppExceptionWrapper）会穿过 libffi 帧；libffi 的
 * closure 帧没有 CFI → 异常展开失败 → std::terminate → SIGABRT。
 * 就地 catch(...) 保证调用失败而不是崩游戏。
 * ============================================================================ */

constexpr void call_original_guarded(ffi_cif* cif, void* fn, void* ret, void** args) noexcept {
    try {
        ffi_call(cif, FFI_FN(fn), ret, args);
    } catch (...) {
        /* 托管异常：跳过原函数，postfix 照常执行 */
        error("Managed exception caught in hook dispatcher");
    }
}

/* ============================================================================
 * Hook 句柄 / 节点
 * ============================================================================ */

struct hook_handle {
    patch_handle_t method = nullptr;
    void* original = nullptr;
    void* trampoline = nullptr;
    void* closure    = nullptr;
    ffi_cif cif{};
    patch_method_signature_t sig{};
    bool has_sig = false;
    bool hooked  = false;

    std::vector<prefix_callback_t>  prefixes;
    std::vector<postfix_callback_t> postfixes;

    ffi_type** arg_types = nullptr;
    size_t     arg_types_count = 0;
    ffi_type*  return_ffi_type = nullptr;
};

struct hook_node {
    patch_hook_id_t    id = PATCH_HOOK_INVALID_ID;
    hook_handle*        handle = nullptr;
    prefix_callback_t  prefix = nullptr;
    postfix_callback_t postfix = nullptr;
};

struct hook_state {
    std::unordered_map<void*, hook_handle*>           by_method;
    std::unordered_map<patch_hook_id_t, hook_node*>   by_id;
    patch_hook_id_t next_id = 0;

    ~hook_state() {
        for (auto& [_, h] : by_method) delete h;
        for (auto& [_, n] : by_id)     delete n;
    }
};

hook_state& hooks() {
    static hook_state s;
    return s;
}

/* ============================================================================
 * 可执行内存
 * ============================================================================ */

void* allocate_executable(const size_t size) {
    const long page = sysconf(_SC_PAGESIZE);
    if (page <= 0) {
        error("sysconf(_SC_PAGESIZE) failed");
        return nullptr;
    }
    const size_t mapped = (size + page - 1) & ~static_cast<size_t>(page - 1);

#if defined(__aarch64__)
    constexpr size_t align = 16;
#elif defined(__arm__)
    constexpr size_t align = 8;
#else
    constexpr size_t align = 16;
#endif

    void* raw = mmap(nullptr, mapped + align - 1,
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) {
        error("mmap failed");
        return nullptr;
    }

    const uintptr_t a =
        (reinterpret_cast<uintptr_t>(raw) + align - 1) & ~(align - 1);
    return reinterpret_cast<void*>(a);
}

void free_executable(void* p, const size_t size) {
    if (!p) return;
    const long page = sysconf(_SC_PAGESIZE);
    const size_t mapped = (size + page - 1) & ~static_cast<size_t>(page - 1);
    munmap(p, mapped);
}

/* ============================================================================
 * 跳板
 * ============================================================================ */

void dispatcher_static(ffi_cif* cif, void* ret, void** args, void* user) {
    const auto* h = static_cast<hook_handle*>(user);
    bool call_original = true;

    for (auto* p : h->prefixes) {
        if (p && p(nullptr, args, &h->sig, ret)) {
            call_original = false;
            break;
        }
    }
    if (call_original) call_original_guarded(cif, h->original, ret, args);
    for (auto* p : h->postfixes) {
        if (p) p(nullptr, args, ret, &h->sig);
    }
}

void dispatcher_instance(ffi_cif* cif, void* ret, void** args, void* user) {
    const auto* h = static_cast<hook_handle*>(user);
    void* instance = *static_cast<void**>(args[0]);
    void** actual = args + 1;
    bool call_original = true;

    for (auto* p : h->prefixes) {
        if (p && p(instance, actual, &h->sig, ret)) {
            call_original = false;
            break;
        }
    }
    if (call_original) call_original_guarded(cif, h->original, ret, args);
    for (auto* p : h->postfixes) {
        if (p) p(instance, actual, ret, &h->sig);
    }
}

/* ============================================================================
 * 创建 / 销毁闭包
 * ============================================================================ */

bool create_closure(hook_handle* h) {
    if (!h || h->hooked) return true;

    void(*cb)(ffi_cif*, void*, void**, void*) =
        h->sig.is_instance ? dispatcher_instance : dispatcher_static;

    h->return_ffi_type = patch_type_to_ffi_type(h->sig.return_type,
                                                 h->sig.return_type_handle);

    const size_t explicit_count = tefstd_vector_size(&h->sig.arg_types);
    const size_t total = explicit_count
                       + (h->sig.is_instance ? 1 : 0)
                       + (il2cpp_method_is_generic(h->method) ? 1 : 0);

    h->arg_types = static_cast<ffi_type**>(
        std::malloc(total * sizeof(ffi_type*)));
    if (!h->arg_types) {
        error("malloc arg_types failed");
        return false;
    }

    size_t idx = 0;
    if (h->sig.is_instance) h->arg_types[idx++] = &ffi_type_pointer;

    for (size_t i = 0; i < explicit_count; ++i) {
        const auto* at = static_cast<patch_type_t*>(tefstd_vector_at(&h->sig.arg_types, i));
        const auto* ah = static_cast<patch_handle_t*>(tefstd_vector_at(&h->sig.arg_type_handles, i));
        h->arg_types[idx++] = patch_type_to_ffi_type(*at, ah ? *ah : nullptr);
    }

    if (il2cpp_method_is_generic(h->method))
        h->arg_types[idx++] = &ffi_type_pointer;

    h->arg_types_count = total;

    const ffi_abi abi = FFI_SYSV;
    if (ffi_prep_cif(&h->cif, abi, static_cast<unsigned>(total),
                     h->return_ffi_type, h->arg_types) != FFI_OK) {
        error("ffi_prep_cif failed");
        std::free(h->arg_types);
        h->arg_types = nullptr;
        return false;
    }

    constexpr size_t closure_size = sizeof(ffi_closure);
    void* mem = allocate_executable(closure_size);
    if (!mem) {
        std::free(h->arg_types);
        h->arg_types = nullptr;
        return false;
    }

    h->closure    = mem;
    h->trampoline = mem;

    if (ffi_prep_closure_loc(static_cast<ffi_closure*>(h->closure),
                             &h->cif, cb, h, h->trampoline) != FFI_OK) {
        free_executable(mem, closure_size);
        h->closure = h->trampoline = nullptr;
        std::free(h->arg_types);
        h->arg_types = nullptr;
        return false;
    }

    if (void* method_ptr = patchlib_method_get_pointer(h->method); !method_ptr ||
                                                                   DobbyHook(method_ptr,
                                                                             h->trampoline,
                                                                             &h->original) != 0) {
        free_executable(h->closure, closure_size);
        h->closure = h->trampoline = nullptr;
        std::free(h->arg_types);
        h->arg_types = nullptr;
        return false;
    }

    h->hooked = true;
    return true;
}

void destroy_closure(hook_handle* h) {
    if (!h || !h->hooked) return;

    if (void* method_ptr = patchlib_method_get_pointer(h->method)) DobbyDestroy(method_ptr);

    if (h->closure) free_executable(h->closure, sizeof(ffi_closure));

    if (h->arg_types) {
        const size_t explicit_count = tefstd_vector_size(&h->sig.arg_types);
        for (size_t i = 0; i < explicit_count && i < h->arg_types_count; ++i) {
            auto* ah = static_cast<patch_handle_t*>(tefstd_vector_at(&h->sig.arg_type_handles, i));
            if (ah && h->arg_types[i]) patch_ffi_type_free(h->arg_types[i], *ah);
        }
        std::free(h->arg_types);
        h->arg_types = nullptr;
    }
    if (h->return_ffi_type) {
        patch_ffi_type_free(h->return_ffi_type, h->sig.return_type_handle);
        h->return_ffi_type = nullptr;
    }

    h->closure = h->trampoline = h->original = nullptr;
    h->hooked = false;
}

hook_handle* get_or_create(patch_handle_t method) {
    auto& s = hooks();
    if (const auto it = s.by_method.find(method); it != s.by_method.end()) return it->second;

    auto* h = new hook_handle();
    h->method = method;
    if (!patchlib_method_get_signature(method, &h->sig)) {
        delete h;
        return nullptr;
    }
    h->has_sig = true;
    s.by_method[method] = h;
    return h;
}

} // namespace

/* ============================================================================
 * 导出函数
 * ============================================================================ */

extern "C" {

patch_hook_id_t patchlib_install_prepost_hook(
    patch_handle_t method, prefix_callback_t prefix, postfix_callback_t postfix) {
    if (!patchlib_is_valid(method) || (!prefix && !postfix))
        return PATCH_HOOK_INVALID_ID;

    auto&[by_method, by_id, next_id] = hooks();
    hook_handle* h = get_or_create(method);
    if (!h) return PATCH_HOOK_INVALID_ID;

    if (!h->hooked && !create_closure(h)) {
        by_method.erase(method);
        if (h->has_sig) patchlib_method_signature_free(&h->sig);
        delete h;
        return PATCH_HOOK_INVALID_ID;
    }

    auto* node = new hook_node();
    node->id      = next_id++;
    node->handle  = h;
    node->prefix  = prefix;
    node->postfix = postfix;

    if (prefix)  h->prefixes.push_back(prefix);
    if (postfix) h->postfixes.push_back(postfix);

    by_id[node->id] = node;
    info("Hook installed: method={} id={}", fmt::ptr(method), node->id);
    return node->id;
}

bool patchlib_uninstall_hook(const patch_hook_id_t hook_id) {
    auto& s = hooks();
    if (hook_id == PATCH_HOOK_INVALID_ID) return false;

    const auto it = s.by_id.find(hook_id);
    if (it == s.by_id.end()) return false;

    const hook_node* node = it->second;
    hook_handle* h  = node->handle;

    if (node->prefix)
        std::erase(h->prefixes, node->prefix);
    if (node->postfix)
        std::erase(h->postfixes, node->postfix);

    if (h->prefixes.empty() && h->postfixes.empty() && h->hooked)
        destroy_closure(h);

    s.by_id.erase(it);
    delete node;

    if (h->prefixes.empty() && h->postfixes.empty()) {
        s.by_method.erase(h->method);
        if (h->has_sig) patchlib_method_signature_free(&h->sig);
        delete h;
    }

    info("Hook uninstalled: id={}", static_cast<int>(hook_id));
    return true;
}

bool patchlib_install_typed_hook(patch_handle_t method, void* detour,
                                 void** original) {
    if (!patchlib_is_valid(method) || !detour || !original) {
        error("install_typed_hook: invalid argument");
        return false;
    }

    void* addr = patchlib_method_get_pointer(method);
    if (!addr) {
        error("install_typed_hook: method pointer is NULL");
        return false;
    }

    if (DobbyHook(addr,
                  detour,
                  original) != 0) {
        error("install_typed_hook: DobbyHook failed at {}", fmt::ptr(addr));
        return false;
    }
    info("install_typed_hook: installed at {} (original={})",
         fmt::ptr(addr), fmt::ptr(*original));
    return true;
}

bool patchlib_uninstall_typed_hook(patch_handle_t method) {
    if (!patchlib_is_valid(method)) {
        error("uninstall_typed_hook: invalid method");
        return false;
    }

    void* addr = patchlib_method_get_pointer(method);
    if (!addr) {
        error("uninstall_typed_hook: method pointer is NULL");
        return false;
    }

    if (DobbyDestroy(addr) != 0) {
        error("uninstall_typed_hook: DobbyDestroy failed at {}", fmt::ptr(addr));
        return false;
    }
    info("uninstall_typed_hook: uninstalled at {}", fmt::ptr(addr));
    return true;
}

}