/*******************************************************************************
 * tefkernel - modloader_core
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

#include "modloader_core_imp.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

#include "kernel_state.hpp"
#include "tef_core_imp.hpp"
#include "memdl/memdl.h"
#include "tef/core/log.hpp"
#include "tefpackage/tefpkg.h"

namespace tef::ml {
    using log::debug;
    using log::error;
    using log::info;
    using log::warn;

    mod_loader_registry &mod_loader_registry::instance() {
        static mod_loader_registry r;
        return r;
    }

    std::string mod_loader_registry::make_private_dir(const char *pkg_id) {
        if (!pkg_id) return {};
        std::string s = tef::state::working_dir();
        s += "/modloader/private/";
        s += pkg_id;
        return s;
    }

    std::string mod_loader_registry::make_logs_dir(const char *pkg_id) {
        if (!pkg_id) return {};
        std::string s = tef::state::working_dir();
        s += "/modloader/logs/";
        s += pkg_id;
        return s;
    }

    bool mod_loader_registry::any_loader_depends_on(const char *pkg_id,
                                                    const ml_handle *exclude) const {
        if (!pkg_id) return false;
        for (const auto &mh: handles_) {
            if (!mh || mh.get() == exclude) continue;
            if (!mh->ml_entry || !mh->ml_entry->info) continue;
            const ml_info_t *info = mh->ml_entry->info;
            if (!info->plugin_dependencies || info->plugin_dependencies_sizes <= 0)
                continue;
            for (int i = 0; i < info->plugin_dependencies_sizes; ++i) {
                if (info->plugin_dependencies[i] &&
                    std::strcmp(info->plugin_dependencies[i], pkg_id) == 0)
                    return true;
            }
        }
        return false;
    }

    void mod_loader_registry::cleanup_unreferenced_plugins(const ml_info_t *info) const {
        if (!info || !info->plugin_dependencies || info->plugin_dependencies_sizes <= 0) {
            debug("No plugin dependencies to cleanup");
            return;
        }

        log::info("Checking unreferenced plugins after modloader {} unload", info->pkg_id);

        auto &preg = tef::plugin::plugin_registry::instance();

        for (int i = 0; i < info->plugin_dependencies_sizes; ++i) {
            const char *dep = info->plugin_dependencies[i];
            if (!dep) continue;
            debug("Check dependency {}", dep);

            if (!any_loader_depends_on(dep, nullptr)) {
                if (auto *plugin = preg.find_by_pkg_id(dep)) {
                    log::info("Cleaning up unreferenced plugin {}", dep);
                    preg.cleanup(plugin);
                } else {
                    debug("Plugin {} not found or already cleaned", dep);
                }
            } else {
                debug("Plugin {} still referenced by another modloader", dep);
            }
        }
    }

    void mod_loader_registry::free_loader(const std::unique_ptr<ml_handle> &mh) const {
        if (!mh) return;
        debug("Freeing modloader {} (index={})", fmt::ptr(mh.get()), mh->index);

        const ml_info_t *info = nullptr;
        if (mh->ml_entry && mh->ml_entry->info) info = mh->ml_entry->info;

        if (mh->ml_entry) {
            if (mh->ml_entry->ops && mh->ml_entry->ops->cleanup_ml) {
                mh->ml_entry->ops->cleanup_ml(mh->ml_entry);
            }

            std::free(const_cast<char *>(mh->ml_entry->private_dir));
            std::free(const_cast<char *>(mh->ml_entry->logs_dir));

            if (mh->ml_entry->pkg_handle) {
                tefpkg_close(mh->ml_entry->pkg_handle);
                mh->ml_entry->pkg_handle = nullptr;
            }

            std::free(mh->ml_entry);
            mh->ml_entry = nullptr;
        }

        if (mh->handle) {
            debug("Closing dl handle");
            memdl_close(mh->handle);
            mh->handle = nullptr;
        }

        if (info) cleanup_unreferenced_plugins(info);
    }

    bool mod_loader_registry::load(void *lib_handle, tefpkg_t *pkg, ml_handle **out) {
        if (!lib_handle) {
            error("Invalid lib handle");
            return false;
        }

        auto *create_fn = reinterpret_cast<const ml_ops_t* (*)()>(
            memdl_sym(lib_handle, "ml_create"));
        if (!create_fn) {
            error("Missing ml_create in {}: {}", fmt::ptr(lib_handle), memdl_error());
            memdl_close(lib_handle);
            return false;
        }

        plugin::plugin_registry::instance().register_shared_library(lib_handle);

        const ml_ops_t *ops = create_fn();
        if (!ops) {
            error("ml_create returned NULL");
            memdl_close(lib_handle);
            return false;
        }

        if (!ops->get_info) {
            error("ModLoader does not support get_info");
            memdl_close(lib_handle);
            return false;
        }

        const ml_info_t *info = ops->get_info();
        if (!info || !info->pkg_id) {
            error("ModLoader get_info returned NULL or NULL pkg_id");
            memdl_close(lib_handle);
            return false;
        }

        log::info("Loading modloader: pkg_id={}, version={}, api_version={}",
             info->pkg_id,
             info->version ? info->version : "NULL",
             info->api_version);

        auto mh = std::make_unique<ml_handle>();
        mh->handle = lib_handle;
        mh->index = handles_.size();

        auto *entry = static_cast<ml_entry_t *>(std::malloc(sizeof(ml_entry_t)));
        if (!entry) {
            error("malloc ml_entry failed");
            memdl_close(lib_handle);
            return false;
        }
        std::memset(entry, 0, sizeof(ml_entry_t));

        entry->info = const_cast<ml_info_t *>(info);
        entry->ops = const_cast<ml_ops_t *>(ops);
        entry->pkg_handle = pkg;

        const auto priv = make_private_dir(info->pkg_id);
        const auto logs = make_logs_dir(info->pkg_id);

        entry->private_dir = ::strdup(priv.c_str());
        entry->logs_dir = ::strdup(logs.c_str());

        if (!entry->private_dir || !entry->logs_dir) {
            error("Failed to allocate dir strings");
            std::free(const_cast<char *>(entry->private_dir));
            std::free(const_cast<char *>(entry->logs_dir));
            std::free(entry);
            memdl_close(lib_handle);
            return false;
        }

        mh->ml_entry = entry;

        ml_handle *raw = mh.get();
        handles_.push_back(std::move(mh));

        if (entry->ops->init_ml) {
            if (const ml_result_t r = entry->ops->init_ml(entry); r != ML_SUCCESS) {
                error("ModLoader init_ml failed: result={}", static_cast<int>(r));
                handles_.pop_back();
                return false;
            }
        }

        if (out) *out = raw;
        log::info("ModLoader loaded: pkg_id={}, index={}", info->pkg_id, raw->index);
        return true;
    }

    void mod_loader_registry::cleanup(ml_handle *handle) {
        if (!handle) {
            warn("Cleanup NULL modloader");
            return;
        }

        info("Cleaning up modloader {} (index={}, pkg_id={})",
             fmt::ptr(handle), handle->index,
             (handle->ml_entry && handle->ml_entry->info)
                 ? handle->ml_entry->info->pkg_id
                 : "NULL");

        const auto it = std::ranges::find_if(handles_,
                                             [handle](const std::unique_ptr<ml_handle> &p) {
                                                 return p.get() == handle;
                                             });
        if (it == handles_.end()) {
            warn("ModLoader {} not found in registry", fmt::ptr(handle));
            return;
        }

        const std::unique_ptr<ml_handle> owned = std::move(*it);
        handles_.erase(it);

        for (std::size_t i = 0; i < handles_.size(); ++i)
            handles_[i]->index = i;

        free_loader(owned);
        info("ModLoader cleanup done");
    }

    void mod_loader_registry::cleanup_all() {
        info("Cleaning up all {} modloaders", handles_.size());

        std::vector<const char *> all_deps;
        for (const auto &mh: handles_) {
            if (!mh || !mh->ml_entry || !mh->ml_entry->info) continue;
            const ml_info_t *info = mh->ml_entry->info;
            if (!info->plugin_dependencies || info->plugin_dependencies_sizes <= 0) continue;
            for (int i = 0; i < info->plugin_dependencies_sizes; ++i)
                if (info->plugin_dependencies[i])
                    all_deps.push_back(info->plugin_dependencies[i]);
        }

        while (!handles_.empty()) {
            std::unique_ptr<ml_handle> owned = std::move(handles_.back());
            handles_.pop_back();
            free_loader(owned);
        }

        auto &preg = plugin::plugin_registry::instance();
        for (const char *dep: all_deps) {
            if (!dep) continue;
            if (!any_loader_depends_on(dep, nullptr)) {
                if (auto *p = preg.find_by_pkg_id(dep)) {
                    info("Cleaning up plugin after all modloaders unloaded: {}", dep);
                    preg.cleanup(p);
                }
            }
        }

        info("All modloaders and their plugins cleaned up");
    }

    ml_handle *mod_loader_registry::by_index(std::size_t index) const {
        if (index >= handles_.size()) {
            warn("ModLoader index {} out of bounds (total={})", index, handles_.size());
            return nullptr;
        }
        return handles_[index].get();
    }

    std::size_t mod_loader_registry::count() const {
        debug("ModLoader count: {}", handles_.size());
        return handles_.size();
    }

    const ml_info_t *mod_loader_registry::get_info(const ml_handle *handle) {
        if (!handle || !handle->ml_entry) {
            error("Invalid modloader handle");
            return nullptr;
        }
        return handle->ml_entry->info;
    }
}
