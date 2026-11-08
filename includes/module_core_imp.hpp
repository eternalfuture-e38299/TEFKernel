/*******************************************************************************
 * tefkernel - module_core_imp
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

#include "module/module_core.h"
#include "tefpackage/tefpkg.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace tef::module {
    /* ============================================================================
     * module_handle_t 的 C++ 完整定义
     *
     * 与公开 ABI 的 struct module_handle_t 字段完全一致（module_core_imp.h 已删除）。
     * 外部只通过 tefkernel_get_module_by_index 拿到 module_handle_t*，
     * 内部用 ModuleHandle 别名。
     * ============================================================================ */

    struct module_handle_t {
        void *handle = nullptr; /* 动态库句柄 */
        module_entry_t *module_entry = nullptr; /* 条目指针 */
        std::size_t index = 0; /* 索引 */
    };

    using module_handle = module_handle_t;

    /* ============================================================================
     * 全局模块注册表
     * ============================================================================ */

    class module_registry {
    public:
        static module_registry &instance();

        /* ---- 加载 / 清理 ---- */
        bool load(void *lib_handle, tefpkg_t *pkg, module_handle **out);

        void cleanup(module_handle *handle);

        void cleanup_all();

        /* ---- 初始化 / 热重载 ---- */
        bool init_all();

        void hot_reload_all() const;

        /* ---- 查询 ---- */
        [[nodiscard]] module_handle *by_index(std::size_t index) const;

        [[nodiscard]] std::size_t count() const;

        [[nodiscard]] static const module_info_t *get_info(const module_handle *handle);

        /* ---- 遍历（供 tpf_core / modloader 用） ---- */
        [[nodiscard]] const std::vector<std::unique_ptr<module_handle> > &handles() const {
            return handles_;
        }

        /* ---- 插件引用查询（只读，供 tpf_core 判断） ---- */
        [[nodiscard]] bool check_plugin_references(const char *pkg_id) const;

    private:
        module_registry() = default;

        void free_module(const std::unique_ptr<module_handle> &mh) const;

        void cleanup_unreferenced_plugins(const module_info_t *info) const;

        static std::string make_private_dir(const char *pkg_id);

        static std::string make_logs_dir(const char *pkg_id);

        std::vector<std::unique_ptr<module_handle> > handles_;
    };

    /* ============================================================================
     * C ↔ C++ 转换（module_handle_t 类型相同，无需 reinterpret_cast）
     * ============================================================================ */

    inline module_handle_t *to_c(module_handle *m) { return m; }
    inline module_handle *from_c(module_handle_t *m) { return m; }
}
