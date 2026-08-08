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
 * Created: 2026/7/25
 *******************************************************************************/

#include "terraria/texture2d.h"

#include "patchlib/method.h"
#include "patchlib/property.h"
#include "patchlib/struct/array.h"
#include "terraria/main.h"

static patch_handle_t texture2d_ctor = PATCH_NULL;
static patch_handle_t texture2d_set_data = PATCH_NULL;
static patch_handle_t texture2d_get_height = PATCH_NULL;
static patch_handle_t texture2d_get_width = PATCH_NULL;
static bool server_mode = false;


void terraria_texture2d_init(bool is_server) {
    server_mode = is_server;
    patch_handle_t texture2d_class = patchlib_type_get_type("Microsoft.Xna.Framework.Graphics", "Texture2D");
    texture2d_ctor = patchlib_type_get_method_by_param_count(texture2d_class, ".ctor", 5);
    texture2d_set_data = patchlib_type_get_method_by_param_count(texture2d_class, "SetData", 1);
    patch_handle_t texture2d_height = patchlib_type_get_property(texture2d_class, "Height");
    patch_handle_t texture2d_width = patchlib_type_get_property(texture2d_class, "Width");

    texture2d_get_height = patchlib_property_get_get_method(texture2d_height);
    texture2d_get_width = patchlib_property_get_get_method(texture2d_width);

    patchlib_free(texture2d_height);
    patchlib_free(texture2d_width);
    patchlib_free(texture2d_class);
}

patch_handle_t terraria_texture2d_create(int width, int height, texture_format_t texture_format, void* data, size_t data_size) {
    if (server_mode) return PATCH_NULL;

    static bool mip_map = false;
    patch_handle_t texture2d = PATCH_NULL;

    patch_handle_t uint8_type = patchlib_get_basic_type(PATCH_UINT8);
    tefstd_vector_t generic_type;
    tefstd_vector_init(&generic_type, sizeof(patch_handle_t));
    tefstd_vector_push_back(&generic_type, &uint8_type);
    patch_handle_t set_data_uint8 = patchlib_method_make_generic_instance(texture2d_set_data, &generic_type);
    patch_handle_t graphics_device = terraria_main_get_graphics_device();
    void* args[5] = { &graphics_device, &width, &height, &mip_map, &texture_format };
    patchlib_constructor_invoke(texture2d_ctor, &texture2d, args);

    patch_handle_t data_array = patchlib_array_create(data_size, uint8_type);
    patchlib_array_copy_from_c(data_array, data, data_size);

    void* set_data_args[1] = { &data_array };
    patchlib_method_invoke_args(set_data_uint8, texture2d, NULL, set_data_args);

    patchlib_free(graphics_device);
    patchlib_free(uint8_type);
    patchlib_free(data_array);
    patchlib_free(set_data_uint8);
    tefstd_vector_destroy(&generic_type);

    return texture2d;
}

int terraria_texture2d_get_width(patch_handle_t texture2d) {
    int width = -1;
    patchlib_method_invoke_args(texture2d_get_width, texture2d, &width, NULL);
    return width;
}

int terraria_texture2d_get_height(patch_handle_t texture2d) {
    int height = -1;
    patchlib_method_invoke_args(texture2d_get_height, texture2d, &height, NULL);
    return height;
}

patch_handle_t terraria_texture2d_get_class() {
    return patchlib_type_get_type("Microsoft.Xna.Framework.Graphics", "Texture2D");
}