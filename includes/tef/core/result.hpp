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

#include <string_view>
#include <utility>
#include <variant>

namespace tef::core {

    template<typename T, typename E>
    class [[nodiscard]] result {
    public:
        result(T v) : data_(std::in_place_index<0>, std::move(v)) {}
        result(E e) : data_(std::in_place_index<1>, e) {}

        [[nodiscard]] bool ok() const noexcept { return data_.index() == 0; }
        explicit operator bool() const noexcept { return ok(); }

        [[nodiscard]] T&       value()       { return std::get<0>(data_); }
        [[nodiscard]] const T& value() const { return std::get<0>(data_); }

        [[nodiscard]] T value_or(T fallback) const {
            return ok() ? std::get<0>(data_) : std::move(fallback);
        }

        [[nodiscard]] E err() const noexcept { return std::get<1>(data_); }

    private:
        std::variant<T, E> data_;
    };

    template<typename E>
    class [[nodiscard]] result<void, E> {
    public:
        result() : ok_(true) {}
        result(E e) : ok_(false), err_(e) {}

        [[nodiscard]] bool ok() const noexcept { return ok_; }
        explicit operator bool() const noexcept { return ok_; }
        [[nodiscard]] E err() const noexcept { return err_; }

    private:
        bool ok_;
        E    err_{};
    };

    /* ============================================================================
     * 错误传播宏
     * ============================================================================ */

    /* 纯检查：失败则 return Err()。expr 可为 Result 变量或临时表达式。 */
#define TEF_TRY(expr) do { \
    if (auto _tef_r_ = (expr); !_tef_r_) \
        return _tef_r_.err(); \
} while (0)

    /* 声明 + 检查：
     *   TEF_TRY_DECLARE(auto v, Foo());
     * 展开后 v 在宏之后仍可用。 */
#define TEF_TRY_DECLARE(decl, expr) \
    decl = (expr); \
    TEF_TRY(decl)

}