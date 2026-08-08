/*******************************************************************************
 * tefkernel - main
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
 * Created: 2026/7/24
 *******************************************************************************/

#include "patchlib/field.h"
#include "terraria/main.h"
#include "internal/terraria/main.h"

#include "patchlib/method.h"
#include "patchlib/property.h"

static patch_handle_t get_graphics_device = PATCH_NULL;
static patch_handle_t cur_release = PATCH_NULL;
static patch_handle_t instance = PATCH_NULL;

void terraria_main_init(bool is_server) {
    patch_handle_t main_class = patchlib_type_get_type("Terraria", "Main");
    patch_handle_t game_class = PATCH_NULL;
    if (is_server)
      game_class = patchlib_type_get_type("Terraria.Server", "Game");
    else game_class = patchlib_type_get_type("Microsoft.Xna.Framework", "Game");

    patch_handle_t graphics_device = patchlib_type_get_property(game_class, "GraphicsDevice");
    get_graphics_device = patchlib_property_get_get_method(graphics_device);
    cur_release = patchlib_type_get_field(main_class, "curRelease");
    instance = patchlib_type_get_field(main_class, "instance");

    patchlib_free(game_class);
    patchlib_free(main_class);
    patchlib_free(graphics_device);
}


int terraria_main_get_cur_release() {
    int game_release = -1;
    if (patchlib_is_valid(cur_release)) {
        patchlib_field_get_value(cur_release, PATCH_NULL, &game_release);
    }

    return game_release;
}

patch_handle_t terraria_main_get_graphics_device() {
    patch_handle_t graphics_device = PATCH_NULL;
    patch_handle_t main_instance = PATCH_NULL;

    patchlib_field_get_value(instance, NULL, &main_instance);
    patchlib_method_invoke_args(get_graphics_device, main_instance, &graphics_device, NULL);

    patchlib_free(main_instance);

    return graphics_device;
}