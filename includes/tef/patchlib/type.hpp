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

#pragma once

#include "handle.hpp"

#include <vector>
#include <string>

namespace tef::patchlib {
    struct field;
    struct method;
    struct property;

    struct type : handle {
        type() = default;

        type(const handle &h) : handle(h) {
        }

        type(const std::string &ns, const std::string &name);

        explicit type(const handle &h, const bool own) : handle(h.raw(), own) {
        }

        ~type() = default;

        void manual_destroy();

        type(const type &other);

        type &operator=(const type &other);

        type(type &&other) noexcept;

        type &operator=(type &&other) noexcept;

        [[nodiscard]] bool is_valid() const;

        void reset(const handle &new_handle = {}, bool take_ownership = false);

        [[nodiscard]] handle get_mono_type() const;

        [[nodiscard]] std::string get_name() const;

        [[nodiscard]] std::string get_namespace() const;

        [[nodiscard]] std::string get_full_name() const;

        [[nodiscard]] type get_parent() const;

        static type get_basic_type(patch_type_t basic);

        [[nodiscard]] handle new_instance() const;

        [[nodiscard]] type make_generic_type(const std::vector<handle> &args) const;

        [[nodiscard]] type get_inner_type(const std::string &name) const;

        [[nodiscard]] field get_field(const std::string &name) const;

        [[nodiscard]] property get_property(const std::string &name) const;

        [[nodiscard]] method get_method(const std::string &name) const;

        [[nodiscard]] method get_method(const std::string &name, int argc) const;

        [[nodiscard]] method get_method(const std::string &name,
                                        const std::vector<std::string> &arg_names) const;

        [[nodiscard]] method get_method(const std::string &name,
                                        const std::vector<type> &arg_types) const;

        [[nodiscard]] method get_method(const std::string &name,
                                        const std::vector<type> &arg_types,
                                        const std::vector<std::string> &arg_names) const;

        [[nodiscard]] std::vector<type> get_inner_types(bool including_parent = false) const;

        [[nodiscard]] std::vector<method> get_methods(bool including_parent = false) const;

        [[nodiscard]] std::vector<field> get_fields(bool including_parent = false) const;

        [[nodiscard]] std::vector<property> get_properties(bool including_parent = false) const;

        bool operator==(const type &other) const;

        bool operator!=(const type &other) const;

        explicit operator bool() const;

        bool operator==(const handle &other) const;

        bool operator!=(const handle &other) const;
    };
}
