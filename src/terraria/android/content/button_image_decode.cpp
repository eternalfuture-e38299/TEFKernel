/*******************************************************************************
 * LuaLoader - image_decode：通用 PNG 解码（供 GUI 图片与物品贴图共用）
 * Copyright (C) 2026 lzup333
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
 *******************************************************************************/

#include "button_image_decode.hpp"

// 唯一的 stb_image 实现，仅启用 PNG
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "button_stb_image.h"

#include <cstdio>

namespace tefkernel::image {

    bool decode_png(const void *data, const size_t size,
                    std::vector<unsigned char> &out, int &w, int &h) {
        out.clear();
        w = h = 0;
        if (!data || size == 0) return false;
        int channels = 0;
        stbi_uc *px = stbi_load_from_memory(static_cast<const stbi_uc *>(data),
                                            static_cast<int>(size), &w, &h, &channels, 4);
        if (!px) return false;
        out.assign(px, px + static_cast<size_t>(w) * h * 4);
        stbi_image_free(px);
        return true;
    }

    bool decode_png_file(const std::string &path,
                         std::vector<unsigned char> &out, int &w, int &h) {
        FILE *f = std::fopen(path.c_str(), "rb");
        if (!f) return false;
        std::fseek(f, 0, SEEK_END);
        const long n = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (n <= 0) {
            std::fclose(f);
            return false;
        }
        std::vector<unsigned char> bytes(static_cast<size_t>(n));
        const size_t read = std::fread(bytes.data(), 1, bytes.size(), f);
        std::fclose(f);
        if (read != bytes.size()) return false;
        return decode_png(bytes.data(), bytes.size(), out, w, h);
    }

} // namespace tefkernel::image
