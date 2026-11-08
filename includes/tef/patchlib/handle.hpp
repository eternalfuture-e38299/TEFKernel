/*******************************************************************************
 * tefkernel - handle
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

#include "patchlib/type.h"

namespace tef::patchlib {
    struct type;

    struct handle {
        patch_handle_t _value{};
        bool _own{};

        handle() = default;

        // patch_handle_t -> handle（隐式，默认不拥有）
        handle(patch_handle_t v) : _value(v) {
        }

        // 显式指定所有权
        explicit handle(patch_handle_t v, const bool own) : _value(v), _own(own) {
        }

        // type -> handle
        handle(const type &t);

        handle(const handle &other);

        handle &operator=(const handle &other);

        handle(handle &&other) noexcept;

        handle &operator=(handle &&other) noexcept;

        ~handle();

        [[nodiscard]] bool is_valid() const;

        void reset(patch_handle_t new_handle = nullptr, bool take_ownership = false);

        patch_handle_t release();

        [[nodiscard]] type get_type() const;

        [[nodiscard]] patch_handle_t raw() const { return _value; }

        patch_handle_t *raw_ptr() { return &_value; }

        [[nodiscard]] handle copy() const {
            patch_handle_t r = patchlib_handle_copy(_value);
            return handle(r, r != nullptr);
        }

        [[nodiscard]] static handle copy_of(patch_handle_t r) {
            patch_handle_t c = patchlib_handle_copy(r);
            return handle(c, c != nullptr);
        }

        bool operator==(const handle &other) const { return _value == other._value; }
        bool operator!=(const handle &other) const { return !(*this == other); }
        explicit operator bool() const { return is_valid(); }
    };
}
