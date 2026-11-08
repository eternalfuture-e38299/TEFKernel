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

#include "terraria/texture2d.h"                       // include_abi，不许改
#include "terraria/content/texture2d.hpp"

#include "terraria/abi/guard.hpp"

namespace {
    using terraria::content::texture2d;
    using terraria::abi::guard;
} // namespace

extern "C" {
patch_handle_t terraria_texture2d_create(const int width, const int height,
                                         const texture_format_t texture_format,
                                         void *data, const size_t data_size) {
    return guard([&] {
        auto tex = texture2d::instance().create(width, height, texture_format, static_cast<uint8_t *>(data), data_size);
        return tex.release(); // ABI 返回裸 handle，调用方接管所有权
    }, static_cast<patch_handle_t>(nullptr));
}

int terraria_texture2d_get_width(patch_handle_t texture2d_handle) {
    return guard([&] {
        return texture2d::instance().width(texture2d_handle);
    }, -1);
}

int terraria_texture2d_get_height(patch_handle_t texture2d_handle) {
    return guard([&] {
        return texture2d::instance().height(texture2d_handle);
    }, -1);
}

patch_handle_t terraria_texture2d_get_class(void) {
    return guard([&] {
        return texture2d::instance().xna_class();
    }, static_cast<patch_handle_t>(nullptr));
}
} // extern "C"
