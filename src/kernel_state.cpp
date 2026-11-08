/*******************************************************************************
 * tefkernel - kernel_state
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

#include "kernel_state.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

namespace tef::state {
    namespace {
        char *g_working_dir = nullptr;
    }

    char *working_dir() noexcept {
        return g_working_dir;
    }

    void set_working_dir(char *dir) noexcept {
        g_working_dir = dir;
    }

    void set_working_dir(const char *dir) {
        if (g_working_dir) {
            std::free(g_working_dir);
            g_working_dir = nullptr;
        }
        if (dir) {
            const std::size_t n = std::strlen(dir);
            g_working_dir = static_cast<char *>(std::malloc(n + 1));
            if (g_working_dir) {
                std::memcpy(g_working_dir, dir, n + 1);
            }
        }
    }
}
