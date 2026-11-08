/*******************************************************************************
 * tefkernel - result
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
 * Created: 2026/11/6
 *******************************************************************************/

#pragma once

#include "tef/core/result.hpp"
#include "tefpackage/tefpkg.h"

#include <cstdint>
#include <string_view>

namespace tef::pkg {

    enum class error : int32_t {
        ok           = 0,
        general      = -1,
        signature    = -2,
        corrupt      = -3,
        memory       = -4,
        io           = -5,
        keyfile      = -6,
        not_found     = -7,
        invalid      = -8,
        not_signature = -9,
        integrity    = -10,
        no_space      = -11,
    };

    constexpr std::string_view to_string(const error e) noexcept {
        switch (e) {
            case error::ok:           return "ok";
            case error::general:      return "general error";
            case error::signature:    return "signature mismatch";
            case error::corrupt:      return "corrupt data";
            case error::memory:       return "out of memory";
            case error::io:           return "io error";
            case error::keyfile:      return "keyfile error";
            case error::not_found:     return "not found";
            case error::invalid:      return "invalid argument";
            case error::not_signature: return "not signed";
            case error::integrity:    return "integrity check failed";
            case error::no_space:      return "no space";
        }
        return "unknown";
    }

    template<typename T>
    using result = core::result<T, error>;

    using status = core::result<void, error>;

    constexpr tefpkg_result_t to_c(error e) noexcept {
        return static_cast<tefpkg_result_t>(static_cast<int32_t>(e));
    }

    constexpr error from_c(const tefpkg_result_t r) noexcept {
        return static_cast<error>(static_cast<int32_t>(r));
    }
}