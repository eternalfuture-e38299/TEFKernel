/*******************************************************************************
 * tefkernel - desktop_runtime_core
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

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "crash_handler.hpp"
#include "kernel_state.hpp"
#include "runtime.hpp"
#include "tef/core/log.hpp"
#include "terraria/main_state.hpp"


namespace {
    using tef::log::info;

    std::string normalize_dir(const std::string& dir) {
        if (dir.empty()) return dir;
        if (dir.back() == '/' || dir.back() == '\\')
            return dir.substr(0, dir.size() - 1);
        return dir;
    }

    std::string make_log_dir(const std::string& workdir) {
        return normalize_dir(workdir) + "/logs/tefkernel";
    }

    extern "C" API_EXPORT int init_tefkernel(const char* workdir, const bool is_server) {
        if (!workdir) {
            std::fprintf(stderr, "init_tefkernel: NULL workdir\n");
            return -1;
        }

        if (is_server) {
            std::printf("Running Server Client\n");
            info("Running Server Client");
        }

        tef::state::set_working_dir(workdir);
        if (!tef::state::working_dir()) {
            std::fprintf(stderr, "Failed to allocate working directory\n");
            return -1;
        }

        const std::string log_dir = make_log_dir(workdir);
        const std::string log_config = log_dir + "/runtime";

        if (std::error_code ec; std::filesystem::is_directory(log_dir, ec)) {
            std::printf("Log directory found: %s\n", log_dir.c_str());
            info("Log directory found: {}", log_dir);
            tef::log::init(log_config.c_str());
        } else {
            std::printf("Log system initialized (console only)\n");
            tef::log::init(nullptr);
        }

        tef::crash::init();
        terraria::main_state::instance().initialize(is_server);


        tef::runtime::init();
        tef::runtime::load();
        tef::runtime::start();

        info("TEFKernel initialization completed successfully");
        info("Working directory: {}", workdir);
        info("Log path: {}", log_config);
        return 0;
    }
}