/*******************************************************************************
 * tefkernel - scope_guard
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

#include <utility>

namespace tef::core {

    template<typename F>
    class [[nodiscard]] scope_guard {
    public:
        explicit scope_guard(F&& f) : fn_(std::forward<F>(f)) {}
        ~scope_guard() { if (active_) fn_(); }

        scope_guard(scope_guard&& o) noexcept
            : fn_(std::move(o.fn_)), active_(o.active_) { o.active_ = false; }

        scope_guard(const scope_guard&) = delete;
        scope_guard& operator=(const scope_guard&) = delete;

        void dismiss() noexcept { active_ = false; }

    private:
        F    fn_;
        bool active_ = true;
    };

    template<typename F>
    [[nodiscard]] constexpr scope_guard<F> make_guard(F&& f) {
        return scope_guard<F>(std::forward<F>(f));
    }

#define TEF_DEFER(code) \
    auto _tef_guard_##__LINE__ = ::tef::core::make_guard([&]() { code; })

} // namespace tef::core