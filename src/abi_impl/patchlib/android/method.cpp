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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2025/11/23
 *******************************************************************************/

#include "patchlib/method.h"

#include <array>

#include "patchlib/type.h"

#include <ffi.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "tef/core/log.hpp"
#include "private.hpp"

extern "C" {
#include "../il2cpp_api.h"
}


using tef::log::debug;
using tef::log::error;

namespace {
    constexpr int tef_ffi_call_guarded(ffi_cif *cif, void (*fn)(), void *rvalue, void **avalue) {
        try {
            ffi_call(cif, FFI_FN(fn), rvalue, avalue);
            return 0;
        } catch (...) {
            return 1;
        }
    }

    /* Il2CppReflectionMethod 布局（Android il2cpp）：
     *   Il2CppObject 头（klass / monitor）
     *   method（MethodInfo*）
     *   name（Il2CppString*）
     *   reftype（Il2CppReflectionType*） */
    struct il2cpp_reflection_method {
        // ReSharper disable once CppDeclaratorNeverUsed
        void *klass;
        // ReSharper disable once CppDeclaratorNeverUsed
        void *monitor;
        const void *method;
        // ReSharper disable once CppDeclaratorNeverUsed
        void *name;
        // ReSharper disable once CppDeclaratorNeverUsed
        void *reftype;
    };

    /* 方法指针（Android = method 首字段） */
    void *method_ptr(patch_handle_t method) {
        return method ? *static_cast<void **>(method) : nullptr;
    }

    /* RAII 管理 patch_method_signature_t（get_signature 成功才释放） */
    class signature_guard {
    public:
        signature_guard() = default;

        ~signature_guard() { if (ok_) patchlib_method_signature_free(&sig_); }

        signature_guard(const signature_guard &) = delete;

        signature_guard &operator=(const signature_guard &) = delete;

        bool get(patch_handle_t method) {
            std::memset(&sig_, 0, sizeof(sig_));
            ok_ = patchlib_method_get_signature(method, &sig_);
            return ok_;
        }

        patch_method_signature_t *operator->() noexcept { return &sig_; }
        explicit operator bool() const noexcept { return ok_; }

    private:
        patch_method_signature_t sig_{};
        bool ok_ = false;
    };
} // namespace

extern "C" {
/* ============================================================================
 * 方法指针
 * ============================================================================ */

void *patchlib_method_get_pointer(patch_handle_t method) {
    return method_ptr(method);
}

/* ============================================================================
 * 调用（Android：FFI + MethodInfo* 隐藏参数）
 *
 * Android 的 MethodInfo* 是最后一个隐藏参数——libffi 调用时必须传
 * &method（指向 MethodInfo* 的地址），而不是 method 本身。
 * ============================================================================ */

bool patchlib_method_invoke_args(patch_handle_t method, patch_handle_t instance,
                                 void *return_value, void **args) {
    debug("patchlib_method_invoke_args: method={} instance={}",
          fmt::ptr(method), fmt::ptr(instance));

    if (!patchlib_is_valid(method)) {
        error("Invalid method handle");
        return false;
    }

    signature_guard sig;
    if (!sig.get(method)) {
        error("Failed to get method signature");
        return false;
    }

    if (sig->is_instance && !patchlib_is_valid(instance)) {
        error("Instance method requires valid instance");
        return false;
    }

    void *mptr = method_ptr(method);
    if (!mptr) {
        error("Failed to get method pointer");
        return false;
    }

    const size_t explicit_count = tefstd_vector_size(&sig->arg_types);
    const int total = static_cast<int>(explicit_count)
                      + (sig->is_instance ? 1 : 0)
                      + 1; /* MethodInfo* */

    std::vector<ffi_type *> atypes(total);
    std::vector<void *> avalues(total);

    /*
     * 引用类型 null 防御：对 PATCH_OBJECT 参数，如果调用方传 null，
     * 用零初始化临时对象代替，避免 il2cpp 内部对 null 解引用崩溃。
     * 每个参数一个 256 字节缓冲（足够覆盖大多数对象头 + 常见字段）。
     */
    static thread_local std::array<std::array<char, 256>, 16> null_obj_bufs{};

    int idx = 0;

    /* this */
    if (sig->is_instance) {
        atypes[idx] = &ffi_type_pointer;
        avalues[idx] = &instance;
        ++idx;
    }

    /* 显式参数 */
    size_t null_obj_slot = 0;
    for (size_t i = 0; i < explicit_count; ++i) {
        const auto *at = static_cast<patch_type_t *>(tefstd_vector_at(&sig->arg_types, i));
        const auto *ah = static_cast<patch_handle_t *>(tefstd_vector_at(&sig->arg_type_handles, i));
        if (!at) {
            error("Failed to get argument type at index {}", i);
            return false;
        }
        atypes[idx] = patch_type_to_ffi_type(*at, ah ? *ah : nullptr);
        if (!atypes[idx]) {
            error("Invalid argument type at index {}", i);
            return false;
        }

        void *arg_ptr = args ? args[i] : nullptr;

        /*
         * 统一指针语义：
         *   - ffi_type_pointer 参数：avalues[idx] 必须指向"存放指针值的 8 字节"。
         *     如果 arg_ptr 为 null（调用方没传），用 &null_ptr 兜底。
         *   - 值类型参数（int / bool / float...）：avalues[idx] 指向值本身，
         *     直接是 arg_ptr。
         *
         * 对 PATCH_OBJECT 且 arg_ptr 为 null / 指向 null 的情况，
         * 用零初始化对象替代，避免 il2cpp 对 null 解引用。
         */
        if (atypes[idx] == &ffi_type_pointer) {
            if (arg_ptr && *static_cast<void **>(arg_ptr) == nullptr &&
                *at == PATCH_OBJECT && null_obj_slot < null_obj_bufs.size()) {
                /* 引用类型 null → 用零对象兜底 */
                void *zero_obj = null_obj_bufs[null_obj_slot++].data();
                debug("invoke_args: arg[{}] is null PATCH_OBJECT, using zero buffer {}",
                      i, fmt::ptr(zero_obj));
                avalues[idx] = &zero_obj;
            } else {
                avalues[idx] = arg_ptr;   // 已经是"指向指针值的 8 字节"
            }
        } else {
            /* 值类型：arg_ptr 指向值本身 */
            avalues[idx] = arg_ptr;
        }

        ++idx;
    }

    /* MethodInfo* —— ffi 需要「指向参数值」的地址，所以传 &method */
    atypes[idx] = &ffi_type_pointer;
    avalues[idx] = &method;
    ++idx;

    /* 返回类型 */
    ffi_type *ret_type = patch_type_to_ffi_type(sig->return_type,
                                                sig->return_type_handle);
    if (!ret_type) {
        error("Invalid return type");
        return false;
    }

    const ffi_abi abi = FFI_SYSV;

    ffi_cif cif;
    if (ffi_prep_cif(&cif, abi, static_cast<unsigned>(total),
                     ret_type, atypes.data()) != FFI_OK) {
        error("ffi_prep_cif failed");
        patch_ffi_type_free(ret_type, sig->return_type_handle);
        return false;
    }

    const int guard = tef_ffi_call_guarded(&cif, FFI_FN(mptr),
                                           return_value, avalues.data());
    if (guard) error("Managed exception caught: method={}", fmt::ptr(method));

    /* 释放 ffi_type（只释放显式参数 + 返回类型） */
    for (size_t i = 0; i < explicit_count; ++i) {
        if (const auto *ah = static_cast<patch_handle_t *>(tefstd_vector_at(&sig->arg_type_handles, i)); ah && atypes[i]) patch_ffi_type_free(atypes[i], *ah);
    }
    patch_ffi_type_free(ret_type, sig->return_type_handle);

    return guard == 0;
}

bool patchlib_constructor_invoke(patch_handle_t ctor,
                                 patch_handle_t *out, void **args) {
    if (!ctor || !out) return false;
    *out = nullptr;

    patch_handle_t inst = patchlib_type_new_instance(il2cpp_method_get_class(ctor));
    if (!inst) return false;

    *out = inst;
    return patchlib_method_invoke_args(ctor, inst, nullptr, args);
}

/* ============================================================================
 * 泛型方法实例化（Android：反射 MakeGenericMethod）
 *
 * il2cpp_method_make_generic 在 Android 不可用，必须走反射：
 *   1. 构造 Il2CppReflectionMethod（method = MethodInfo*）
 *   2. 把 template_types 包成 Il2CppArray<System.Type>
 *   3. 调 patchlib_MakeGenericMethod_impl(reflection_method, type_array)
 *   4. 从返回值读 method 字段
 * ============================================================================ */

patch_handle_t patchlib_method_make_generic_instance(
    patch_handle_t method, const tefstd_vector_t *template_types) {
    debug("patchlib_method_make_generic_instance: method={}", fmt::ptr(method));

    if (!patchlib_is_valid(method)) {
        error("Invalid method handle");
        return nullptr;
    }

    il2cpp_reflection_method reflect{};
    reflect.method = static_cast<const void *>(method);

    void *type_array = create_type_array_from_vector(
        template_types,
        il2cpp_class_from_name(il2cpp_get_corlib(), "System", "Type"));
    if (!type_array) {
        error("Failed to create type array");
        return nullptr;
    }

    const auto fn = reinterpret_cast<void* (*)(void *, void *)>(
        method_ptr(patchlib_make_generic_method_impl));
    if (!fn) {
        error("patchlib_MakeGenericMethod_impl not resolved");
        return nullptr;
    }

    auto *result_obj = static_cast<const il2cpp_reflection_method *>(
        fn(&reflect, type_array));
    if (!result_obj) {
        error("MakeGenericMethod returned NULL");
        return nullptr;
    }

    const auto result_method =
            const_cast<patch_handle_t>(result_obj->method);
    if (!result_method) {
        error("Failed to convert reflection result to MethodInfo");
        return nullptr;
    }

    debug("Generic method created: {}", fmt::ptr(result_method));
    return result_method;
}
}
