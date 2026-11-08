/*******************************************************************************
 * tefkernel - module_core
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

#include "module_core_imp.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

#include "kernel_state.hpp"
#include "tef_core_imp.hpp"
#include "memdl/memdl.h"
#include "tef/core/log.hpp"
#include "tefpackage/tefpkg.h"

namespace tef::module {
    using log::debug;
    using log::error;
    using log::info;
    using log::warn;

    module_registry &module_registry::instance() {
        static module_registry r;
        return r;
    }

    std::string module_registry::make_private_dir(const char *pkg_id) {
        if (!pkg_id) return {};
        std::string s = state::working_dir();
        s += "/module/private/";
        s += pkg_id;
        return s;
    }

    std::string module_registry::make_logs_dir(const char *pkg_id) {
        if (!pkg_id) return {};
        std::string s = tef::state::working_dir();
        s += "/module/logs/";
        s += pkg_id;
        return s;
    }

    bool module_registry::check_plugin_references(const char *pkg_id) const {
        if (!pkg_id) return false;

        for (const auto &mh: handles_) {
            if (!mh || !mh->module_entry || !mh->module_entry->info) continue;
            const module_info_t *info = mh->module_entry->info;
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

    void module_registry::cleanup_unreferenced_plugins(const module_info_t *info) const {
        if (!info || !info->plugin_dependencies || info->plugin_dependencies_sizes <= 0) {
            debug("No plugin dependencies to cleanup");
            return;
        }

        log::info("Checking unreferenced plugins after module {} unload", info->pkg_id);

        auto &preg = tef::plugin::plugin_registry::instance();

        for (int i = 0; i < info->plugin_dependencies_sizes; ++i) {
            const char *dep = info->plugin_dependencies[i];
            if (!dep) continue;

            debug("Check dependency {}", dep);

            if (const bool still = check_plugin_references(dep); !still) {
                if (auto *plugin = preg.find_by_pkg_id(dep)) {
                    log::info("Cleaning up unreferenced plugin {}", dep);
                    preg.cleanup(plugin);
                } else {
                    debug("Plugin {} not found or already cleaned", dep);
                }
            } else {
                debug("Plugin {} still referenced by other modules", dep);
            }
        }
    }

    void module_registry::free_module(const std::unique_ptr<module_handle> &mh) const {
        if (!mh) return;

        debug("Freeing module {} (index={})", fmt::ptr(mh.get()), mh->index);

        const module_info_t *info = nullptr;
        if (mh->module_entry && mh->module_entry->info) {
            info = mh->module_entry->info;
        }

        if (mh->module_entry) {
            if (mh->module_entry->ops && mh->module_entry->ops->cleanup_module) {
                mh->module_entry->ops->cleanup_module(mh->module_entry);
            }

            std::free(const_cast<char *>(mh->module_entry->private_dir));
            std::free(const_cast<char *>(mh->module_entry->logs_dir));

            if (mh->module_entry->pkg_handle) {
                tefpkg_close(mh->module_entry->pkg_handle);
                mh->module_entry->pkg_handle = nullptr;
            }

            std::free(mh->module_entry);
            mh->module_entry = nullptr;
        }

        if (mh->handle) {
            debug("Closing dl handle");
            memdl_close(mh->handle);
            mh->handle = nullptr;
        }

        if (info) cleanup_unreferenced_plugins(info);
    }

    bool module_registry::load(void *lib_handle, tefpkg_t *pkg, module_handle **out) {
        if (!lib_handle) {
            error("Invalid lib handle");
            return false;
        }

        auto *create_fn = reinterpret_cast<const module_ops_t* (*)()>(
            memdl_sym(lib_handle, "module_create"));
        if (!create_fn) {
            error("Missing module_create in {}", fmt::ptr(lib_handle));
            memdl_close(lib_handle);
            return false;
        }

        const module_ops_t *ops = create_fn();
        if (!ops) {
            error("module_create returned NULL");
            memdl_close(lib_handle);
            return false;
        }

        if (!ops->get_info) {
            error("Module does not support get_info");
            memdl_close(lib_handle);
            return false;
        }

        const module_info_t *info = ops->get_info();
        if (!info || !info->pkg_id) {
            error("Module get_info returned NULL or NULL pkg_id");
            memdl_close(lib_handle);
            return false;
        }

        log::info("Loading module: pkg_id={}, version={}, api_version={}",
             info->pkg_id,
             info->version ? info->version : "NULL",
             info->api_version);

        auto mh = std::make_unique<module_handle>();
        mh->handle = lib_handle;
        mh->index = handles_.size();

        auto *entry = static_cast<module_entry_t *>(std::malloc(sizeof(module_entry_t)));
        if (!entry) {
            error("malloc module_entry failed");
            memdl_close(lib_handle);
            return false;
        }
        std::memset(entry, 0, sizeof(module_entry_t));

        entry->info = const_cast<module_info_t *>(info);
        entry->ops = const_cast<module_ops_t *>(ops);
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

        mh->module_entry = entry;

        if (info->plugin_dependencies && info->plugin_dependencies_sizes > 0) {
            auto &preg = plugin::plugin_registry::instance();
            for (int i = 0; i < info->plugin_dependencies_sizes; ++i) {
                if (info->plugin_dependencies[i])
                    preg.add_ref(info->plugin_dependencies[i]);
            }
        }

        module_handle *raw = mh.get();
        handles_.push_back(std::move(mh));

        plugin::plugin_registry::instance().register_all_symbols_to_library(lib_handle);

        if (out) *out = raw;
        log::info("Module loaded: pkg_id={}, index={}", info->pkg_id, raw->index);
        return true;
    }

    void module_registry::cleanup(module_handle *handle) {
        if (!handle) {
            warn("Cleanup NULL module");
            return;
        }

        info("Cleaning up module {} (index={}, pkg_id={})",
             fmt::ptr(handle), handle->index,
             (handle->module_entry && handle->module_entry->info)
                 ? handle->module_entry->info->pkg_id
                 : "NULL");

        const auto it = std::ranges::find_if(handles_,
                                             [handle](const std::unique_ptr<module_handle> &p) {
                                                 return p.get() == handle;
                                             });
        if (it == handles_.end()) {
            warn("Module {} not found in registry", fmt::ptr(handle));
            return;
        }

        const std::unique_ptr<module_handle> owned = std::move(*it);
        handles_.erase(it);

        for (std::size_t i = 0; i < handles_.size(); ++i)
            handles_[i]->index = i;

        free_module(owned);
        info("Module cleanup done");
    }

    void module_registry::cleanup_all() {
        info("Cleaning up all {} modules", handles_.size());

        while (!handles_.empty()) {
            std::unique_ptr<module_handle> owned = std::move(handles_.back());
            handles_.pop_back();
            free_module(owned);
        }

        info("All modules cleaned up");
    }

    module_handle *module_registry::by_index(std::size_t index) const {
        if (index >= handles_.size()) {
            warn("Module index {} out of bounds (total={})", index, handles_.size());
            return nullptr;
        }
        return handles_[index].get();
    }

    std::size_t module_registry::count() const {
        debug("Module count: {}", handles_.size());
        return handles_.size();
    }

    const module_info_t *module_registry::get_info(const module_handle *handle) {
        if (!handle || !handle->module_entry) {
            error("Invalid module handle");
            return nullptr;
        }
        return handle->module_entry->info;
    }

    bool module_registry::init_all() {
        if (handles_.empty()) {
            debug("No modules to initialize");
            return true;
        }

        const std::size_t n = handles_.size();
        info("Initializing all {} modules", n);
        bool all_ok = true;

        std::vector<bool> to_delete(n, false);

        for (std::size_t i = 0; i < n; ++i) {
            const auto *mh = handles_[i].get();
            if (!mh || !mh->module_entry) {
                warn("Invalid module at index {}", i);
                continue;
            }

            module_entry_t *entry = mh->module_entry;
            const char *pkg = entry->info ? entry->info->pkg_id : "unknown";

            if (!entry->ops || !entry->ops->init_module) {
                debug("Module {} ({}) has no init_module", i, pkg);
                continue;
            }

            debug("Initializing module {} ({})", i, pkg);
            if (!entry->ops->init_module(entry)) {
                error("Module {} ({}) init failed", i, pkg);
                all_ok = false;
                continue;
            }

            std::uint64_t hash = ~0ULL;

            if (pkg && std::strcmp(pkg, "eternal.future.languagepackextension") == 0)
                hash = 0x230ABADD06487BE7ULL;
            else if (pkg && std::strcmp(pkg, "eternal.future.texturepackextension") == 0)
                hash = 0xDA2BB9F26C5C1E01ULL;
            else if (pkg && std::strcmp(pkg, "eternal.future.fontpackextension") == 0)
                hash = 0xB8B74AFCF15B6F37ULL;
            else if (pkg && std::strcmp(pkg, "eternal.future.audiopackextension") == 0)
                hash = 0x8D5A0F2C4E6B1A93ULL;

            if (hash != ~0ULL) {
                if (tefpkg_verify_signature(entry->pkg_handle, hash) == tef_ok) {
                    info("Module {} signature verified, marking for deletion", pkg);
                    to_delete[i] = true;
                } else {
                    warn("Module {} signature verification failed", pkg);
                }
            }
        }

        std::size_t deleted = 0;
        for (std::size_t i = n; i > 0; --i) {
            const std::size_t idx = i - 1;
            if (!to_delete[idx]) continue;

            const auto *mh = handles_[idx].get();
            const char *pkg = (mh && mh->module_entry && mh->module_entry->info)
                                  ? mh->module_entry->info->pkg_id
                                  : "unknown";
            info("Removing module {} (index={})", pkg, idx);

            std::unique_ptr<module_handle> owned = std::move(handles_[idx]);
            handles_.erase(handles_.begin() + static_cast<std::ptrdiff_t>(idx));

            free_module(owned);
            ++deleted;
        }

        if (deleted > 0) {
            info("Removed {} verified extension modules", deleted);
        }

        for (std::size_t i = 0; i < handles_.size(); ++i)
            handles_[i]->index = i;

        info("Init all completed: {} (remaining={})",
             all_ok ? "success" : "with errors", handles_.size());
        return all_ok;
    }

    void module_registry::hot_reload_all() const {
        if (handles_.empty()) {
            debug("No modules to hot reload");
            return;
        }

        info("Hot reloading all {} modules", handles_.size());

        for (std::size_t i = 0; i < handles_.size(); ++i) {
            const auto *mh = handles_[i].get();
            if (!mh || !mh->module_entry || !mh->module_entry->ops ||
                !mh->module_entry->ops->hot_reload)
                continue;

            const char *pkg = mh->module_entry->info ? mh->module_entry->info->pkg_id : "unknown";
            debug("hot_reload module {} ({})", i, pkg);
            mh->module_entry->ops->hot_reload(mh->module_entry);
        }

        info("Hot reload done");
    }
}
