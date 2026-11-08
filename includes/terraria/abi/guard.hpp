/*******************************************************************************
 * tefkernel - guard
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

namespace terraria::abi {
    // 非 void
    template<typename F, typename Default>
    constexpr auto guard(F &&f, Default &&def) noexcept -> std::enable_if_t<
        !std::is_void_v<std::invoke_result_t<F> >, std::invoke_result_t<F> > {
        try { return f(); } catch (...) { return std::forward<Default>(def); }
    }

    // void
    template<typename F>
    constexpr auto guard_void(F &&f) noexcept -> std::enable_if_t<
        std::is_void_v<std::invoke_result_t<F> >, void> {
        try { f(); } catch (...) {
        }
    }
}