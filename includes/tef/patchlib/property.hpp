/*******************************************************************************
 * tefkernel - property
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
#include "method.hpp"
#include "patchlib/property.h"

#include <string>

namespace tef::patchlib {
    struct property : handle {
        property() = default;

        property(const handle &h) : handle(h) {
        }

        explicit property(const handle &h, bool own) : handle(h.raw(), own) {
        }

        property(const property &) = default;

        property &operator=(const property &) = default;

        property(property &&) noexcept = default;

        property &operator=(property &&) noexcept = default;

        [[nodiscard]] bool is_valid() const { return patchlib_is_valid(_value); }

        [[nodiscard]] std::string get_name() const;

        [[nodiscard]] method get_get_method() const;

        [[nodiscard]] method get_set_method() const;

        explicit operator bool() const { return is_valid(); }
        bool operator==(const property &o) const { return _value == o._value; }
        bool operator!=(const property &o) const { return !(*this == o); }
    };
}
