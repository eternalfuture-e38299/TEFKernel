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

#include "tef/patchlib/handle.hpp"

#include "abi_impl/patchlib/il2cpp_api.h"
#include "tef/patchlib/type.hpp"

namespace tef::patchlib {
    // type -> handle（视图，不接管）
    handle::handle(const type &t) : _value(t._value) {
    }

    handle::handle(const handle &other) : _value(other._value) {
        if (other._own && other._value != nullptr) {
            _value = patchlib_handle_copy(other._value);
            _own = (_value != nullptr);
        }
    }

    handle &handle::operator=(const handle &other) {
        if (this != &other) {
            reset();
            if (other._own && other._value != nullptr) {
                _value = patchlib_handle_copy(other._value);
                _own = (_value != nullptr);
            } else {
                _value = other._value;
                _own = false;
            }
        }
        return *this;
    }

    handle::handle(handle &&other) noexcept
        : _value(other._value), _own(other._own) {
        other._value = nullptr;
        other._own = false;
    }

    handle &handle::operator=(handle &&other) noexcept {
        if (this != &other) {
            reset();
            _value = other._value;
            _own = other._own;
            other._value = nullptr;
            other._own = false;
        }
        return *this;
    }

    handle::~handle() {
        if (_own && _value) patchlib_free(_value);
    }

    bool handle::is_valid() const { return patchlib_is_valid(_value); }

    void handle::reset(patch_handle_t new_handle, const bool take_ownership) {
        if (_own && _value) patchlib_free(_value);
        _value = new_handle;
        _own = take_ownership;
    }

    patch_handle_t handle::release() {
        patch_handle_t v = _value;
        _value = nullptr;
        _own = false;
        return v;
    }

    type handle::get_type() const {
        return type(il2cpp_object_get_class(_value), true);
    }
}
