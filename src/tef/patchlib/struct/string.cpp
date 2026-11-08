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

#include "patchlib/struct/string.h"
#include "tef/patchlib/struct/string.hpp"

#include <cstdlib>
#include <stdexcept>

namespace tef::patchlib::structs {
    string::string(const char *str) : handle() {
        _value = patchlib_string_create(str);
        _own = true;
        if (_value == nullptr) {
            throw std::runtime_error("Failed to create string");
        }
    }

    string &string::operator=(const char *str) {
        reset();
        _value = patchlib_string_create(str);
        _own = true;
        if (_value == nullptr) {
            throw std::runtime_error("Failed to create string");
        }
        return *this;
    }

    std::unique_ptr<char, decltype(&free)> string::c_str() const {
        char *str = patchlib_string_cstr(_value);
        return {str, free};
    }

    std::string string::to_string() const {
        const auto ptr = c_str();
        return ptr ? std::string(ptr.get()) : std::string();
    }

    size_t string::length() const { return patchlib_string_length(_value); }
    bool string::empty() const { return patchlib_string_empty(_value); }

    bool string::operator==(const string &other) const { return to_string() == other.to_string(); }
    bool string::operator==(const char *str) const { return to_string() == str; }
    bool string::operator==(const std::string &str) const { return to_string() == str; }

    bool string::operator<(const string &other) const { return to_string() < other.to_string(); }
    bool string::operator>(const string &other) const { return to_string() > other.to_string(); }
    bool string::operator<=(const string &other) const { return to_string() <= other.to_string(); }
    bool string::operator>=(const string &other) const { return to_string() >= other.to_string(); }

    bool operator==(const char *lhs, const string &rhs) { return rhs == lhs; }
    bool operator==(const std::string &lhs, const string &rhs) { return rhs == lhs; }
    bool operator!=(const char *lhs, const string &rhs) { return rhs != lhs; }
    bool operator!=(const std::string &lhs, const string &rhs) { return rhs != lhs; }
}
