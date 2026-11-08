/*******************************************************************************
 * tefkernel - field
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

#include <string>
#include <type_traits>

#include "patchlib/field.h"

namespace tef::patchlib {
    struct field : handle {
        handle _instance{};

        field() = default;

        field(const handle &h) : handle(h) {
        }

        explicit field(const handle &h, const bool own) : handle(h.raw(), own) {
        }

        field(const field &) = default;

        field &operator=(const field &) = default;

        field(field &&) noexcept = default;

        field &operator=(field &&) noexcept = default;

        [[nodiscard]] bool is_valid() const { return patchlib_is_valid(_value); }

        [[nodiscard]] std::string get_name() const;

        [[nodiscard]] bool is_static() const;

        [[nodiscard]] bool is_instance() const;

        [[nodiscard]] bool is_const() const;

        [[nodiscard]] bool is_thread_static() const;

        void set_instance(const handle &inst) { _instance = inst; }
        [[nodiscard]] const handle &get_instance() const { return _instance; }
        void clear_instance() { _instance.reset(); }

        // ---- 显式读 / 写 ----
        template<typename T>
        T get_value() const {
            if constexpr (std::is_same_v<T, handle>) {
                patch_handle_t r = nullptr;
                patchlib_field_get_value(_value, _instance.raw(), &r);
                return handle(r, false);
            } else {
                T v;
                patchlib_field_get_value(_value, _instance.raw(), &v);
                return v;
            }
        }

        template<typename T>
        void set_value(const T &v) const {
#if !defined(__ANDROID__)
            if constexpr (std::is_same_v<T, handle>) {
                patch_handle_t r = v.raw();
                patchlib_field_set_value(_value, _instance.raw(), &r);
            } else {
                patchlib_field_set_value(_value, _instance.raw(),
                                         const_cast<void *>(static_cast<const void *>(&v)));
            }
#else
            if constexpr (std::is_same_v<T, handle>) {
                *static_cast<patch_handle_t *>(get_pointer(_instance.raw())) = v.raw();
            } else {
                *static_cast<T *>(get_pointer(_instance.raw())) = v;
            }
#endif
        }

        template<typename T>
        T get_value(const handle &inst) const {
            field tmp(*this);
            tmp.set_instance(inst);
            return tmp.get_value<T>();
        }

        template<typename T>
        void set_value(const handle &inst, const T &v) {
            set_instance(inst);
            set_value<T>(v);
        }

        // ---- field[instance] ----
        [[nodiscard]] field operator[](const handle &inst) const {
            field tmp(*this);
            tmp.set_instance(inst);
            return tmp;
        }

        // ---- 隐式取值 ----
        template<typename T,
            typename = std::enable_if_t<
                !std::is_same_v<T, bool> &&
                !std::is_same_v<T, handle> &&
                !std::is_same_v<T, field>> >
        operator T() const { return get_value<T>(); }

        // ---- 写入 << ----
        template<typename T>
        std::enable_if_t<!std::is_same_v<T, field>, const field &>
        operator<<(const T &v) const {
            set_value<T>(v);
            return *this;
        }

        // ---- 比较 ----
        template<typename T>
        std::enable_if_t<!std::is_same_v<T, handle> && !std::is_same_v<T, field>, bool>
        operator==(const T &v) const { return get_value<T>() == v; }

        template<typename T>
        std::enable_if_t<!std::is_same_v<T, handle> && !std::is_same_v<T, field>, bool>
        operator!=(const T &v) const { return !(*this == v); }

        [[nodiscard]] size_t get_size() const;

        [[nodiscard]] patch_type_t get_type() const;

#if __ANDROID__
        [[nodiscard]] void *get_pointer(void *inst) const;
#endif

        explicit operator bool() const { return is_valid(); }
        bool operator==(const field &o) const { return _value == o._value; }
        bool operator!=(const field &o) const { return !(*this == o); }
    };
}
