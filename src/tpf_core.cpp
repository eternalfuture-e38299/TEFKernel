/*******************************************************************************
 * tefkernel - tpf_core
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

#include "tefplugin/tpf_core.h"
#include "tef_core_imp.hpp"

#include <algorithm>
#include <cstring>
#include <thread>
#include <vector>

#include "module_core_imp.hpp"
#include "modloader_core_imp.hpp"
#include "memdl/memdl.h"
#include "modloader/modloader_core.h"
#include "module/module_core.h"
#include "patchlib/thread.h"
#include "tef/core/log.hpp"

namespace tef::plugin {
    using log::debug;
    using log::error;
    using log::info;
    using log::trace;
    using log::warn;

    /* ============================================================================
     * 单例
     * ============================================================================ */

    plugin_registry &plugin_registry::instance() {
        static plugin_registry r;
        return r;
    }

    /* ============================================================================
     * 符号注册
     * ============================================================================ */

    bool plugin_registry::register_symbol(plugin_handle *handle, const char *name,
                                          const void *addr) const {
        if (!handle || !name || !addr) {
            error("Invalid parameters for symbol registration");
            return false;
        }

        handle->sym_names.push_back(name);
        handle->sym_addrs.push_back(addr);

        for (void *lib: libraries_) {
            if (!lib) continue;
            if (const auto slot = static_cast<void **>(memdl_sym(lib, name))) {
                *slot = const_cast<void *>(addr);
            }
        }
        return true;
    }

    /* ============================================================================
     * 引用计数
     * ============================================================================ */

    void plugin_registry::add_ref(const char *pkg_id) {
        if (!pkg_id) return;

        if (const auto it = refs_.find(pkg_id); it != refs_.end()) {
            it->second.ref_count++;
            debug("Plugin {} ref -> {}", pkg_id, it->second.ref_count);
        } else {
            refs_.emplace(pkg_id, ref_entry{.pkg_id = pkg_id, .ref_count = 1});
            debug("Plugin {} ref init 1", pkg_id);
        }
    }

    void plugin_registry::remove_ref(const char *pkg_id) {
        if (!pkg_id) return;

        const auto it = refs_.find(pkg_id);
        if (it == refs_.end()) {
            warn("Plugin {} not tracked", pkg_id);
            return;
        }

        it->second.ref_count--;
        debug("Plugin {} ref -> {}", pkg_id, it->second.ref_count);

        if (it->second.ref_count <= 0) {
            refs_.erase(it);
            debug("Plugin {} untracked", pkg_id);
        }
    }

    int plugin_registry::get_ref_count(const char *pkg_id) const {
        if (!pkg_id) return 0;
        const auto it = refs_.find(pkg_id);
        return it == refs_.end() ? 0 : it->second.ref_count;
    }

    bool plugin_registry::has_references(const char *pkg_id) const {
        if (!pkg_id) return false;

        int count = get_ref_count(pkg_id);

        /* 模块依赖：遍历 module_registry */
        for (const auto &mh: module::module_registry::instance().handles()) {
            if (!mh || !mh->module_entry || !mh->module_entry->info) continue;

            const module_info_t *info = mh->module_entry->info;
            if (!info->plugin_dependencies || info->plugin_dependencies_sizes <= 0)
                continue;

            for (int j = 0; j < info->plugin_dependencies_sizes; ++j) {
                if (info->plugin_dependencies[j] &&
                    std::strcmp(info->plugin_dependencies[j], pkg_id) == 0) {
                    ++count;
                    break;
                }
            }
        }

        /* ModLoader 依赖：遍历 mod_loader_registry */
        for (const auto &mh: ml::mod_loader_registry::instance().handles()) {
            if (!mh || !mh->ml_entry || !mh->ml_entry->info) continue;

            const ml_info_t *info = mh->ml_entry->info;
            if (!info->plugin_dependencies || info->plugin_dependencies_sizes <= 0)
                continue;

            for (int j = 0; j < info->plugin_dependencies_sizes; ++j) {
                if (info->plugin_dependencies[j] &&
                    std::strcmp(info->plugin_dependencies[j], pkg_id) == 0) {
                    ++count;
                    break;
                }
            }
        }

        debug("Plugin {} has {} references", pkg_id, count);
        return count > 0;
    }

    /* ============================================================================
     * 查询
     * ============================================================================ */

    plugin_handle *plugin_registry::find_by_pkg_id(const char *pkg_id) const {
        if (!pkg_id) {
            error("NULL pkg_id provided to FindByPkgId");
            return nullptr;
        }

        debug("Find plugin {} (total={})", pkg_id, plugins_.size());

        for (const auto &p: plugins_) {
            if (!p || !p->ops || !p->ops->get_info) continue;
            if (const tpf_plugin_info_t *info = p->ops->get_info();
                info && info->pkg_id && std::strcmp(info->pkg_id, pkg_id) == 0) {
                debug("Found plugin {} at index {}", pkg_id, p->index);
                return p.get();
            }
        }

        debug("Plugin {} not found", pkg_id);
        return nullptr;
    }

    bool plugin_registry::exists(const char *pkg_id) const {
        if (!pkg_id) return false;
        if (!initialized_) return false;
        return find_by_pkg_id(pkg_id) != nullptr;
    }

    /* ============================================================================
     * 插件生命周期
     * ============================================================================ */

    void plugin_registry::free_plugin(const std::unique_ptr<plugin_handle> &plugin) {
        if (!plugin) return;
        debug("Freeing plugin {}", fmt::ptr(plugin.get()));

        if (plugin->ops && plugin->ops->cleanup) {
            plugin->ops->cleanup(to_c(plugin.get()));
        }

        if (plugin->lib_handle) {
            memdl_close(plugin->lib_handle);
            plugin->lib_handle = nullptr;
        }
    }

    bool plugin_registry::load(void *lib_handle, plugin_handle **out) {
        if (!lib_handle) {
            error("Invalid handle provided to Load");
            return false;
        }

        info("Loading plugin from handle {}", fmt::ptr(lib_handle));

        if (!initialized_) initialized_ = true;

        if (plugins_.empty()) {
            init_kernel();
        }

        auto *create_fn = reinterpret_cast<tpf_plugin_ops_t* (*)()>(
            memdl_sym(lib_handle, "tpf_create_plugin"));
        if (!create_fn) {
            error("Failed to find tpf_create_plugin in {}", fmt::ptr(lib_handle));
            memdl_close(lib_handle);
            return false;
        }

        if (const auto slot = static_cast<void **>(memdl_sym(lib_handle, "tpf_register_symbol"))) {
            *slot = reinterpret_cast<void *>(&tpf_register_symbol);
        }

        register_shared_library(lib_handle);

        auto plugin = std::make_unique<plugin_handle>();
        plugin->lib_handle = lib_handle;
        plugin->ops = create_fn();
        if (!plugin->ops) {
            error("tpf_create_plugin returned NULL");
            memdl_close(lib_handle);
            return false;
        }

        plugin->index = plugins_.size();
        debug("Created plugin index={}", plugin->index);

        plugin_handle *raw = plugin.get();
        plugins_.push_back(std::move(plugin));

        if (!raw->ops->initialize || !raw->ops->initialize(to_c(raw))) {
            error("Plugin initialization failed");
            plugins_.pop_back();
            return false;
        }

        if (raw->ops->get_info) {
            if (const tpf_plugin_info_t *info = raw->ops->get_info(); info && info->pkg_id) add_ref(info->pkg_id);
        }

        info("Plugin initialized: {}", fmt::ptr(raw));
        if (out) *out = raw;
        return true;
    }

    bool plugin_registry::cleanup(plugin_handle *plugin) {
        if (!plugin) {
            warn("Attempted to cleanup NULL plugin");
            return false;
        }

        info("Cleaning up plugin {} (index={})", fmt::ptr(plugin), plugin->index);

        for (void *lib: libraries_) {
            if (!lib) continue;
            for (const char *name: plugin->sym_names) {
                if (!name) continue;
                if (const auto slot = static_cast<void **>(memdl_sym(lib, name))) {
                    debug("Nullifying symbol {} in library {}", name, fmt::ptr(lib));
                    *slot = nullptr;
                }
            }
        }

        const auto it = std::ranges::find_if(plugins_,
                                             [plugin](const std::unique_ptr<plugin_handle> &p) {
                                                 return p.get() == plugin;
                                             });
        if (it != plugins_.end()) {
            if ((*it)->ops && (*it)->ops->get_info) {
                if (const tpf_plugin_info_t *info = (*it)->ops->get_info(); info && info->pkg_id) remove_ref(
                    info->pkg_id);
            }
            plugins_.erase(it);
        }

        info("Plugin cleanup completed");
        return true;
    }

    /* ============================================================================
     * 共享库
     * ============================================================================ */

    bool plugin_registry::register_shared_library(void *lib_handle) {
        if (!lib_handle) {
            error("Invalid handle provided to RegisterSharedLibrary");
            return false;
        }

        info("Registering shared library {}", fmt::ptr(lib_handle));

        if (std::ranges::find(libraries_, lib_handle) != libraries_.end()) {
            debug("Library {} already registered", fmt::ptr(lib_handle));
            return true;
        }

        libraries_.push_back(lib_handle);
        libs_initialized_ = true;

        debug("Library {} added (total={})", fmt::ptr(lib_handle), libraries_.size());

        register_all_symbols_to_library(lib_handle);
        return true;
    }

    bool plugin_registry::register_all_symbols_to_library(void *lib_handle) const {
        if (!lib_handle) {
            error("Invalid handle provided to RegisterAllSymbolsToLibrary");
            return false;
        }

        if (plugins_.empty()) {
            debug("No plugins loaded to register symbols");
            return true;
        }

        info("Registering {} plugins' symbols to library {}",
             plugins_.size(), fmt::ptr(lib_handle));

        std::size_t total = 0;
        bool any = false;

        for (const auto &p: plugins_) {
            if (!p) continue;
            std::size_t ok = 0;
            for (std::size_t i = 0; i < p->sym_names.size(); ++i) {
                const char *name = p->sym_names[i];
                const void *addr = p->sym_addrs[i];
                if (!name || !addr) continue;
                if (const auto slot = static_cast<void **>(memdl_sym(lib_handle, name))) {
                    *slot = const_cast<void *>(addr);
                    ++ok;
                    ++total;
                }
            }
            if (ok > 0) {
                any = true;
                debug("Plugin {}: {}/{} symbols registered",
                      p->index, ok, p->sym_names.size());
            }
        }

        info("Registered {} symbols to {} (success={})",
             total, fmt::ptr(lib_handle), any);
        return any;
    }

    /* ============================================================================
     * 初始化
     * ============================================================================ */

    bool plugin_registry::init_all() const {
        init_kernel();

        if (plugins_.empty()) {
            debug("No plugins to initialize");
            return true;
        }

        info("Initializing {} plugins", plugins_.size());
        bool all_ok = true;

        for (const auto &p: plugins_) {
            if (!p || !p->ops || !p->ops->initialize) continue;
            const char *pkg = (p->ops->get_info && p->ops->get_info()->pkg_id)
                                  ? p->ops->get_info()->pkg_id
                                  : "unknown";
            debug("Initializing plugin {} ({})", p->index, pkg);
            if (!p->ops->initialize(to_c(p.get()))) {
                error("Plugin {} initialization failed", p->index);
                all_ok = false;
            }
        }

        for (void *lib: libraries_) {
            if (!lib) continue;
            if (!register_all_symbols_to_library(lib)) {
                warn("Some symbols failed to register for library {}", fmt::ptr(lib));
                all_ok = false;
            }
        }

        info("InitAll completed: {}", all_ok ? "success" : "with errors");
        return all_ok;
    }
} // namespace tef::plugin

/* ============================================================================
 * extern "C" 导出（与旧 tpf_core.c 一一对应）
 * ============================================================================ */

using tef::plugin::from_c;
using tef::plugin::plugin_handle;
using tef::plugin::plugin_registry;
using tef::plugin::to_c;

extern "C" {
/* ---- 注册符号 ---- */

bool tpf_register_symbol(plugin_handle_t *this_handle, const char *name,
                         const void *addr) {
    if (!this_handle) return false;
    return plugin_registry::instance().register_symbol(from_c(this_handle), name, addr);
}

bool tpf_register_shared_plugin_library(void *handle) {
    return plugin_registry::instance().register_shared_library(handle);
}

/* ---- 查询 ---- */

bool tpf_plugin_exists(const char *pkg_id) {
    return plugin_registry::instance().exists(pkg_id);
}

plugin_handle_t *tpf_get_plugin_by_id(const char *pkg_id) {
    return to_c(plugin_registry::instance().find_by_pkg_id(pkg_id));
}

/* ---- 加载 / 清理 ---- */

bool tpf_load_plugin(void *handle, plugin_handle_t **out_plugin) {
    plugin_handle *raw = nullptr;
    if (!plugin_registry::instance().load(handle, &raw)) return false;
    if (out_plugin) *out_plugin = to_c(raw);
    return true;
}

bool tpf_cleanup_plugin(plugin_handle_t *plugin) {
    return plugin_registry::instance().cleanup(from_c(plugin));
}

/* ---- 符号批量注册 ---- */

bool tpf_register_all_plugin_symbols_to_library(void *handle) {
    return plugin_registry::instance().register_all_symbols_to_library(handle);
}

bool tpf_register_plugin_symbols(const plugin_handle_t *plugin) {
    if (!plugin) {
        tef::log::error("Invalid plugin provided to tpf_register_plugin_symbols");
        return false;
    }

    auto &reg = plugin_registry::instance();
    if (!reg.is_initialized()) {
        tef::log::warn("Plugin registry not initialized");
        return false;
    }

    /* 原版用多线程逐库注册；这里保持相同的语义但串行，
     * 因为 memdl_sym 是内存查找，线程开销大于收益，且并发删除有风险。 */
    bool any = false;
    for (void *lib: reg.libraries()) {
        if (!lib) continue;
        if (reg.register_all_symbols_to_library(lib)) any = true;
    }
    return any;
}

/* ---- 初始化 ---- */

bool tpf_initialize_all_plugins(void) {
    return plugin_registry::instance().init_all();
}

/* ---- 引用计数 ---- */

void tpf_init_plugin_refs(void) {
    /* 单例懒加载，无需额外初始化 */
}

void tpf_add_plugin_ref(const char *pkg_id) {
    plugin_registry::instance().add_ref(pkg_id);
}

void tpf_remove_plugin_ref(const char *pkg_id) {
    plugin_registry::instance().remove_ref(pkg_id);
}

int tpf_get_plugin_ref_count(const char *pkg_id) {
    return plugin_registry::instance().get_ref_count(pkg_id);
}

bool tpf_check_plugin_references(const char *pkg_id) {
    return plugin_registry::instance().has_references(pkg_id);
}
} // extern "C"

/* ============================================================================
 * 内核符号注册（tpf_init_libtefkernel）
 * ============================================================================ */

namespace {
    struct kernel_plugin_holder {
        plugin_handle *handle = nullptr;
    };

    kernel_plugin_holder &kernel_plugin() {
        static kernel_plugin_holder h;
        return h;
    }
} // namespace

#define TPF_KERNEL_SYMBOL(func) \
    tpf_register_symbol(to_c(kernel_plugin().handle), #func, \
                        reinterpret_cast<const void*>(func))

/* 所有公开头（供符号注册时取函数地址） */
#include "tefstd/hashmap.h"
#include "tefstd/skipmap.h"
#include "tefstd/log.h"
#include "tefstd/vector.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/property.h"
#include "patchlib/type.h"
#include "patchlib/struct/array.h"
#include "patchlib/struct/dictionary.h"
#include "patchlib/struct/list.h"
#include "patchlib/struct/string.h"
#include "tefpackage/tefpkg.h"
#include "terraria/main.h"
#include "terraria/texture2d.h"
#include "terraria/asset.h"
#include "terraria/recipe_manager.h"
#include "terraria/language_manager.h"
#include "terraria/content/item_manager.h"
#include "terraria/content/projectile_manager.h"
#include "terraria/content/npc_manager.h"
#include "terraria/content/buff_manager.h"
#include "terraria/content/audio_manager.h"
#include "terraria/content/prefix_manager.h"
#include "terraria/gui.h"
#include "terraria/netmanager.h"

#if defined(__ANDROID__)
#   include "terraria/content/button_manager.h"
#endif

bool plugin_registry::init_kernel() {
    info("Initializing libtefkernel");

    if (kernel_plugin().handle) {
        debug("Kernel plugin already initialized");
        return false;
    }

    auto *hp = new plugin_handle();
    hp->index = 0;
    kernel_plugin().handle = hp;

    /* ---- memdl ---- */
    TPF_KERNEL_SYMBOL(memdl_open_file);
    TPF_KERNEL_SYMBOL(memdl_open);
    TPF_KERNEL_SYMBOL(memdl_sym);
    TPF_KERNEL_SYMBOL(memdl_close);
    TPF_KERNEL_SYMBOL(memdl_error);
    TPF_KERNEL_SYMBOL(memdl_get_arch);
    TPF_KERNEL_SYMBOL(memdl_validate);
    TPF_KERNEL_SYMBOL(memdl_get_platform);

    /* ---- hashmap ---- */
    TPF_KERNEL_SYMBOL(tefstd_hash_str);
    TPF_KERNEL_SYMBOL(tefstd_hash_mem);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_init);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_free);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_put);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_get);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_del);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_has);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_len);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_clear);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_iter);
    TPF_KERNEL_SYMBOL(tefstd_hashmap_next);

    /* ---- skipmap ---- */
    TPF_KERNEL_SYMBOL(tefstd_skipmap_init);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_free);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_put);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_get);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_del);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_min);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_max);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_range);
    TPF_KERNEL_SYMBOL(tefstd_skipmap_next);

    /* ---- vector ---- */
    TPF_KERNEL_SYMBOL(tefstd_vector_init);
    TPF_KERNEL_SYMBOL(tefstd_vector_destroy);
    TPF_KERNEL_SYMBOL(tefstd_vector_push_back);
    TPF_KERNEL_SYMBOL(tefstd_vector_pop_back);
    TPF_KERNEL_SYMBOL(tefstd_vector_at);
    TPF_KERNEL_SYMBOL(tefstd_vector_size);
    TPF_KERNEL_SYMBOL(tefstd_vector_capacity);
    TPF_KERNEL_SYMBOL(tefstd_vector_clear);
    TPF_KERNEL_SYMBOL(tefstd_vector_reserve);
    TPF_KERNEL_SYMBOL(tefstd_vector_erase);
    TPF_KERNEL_SYMBOL(tefstd_vector_remove_value);
    TPF_KERNEL_SYMBOL(tefstd_vector_init_from_array);

    /* ---- tefstd log ---- */
    TPF_KERNEL_SYMBOL(tefstd_log_open);
    TPF_KERNEL_SYMBOL(tefstd_log_close);
    TPF_KERNEL_SYMBOL(tefstd_log_write);
    TPF_KERNEL_SYMBOL(tefstd_log_write_ex);
    TPF_KERNEL_SYMBOL(tefstd_log_set_level);
    TPF_KERNEL_SYMBOL(tefstd_log_flush);
    TPF_KERNEL_SYMBOL(tefstd_log_dropped);

    /* ---- tefpkg ---- */
    TPF_KERNEL_SYMBOL(tefpkg_create_reserved_from_file);
    TPF_KERNEL_SYMBOL(tefpkg_create_reserved_from_memory);
    TPF_KERNEL_SYMBOL(tefpkg_open_readonly);
    TPF_KERNEL_SYMBOL(tefpkg_open_from_memory);
    TPF_KERNEL_SYMBOL(tefpkg_save_file);
    TPF_KERNEL_SYMBOL(tefpkg_save_memory_file);
    TPF_KERNEL_SYMBOL(tefpkg_close);
    TPF_KERNEL_SYMBOL(tefpkg_add_entry_from_memory);
    TPF_KERNEL_SYMBOL(tefpkg_add_entry_from_file);
    TPF_KERNEL_SYMBOL(tefpkg_extract_entry_to_memory);
    TPF_KERNEL_SYMBOL(tefpkg_extract_entry_to_file);
    TPF_KERNEL_SYMBOL(tefpkg_get_entry_info);
    TPF_KERNEL_SYMBOL(tefpkg_get_entries_count);
    TPF_KERNEL_SYMBOL(tefpkg_get_reserved_entries);
    TPF_KERNEL_SYMBOL(tefpkg_verify_entry);
    TPF_KERNEL_SYMBOL(tefpkg_verify_pkg);
    TPF_KERNEL_SYMBOL(tefpkg_verify_signature);
    TPF_KERNEL_SYMBOL(tefpkg_sign_package);

    /* ---- patchlib 基础 ---- */
    TPF_KERNEL_SYMBOL(get_size_from_patch_type);
    TPF_KERNEL_SYMBOL(patchlib_is_valid);
    TPF_KERNEL_SYMBOL(patchlib_type_get_type);
    TPF_KERNEL_SYMBOL(patchlib_get_basic_type);
    TPF_KERNEL_SYMBOL(patchlib_type_new_instance);
    TPF_KERNEL_SYMBOL(patchlib_type_make_generic_type);
#if defined(__ANDROID__)
    TPF_KERNEL_SYMBOL(patchlib_type_get_mono_type);
#endif
    TPF_KERNEL_SYMBOL(patchlib_type_get_name);
    TPF_KERNEL_SYMBOL(patchlib_type_get_namespace);
    TPF_KERNEL_SYMBOL(patchlib_type_get_full_name);
    TPF_KERNEL_SYMBOL(patchlib_type_get_parent);
    TPF_KERNEL_SYMBOL(patchlib_type_get_inner_type);
    TPF_KERNEL_SYMBOL(patchlib_type_get_field);
    TPF_KERNEL_SYMBOL(patchlib_type_get_property);
    TPF_KERNEL_SYMBOL(patchlib_type_get_method);
    TPF_KERNEL_SYMBOL(patchlib_type_get_method_by_param_count);
    TPF_KERNEL_SYMBOL(patchlib_type_get_method_by_param_names);
    TPF_KERNEL_SYMBOL(patchlib_type_get_method_by_param_types);
    TPF_KERNEL_SYMBOL(patchlib_type_get_method_by_signature);
    TPF_KERNEL_SYMBOL(patchlib_type_get_inner_types);
    TPF_KERNEL_SYMBOL(patchlib_type_get_methods);
    TPF_KERNEL_SYMBOL(patchlib_type_get_fields);
    TPF_KERNEL_SYMBOL(patchlib_type_get_properties);

    /* ---- patchlib field ---- */
    TPF_KERNEL_SYMBOL(patchlib_field_get_name);
    TPF_KERNEL_SYMBOL(patchlib_field_is_static);
    TPF_KERNEL_SYMBOL(patchlib_field_is_instance);
    TPF_KERNEL_SYMBOL(patchlib_field_is_const);
    TPF_KERNEL_SYMBOL(patchlib_field_is_thread_static);
    TPF_KERNEL_SYMBOL(patchlib_field_get_value);
    TPF_KERNEL_SYMBOL(patchlib_field_set_value);
    TPF_KERNEL_SYMBOL(patchlib_field_get_type);

    /* ---- patchlib method ---- */
    TPF_KERNEL_SYMBOL(patchlib_method_get_name);
    TPF_KERNEL_SYMBOL(patchlib_method_get_param_count);
    TPF_KERNEL_SYMBOL(patchlib_method_is_instance);
    TPF_KERNEL_SYMBOL(patchlib_method_is_static);
    TPF_KERNEL_SYMBOL(patchlib_method_make_generic_instance);
    TPF_KERNEL_SYMBOL(patchlib_method_invoke_args);
    TPF_KERNEL_SYMBOL(patchlib_constructor_invoke);
    TPF_KERNEL_SYMBOL(patchlib_method_get_token);
    TPF_KERNEL_SYMBOL(patchlib_method_get_signature);
    TPF_KERNEL_SYMBOL(patchlib_method_signature_free);
    TPF_KERNEL_SYMBOL(patchlib_install_prepost_hook);
    TPF_KERNEL_SYMBOL(patchlib_uninstall_hook);

    /* ---- patchlib property ---- */
    TPF_KERNEL_SYMBOL(patchlib_property_get_name);
    TPF_KERNEL_SYMBOL(patchlib_property_get_get_method);
    TPF_KERNEL_SYMBOL(patchlib_property_get_set_method);

    /* ---- patchlib array ---- */
    TPF_KERNEL_SYMBOL(patchlib_array_create);
    TPF_KERNEL_SYMBOL(patchlib_array_at);
    TPF_KERNEL_SYMBOL(patchlib_array_set);
    TPF_KERNEL_SYMBOL(patchlib_array_fill);
    TPF_KERNEL_SYMBOL(patchlib_array_empty);
    TPF_KERNEL_SYMBOL(patchlib_array_length);
    TPF_KERNEL_SYMBOL(patchlib_array_element_size);

    /* ---- patchlib list ---- */
    TPF_KERNEL_SYMBOL(patchlib_list_create);
    TPF_KERNEL_SYMBOL(patchlib_list_copy_from);
    TPF_KERNEL_SYMBOL(patchlib_list_add);
    TPF_KERNEL_SYMBOL(patchlib_list_remove);
    TPF_KERNEL_SYMBOL(patchlib_list_remove_at);
    TPF_KERNEL_SYMBOL(patchlib_list_clear);
    TPF_KERNEL_SYMBOL(patchlib_list_get_array);

    /* ---- patchlib dictionary ---- */
    TPF_KERNEL_SYMBOL(patchlib_dictionary_create);
    TPF_KERNEL_SYMBOL(patchlib_dictionary_add);
    TPF_KERNEL_SYMBOL(patchlib_dictionary_get_value);
    TPF_KERNEL_SYMBOL(patchlib_dictionary_set_value);
    TPF_KERNEL_SYMBOL(patchlib_dictionary_clear);
    TPF_KERNEL_SYMBOL(patchlib_dictionary_length);
    TPF_KERNEL_SYMBOL(patchlib_dictionary_remove);

    /* ---- patchlib string ---- */
    TPF_KERNEL_SYMBOL(patchlib_string_create);
    TPF_KERNEL_SYMBOL(patchlib_string_cstr);
    TPF_KERNEL_SYMBOL(patchlib_string_empty);
    TPF_KERNEL_SYMBOL(patchlib_string_length);

#if __ANDROID__
    /* ---- Android 专属 ---- */
    TPF_KERNEL_SYMBOL(patchlib_object_root);
    TPF_KERNEL_SYMBOL(patchlib_object_unroot);
    TPF_KERNEL_SYMBOL(patchlib_field_get_pointer);
    TPF_KERNEL_SYMBOL(patchlib_field_get_size);
    TPF_KERNEL_SYMBOL(patchlib_method_get_pointer);
    TPF_KERNEL_SYMBOL(patchlib_install_typed_hook);
    TPF_KERNEL_SYMBOL(patchlib_uninstall_typed_hook);
    TPF_KERNEL_SYMBOL(patchlib_thread_attach);
    TPF_KERNEL_SYMBOL(patchlib_thread_current);
    TPF_KERNEL_SYMBOL(patchlib_thread_detach);
#else
    /* ---- 桌面端资源管理 ---- */
    TPF_KERNEL_SYMBOL(patchlib_free);
    TPF_KERNEL_SYMBOL(patchlib_handle_copy);
#endif

    TPF_KERNEL_SYMBOL(tpf_register_shared_plugin_library);

    /* ---- terraria main ---- */
    TPF_KERNEL_SYMBOL(terraria_main_get_cur_release);
    TPF_KERNEL_SYMBOL(terraria_main_get_graphics_device);
    TPF_KERNEL_SYMBOL(terraria_main_get_local_player);
    TPF_KERNEL_SYMBOL(terraria_main_is_server);

    /* ---- terraria texture2d ---- */
    TPF_KERNEL_SYMBOL(terraria_texture2d_create);
    TPF_KERNEL_SYMBOL(terraria_texture2d_get_width);
    TPF_KERNEL_SYMBOL(terraria_texture2d_get_height);
    TPF_KERNEL_SYMBOL(terraria_texture2d_get_class);

    /* ---- terraria asset ---- */
    TPF_KERNEL_SYMBOL(terraria_asset_create);
    TPF_KERNEL_SYMBOL(terraria_asset_get_generic_class);
    TPF_KERNEL_SYMBOL(terraria_asset_get_value);

    /* ---- terraria recipe ---- */
    TPF_KERNEL_SYMBOL(terraria_recipe_manager_get_current_recipe);
    TPF_KERNEL_SYMBOL(terraria_recipe_set_result);
    TPF_KERNEL_SYMBOL(terraria_recipe_set_material);
    TPF_KERNEL_SYMBOL(terraria_recipe_set_station);
    TPF_KERNEL_SYMBOL(terraria_recipe_add);
    TPF_KERNEL_SYMBOL(terraria_recipe_manager_register_callback);
    TPF_KERNEL_SYMBOL(terraria_recipe_manager_unregister_callback);

    /* ---- terraria language ---- */
    TPF_KERNEL_SYMBOL(terraria_language_manager_create_localized_text);
    TPF_KERNEL_SYMBOL(terraria_language_manager_get_localized_text);
    TPF_KERNEL_SYMBOL(terraria_language_manager_register_callback);
    TPF_KERNEL_SYMBOL(terraria_language_manager_unregister_callback);

/*#if defined(__ANDROID__)
    /* ---- Android 按钮 ---- #1#
    TPF_KERNEL_SYMBOL(terraria_button_manager_available);
    TPF_KERNEL_SYMBOL(terraria_button_manager_error);
    TPF_KERNEL_SYMBOL(terraria_button_manager_configure_storage);
    TPF_KERNEL_SYMBOL(terraria_button_manager_install);
    TPF_KERNEL_SYMBOL(terraria_button_manager_shutdown);
    TPF_KERNEL_SYMBOL(terraria_button_manager_register);
    TPF_KERNEL_SYMBOL(terraria_button_manager_unregister);
    TPF_KERNEL_SYMBOL(terraria_button_manager_unregister_owner);
    TPF_KERNEL_SYMBOL(terraria_button_manager_down);
    TPF_KERNEL_SYMBOL(terraria_button_manager_pressed);
    TPF_KERNEL_SYMBOL(terraria_button_manager_released);
    TPF_KERNEL_SYMBOL(terraria_button_manager_set_enabled);
    TPF_KERNEL_SYMBOL(terraria_button_manager_set_visible);
#endif*/

    /* ---- item manager ---- */
    TPF_KERNEL_SYMBOL(terraria_item_manager_register_item);
    TPF_KERNEL_SYMBOL(terraria_item_manager_unregister_item_by_id);
    TPF_KERNEL_SYMBOL(terraria_item_manager_unregister_item);
    TPF_KERNEL_SYMBOL(terraria_item_manager_get_item);
    TPF_KERNEL_SYMBOL(terraria_item_manager_get_items);
    TPF_KERNEL_SYMBOL(terraria_item_manager_set_prefix_class);
    TPF_KERNEL_SYMBOL(terraria_item_manager_apply_prefix_classes);

    /* ---- projectile manager ---- */
    TPF_KERNEL_SYMBOL(terraria_projectile_manager_register_projectile);
    TPF_KERNEL_SYMBOL(terraria_projectile_manager_unregister_projectile_by_id);
    TPF_KERNEL_SYMBOL(terraria_projectile_manager_unregister_projectile);
    TPF_KERNEL_SYMBOL(terraria_projectile_manager_get_projectile);
    TPF_KERNEL_SYMBOL(terraria_projectile_manager_get_projectiles);
    TPF_KERNEL_SYMBOL(terraria_projectile_manager_find_projectile);
    TPF_KERNEL_SYMBOL(terraria_projectile_id_get_count);

    /* ---- npc manager ---- */
    TPF_KERNEL_SYMBOL(terraria_npc_manager_register_npc);
    TPF_KERNEL_SYMBOL(terraria_npc_manager_unregister_npc_by_id);
    TPF_KERNEL_SYMBOL(terraria_npc_manager_unregister_npc);
    TPF_KERNEL_SYMBOL(terraria_npc_manager_get_npc);
    TPF_KERNEL_SYMBOL(terraria_npc_manager_get_npcs);
    TPF_KERNEL_SYMBOL(terraria_npc_manager_find_npc);
    TPF_KERNEL_SYMBOL(terraria_npc_id_get_count);
    TPF_KERNEL_SYMBOL(terraria_npc_manager_spawn);
    TPF_KERNEL_SYMBOL(terraria_npc_manager_drop_item);

    /* ---- buff manager ---- */
    TPF_KERNEL_SYMBOL(terraria_buff_manager_register_buff);
    TPF_KERNEL_SYMBOL(terraria_buff_manager_unregister_buff_by_id);
    TPF_KERNEL_SYMBOL(terraria_buff_manager_unregister_buff);
    TPF_KERNEL_SYMBOL(terraria_buff_manager_get_buff);
    TPF_KERNEL_SYMBOL(terraria_buff_manager_get_buffs);
    TPF_KERNEL_SYMBOL(terraria_buff_manager_find_buff);
    TPF_KERNEL_SYMBOL(terraria_buff_id_get_count);
    TPF_KERNEL_SYMBOL(terraria_buff_manager_add);
    TPF_KERNEL_SYMBOL(terraria_buff_manager_time);
    TPF_KERNEL_SYMBOL(terraria_buff_manager_remove);
    TPF_KERNEL_SYMBOL(terraria_buff_manager_clear_debuffs);

    /* ---- gui ---- */
    TPF_KERNEL_SYMBOL(terraria_gui_add_draw);
    TPF_KERNEL_SYMBOL(terraria_gui_remove_draw);
    TPF_KERNEL_SYMBOL(terraria_gui_rect);
    TPF_KERNEL_SYMBOL(terraria_gui_text);
    TPF_KERNEL_SYMBOL(terraria_gui_string_text);
    TPF_KERNEL_SYMBOL(terraria_gui_image);

    /* ---- audio ---- */
    TPF_KERNEL_SYMBOL(terraria_audio_manager_register);
    TPF_KERNEL_SYMBOL(terraria_audio_manager_unregister);
    TPF_KERNEL_SYMBOL(terraria_audio_manager_play);
    TPF_KERNEL_SYMBOL(terraria_audio_manager_play_at);
    TPF_KERNEL_SYMBOL(terraria_audio_manager_pause);
    TPF_KERNEL_SYMBOL(terraria_audio_manager_resume);
    TPF_KERNEL_SYMBOL(terraria_audio_manager_stop);
    TPF_KERNEL_SYMBOL(terraria_audio_manager_set_volume);
    TPF_KERNEL_SYMBOL(terraria_audio_manager_is_playing);
    TPF_KERNEL_SYMBOL(terraria_audio_manager_stop_all);
    TPF_KERNEL_SYMBOL(terraria_audio_manager_set_master_volume);

    /* ---- prefix ---- */
    TPF_KERNEL_SYMBOL(terraria_prefix_manager_register_prefix);
    TPF_KERNEL_SYMBOL(terraria_prefix_manager_unregister_prefix_by_id);
    TPF_KERNEL_SYMBOL(terraria_prefix_manager_get_prefix);
    TPF_KERNEL_SYMBOL(terraria_prefix_manager_find_prefix);
    TPF_KERNEL_SYMBOL(terraria_prefix_id_get_count);
    TPF_KERNEL_SYMBOL(terraria_prefix_manager_remaining_slots);

    /* ---- net ---- */
    TPF_KERNEL_SYMBOL(terraria_net_send_tile_action);
    TPF_KERNEL_SYMBOL(terraria_net_send_tile_square);
    TPF_KERNEL_SYMBOL(terraria_net_send_mod_message);
    TPF_KERNEL_SYMBOL(terraria_net_set_mod_message_handler);

    info("Kernel symbols registered: {}",
         kernel_plugin().handle->sym_names.size());

    return true;
}
