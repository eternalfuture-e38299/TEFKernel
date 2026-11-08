/*******************************************************************************
 * tefkernel - hook
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
 * Created: 2026/11/7
 *******************************************************************************/

#pragma once

#include "handle.hpp"
#include "patchlib/method.h"

#include <type_traits>
#include <utility>

namespace tef::patchlib {
    struct hook_id {
        hook_id() = default;

        explicit hook_id(const patch_hook_id_t id) : id_(id) {
        }

        hook_id(const patch_hook_id_t id, void *owner) : id_(id), owner_(owner) {
        }

        [[nodiscard]] patch_hook_id_t get() const { return id_; }
        [[nodiscard]] bool is_valid() const { return id_ != PATCH_HOOK_INVALID_ID; }
        void reset() { id_ = PATCH_HOOK_INVALID_ID; }

        bool operator==(const hook_id &o) const { return id_ == o.id_; }
        bool operator!=(const hook_id &o) const { return id_ != o.id_; }
        explicit operator bool() const { return is_valid(); }

    private:
        patch_hook_id_t id_ = PATCH_HOOK_INVALID_ID;
        void *owner_ = nullptr;
    };

    // ==================== slot_proxy ====================
    class slot_proxy {
    public:
        explicit slot_proxy(void *slot) noexcept : _slot(slot) {
        }

        template<typename T>
        constexpr operator T() const {
            if constexpr (std::is_same_v<T, handle>) {
                return handle(*static_cast<patch_handle_t *>(_slot), false);
            } else {
                return *static_cast<T *>(_slot);
            }
        }

        template<typename T>
        constexpr slot_proxy &operator=(const T &value) {
            if constexpr (std::is_same_v<T, handle>) {
                *static_cast<patch_handle_t *>(_slot) = value.raw();
            } else {
                *static_cast<T *>(_slot) = value;
            }
            return *this;
        }

        template<typename T>
        constexpr slot_proxy &operator+=(const T &v) { return apply<T>([&](T &x) { x += v; }); }

        template<typename T>
        constexpr slot_proxy &operator-=(const T &v) { return apply<T>([&](T &x) { x -= v; }); }

        template<typename T>
        constexpr slot_proxy &operator*=(const T &v) { return apply<T>([&](T &x) { x *= v; }); }

        template<typename T>
        constexpr slot_proxy &operator/=(const T &v) { return apply<T>([&](T &x) { x /= v; }); }

        template<typename T>
        constexpr slot_proxy &operator%=(const T &v) { return apply<T>([&](T &x) { x %= v; }); }

        template<typename T>
        constexpr slot_proxy &operator&=(const T &v) { return apply<T>([&](T &x) { x &= v; }); }

        template<typename T>
        constexpr slot_proxy &operator|=(const T &v) { return apply<T>([&](T &x) { x |= v; }); }

        template<typename T>
        constexpr slot_proxy &operator^=(const T &v) { return apply<T>([&](T &x) { x ^= v; }); }

        template<typename T>
        constexpr slot_proxy &operator<<=(const T &v) { return apply<T>([&](T &x) { x <<= v; }); }

        template<typename T>
        constexpr slot_proxy &operator>>=(const T &v) { return apply<T>([&](T &x) { x >>= v; }); }

        constexpr slot_proxy &operator++() { return apply<int>([](int &x) { ++x; }); }
        constexpr slot_proxy &operator--() { return apply<int>([](int &x) { --x; }); }

        template<typename T>
        T operator++(int) const {
            T old = get<T>();
            apply<T>([](T &x) { ++x; });
            return old;
        }

        template<typename T>
        T operator--(int) const {
            T old = get<T>();
            apply<T>([](T &x) { --x; });
            return old;
        }

        template<typename T>
        T get() const {
            if constexpr (std::is_same_v<T, handle>) {
                return handle(*static_cast<patch_handle_t *>(_slot), false);
            } else {
                return *static_cast<T *>(_slot);
            }
        }

        template<typename T>
        constexpr slot_proxy &set(const T &value) {
            if constexpr (std::is_same_v<T, handle>) {
                *static_cast<patch_handle_t *>(_slot) = value.raw();
            } else {
                *static_cast<T *>(_slot) = value;
            }
            return *this;
        }

        [[nodiscard]] handle take_ownership() const {
            return handle::copy_of(*static_cast<patch_handle_t *>(_slot));
        }

        [[nodiscard]] void *raw() const { return _slot; }

    private:
        template<typename T, typename F>
        constexpr slot_proxy &apply(F &&f) {
            static_assert(!std::is_same_v<T, handle>,
                          "handle does not support arithmetic/bitwise operators");
            T &x = *static_cast<T *>(_slot);
            f(x);
            return *this;
        }

        void *_slot;
    };

    // ==================== hook_args / hook_result ====================
    class hook_args {
    public:
        explicit hook_args(void **args) noexcept : _args(args) {
        }

        [[nodiscard]] constexpr void *raw_arg(const int i) const { return _args[i]; }
        [[nodiscard]] constexpr void **raw_args() const { return _args; }

        template<typename T>
        constexpr T get(const int i) const { return slot_proxy(_args[i]).get<T>(); }

        template<typename T>
        constexpr void set(const int i, const T &value) const { slot_proxy(_args[i]).set<T>(value); }

        [[nodiscard]] slot_proxy operator[](const int i) const { return slot_proxy(_args[i]); }

    private:
        void **_args;
    };

    class hook_result {
    public:
        explicit hook_result(void *result) noexcept : _result(result) {
        }

        [[nodiscard]] bool is_valid() const { return _result != nullptr; }
        [[nodiscard]] void *raw() const { return _result; }

        template<typename T>
        constexpr T get() const {
            if (_result == nullptr) return T{};
            return slot_proxy(_result).get<T>();
        }

        template<typename T>
        constexpr void set(const T &value) const {
            if (_result != nullptr) slot_proxy(_result).set<T>(value);
        }

        [[nodiscard]] slot_proxy proxy() const { return slot_proxy(_result); }

        template<typename T>
        constexpr operator T() const { return get<T>(); }

        template<typename T>
        constexpr hook_result &operator=(const T &value) {
            set<T>(value);
            return *this;
        }

        [[nodiscard]] handle take_ownership() const { return slot_proxy(_result).take_ownership(); }

    private:
        void *_result;
    };

    // ==================== 空回调占位 ====================
    struct no_prefix {
        bool operator()(const handle &, hook_args &, hook_result, const patch_method_signature_t *) const {
            return true;
        }

        static constexpr bool k_is_empty = true;
    };

    struct no_postfix {
        void operator()(const handle &, hook_args &, hook_result, const patch_method_signature_t *) const {
        }

        static constexpr bool k_is_empty = true;
    };

    // ==================== hook_slot：唯一 tag ====================
    template<int N>
    struct hook_slot {
    };

    // ==================== hook_callbacks ====================
    /**
     * @brief Hook 回调包装（模板跳板）。
     *
     * Tag 保证每个 hook 注册点有独立跳板（独立 s_active），
     * 因此同一 (PrefixFn, PostfixFn) 类型组合可以有多个活跃 hook，
     * 只要 Tag 不同。
     *
     * PrefixFn / PostfixFn 任意一个可以是 no_prefix / no_postfix（空回调）。
     */
    template<typename Tag, typename PrefixFn, typename PostfixFn>
    class hook_callbacks {
    public:
        hook_callbacks(PrefixFn prefix, PostfixFn postfix)
            : _prefix(std::move(prefix)), _postfix(std::move(postfix)) {
            s_active = this;
        }

        ~hook_callbacks() {
            if (s_active == this) s_active = nullptr;
        }

        hook_callbacks(const hook_callbacks &) = delete;

        hook_callbacks &operator=(const hook_callbacks &) = delete;

        // ---- 跳板：匹配 prefix_callback_t ----
        static bool trampoline_prefix(patch_handle_t instance, void **args,
                                      const patch_method_signature_t *sig,
                                      void *result) {
            if constexpr (std::is_same_v<PrefixFn, no_prefix>) {
                return true;
            } else {
                if (s_active == nullptr) return true;
                hook_args ctx(args);
                const hook_result res(result);
                return invoke_prefix(s_active->_prefix, handle(instance, false), ctx, res, sig);
            }
        }

        // ---- 跳板：匹配 postfix_callback_t ----
        static void trampoline_postfix(patch_handle_t instance, void **args,
                                       void *result,
                                       const patch_method_signature_t *sig) {
            if constexpr (!std::is_same_v<PostfixFn, no_postfix>) {
                if (s_active == nullptr) return;
                hook_args ctx(args);
                hook_result res(result);
                s_active->_postfix(handle(instance, false), ctx, res, sig);
            }
        }

        // ---- 是否注册某跳板 ----
        static constexpr bool has_prefix() { return !std::is_same_v<PrefixFn, no_prefix>; }
        static constexpr bool has_postfix() { return !std::is_same_v<PostfixFn, no_postfix>; }

    private:
        static bool invoke_prefix(PrefixFn &fn, handle inst, hook_args &args,
                                  hook_result result, const patch_method_signature_t *sig) {
            if constexpr (std::is_invocable_v<PrefixFn &, handle, hook_args &,
                hook_result, const patch_method_signature_t *>) {
                return fn(std::move(inst), args, result, sig);
            } else {
                return fn(inst.raw(), args.raw_args(), sig, result.raw());
            }
        }

        PrefixFn _prefix;
        PostfixFn _postfix;

        static inline hook_callbacks *s_active = nullptr;
    };

    // 便捷构造（自动推导 Tag 需要显式给，所以这里不带 Tag 版本的工厂不给）
    template<typename Tag, typename PrefixFn, typename PostfixFn>
    [[nodiscard]] auto make_hook_callbacks(PrefixFn prefix, PostfixFn postfix) {
        return new hook_callbacks<Tag, PrefixFn, PostfixFn>(
            std::move(prefix), std::move(postfix));
    }
}
