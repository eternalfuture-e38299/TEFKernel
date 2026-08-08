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
 * Created: 2026/4/12
 *******************************************************************************/

#include "terraria/texture2d.h"
#include "internal/terraria/texture2d.h"

#include <string.h>

#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/method.h"

static patch_handle_t unity_texture2d_class = PATCH_NULL;
static patch_handle_t unity_texture2d_get_writable_image_data = PATCH_NULL; // void* GetWritableImageData(int frame)
static patch_handle_t unity_texture2d_get_raw_image_data_size = PATCH_NULL; // ulong GetRawImageDataSize()
static patch_handle_t unity_texture2d_apply = PATCH_NULL;   // public void Apply() {}
static patch_handle_t unity_texture2d_create = PATCH_NULL; // Texture2D(int width, int height, TextureFormat textureFormat, bool mipChain)
static patch_handle_t unity_texture_set_filter_mode = PATCH_NULL; // void set_filterMode(FilterMode value) { }
static patch_handle_t unity_texture_set_wrap_mode = PATCH_NULL; // void set_wrapMode(TextureWrapMode value) { }

static patch_handle_t xna_texture2d_class = PATCH_NULL;
static patch_handle_t xna_texture2d_create = PATCH_NULL; // Texture2D(UnityEngine.Texture2D texture)

static patch_handle_t xna_texture2d_width = PATCH_NULL; // public readonly int Width;
static patch_handle_t xna_texture2d_height = PATCH_NULL; // public readonly int Height;


void terraria_texture2d_init(bool is_server) {
    patch_handle_t unity_texture_class = patchlib_type_get_type("UnityEngine", "Texture");
    unity_texture2d_class = patchlib_type_get_type("UnityEngine", "Texture2D");
    unity_texture2d_create = patchlib_type_get_method_by_param_count(unity_texture2d_class, ".ctor", 4);
    unity_texture2d_get_writable_image_data = patchlib_type_get_method_by_param_count(unity_texture2d_class, "GetWritableImageData", 1);
    unity_texture2d_get_raw_image_data_size = patchlib_type_get_method_by_param_count(unity_texture2d_class, "GetRawImageDataSize", 0);
    unity_texture2d_apply = patchlib_type_get_method_by_param_count(unity_texture2d_class, "Apply", 0);
    unity_texture_set_filter_mode = patchlib_type_get_method_by_param_count(unity_texture_class, "set_filterMode", 1);
    unity_texture_set_wrap_mode = patchlib_type_get_method_by_param_count(unity_texture_class, "set_wrapMode", 1);

    xna_texture2d_class = patchlib_type_get_type("Microsoft.Xna.Framework.Graphics", "Texture2D");
    xna_texture2d_create = patchlib_type_get_method_by_param_types(xna_texture2d_class, ".ctor", 1, &unity_texture2d_class);
    xna_texture2d_width = patchlib_type_get_field(xna_texture2d_class, "Width");
    xna_texture2d_height = patchlib_type_get_field(xna_texture2d_class, "Height");
}

static patch_handle_t create_unity_texture2d(const int width, const int height, const texture_format_t texture_format, const void* data, const size_t data_size) {
    patch_handle_t unity_texture2d = patchlib_type_new_instance(unity_texture2d_class);
    if (unity_texture2d == PATCH_NULL) {
        TEKLOG_ERROR("Failed to create Unity Texture2D instance");
        return PATCH_NULL;
    }

    // 调用构造函数
    ((void(*)(void*, int, int, int, bool))patchlib_method_get_pointer(unity_texture2d_create))(
        unity_texture2d, width, height, (int)texture_format, false);

    // 设置过滤和包裹模式
    ((void(*)(void*, int))patchlib_method_get_pointer(unity_texture_set_filter_mode))(unity_texture2d, 0);
    ((void(*)(void*, int))patchlib_method_get_pointer(unity_texture_set_wrap_mode))(unity_texture2d, 1);

    // 获取可写图像数据指针
    void* pixel_data = ((void*(*)(void*, int))patchlib_method_get_pointer(unity_texture2d_get_writable_image_data))(
        unity_texture2d, 0);

    if (pixel_data == NULL) {
        TEKLOG_ERROR("GetWritableImageData returned NULL");
        return PATCH_NULL;
    }

    // 获取图像数据大小
    const uint64_t pixel_data_size = ((uint64_t(*)(void*))patchlib_method_get_pointer(unity_texture2d_get_raw_image_data_size))(
        unity_texture2d);

    // ★★★ 修复：正确的检查逻辑 ★★★
    if (pixel_data_size != data_size) {
        TEKLOG_ERROR("Data size mismatch: expected %zu, got %lu", data_size, pixel_data_size);
        return PATCH_NULL;
    }

    // 复制纹理数据
    memcpy(pixel_data, data, data_size);

    ((void(*)(void*))patchlib_method_get_pointer(unity_texture2d_apply))(unity_texture2d);

    return unity_texture2d;
}

patch_handle_t terraria_texture2d_create(const int width, const int height, const texture_format_t texture_format, void* data, const size_t data_size) {
    patch_handle_t unity_texture2d = create_unity_texture2d(width, height, texture_format, data, data_size);
    patch_handle_t xna_texture2d = patchlib_type_new_instance(xna_texture2d_class);
    ((void(*)(void*, void*))patchlib_method_get_pointer(xna_texture2d_create))(xna_texture2d, unity_texture2d);

    return xna_texture2d;
}

int terraria_texture2d_get_width(patch_handle_t texture2d) {
    int width = -1;
    patchlib_field_get_value(xna_texture2d_width, texture2d, &width);
    return width;
}

int terraria_texture2d_get_height(patch_handle_t texture2d) {
    int height = -1;
    patchlib_field_get_value(xna_texture2d_height, texture2d, &height);
    return height;
}

patch_handle_t terraria_texture2d_get_class() {
    return xna_texture2d_class;
}