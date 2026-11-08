/*******************************************************************************
 * tefkernel - asset_factory
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

#include "../init/bootstrap.hpp"
#include "tef/patchlib/type.hpp"
#include "tef/patchlib/method.hpp"

namespace terraria::content {
    /**
     * ReLogic.Content.Asset`1 的构造与读取。
     *
     * 关键约束：
     *  - `asset` 的 Value 是只读 backing field，写入前必须 set_State。
     *  - Android 上 Asset`1[T] 的泛型实例化用 OBJECT 作为 mono 类型；
     *    桌面用真实 element type。
     *  - `create` 返回的 handle 是 owning；`get_generic_class` 返回新建句柄
     *    （每次调用都新建，调用方接管）；`get_value` 返回新建句柄。
     */
    class asset_factory final : public init::subsystem {
    public:
        static asset_factory &instance();

        [[nodiscard]] std::string_view name() const override { return "asset"; }
        void early_init() override;
        void shutdown() override;

        // 输入 element_type 句柄（借用）；返回 owning type。
        [[nodiscard]] tef::patchlib::type get_generic_class(const tef::patchlib::handle &element_type) const;

        // 输入 type 句柄（借用），返回新建的 Asset`1[T] 实例（owning）。
        [[nodiscard]] tef::patchlib::handle create(const tef::patchlib::handle &type,
                                                   const tef::patchlib::handle &value) const;

        // 读 Asset`1[T].<Value>k__BackingField（返回新建句柄）
        [[nodiscard]] static tef::patchlib::handle get_value(const tef::patchlib::handle &asset);

    private:
        asset_factory() = default;

        tef::patchlib::type asset_class_;
        bool resolved_ = false;
    };
}