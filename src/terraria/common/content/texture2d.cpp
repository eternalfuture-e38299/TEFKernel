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

#include "terraria/content/texture2d.hpp"
#include "terraria/main_state.hpp"

#include <cstring>

#include "tef/core/log.hpp"

#include "tef/patchlib/struct/array.hpp"

namespace terraria::content {
    texture2d &texture2d::instance() {
        static texture2d inst;
        return inst;
    }

    // ---------------------------------------------------------------------------
    // 句柄解析
    // ---------------------------------------------------------------------------

    void texture2d::early_init() {
        if (resolved_) return;
        resolved_ = true;
        xna_class_ = patchlib::type("Microsoft.Xna.Framework.Graphics", "Texture2D");
        if (is_server_) {
            // 服务端不加载纹理，只缓存类句柄
            return;
        }

        xna_width_ = xna_class_.get_field("Width");
        xna_height_ = xna_class_.get_field("Height");

#if defined(__ANDROID__)
        auto unity_texture_class_ = patchlib::type("UnityEngine", "Texture");
        auto unity_texture2d_class_ = patchlib::type("UnityEngine", "Texture2D");

        if (unity_texture2d_class_) {
            unity_ctor4_ = unity_texture2d_class_.get_method(".ctor", 4);
            unity_get_writable_ = unity_texture2d_class_.get_method("GetWritableImageData", 1);
            unity_get_raw_size_ = unity_texture2d_class_.get_method("GetRawImageDataSize", 0);
            unity_apply_ = unity_texture2d_class_.get_method("Apply", 0);
        }
        if (unity_texture_class_) {
            unity_set_filter_ = unity_texture_class_.get_method("set_filterMode", 1);
            unity_set_wrap_ = unity_texture_class_.get_method("set_wrapMode", 1);
        }
        if (xna_class_)
            xna_ctor_from_unity_ = xna_class_.get_method(
                ".ctor", {unity_texture2d_class_});
#else
        if (!xna_class_) return;
        xna_ctor5_ = xna_class_.get_method(".ctor", 5);
        xna_set_data_ = xna_class_.get_method("SetData", 1);
#endif
    }

    // ---------------------------------------------------------------------------
    // 尺寸读取
    // ---------------------------------------------------------------------------

    int texture2d::width(const patchlib::handle &tex) const {
        if (!tex) return -1;
        const int w = xna_width_[tex];
        return w;
    }

    int texture2d::height(const patchlib::handle &tex) const {
        if (!tex) return -1;
        const int h = xna_height_[tex];
        return h;
    }

    // ---------------------------------------------------------------------------
    // 创建入口
    // ---------------------------------------------------------------------------
#if !defined(__ANDROID__)
    // ---------------------------------------------------------------------------
    // 桌面：GraphicsDevice + XNA Texture2D(width, height, mipmap, format) + SetData<uint8>
    // ---------------------------------------------------------------------------
    patchlib::handle texture2d::create(int width, int height, const texture_format_t format,
                                       const uint8_t *data, size_t data_size) const {
        if (is_server_) {
            log::info("texture2d::create: server mode, returning null");
            return {};
        }
        if (!data || data_size == 0 || width <= 0 || height <= 0) return {};

        if (!xna_ctor5_ || !xna_set_data_) {
            log::error("texture2d: desktop handles not resolved");
            return {};
        }

        // GraphicsDevice 从 Main.instance 拿（Main.instance 是当前 Game）
        const patchlib::handle graphics_device = main_state::instance().graphics_device();
        if (!graphics_device) {
            log::error("texture2d: GraphicsDevice unavailable");
            return {};
        }

        // .ctor(GraphicsDevice, int, int, bool, TextureFormat)
        patchlib::handle texture = xna_ctor5_.invoke_ctor(graphics_device, width, height, false, format);

        if (!texture) {
            log::error("texture2d: Texture2D ctor failed ({}x{})", width, height);
            return {};
        }

        // 校验尺寸
        const int tw = xna_width_[texture];
        if (const int th = xna_height_[texture]; tw != width || th != height) {
            log::warn("texture2d: size mismatch got {}x{} expected {}x{}", tw, th, width, height);
        }

        // SetData<uint8>(byte[])
        auto uint8_type = patchlib::type::get_basic_type(PATCH_UINT8);
        if (!uint8_type) return {};

        const auto set_data_gen = xna_set_data_.make_generic_instance({uint8_type});
        if (!set_data_gen) {
            log::error("texture2d: SetData<uint8> generic failed");
            return {};
        }

        auto data_array = patchlib::structs::array<uint8_t>(data_size);
        if (!data_array) {
            log::error("texture2d: data array create failed (%zu)", data_size);
            return {};
        }
        data_array.copy_from_c(data, data_size);
        set_data_gen[texture](data_array);

        // data_array / set_data_gen / uint8_type / graphics_device 全是 RAII，自动释放
        return texture;
    }
#else
    // ---------------------------------------------------------------------------
    // Android：Unity Texture2D → GetWritableImageData → memcpy → Apply → XNA 包装
    // ---------------------------------------------------------------------------

    patchlib::handle texture2d::create(const int width, const int height, const texture_format_t format,
                                       const uint8_t *data, size_t data_size) const {
        if (!data || data_size == 0 || width <= 0 || height <= 0) return {};

        if (!unity_ctor4_ || !unity_get_writable_ ||
            !unity_get_raw_size_ || !unity_apply_ || !xna_ctor_from_unity_) {
            log::error("texture2d: android handles not resolved");
            return {};
        }

        // new UnityEngine.Texture2D , .ctor(width, height, format, mipChain)
        const patchlib::handle unity_tex = unity_ctor4_.invoke_ctor(width, height, format, false);
        if (!unity_tex) {
            log::error("texture2d: unity Texture2D instantiate failed");
            return {};
        }

        // filterMode = Point(0), wrapMode = Clamp(1) —— 与原 C 实现一致
        unity_set_filter_[unity_tex](0);
        unity_set_wrap_[unity_tex](1);

        // GetWritableImageData(0) → void*
        const auto pixel_ptr = unity_get_writable_[unity_tex].invoke<void *>(0);

        if (!pixel_ptr) {
            log::error("texture2d: GetWritableImageData returned null");
            return {};
        }

        // GetRawImageDataSize()
        if (const auto raw_size = unity_get_raw_size_[unity_tex].invoke<uint64_t>(); raw_size != data_size) {
            log::error("texture2d: data size mismatch expected %zu got %llu",
                       data_size, static_cast<unsigned long long>(raw_size));
            return {};
        }

        // memcpy + Apply
        std::memcpy(pixel_ptr, data, data_size);
        unity_apply_[unity_tex]();

        // XNA Texture2D(Unity.Texture2D) 包装
        patchlib::handle xna_tex = xna_ctor_from_unity_.invoke_ctor(unity_tex);
        if (!xna_tex) {
            log::error("texture2d: xna Texture2D instantiate failed");
            return {};
        }

        // unity_tex 由 xna_tex 持有；RAII 出作用域会释放 unity_tex 的 patchlib 句柄，
        // 但底层的 Unity 对象仍被 XNA 包装引用，不受影响。
        return xna_tex;
    }
#endif
    void texture2d::shutdown() {
        // 只清 C++ 持有的 patchlib 句柄；游戏对象本身不归我们管
        xna_width_.reset();
        xna_height_.reset();
#if !defined(__ANDROID__)
        xna_ctor5_.reset();
        xna_set_data_.reset();
#else
        unity_ctor4_.reset();
        unity_get_writable_.reset();
        unity_get_raw_size_.reset();
        unity_apply_.reset();
        unity_set_filter_.reset();
        unity_set_wrap_.reset();
        xna_ctor_from_unity_.reset();
        unity_filter_wrap_resolved_.reset();
#endif
        resolved_ = false;
    }
}
