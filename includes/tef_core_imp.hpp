/*******************************************************************************
 * tefkernel - tef_core_imp
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

#include "tefplugin/tpf_core.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tef::plugin {
    /* plugin_handle_t 的 C++ 定义（仅内核可见） */
    struct plugin_handle {
        void *lib_handle = nullptr;
        const tpf_plugin_ops_t *ops = nullptr;
        std::vector<const char *> sym_names;
        std::vector<const void *> sym_addrs;
        std::size_t index = 0;
    };

    struct ref_entry {
        std::string pkg_id;
        int ref_count = 0;
    };

    class plugin_registry {
    public:
        static plugin_registry &instance();

        bool register_symbol(plugin_handle *handle, const char *name, const void *addr) const;

        bool load(void *lib_handle, plugin_handle **out);

        bool cleanup(plugin_handle *plugin);

        bool register_shared_library(void *lib_handle);

        bool register_all_symbols_to_library(void *lib_handle) const;

        plugin_handle *find_by_pkg_id(const char *pkg_id) const;

        bool exists(const char *pkg_id) const;

        void add_ref(const char *pkg_id);

        void remove_ref(const char *pkg_id);

        int get_ref_count(const char *pkg_id) const;

        bool has_references(const char *pkg_id) const;

        bool init_all() const;

        static bool init_kernel();

        std::vector<std::unique_ptr<plugin_handle> > &plugins() { return plugins_; }
        std::vector<void *> &libraries() { return libraries_; }
        bool is_initialized() const { return initialized_; }

    private:
        plugin_registry() = default;

        static void free_plugin(const std::unique_ptr<plugin_handle> &plugin);

        std::vector<std::unique_ptr<plugin_handle> > plugins_;
        std::vector<void *> libraries_;
        std::unordered_map<std::string, ref_entry> refs_;
        bool initialized_ = false;
        bool libs_initialized_ = false;
    };

    inline plugin_handle_t *to_c(plugin_handle *p) {
        return reinterpret_cast<plugin_handle_t *>(p);
    }

    inline plugin_handle *from_c(plugin_handle_t *p) {
        return reinterpret_cast<plugin_handle *>(p);
    }
}
