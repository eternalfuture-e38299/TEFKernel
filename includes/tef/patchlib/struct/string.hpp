/*******************************************************************************
 * tefkernel - string
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

#include "../handle.hpp"
#include "../type.hpp"
#include "patchlib/struct/string.h"

#include <memory>
#include <string>

namespace tef::patchlib::structs {
    struct string : handle {
        string() = default;

        string(const handle &h) : handle(h) {
        }

        explicit string(const handle &h, bool own) : handle(h.raw(), own) {
        }

        explicit string(const char *str);

        explicit string(const std::string &str) : string(str.c_str()) {
        }

        ~string() = default;

        string(const string &other) : handle(static_cast<const handle &>(other)) {
        }

        string &operator=(const string &other) {
            if (this != &other) handle::operator=(static_cast<const handle &>(other));
            return *this;
        }

        string(string &&other) noexcept : handle(std::move(static_cast<handle &>(other))) {
        }

        string &operator=(string &&other) noexcept {
            if (this != &other) handle::operator=(std::move(static_cast<handle &>(other)));
            return *this;
        }

        string &operator=(const char *str);

        string &operator=(const std::string &str) { return *this = str.c_str(); }

        [[nodiscard]] bool is_valid() const { return patchlib_is_valid(_value); }

        void reset(const handle &new_handle = {}, bool take_ownership = false) {
            handle::reset(new_handle.raw(), take_ownership);
        }

        handle release() {
            handle h(_value, _own);
            _value = nullptr;
            _own = false;
            return h;
        }

        [[nodiscard]] std::unique_ptr<char, decltype(&free)> c_str() const;

        [[nodiscard]] std::string to_string() const;

        [[nodiscard]] size_t length() const;

        [[nodiscard]] bool empty() const;

        explicit operator bool() const { return is_valid() && !empty(); }

        bool operator==(const string &other) const;

        bool operator==(const char *str) const;

        bool operator==(const std::string &str) const;

        bool operator!=(const string &other) const { return !(*this == other); }
        bool operator!=(const char *str) const { return !(*this == str); }
        bool operator!=(const std::string &str) const { return !(*this == str); }

        bool operator<(const string &other) const;

        bool operator>(const string &other) const;

        bool operator<=(const string &other) const;

        bool operator>=(const string &other) const;

        friend bool operator==(const char *lhs, const string &rhs);

        friend bool operator==(const std::string &lhs, const string &rhs);

        friend bool operator!=(const char *lhs, const string &rhs);

        friend bool operator!=(const std::string &lhs, const string &rhs);
    };
}
