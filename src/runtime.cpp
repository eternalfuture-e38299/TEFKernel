/*******************************************************************************
 * tefkernel - runtime
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

/*******************************************************************************
 * tefkernel - runtime
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *******************************************************************************/

#include "runtime.hpp"
#include "crash_handler.hpp"
#include "kernel_state.hpp"
#include "mod_core.hpp"
#include "modloader_core_imp.hpp"
#include "module_core_imp.hpp"
#include "tef/core/log.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <ranges>
#include <string>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "../includes/tef_core_imp.hpp"
#include "memdl/memdl.h"
#include "patchlib/thread.h"
#include "tefpackage/tefpkg.h"

namespace tef::runtime {
    using log::debug;
    using log::error;
    using log::info;
    using log::warn;

    /* ============================================================================
     * 包哈希表
     * ============================================================================ */

    using package_map = std::unordered_map<std::string, tefpkg_t *>;
    using plugin_id_set = std::unordered_set<std::string>;

    namespace {
        package_map g_plugin_pkgs;
        package_map g_modloader_pkgs;
        package_map g_module_pkgs;
    }

    /* ============================================================================
     * 热重载线程
     * ============================================================================ */

    namespace {
        std::thread g_hotreload_thread;
        std::mutex g_hotreload_mutex;
        std::atomic g_hotreload_running{false};
        std::atomic g_hotreload_enabled{true};
    }

    /* ============================================================================
     * 并行任务
     * ============================================================================ */

    namespace {
        struct async_task {
            std::thread thread;

            void (*work_fn)(void *) = nullptr;

            void *arg = nullptr;
        };

        void async_worker(const async_task *task) {
#if defined(TEF_PLATFORM_ANDROID)
            patch_handle_t il2cpp_thread = patchlib_thread_attach();
#endif

            task->work_fn(task->arg);

#if defined(TEF_PLATFORM_ANDROID)
            patchlib_thread_detach(il2cpp_thread);
#endif
        }

        void parallel_execute(std::vector<async_task> &tasks) {
            for (auto &t: tasks) {
                if (!t.work_fn) continue;
                try {
                    t.thread = std::thread(async_worker, &t);
                } catch (const std::system_error &e) {
                    error("Failed to create async worker: {}", e.what());
                    t.work_fn(t.arg);
                }
            }
            for (auto &t: tasks) {
                if (t.thread.joinable()) t.thread.join();
            }
        }
    } // namespace

    /* ============================================================================
     * 读 enables.txt
     * ============================================================================ */

    namespace {
        std::vector<std::string> load_enabled_ids(const char *base_dir) {
            std::vector<std::string> out;

            std::string path = state::working_dir();
            path += '/';
            path += base_dir;
            path += "/enables.txt";

            std::ifstream f(path);
            if (!f) return out;

            std::string line;
            while (std::getline(f, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty() || line[0] == '#' || line[0] == ';') continue;
                out.push_back(std::move(line));
            }
            return out;
        }
    } // namespace

    /* ============================================================================
     * 加载单个目录的包
     * ============================================================================ */

    namespace {
        void load_single_pkg_type(const char *type_name, package_map &target) {
            info("Loading {} packages", type_name);

            const auto ids = load_enabled_ids(type_name);
            if (ids.empty()) {
                debug("No enabled {} packages", type_name);
                return;
            }

            debug("Found {} enabled {} packages", ids.size(), type_name);

            int loaded = 0, failed = 0;

            for (const auto &pkg_id: ids) {
                std::string pkg_path = tef::state::working_dir();
                pkg_path += '/';
                pkg_path += type_name;
                pkg_path += "/pkg/";
                pkg_path += pkg_id;
                pkg_path += ".tefpkg";

                tefpkg_t *pkg = nullptr;
                if (const tefpkg_result_t r = tefpkg_open_readonly(pkg_path.c_str(), &pkg); r != tef_ok || !pkg) {
                    warn("Failed to open {}: {} (result={})", pkg_id, pkg_path, static_cast<int>(r));
                    ++failed;
                    continue;
                }

                if (target.emplace(pkg_id, pkg).second) {
                    debug("Loaded {}: {}", type_name, pkg_id);
                    ++loaded;
                } else {
                    warn("Duplicate package id: {}", pkg_id);
                    tefpkg_close(pkg);
                    ++failed;
                }
            }

            info("{} packages: {} loaded, {} failed", type_name, loaded, failed);
        }

        void scan_pkg_dir(const char *type_name, package_map &target) {
            info("Scanning {} packages (on-demand mode)", type_name);

            std::string dir_path = tef::state::working_dir();
            dir_path += '/';
            dir_path += type_name;
            dir_path += "/pkg";

            std::error_code ec;
            if (!std::filesystem::exists(dir_path, ec)) {
                debug("Directory not found: {}", dir_path);
                return;
            }

            int loaded = 0, failed = 0;

            for (const auto &ent: std::filesystem::directory_iterator(dir_path, ec)) {
                if (ec) break;

                const auto &path = ent.path();
                if (path.extension() != ".tefpkg") continue;

                std::string pkg_id = path.stem().string();

                tefpkg_t *pkg = nullptr;
                if (const tefpkg_result_t r = tefpkg_open_readonly(path.string().c_str(), &pkg); r != tef_ok || !pkg) {
                    warn("Failed to open {}: {} (result={})", pkg_id, path.string(), static_cast<int>(r));
                    ++failed;
                    continue;
                }

                if (target.emplace(pkg_id, pkg).second) {
                    debug("Discovered {}: {}", type_name, pkg_id);
                    ++loaded;
                } else {
                    warn("Duplicate package id: {}", pkg_id);
                    tefpkg_close(pkg);
                    ++failed;
                }
            }

            info("{} packages discovered: {} loaded, {} failed", type_name, loaded, failed);
        }
    } // namespace

    /* ============================================================================
     * 热重载：目录修改检测
     * ============================================================================ */

    namespace {
        std::time_t get_directory_mtime(const char *path) {
            std::error_code ec;
            const auto t = std::filesystem::last_write_time(path, ec);
            if (ec) return 0;
            return std::chrono::duration_cast<std::chrono::seconds>(
                t.time_since_epoch()).count();
        }

        bool check_working_dir_modified(std::time_t &last_check_time) {
            const std::time_t cur = get_directory_mtime(state::working_dir());

            if (last_check_time == 0) {
                last_check_time = cur;
                return false;
            }

            if (cur != last_check_time) {
                debug("Working directory modified: old={} new={}", last_check_time, cur);
                last_check_time = cur;
                return true;
            }
            return false;
        }

        void hot_reload_thread_func() {
            info("Hot reload thread started");

            std::time_t last_check_time = 0;

            while (g_hotreload_running.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::seconds(1));

                std::lock_guard lock(g_hotreload_mutex);
                if (g_hotreload_enabled.load(std::memory_order_relaxed) &&
                    check_working_dir_modified(last_check_time)) {
                    info("Detected working directory modification, hot reloading");
                    hot_reload();
                    info("Hot reload completed");
                }
            }

            info("Hot reload thread stopped");
        }
    } // namespace

    /* ============================================================================
     * Init
     * ============================================================================ */

    void init() {
        info("Initializing package runtime system");

        crash::init();

        plugin::plugin_registry::init_kernel();
        debug("Initialized libtefkernel");

        const char *wd = state::working_dir();
        if (!wd || !*wd) {
            error("runtime init: working dir is not set, skip package loading");
            return;
        }

        load_single_pkg_type("modloader", g_modloader_pkgs);
        scan_pkg_dir("plugin", g_plugin_pkgs);
        load_single_pkg_type("module", g_module_pkgs);

        const std::size_t total = g_plugin_pkgs.size() +
                                  g_modloader_pkgs.size() +
                                  g_module_pkgs.size();

        info("Package initialization complete: {} total packages", total);
    }

    /* ============================================================================
     * 依赖收集 / 加载
     * ============================================================================ */

    namespace {
        void collect_deps_from_pkg(const tefpkg_t *pkg, const char *create_symbol,
                                   plugin_id_set &required) {
            if (!pkg || !create_symbol) return;

            uint8_t *dylib = nullptr;
            uint32_t dylib_size = 0;
            if (tefpkg_extract_entry_to_memory(pkg, TEFPKG_ID_DYLIB, &dylib, &dylib_size) != tef_ok
                || !dylib || dylib_size == 0) {
                return;
            }

            void *handle = memdl_open(dylib, dylib_size, MEMDL_LAZY);
            std::free(dylib);
            if (!handle) return;

            auto collect_from = [&](const char **deps, const int n) {
                for (int i = 0; i < n; ++i) {
                    if (deps[i]) {
                        required.emplace(deps[i]);
                        debug("Collected plugin dependency: {}", deps[i]);
                    }
                }
            };

            if (std::strcmp(create_symbol, "module_create") == 0) {
                auto *fn = reinterpret_cast<const module_ops_t* (*)()>(
                    memdl_sym(handle, "module_create"));
                if (fn) {
                    if (const module_ops_t *ops = fn(); ops && ops->get_info) {
                        if (const module_info_t *info = ops->get_info();
                            info && info->plugin_dependencies && info->plugin_dependencies_sizes > 0)
                            collect_from(info->plugin_dependencies, info->plugin_dependencies_sizes);
                    }
                }
            } else if (std::strcmp(create_symbol, "ml_create") == 0) {
                auto *fn = reinterpret_cast<const ml_ops_t* (*)()>(
                    memdl_sym(handle, "ml_create"));
                if (fn) {
                    if (const ml_ops_t *ops = fn(); ops && ops->get_info) {
                        if (const ml_info_t *info = ops->get_info();
                            info && info->plugin_dependencies && info->plugin_dependencies_sizes > 0)
                            collect_from(info->plugin_dependencies, info->plugin_dependencies_sizes);
                    }
                }
            }

            memdl_close(handle);
        }

        void collect_required_plugins(plugin_id_set &required) {
            info("Collecting required plugins");

            for (const auto &pkg: g_module_pkgs | std::views::values)
                if (pkg) collect_deps_from_pkg(pkg, "module_create", required);

            for (const auto &pkg: g_modloader_pkgs | std::views::values)
                if (pkg) collect_deps_from_pkg(pkg, "ml_create", required);

            info("Collected {} required plugins", required.size());
        }

        void load_required_plugins(const plugin_id_set &required) {
            info("Loading required plugins");

            int loaded = 0, failed = 0;

            for (auto &[pkg_id, pkg]: g_plugin_pkgs) {
                if (!pkg) continue;
                if (!required.contains(pkg_id)) {
                    debug("Skipping plugin (not required): {}", pkg_id);
                    continue;
                }

                uint8_t *dylib = nullptr;
                uint32_t dylib_size = 0;
                const tefpkg_result_t r = tefpkg_extract_entry_to_memory(
                    pkg, TEFPKG_ID_DYLIB, &dylib, &dylib_size);

                if (r != tef_ok || !dylib || dylib_size == 0) {
                    warn("Failed to extract plugin dylib: {}", pkg_id);
                    ++failed;
                    continue;
                }

                void *handle = memdl_open(dylib, dylib_size, MEMDL_LAZY);
                std::free(dylib);

                if (!handle) {
                    warn("Failed to open dylib for plugin: {}", pkg_id);
                    ++failed;
                    continue;
                }

                if (tef::plugin::plugin_registry::instance().load(handle, nullptr)) {
                    ++loaded;
                    debug("Loaded required plugin: {}", pkg_id);
                } else {
                    warn("Failed to load plugin: {}", pkg_id);
                    ++failed;
                }
            }

            info("Required plugins: {} loaded, {} failed", loaded, failed);
        }

        void load_all_mod_loaders() {
            info("Loading all modloaders");

            int loaded = 0, failed = 0;

            for (auto &[pkg_id, pkg]: g_modloader_pkgs) {
                if (!pkg) continue;

                uint8_t *dylib = nullptr;
                uint32_t dylib_size = 0;
                const tefpkg_result_t r = tefpkg_extract_entry_to_memory(
                    pkg, TEFPKG_ID_DYLIB, &dylib, &dylib_size);

                if (r != tef_ok || !dylib || dylib_size == 0) {
                    warn("Failed to extract modloader dylib: {}", pkg_id);
                    ++failed;
                    continue;
                }

                void *handle = memdl_open(dylib, dylib_size, MEMDL_LAZY);
                std::free(dylib);

                if (!handle) {
                    warn("Failed to open dylib for modloader: {}", pkg_id);
                    ++failed;
                    continue;
                }

                if (tef::ml::mod_loader_registry::instance().load(handle, pkg, nullptr)) {
                    ++loaded;
                    debug("Loaded modloader: {}", pkg_id);
                } else {
                    warn("Failed to load modloader: {}", pkg_id);
                    ++failed;
                }
            }

            info("ModLoaders: {} loaded, {} failed", loaded, failed);
        }

        void load_all_modules() {
            info("Loading all modules");

            int loaded = 0, failed = 0;

            for (auto &[pkg_id, pkg]: g_module_pkgs) {
                if (!pkg) continue;

                uint8_t *dylib = nullptr;
                uint32_t dylib_size = 0;
                const tefpkg_result_t r = tefpkg_extract_entry_to_memory(
                    pkg, TEFPKG_ID_DYLIB, &dylib, &dylib_size);

                if (r != tef_ok || !dylib || dylib_size == 0) {
                    warn("Failed to extract module dylib: {}", pkg_id);
                    ++failed;
                    continue;
                }

                void *handle = memdl_open(dylib, dylib_size, MEMDL_LAZY);
                std::free(dylib);

                if (!handle) {
                    warn("Failed to open dylib for module: {}", pkg_id);
                    ++failed;
                    continue;
                }

                if (module::module_registry::instance().load(handle, pkg, nullptr)) {
                    ++loaded;
                    debug("Loaded module: {}", pkg_id);
                } else {
                    warn("Failed to load module: {}", pkg_id);
                    ++failed;
                }
            }

            info("Modules: {} loaded, {} failed", loaded, failed);
        }

        void load_all_mods() {
            info("Loading all mods");
            if (const int n = tef::mod::mod_registry::instance().load_all(); n > 0) info("Loaded {} mods", n);
            else debug("No mods loaded");
        }
    } // namespace

    /* ============================================================================
     * Load
     * ============================================================================ */

    void load() {
        info("Loading all components");

        plugin_id_set required_plugins;
        collect_required_plugins(required_plugins);
        load_required_plugins(required_plugins);
        // ReSharper disable once CppExpressionWithoutSideEffects
        plugin::plugin_registry::instance().init_all();

        load_all_modules();
        load_all_mod_loaders();
        load_all_mods();

        info("All components loaded");
    }

    /* ============================================================================
     * Start
     * ============================================================================ */

    namespace {
        void init_all_mods() {
            info("Initializing all mods");
            if (const int n = mod::mod_registry::instance().init_all(); n > 0) info("Initialized {} mods", n);
            else debug("No mods initialized");
        }

        void module_init_work(void *) {
            module::module_registry::instance().init_all();
        }

        void mod_loader_init_work(void *arg) {
            if (auto *entry = static_cast<ml_entry_t *>(arg); entry && entry->ops && entry->ops->init_ml)
                entry->ops->init_ml(entry);
        }
    } // namespace

    void start() {
        info("Starting kernel runtime");

        std::vector<ml_entry_t *> ml_entries;
        for (const auto &mh: ml::mod_loader_registry::instance().handles()) {
            if (!mh || !mh->ml_entry || !mh->ml_entry->ops) continue;
            if (mh->ml_entry->ops->init_ml)
                ml_entries.push_back(mh->ml_entry);
        }

        std::vector<async_task> tasks;
        tasks.reserve(1 + ml_entries.size());

        tasks.push_back(async_task{.thread = {}, .work_fn = module_init_work, .arg = nullptr});
        for (auto *e: ml_entries)
            tasks.push_back(async_task{.thread = {}, .work_fn = mod_loader_init_work, .arg = e});

        info("Parallel init: 1 module task + {} modloader tasks", ml_entries.size());
        parallel_execute(tasks);
        info("Modules and modloaders done");

        init_all_mods();
        info("Kernel runtime started");
    }

    /* ============================================================================
     * HotReload
     * ============================================================================ */

    namespace {
        void mod_loader_reload_work(void *arg) {
            struct args {
                const ml_ops_t *ops{};
                mod_manifest_t manifest{};
            };
            auto *a = static_cast<args *>(arg);
            if (a && a->ops && a->ops->reload_mod)
                a->ops->reload_mod(&a->manifest);
        }

        void module_reload_work(void *arg) {
            if (auto *entry = static_cast<module_entry_t *>(arg); entry && entry->ops && entry->ops->hot_reload)
                entry->ops->hot_reload(entry);
        }
    } // namespace

    void hot_reload() {
        info("Manual hot reload triggered");

        struct ml_args_t {
            const ml_ops_t *ops = nullptr;
            mod_manifest_t manifest{};
        };

        std::vector<ml_args_t> ml_args;
        for (const auto &mh: ml::mod_loader_registry::instance().handles()) {
            if (!mh || !mh->ml_entry || !mh->ml_entry->ops) continue;
            if (mh->ml_entry->ops->reload_mod) {
                ml_args_t a;
                a.ops = mh->ml_entry->ops;
                ml_args.push_back(a);
            }
        }

        std::vector<module_entry_t *> mod_entries;
        for (const auto &mh: tef::module::module_registry::instance().handles()) {
            if (!mh || !mh->module_entry || !mh->module_entry->ops) continue;
            if (mh->module_entry->ops->hot_reload)
                mod_entries.push_back(mh->module_entry);
        }

        if (ml_args.empty() && mod_entries.empty()) {
            info("Manual hot reload completed (nothing to reload)");
            return;
        }

        std::vector<async_task> tasks;
        tasks.reserve(ml_args.size() + mod_entries.size());

        for (auto &a: ml_args)
            tasks.push_back(async_task{.thread = {}, .work_fn = mod_loader_reload_work, .arg = &a});
        for (auto *e: mod_entries)
            tasks.push_back(async_task{.thread = {}, .work_fn = module_reload_work, .arg = e});

        info("Hot reloading {} tasks in parallel", tasks.size());
        parallel_execute(tasks);

        info("Manual hot reload completed");
    }

    /* ============================================================================
     * 热重载线程
     * ============================================================================ */

    void start_hot_reload_thread() {
        if (g_hotreload_running.exchange(true, std::memory_order_acq_rel)) {
            warn("Hot reload thread already running");
            return;
        }

        g_hotreload_enabled.store(true, std::memory_order_relaxed);

        try {
            g_hotreload_thread = std::thread(hot_reload_thread_func);
        } catch (const std::system_error &e) {
            error("Failed to create hot reload thread: {}", e.what());
            g_hotreload_running.store(false, std::memory_order_release);
            return;
        }

        info("Hot reload thread started");
    }

    void stop_hot_reload_thread() {
        if (!g_hotreload_running.load(std::memory_order_acquire)) {
            debug("Hot reload thread not running");
            return;
        }

        info("Stopping hot reload thread");
        g_hotreload_running.store(false, std::memory_order_release);
        if (g_hotreload_thread.joinable())
            g_hotreload_thread.join();
        info("Hot reload thread stopped");
    }

    void set_hot_reload_enabled(const bool enabled) {
        std::lock_guard lock(g_hotreload_mutex);
        g_hotreload_enabled.store(enabled, std::memory_order_relaxed);
        info("Hot reload {}", enabled ? "enabled" : "disabled");
    }

    /* ============================================================================
     * Cleanup
     * ============================================================================ */

    namespace {
        void cleanup_table(package_map &table, const char *name) {
            debug("Cleaning up {} table", name);
            int closed = 0;
            for (const auto &pkg: table | std::views::values) {
                if (pkg) {
                    tefpkg_close(pkg);
                    ++closed;
                }
            }
            table.clear();
            debug("Cleaned {} table: {} packages closed", name, closed);
        }
    } // namespace

    void cleanup() {
        info("Cleaning up package runtime system");

        stop_hot_reload_thread();
        module::module_registry::instance().cleanup_all();
        ml::mod_loader_registry::instance().cleanup_all();

        cleanup_table(g_plugin_pkgs, "plugin");
        cleanup_table(g_modloader_pkgs, "modloader");
        cleanup_table(g_module_pkgs, "module");

        info("Package runtime cleanup complete");
    }
} // namespace tef::runtime
