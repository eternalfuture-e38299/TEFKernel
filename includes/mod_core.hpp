/*******************************************************************************
 * tefkernel - mod_core
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

#include <cstddef>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "modloader/modloader_core.h"

namespace tef::ml {
    struct ml_handle_t;
}

namespace tef::mod {
    enum class state {
        unloaded = 0,
        loaded = 1,
        initialized = 2,
        error = 3,
    };

    /* 对应旧 mod_handle_t（私有，可自由改） */
    struct mod_handle {
        std::string mod_id;
        std::string mod_path;
        mod_manifest_t *manifest = nullptr; /* malloc 分配的 C ABI 结构 */
        ml::ml_handle_t *owner_ml = nullptr;
        state _state = state::unloaded;
        std::time_t load_time = 0;
    };

    class mod_registry {
    public:
        static mod_registry &instance();

        /* 加载所有 mod（遍历所有 modloader） */
        int load_all();

        /* 初始化所有 mod */
        [[nodiscard]] int init_all() const;

        /* 卸载所有 mod */
        int cleanup_all();

        /* 查询 */
        [[nodiscard]] std::size_t count() const;

        [[nodiscard]] mod_handle *by_index(std::size_t index) const;

        /* 遍历 */
        [[nodiscard]] const std::vector<std::unique_ptr<mod_handle> > &handles() const {
            return handles_;
        }

    private:
        mod_registry() = default;

        /* 读取某 modloader 的 enables.txt */
        static std::vector<std::string> read_enabled_mods(const char *ml_pkg_id);

        /* 构造 / 释放 mod_manifest_t（C ABI，malloc 分配） */
        static mod_manifest_t *make_manifest(const std::string &mod_path,
                                             const std::string &mod_id,
                                             const char *ml_pkg_id);

        static void free_manifest(mod_manifest_t *m);

        static void free_mod(const std::unique_ptr<mod_handle> &mod);

        std::vector<std::unique_ptr<mod_handle> > handles_;
    };
}
