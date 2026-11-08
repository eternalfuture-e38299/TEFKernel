/*******************************************************************************
 * tefkernel - type
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

#include "patchlib/type.h"
#include "tef/patchlib/type.hpp"
#include "tef/patchlib/field.hpp"
#include "tef/patchlib/method.hpp"
#include "tef/patchlib/property.hpp"

#include <cstdlib>

namespace tef::patchlib {
    type::type(const std::string &ns, const std::string &name)
        : handle(patchlib_type_get_type(ns.c_str(), name.c_str()), true) {
    }

    void type::manual_destroy() { reset(); }

    type::type(const type &other) : handle(static_cast<const handle &>(other)) {
    }

    type &type::operator=(const type &other) = default;

    type::type(type &&other) noexcept : handle(std::move(static_cast<handle &>(other))) {
    }

    type &type::operator=(type &&other) noexcept {
        handle::operator=(std::move(static_cast<handle &>(other)));
        return *this;
    }

    bool type::is_valid() const { return patchlib_is_valid(_value); }

    void type::reset(const handle &new_handle, const bool take_ownership) {
        handle::reset(new_handle.raw(), take_ownership);
    }

    handle type::get_mono_type() const {
#if defined(__ANDROID__)
        return handle(patchlib_type_get_mono_type(_value), true);
#else
        return handle(_value, false);
#endif
    }

    std::string type::get_name() const {
        const char *n = patchlib_type_get_name(_value);
        return n ? std::string(n) : std::string();
    }

    std::string type::get_namespace() const {
        const char *n = patchlib_type_get_namespace(_value);
        return n ? std::string(n) : std::string();
    }

    std::string type::get_full_name() const {
        char *n = patchlib_type_get_full_name(_value);
        if (!n) return {};
        std::string s(n);
        std::free(n);
        return s;
    }

    type type::get_parent() const {
        return {handle(patchlib_type_get_parent(_value), true)};
    }

    type type::get_basic_type(const patch_type_t basic) {
        return {handle(patchlib_get_basic_type(basic), true)};
    }

    handle type::new_instance() const {
        return handle(patchlib_type_new_instance(_value), false);
    }

    type type::make_generic_type(const std::vector<handle> &args) const {
        tefstd_vector_t raw{};
        tefstd_vector_init(&raw, sizeof(patch_handle_t));
        tefstd_vector_reserve(&raw, args.size());
        for (auto &h: args) {
            patch_handle_t raw_h = h.raw();
            tefstd_vector_push_back(&raw, &raw_h);
        }

        handle result(patchlib_type_make_generic_type(_value, &raw), true);
        tefstd_vector_destroy(&raw); // 调用完后释放 raw 内部缓冲区
        return {result};
    }

    type type::get_inner_type(const std::string &name) const {
        return {handle(patchlib_type_get_inner_type(_value, name.c_str()), true)};
    }

    field type::get_field(const std::string &name) const {
        return {handle(patchlib_type_get_field(_value, name.c_str()), true)};
    }

    property type::get_property(const std::string &name) const {
        return {handle(patchlib_type_get_property(_value, name.c_str()), true)};
    }

    method type::get_method(const std::string &name) const {
        return {handle(patchlib_type_get_method(_value, name.c_str()), true)};
    }

    method type::get_method(const std::string &name, const int argc) const {
        return {handle(patchlib_type_get_method_by_param_count(_value, name.c_str(), argc), true)};
    }

    method type::get_method(const std::string &name,
                            const std::vector<std::string> &arg_names) const {
        std::vector<const char *> c;
        c.reserve(arg_names.size());
        for (auto &s: arg_names) c.push_back(s.c_str());
        return {handle(patchlib_type_get_method_by_param_names(
                                 _value, name.c_str(), static_cast<int>(arg_names.size()), c.data()), true)};
    }

    method type::get_method(const std::string &name,
                            const std::vector<type> &arg_types) const {
        std::vector<patch_handle_t> h;
        h.reserve(arg_types.size());
        for (auto &t: arg_types) h.push_back(t.raw());
        return {handle(patchlib_type_get_method_by_param_types(
                                 _value, name.c_str(), static_cast<int>(arg_types.size()), h.data()), true)};
    }

    method type::get_method(const std::string &name,
                            const std::vector<type> &arg_types,
                            const std::vector<std::string> &arg_names) const {
        std::vector<patch_handle_t> h;
        h.reserve(arg_types.size());
        for (auto &t: arg_types) h.push_back(t.raw());
        std::vector<const char *> c;
        c.reserve(arg_names.size());
        for (auto &s: arg_names) c.push_back(s.c_str());
        return {handle(patchlib_type_get_method_by_signature(
                                 _value, name.c_str(), static_cast<int>(arg_types.size()),
                                 h.data(), c.data()), true)};
    }

    std::vector<type> type::get_inner_types(const bool including_parent) const {
        tefstd_vector_t v;
        if (!tefstd_vector_init(&v, sizeof(patch_handle_t))) return {};
        std::vector<type> out;
        if (patchlib_type_get_inner_types(_value, including_parent, &v)) {
            const auto *data = static_cast<patch_handle_t *>(v.data);
            for (size_t i = 0; i < v.size; ++i) out.emplace_back(handle(data[i], true));
        }
        tefstd_vector_destroy(&v);
        return out;
    }

    std::vector<method> type::get_methods(const bool including_parent) const {
        tefstd_vector_t v;
        if (!tefstd_vector_init(&v, sizeof(patch_handle_t))) return {};
        std::vector<method> out;
        if (patchlib_type_get_methods(_value, including_parent, &v)) {
            const auto *data = static_cast<patch_handle_t *>(v.data);
            for (size_t i = 0; i < v.size; ++i) out.emplace_back(handle(data[i], true));
        }
        tefstd_vector_destroy(&v);
        return out;
    }

    std::vector<field> type::get_fields(const bool including_parent) const {
        tefstd_vector_t v;
        if (!tefstd_vector_init(&v, sizeof(patch_handle_t))) return {};
        std::vector<field> out;
        if (patchlib_type_get_fields(_value, including_parent, &v)) {
            const auto *data = static_cast<patch_handle_t *>(v.data);
            for (size_t i = 0; i < v.size; ++i) out.emplace_back(handle(data[i], true));
        }
        tefstd_vector_destroy(&v);
        return out;
    }

    std::vector<property> type::get_properties(const bool including_parent) const {
        tefstd_vector_t v;
        if (!tefstd_vector_init(&v, sizeof(patch_handle_t))) return {};
        std::vector<property> out;
        if (patchlib_type_get_properties(_value, including_parent, &v)) {
            const auto *data = static_cast<patch_handle_t *>(v.data);
            for (size_t i = 0; i < v.size; ++i) out.emplace_back(handle(data[i], true));
        }
        tefstd_vector_destroy(&v);
        return out;
    }

    bool type::operator==(const type &other) const { return patchlib_type_is_same(_value, other._value); }
    bool type::operator!=(const type &other) const { return !(*this == other); }
    type::operator bool() const { return is_valid(); }
    bool type::operator==(const handle &other) const { return patchlib_type_is_same(_value, other.raw()); }
    bool type::operator!=(const handle &other) const { return !(*this == other); }
}
