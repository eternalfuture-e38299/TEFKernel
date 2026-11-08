/*******************************************************************************
 * tefkernel - texture2d
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

#include <cstdint>

#include "../init/bootstrap.hpp"

#include "tef/patchlib/handle.hpp"
#include "tef/patchlib/field.hpp"
#include "tef/patchlib/method.hpp"
#include "tef/patchlib/type.hpp"
#include "terraria/texture2d.h"   // texture_format_t（ABI，不许改）

namespace terraria::content {
    using namespace tef;

    class texture2d final : public init::subsystem {
    public:
        static texture2d &instance();

        [[nodiscard]] std::string_view name() const override { return "texture2d"; }

        void early_init() override; // 解析类/方法句柄（惰性也行，这里提前解析）
        void shutdown() override; // 清句柄

        [[nodiscard]] bool is_server() const { return is_server_; }
        void set_server(const bool v) { is_server_ = v; }

        // 创建纹理；失败返回空 handle。返回的 handle 是 owning。
        patchlib::handle create(int width, int height, texture_format_t format,
                                const uint8_t *data, size_t data_size) const;

        [[nodiscard]] int width(const patchlib::handle &tex) const;

        [[nodiscard]] int height(const patchlib::handle &tex) const;

        // XNA Texture2D 类句柄（转移所有权给调用方）
        [[nodiscard]] patch_handle_t xna_class() const { return xna_class_.copy().release(); }

    private:
        texture2d() = default;

        bool is_server_ = false;
        bool resolved_ = false;

        // 公共
        patchlib::type xna_class_;
        patchlib::field xna_width_;
        patchlib::field xna_height_;

#if !defined(__ANDROID__)
        // 桌面：XNA 构造 + SetData<T>
        patchlib::method xna_ctor5_;
        patchlib::method xna_set_data_;
#else
        // Android：Unity Texture2D + XNA 包装
        patchlib::method unity_ctor4_;
        patchlib::method unity_get_writable_;
        patchlib::method unity_get_raw_size_;
        patchlib::method unity_apply_;
        patchlib::method unity_set_filter_;
        patchlib::method unity_set_wrap_;
        patchlib::method xna_ctor_from_unity_;
        patchlib::field unity_filter_wrap_resolved_;
#endif
    };
}