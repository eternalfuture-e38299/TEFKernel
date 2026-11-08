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
#include "patchlib/type.h"

#include <cstdlib>
#include <cstring>
#include <vector>

#include "tef/core/log.hpp"

extern "C" {
#include "../il2cpp_api.h"
}

using tef::log::debug;
using tef::log::error;

namespace {
    /* 管理 patch_method_signature_t* 的 RAII——
     *  首次 hook：new 出来，成功时放弃所有权（由 C# 加载器持有）
     *  再次 hook：从加载器取，不归我们释放
     *  失败：自动 patchlib_method_signature_free + free */
    struct signature_ptr {
        patch_method_signature_t *p = nullptr;

        ~signature_ptr() {
            if (p) {
                patchlib_method_signature_free(p);
                std::free(p);
            }
        }

        patch_method_signature_t *operator->() const noexcept { return p; }
        explicit operator bool() const noexcept { return p != nullptr; }

        void abandon() noexcept { p = nullptr; }
    };

    signature_ptr make_signature(patch_handle_t method) {
        signature_ptr s;
        s.p = static_cast<patch_method_signature_t *>(
            std::malloc(sizeof(patch_method_signature_t)));
        if (!s.p) return s;
        std::memset(s.p, 0, sizeof(patch_method_signature_t));
        if (!patchlib_method_get_signature(method, s.p)) {
            patchlib_method_signature_free(s.p);
            std::free(s.p);
            s.p = nullptr;
        }
        return s;
    }

    bool is_constructor(patch_handle_t method) {
        const char *name = il2cpp_method_get_name(method);
        return name && std::strcmp(name, ".ctor") == 0;
    }
} // namespace

extern "C" {
/* ============================================================================
 * 调用
 * ============================================================================ */

bool patchlib_method_invoke_args(patch_handle_t method, patch_handle_t instance,
                                 void *return_value, void **args) {
    debug("patchlib_method_invoke_args: method={} instance={}",
          fmt::ptr(method), fmt::ptr(instance));

    if (!patchlib_is_valid(method)) {
        error("Invalid method handle");
        return false;
    }

    if (const bool is_ctor = is_constructor(method);
        !is_ctor && !patchlib_method_is_static(method) && !patchlib_is_valid(instance)) {
        error("Instance method requires valid instance");
        return false;
    }

    if (const bool result = il2cpp_method_invoke(method, instance, args, return_value); !result) {
        error("il2cpp_method_invoke failed");
        return false;
    }
    return true;
}

bool patchlib_constructor_invoke(patch_handle_t ctor,
                                 patch_handle_t *return_instance, void **args) {
    debug("patchlib_constructor_invoke: ctor={} out={}",
          fmt::ptr(ctor), fmt::ptr(return_instance));

    if (!patchlib_is_valid(ctor)) {
        error("Invalid constructor handle");
        return false;
    }
    if (!return_instance) {
        error("return_instance is NULL");
        return false;
    }

    *return_instance = nullptr;
    if (!patchlib_method_invoke_args(ctor, nullptr, return_instance, args)) {
        error("Constructor invocation failed");
        return false;
    }
    debug("Constructor invoked: {}", fmt::ptr(*return_instance));
    return true;
}

/* ============================================================================
 * 泛型实例化（桌面：il2cpp_method_make_generic）
 * ============================================================================ */

patch_handle_t patchlib_method_make_generic_instance(
    patch_handle_t method, const tefstd_vector_t *template_types) {
    debug("patchlib_method_make_generic_instance: method={}", fmt::ptr(method));

    if (!patchlib_is_valid(method)) {
        error("Invalid method handle");
        return nullptr;
    }
    if (!template_types || tefstd_vector_size(template_types) == 0) {
        error("Invalid template types");
        return nullptr;
    }

    const size_t n = tefstd_vector_size(template_types);
    std::vector<void *> type_array(n);
    for (size_t i = 0; i < n; ++i) {
        const auto h = static_cast<patch_handle_t *>(tefstd_vector_at(template_types, i));
        if (!h || !patchlib_is_valid(*h)) {
            error("Invalid type at index {}", i);
            return nullptr;
        }
        type_array[i] = *h;
    }

    patch_handle_t result = il2cpp_method_make_generic(
        method, type_array.data(), static_cast<int>(n));
    if (!patchlib_is_valid(result)) {
        error("il2cpp_method_make_generic failed");
        return nullptr;
    }
    debug("Generic method created: {}", fmt::ptr(result));
    return result;
}

/* ============================================================================
 * Hook（桌面：直接调 C# 加载器的 il2cpp_hook_method）
 *
 * 与 Android 的 Dobby + FFI 闭包不同：桌面端由 C# 加载器负责跳板生成、
 * 回调编组、原始方法 trampoline。C 侧只负责签名生命周期。
 * ============================================================================ */

patch_hook_id_t patchlib_install_prepost_hook(
    patch_handle_t method, const prefix_callback_t prefix, const postfix_callback_t postfix) {
    debug("patchlib_install_prepost_hook: method={} prefix={} postfix={}",
          fmt::ptr(method), fmt::ptr(reinterpret_cast<void *>(prefix)),
          fmt::ptr(reinterpret_cast<void *>(postfix)));

    if (!patchlib_is_valid(method)) {
        error("Invalid method handle");
        return PATCH_HOOK_INVALID_ID;
    }
    if (!prefix && !postfix) {
        error("Both prefix and postfix are NULL");
        return PATCH_HOOK_INVALID_ID;
    }

    const bool already = il2cpp_is_method_hooked(method);
    debug("Method {} already hooked: {}", fmt::ptr(method), already);

    signature_ptr sig;
    bool newly_allocated = false;

    if (!already) {
        sig = make_signature(method);
        if (!sig) {
            error("Failed to get signature for {}", fmt::ptr(method));
            return PATCH_HOOK_INVALID_ID;
        }
        newly_allocated = true;
    } else {
        sig.p = static_cast<patch_method_signature_t *>(il2cpp_get_hooked_method_sig(method));
        if (!sig) {
            error("Failed to get cached signature for {}", fmt::ptr(method));
            return PATCH_HOOK_INVALID_ID;
        }
    }

    const patch_hook_id_t id = il2cpp_hook_method(method, sig.p, reinterpret_cast<void *>(prefix),
                                                  reinterpret_cast<void *>(postfix));
    if (id == PATCH_HOOK_INVALID_ID) {
        error("il2cpp_hook_method failed for {}", fmt::ptr(method));
        /* 首次 hook 失败：SignaturePtr 析构自动释放 sig */
        return PATCH_HOOK_INVALID_ID;
    }

    /* 首次 hook 成功：签名由 C# 加载器持有（它会长期读这个结构体），
     * 本地放弃所有权，释放时机交给最后一次 patchlib_uninstall_hook。 */
    if (newly_allocated) sig.abandon();

    debug("Hook installed: method={} id={} newly_allocated={}",
          fmt::ptr(method), id, newly_allocated);
    return id;
}

bool patchlib_uninstall_hook(const patch_hook_id_t hook_id) {
    debug("patchlib_uninstall_hook: id={}", static_cast<int>(hook_id));

    if (hook_id == PATCH_HOOK_INVALID_ID) {
        error("Invalid hook_id");
        return false;
    }

    patch_handle_t method = il2cpp_get_method_by_hook_node(hook_id);
    if (!patchlib_is_valid(method)) {
        error("Failed to get method for hook {}", static_cast<int>(hook_id));
        return false;
    }

    if (il2cpp_has_single_hook_node(method)) {
        if (auto *sig = static_cast<patch_method_signature_t *>(il2cpp_get_hooked_method_sig(method))) {
            patchlib_method_signature_free(sig);
            std::free(sig);
        }
    }

    const bool ok = il2cpp_unhook_method(hook_id);
    if (!ok)
        error("il2cpp_unhook_method failed for hook {}",
              static_cast<int>(hook_id));
    return ok;
}
}
