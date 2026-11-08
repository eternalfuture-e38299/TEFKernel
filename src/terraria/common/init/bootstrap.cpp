/*******************************************************************************
 * tefkernel - bootstrap
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

#include <ranges>

#include "terraria/init/bootstrap.hpp"

namespace terraria::init {
    bootstrap &bootstrap::instance() {
        static bootstrap inst;
        return inst;
    }

    void bootstrap::register_subsystem(subsystem *s) {
        if (s) subsystems_.push_back(s);
    }

    void bootstrap::set_server(const bool is_server) { is_server_ = is_server; }

    void bootstrap::run_early_init() const {
        for (auto *s: subsystems_) s->early_init();
    }

    void bootstrap::run_main_initialize_prefix() const {
        for (auto *s: subsystems_) s->on_main_initialize_prefix();
    }

    void bootstrap::run_main_initialize_postfix() const {
        for (auto *s: subsystems_) s->on_main_initialize_postfix();
    }

    void bootstrap::run_shutdown() {
        for (const auto &subsystem: std::views::reverse(subsystems_))
            subsystem->shutdown();
        subsystems_.clear();
    }
}
