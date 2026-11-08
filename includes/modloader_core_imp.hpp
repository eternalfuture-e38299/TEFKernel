/*******************************************************************************
 * tefkernel - modloader_core_imp
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

#include "modloader/modloader_core.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace tef::ml {
    /* ml_handle_t 的 C++ 完整定义（公开头只有前向声明） */
    struct ml_handle_t {
        void *handle = nullptr;
        ml_entry_t *ml_entry = nullptr;
        std::size_t index = 0;
    };

    using ml_handle = ml_handle_t;

    class mod_loader_registry {
    public:
        static mod_loader_registry &instance();

        bool load(void *lib_handle, tefpkg_t *pkg, ml_handle **out);

        void cleanup(ml_handle *handle);

        void cleanup_all();

        [[nodiscard]] ml_handle *by_index(std::size_t index) const;

        [[nodiscard]] std::size_t count() const;

        [[nodiscard]] static const ml_info_t *get_info(const ml_handle *handle);

        /* 供 mod_core / tpf_core 遍历 */
        [[nodiscard]] const std::vector<std::unique_ptr<ml_handle> > &handles() const {
            return handles_;
        }

        /* 检查其他 modloader 是否依赖某插件 */
        [[nodiscard]] bool any_loader_depends_on(const char *pkg_id, const ml_handle *exclude) const;

    private:
        mod_loader_registry() = default;

        void free_loader(const std::unique_ptr<ml_handle> &mh) const;

        void cleanup_unreferenced_plugins(const ml_info_t *info) const;

        static std::string make_private_dir(const char *pkg_id);

        static std::string make_logs_dir(const char *pkg_id);

        std::vector<std::unique_ptr<ml_handle> > handles_;
    };

    inline ml_handle_t *to_c(ml_handle *m) { return m; }
    inline ml_handle *from_c(ml_handle_t *m) { return m; }
}
