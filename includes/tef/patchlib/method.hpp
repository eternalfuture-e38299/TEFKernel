/*******************************************************************************
 * tefkernel - method
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

#include <cstring>

#include "handle.hpp"
#include "hook.hpp"
#include "patchlib/method.h"

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace tef::patchlib {
    struct method_signature : patch_method_signature_t {
        method_signature();

        ~method_signature();

        method_signature(method_signature &&) noexcept;

        method_signature &operator=(method_signature &&) noexcept;

        method_signature(const method_signature &) = delete;

        method_signature &operator=(const method_signature &) = delete;

        [[nodiscard]] std::vector<patch_type_t> get_arg_types() const;

        [[nodiscard]] std::vector<std::string> get_arg_names() const;

        [[nodiscard]] const patch_method_signature_t *raw() const { return this; }
    };

    struct method : handle {
        handle _instance{};
        bool _is_ctor{};

        method() = default;

        method(const handle &h) : handle(h) {
        }

        explicit method(const handle &h, bool own) : handle(h.raw(), own) { _is_ctor = check_ctor(); }

        method(const method &) = default;

        method &operator=(const method &) = default;

        method(method &&) noexcept = default;

        method &operator=(method &&) noexcept = default;

        [[nodiscard]] bool is_valid() const { return patchlib_is_valid(_value); }

        void set_instance(const handle &inst) { _instance = inst; }
        [[nodiscard]] const handle &get_instance() const { return _instance; }
        void clear_instance() { _instance.reset(); }

        [[nodiscard]] std::string get_name() const;

        [[nodiscard]] int get_param_count() const;

        [[nodiscard]] bool is_instance() const;

        [[nodiscard]] bool is_static() const;

        [[nodiscard]] int get_token() const;

        [[nodiscard]] patch_type_t get_return_type() const;

        [[nodiscard]] method make_generic_instance(const std::vector<handle> &template_types) const;

#if __ANDROID__
        [[nodiscard]] void *get_pointer() const;
#endif

        template<typename Ret, typename... Args>
        // ReSharper disable once CppNotAllPathsReturnValue
        Ret invoke(Args... args) const {
#if !defined(__ANDROID__)
            void *arg_array[] = {to_raw_arg(args)...};
            if constexpr (std::is_same_v<Ret, handle>) {
                patch_handle_t r = nullptr;
                patchlib_method_invoke_args(_value, _instance.raw(), &r, arg_array);
                return handle(r, true);
            } else if constexpr (std::is_void_v<Ret>) {
                patchlib_method_invoke_args(_value, _instance.raw(), nullptr, arg_array);
            } else {
                Ret r;
                patchlib_method_invoke_args(_value, _instance.raw(), &r, arg_array);
                return r;
            }
#else
            /* Android 走 GetPointer 直调 */
            if constexpr (std::is_same_v<Ret, handle>) {
                if (is_instance()) {
                    auto fn = reinterpret_cast<patch_handle_t (*)(void *, Args..., void *)>(get_pointer());
                    return handle(fn(_instance.raw(), args..., _value), true);
                }
                auto fn = reinterpret_cast<patch_handle_t (*)(Args..., void *)>(get_pointer());
                return handle(fn(args..., _value), true);
            } else if constexpr (std::is_void_v<Ret>) {
                if (is_instance()) {
                    auto fn = reinterpret_cast<void (*)(void *, Args..., void *)>(get_pointer());
                    fn(_instance.raw(), args..., _value);
                } else {
                    auto fn = reinterpret_cast<void (*)(Args..., void *)>(get_pointer());
                    fn(args..., _value);
                }
            } else {
                if (is_instance()) {
                    auto fn = reinterpret_cast<Ret (*)(void *, Args..., void *)>(get_pointer());
                    return fn(_instance.raw(), args..., _value);
                }
                auto fn = reinterpret_cast<Ret (*)(Args..., void *)>(get_pointer());
                return fn(args..., _value);
            }
#endif
        }

        template<typename... Args>
        [[nodiscard]] handle invoke_ctor(Args... args) const {
            void *arg_array[] = {to_raw_arg(args)...};
            patch_handle_t inst = nullptr;
            if (patchlib_constructor_invoke(_value, &inst, arg_array)) {
                return handle(inst, true);
            }
            return {};
        }

        template<typename Ret, typename... Args>
        Ret invoke_with_instance(const handle &inst, Args... args) const {
            method tmp(*this);
            tmp.set_instance(inst);
            return tmp.invoke<Ret>(args...);
        }

        [[nodiscard]] method operator[](const handle &inst) const {
            method tmp(*this);
            tmp.set_instance(inst);
            return tmp;
        }

        template<typename Ret = void, typename... Args>
        Ret operator()(Args... args) const { return invoke<Ret>(args...); }

        [[nodiscard]] std::unique_ptr<method_signature> get_signature() const;

        template<typename Tag, typename Prefix, typename Postfix>
        [[nodiscard]] hook_id install_prepost_hook(Prefix prefix, Postfix postfix) const {
            using CB = hook_callbacks<Tag, Prefix, Postfix>;
            auto *cb = new CB(std::move(prefix), std::move(postfix));
            const patch_hook_id_t id = patchlib_install_prepost_hook(
                _value,
                CB::has_prefix() ? &CB::trampoline_prefix : nullptr,
                CB::has_postfix() ? &CB::trampoline_postfix : nullptr);
            return hook_id(id, cb);
        }

        template<typename Tag, typename Prefix>
        [[nodiscard]] hook_id install_prefix_hook(Prefix prefix) const {
            return install_prepost_hook<Tag>(std::move(prefix), no_postfix{});
        }

        template<typename Tag, typename Postfix>
        [[nodiscard]] hook_id install_postfix_hook(Postfix postfix) const {
            return install_prepost_hook<Tag>(no_prefix{}, std::move(postfix));
        }

        static bool uninstall_hook(const hook_id &id);

        explicit operator bool() const { return is_valid(); }
        bool operator==(const method &o) const { return _value == o._value; }
        bool operator!=(const method &o) const { return !(*this == o); }

    private:
        [[nodiscard]] bool check_ctor() const {
            if (!_value) return false;
            const char *n = patchlib_method_get_name(_value);
            return n && std::strcmp(n, ".ctor") == 0;
        }

        template<typename T>
        static void *to_raw_arg(T &v) {
            if constexpr (std::is_same_v<std::decay_t<T>, handle>) return v.raw_ptr();
            else return &v;
        }

        template<typename T>
        static void *to_raw_arg(const T &v) {
            if constexpr (std::is_same_v<std::decay_t<T>, handle>) return v.raw_ptr();
            else return const_cast<void *>(static_cast<const void *>(&v));
        }
    };
}
