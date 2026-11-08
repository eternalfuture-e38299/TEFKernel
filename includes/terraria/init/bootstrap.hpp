/*******************************************************************************
 * tefkernel - bootstrap
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

#include <string_view>
#include <vector>

namespace terraria::init {

    class subsystem {
    public:
        virtual ~subsystem() = default;
        [[nodiscard]] virtual std::string_view name() const = 0;

        // 解析句柄、装 hook（hook 不卸载，进程结束随游戏退出）
        virtual void early_init() {}

        // Main.Initialize 前缀：分配 ID、resize 已存在的数组（allow_create=false）
        virtual void on_main_initialize_prefix() {}

        // Main.Initialize 后缀：resize(allow_create=true)、texture、l10n、
        // init_static、apply_prefix_classes —— 原 prefix + AlmostEverything 的全部兜底
        virtual void on_main_initialize_postfix() {}

        // 只清理 C++ 拥有的资源（registry、std::vector 等）；hook 不卸载
        virtual void shutdown() {}
    };

    class bootstrap {
    public:
        static bootstrap& instance();

        void register_subsystem(subsystem* s);
        void set_server(bool is_server);
        [[nodiscard]] bool is_server() const { return is_server_; }

        void run_early_init() const;
        void run_main_initialize_prefix() const;
        void run_main_initialize_postfix() const;
        void run_shutdown();

    private:
        std::vector<subsystem*> subsystems_;
        bool is_server_ = false;
    };
}