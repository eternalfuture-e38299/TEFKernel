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

#include "../includes/mod_core.hpp"
#include "../includes/kernel_state.hpp"
#include "../includes/modloader_core_imp.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

#include "../includes/tef/core/log.hpp"

namespace tef::mod {
    using log::debug;
    using log::error;
    using log::info;
    using log::warn;

    mod_registry &mod_registry::instance() {
        static mod_registry r;
        return r;
    }

    std::vector<std::string> mod_registry::read_enabled_mods(const char *ml_pkg_id) {
        std::vector<std::string> out;
        if (!ml_pkg_id) return out;

        std::string path = tef::state::working_dir();
        path += "/mods/";
        path += ml_pkg_id;
        path += "/enables.txt";

        debug("Checking enabled mods file: {}", path);

        std::ifstream f(path);
        if (!f) {
            info("Enabled mods file not found: {}", path);
            return out;
        }

        info("Reading enabled mods from: {}", path);

        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line.empty() || line[0] == '#' || line[0] == ';') {
                debug("Skipping line: '{}' (empty or comment)", line);
                continue;
            }
            out.push_back(std::move(line));
        }
        return out;
    }

    mod_manifest_t *mod_registry::make_manifest(const std::string &mod_path,
                                                const std::string &mod_id,
                                                const char *ml_pkg_id) {
        if (!ml_pkg_id) return nullptr;

        auto *m = static_cast<mod_manifest_t *>(std::malloc(sizeof(mod_manifest_t)));
        if (!m) return nullptr;
        std::memset(m, 0, sizeof(mod_manifest_t));

        m->path = ::strdup(mod_path.c_str());
        m->mod_id = ::strdup(mod_id.c_str());

        std::string priv = tef::state::working_dir();
        priv += "/mods/";
        priv += ml_pkg_id;
        priv += "/private/";
        priv += mod_id;

        std::string logs = tef::state::working_dir();
        logs += "/mods/";
        logs += ml_pkg_id;
        logs += "/logs/";
        logs += mod_id;

        m->private_dir = ::strdup(priv.c_str());
        m->logs_dir = ::strdup(logs.c_str());

        if (!m->path || !m->mod_id || !m->private_dir || !m->logs_dir) {
            free_manifest(m);
            return nullptr;
        }
        return m;
    }

    void mod_registry::free_manifest(mod_manifest_t *m) {
        if (!m) return;
        std::free(const_cast<char *>(m->path));
        std::free(const_cast<char *>(m->mod_id));
        std::free(const_cast<char *>(m->private_dir));
        std::free(const_cast<char *>(m->logs_dir));
        std::free(m);
    }

    void mod_registry::free_mod(const std::unique_ptr<mod_handle> &mod) {
        if (!mod) return;
        if (mod->manifest) {
            free_manifest(mod->manifest);
            mod->manifest = nullptr;
        }
    }

    int mod_registry::load_all() {
        cleanup_all();

        const auto &mlreg = ml::mod_loader_registry::instance();
        const auto &loaders = mlreg.handles();
        if (loaders.empty()) {
            debug("No modloaders loaded");
            return 0;
        }

        int total = 0;
        for (const auto &mlh: loaders) {
            if (!mlh || !mlh->ml_entry || !mlh->ml_entry->info) continue;

            const char *ml_pkg = mlh->ml_entry->info->pkg_id;
            if (!ml_pkg) continue;

            auto enabled = read_enabled_mods(ml_pkg);
            if (enabled.empty()) {
                debug("No enabled mods for {}", ml_pkg);
                continue;
            }

            if (!mlh->ml_entry->ops || !mlh->ml_entry->ops->load_mod) {
                warn("ModLoader {} has no load_mod", ml_pkg);
                continue;
            }

            for (const auto &mod_id: enabled) {
                std::string mod_path = tef::state::working_dir();
                mod_path += "/mods/";
                mod_path += ml_pkg;
                mod_path += "/mod/";
                mod_path += mod_id;

                auto *manifest = make_manifest(mod_path, mod_id, ml_pkg);
                if (!manifest) {
                    error("Failed to create manifest for {}/{}", ml_pkg, mod_id);
                    continue;
                }

                if (const ml_result_t r = mlh->ml_entry->ops->load_mod(manifest); r != ML_SUCCESS) {
                    warn("load_mod failed for {} (result={})", mod_id, static_cast<int>(r));
                    free_manifest(manifest);
                    continue;
                }

                auto mod = std::make_unique<mod_handle>();
                mod->mod_id = mod_id;
                mod->mod_path = mod_path;
                mod->manifest = manifest;
                mod->owner_ml = mlh.get();
                mod->_state = state::loaded;
                mod->load_time = std::time(nullptr);

                handles_.push_back(std::move(mod));
                ++total;
            }
        }

        info("Loaded {} mods total", total);
        return total;
    }

    int mod_registry::init_all() const {
        if (handles_.empty()) return 0;

        int n = 0;
        for (auto &mod: handles_) {
            if (!mod || mod->_state != state::loaded) continue;

            if (!mod->owner_ml || !mod->owner_ml->ml_entry || !mod->owner_ml->ml_entry->ops) {
                mod->_state = state::error;
                continue;
            }

            const auto *ops = mod->owner_ml->ml_entry->ops;
            if (!ops->init_mod) {
                mod->_state = state::initialized;
                ++n;
                continue;
            }

            if (const ml_result_t r = ops->init_mod(mod->manifest); r == ML_SUCCESS) {
                mod->_state = state::initialized;
                ++n;
            } else {
                mod->_state = state::error;
                warn("init_mod failed for {} (result={})", mod->mod_id, static_cast<int>(r));
            }
        }

        info("Initialized {} mods", n);
        return n;
    }

    int mod_registry::cleanup_all() {
        if (handles_.empty()) return 0;

        int n = 0;
        while (!handles_.empty()) {
            auto mod = std::move(handles_.back());
            handles_.pop_back();

            if (mod->_state != state::unloaded &&
                mod->owner_ml && mod->owner_ml->ml_entry &&
                mod->owner_ml->ml_entry->ops &&
                mod->owner_ml->ml_entry->ops->unload_mod) {
                mod->owner_ml->ml_entry->ops->unload_mod(mod->manifest);
            }

            free_mod(mod);
            ++n;
        }

        info("Unloaded {} mods", n);
        return n;
    }

    std::size_t mod_registry::count() const {
        return handles_.size();
    }

    mod_handle *mod_registry::by_index(const std::size_t index) const {
        if (index >= handles_.size()) return nullptr;
        return handles_[index].get();
    }
}
