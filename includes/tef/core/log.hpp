/*******************************************************************************
 * tefkernel - log
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

#include <spdlog/fmt/fmt.h>

#include <string_view>

namespace tef::log {

    enum class level {
        trace    = 0,
        debug    = 1,
        info     = 2,
        warn     = 3,
        error    = 4,
        critical = 5,
    };

    void init(const char* filename);
    void shutdown();
    [[nodiscard]] bool is_initialized() noexcept;

    void write(level level, const char* file, int line, const char* func,
               std::string_view msg);

    /* ============================================================================
     * 格式化便捷函数
     *
     * 用法：
     *   Info("x={}", x);                       // 字面量（编译期校验）
     *   Info(fmt::runtime(s), x);              // 运行时字符串
     * ============================================================================ */

    template<typename... Args>
    void trace(fmt::format_string<Args...> f, Args&&... a) {
        write(level::trace, nullptr, 0, nullptr, fmt::format(f, std::forward<Args>(a)...));
    }
    template<typename... Args>
    void debug(fmt::format_string<Args...> f, Args&&... a) {
        write(level::debug, nullptr, 0, nullptr, fmt::format(f, std::forward<Args>(a)...));
    }
    template<typename... Args>
    void info(fmt::format_string<Args...> f, Args&&... a) {
        write(level::info, nullptr, 0, nullptr, fmt::format(f, std::forward<Args>(a)...));
    }
    template<typename... Args>
    void warn(fmt::format_string<Args...> f, Args&&... a) {
        write(level::warn, nullptr, 0, nullptr, fmt::format(f, std::forward<Args>(a)...));
    }
    template<typename... Args>
    void error(fmt::format_string<Args...> f, Args&&... a) {
        write(level::error, nullptr, 0, nullptr, fmt::format(f, std::forward<Args>(a)...));
    }
    template<typename... Args>
    void critical(fmt::format_string<Args...> f, Args&&... a) {
        write(level::critical, nullptr, 0, nullptr, fmt::format(f, std::forward<Args>(a)...));
    }

}