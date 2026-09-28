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
#include "internal/terraria/main.h"

#include "internal/log.h"
#include "patchlib/method.h"

#include "internal/terraria/item_manager.h"

static patch_handle_t get_graphics_device = PATCH_NULL;
static patch_handle_t cur_release = PATCH_NULL;
static patch_handle_t instance = PATCH_NULL;

static bool initialize_prefix(patch_handle_t this, void **args,
                               const patch_method_signature_t *sig_info, void *result) {
    int base_count = terraria_item_id_get_count();
    size_t custom_count = tefstd_vector_size(&g_terraria_item_registry);

    TEKLOG_INFO("Main.Initialize: %d base + %zu custom items", base_count, custom_count);

    if (custom_count > 0) {
        terraria_item_manager_assign_ids(base_count);
        TEKLOG_INFO("Assigned IDs and resized arrays");
    }
    
    const size_t count = tefstd_vector_size(&g_terraria_item_registry);
    int initialized = 0;
    for (size_t i = 0; i < count; ++i) {
        terraria_item_handle_t** item_handle = tefstd_vector_at(&g_terraria_item_registry, i);
        if (*item_handle && (*item_handle)->item_ops.early_init) {
            (*item_handle)->item_ops.early_init(*item_handle);
            initialized++;
        }
    }

    TEKLOG_INFO("Initialized %d custom items", initialized);

    return false;
}


static void initialize_almost_everything_postfix(patch_handle_t this, void **args, void *result,
                                 const patch_method_signature_t *sig_info) {
    TEKLOG_INFO("Main.Initialize_AlmostEverything completed, resizing arrays...");

    size_t custom_count = tefstd_vector_size(&g_terraria_item_registry);

    if (custom_count > 0) {
        terraria_item_manager_resize();
        terraria_item_manager_init_texture2d();
        TEKLOG_INFO("Arrays resized and textures initialized");
        
        terraria_item_manager_init_localized_text();
    }

    // 初始化静态数据
    const size_t count = tefstd_vector_size(&g_terraria_item_registry);
    int initialized = 0;
    for (size_t i = 0; i < count; ++i) {
        terraria_item_handle_t** item_handle = tefstd_vector_at(&g_terraria_item_registry, i);
        if (*item_handle && (*item_handle)->item_ops.init_static) {
            (*item_handle)->item_ops.init_static(*item_handle);
            initialized++;
        }
    }
    

    TEKLOG_INFO("Initialized %d custom items", initialized);
}

void terraria_main_init(const bool is_server) {
    TEKLOG_DEBUG("terraria_main_init called: is_server=%d", is_server);

    patch_handle_t main_class = patchlib_type_get_type("Terraria", "Main");
    if (!patchlib_is_valid(main_class)) {
        TEKLOG_ERROR("Failed to get Main class");
        return;
    }
    TEKLOG_DEBUG("Got Main class: %p", main_class);

    patch_handle_t game_class = PATCH_NULL;
    if (is_server) {
        game_class = patchlib_type_get_type("Terraria.Server", "Game");
        TEKLOG_DEBUG("Server mode, getting Game class from Terraria.Server");
    } else {
        game_class = patchlib_type_get_type("Microsoft.Xna.Framework", "Game");
        TEKLOG_DEBUG("Client mode, getting Game class from Microsoft.Xna.Framework");
    }

    if (!patchlib_is_valid(game_class)) {
        TEKLOG_ERROR("Failed to get Game class");
        patchlib_free(main_class);
        return;
    }
    TEKLOG_DEBUG("Got Game class: %p", game_class);

    get_graphics_device = patchlib_type_get_method_by_param_count(game_class, "get_GraphicsDevice", 0);
    if (!patchlib_is_valid(get_graphics_device)) {
        TEKLOG_ERROR("Failed to get GraphicsDevice getter method");
    } else {
        TEKLOG_DEBUG("Got GraphicsDevice getter method: %p", get_graphics_device);
    }

    cur_release = patchlib_type_get_field(main_class, "curRelease");
    if (!patchlib_is_valid(cur_release)) {
        TEKLOG_WARN("Failed to get curRelease field");
    } else {
        TEKLOG_DEBUG("Got curRelease field: %p", cur_release);
    }

    instance = patchlib_type_get_field(main_class, "instance");
    if (!patchlib_is_valid(instance)) {
        TEKLOG_WARN("Failed to get instance field");
    } else {
        TEKLOG_DEBUG("Got instance field: %p", instance);
    }

    patch_handle_t initialize_method = patchlib_type_get_method_by_param_count(main_class, "Initialize", 0);
    if (!patchlib_is_valid(initialize_method)) {
        TEKLOG_ERROR("Failed to get Initialize method");
    } else {
        TEKLOG_DEBUG("Got Initialize method: %p, installing hook", initialize_method);
        patch_hook_id_t hook_id = patchlib_install_prepost_hook(initialize_method, initialize_prefix, NULL);
        if (hook_id != PATCH_HOOK_INVALID_ID) {
            TEKLOG_INFO("Initialize hook installed successfully, ID: %d", hook_id);
        } else {
            TEKLOG_ERROR("Failed to install Initialize hook");
        }
        patchlib_free(initialize_method);
    }

    patch_handle_t load_content_method = patchlib_type_get_method_by_param_count(main_class, "Initialize_AlmostEverything", 0);
    if (!patchlib_is_valid(load_content_method)) {
        TEKLOG_ERROR("Failed to get Initialize_AlmostEverything method");
    } else {
        TEKLOG_DEBUG("Got Initialize_AlmostEverything method: %p, installing hook", load_content_method);
        patch_hook_id_t hook_id = patchlib_install_prepost_hook(
            load_content_method,
            NULL,
            initialize_almost_everything_postfix
        );
        if (hook_id != PATCH_HOOK_INVALID_ID) {
            TEKLOG_INFO("Initialize_AlmostEverything hook installed successfully, ID: %d", hook_id);
        } else {
            TEKLOG_ERROR("Failed to install Initialize_AlmostEverything hook");
        }
        patchlib_free(load_content_method);
    }

    patchlib_free(game_class);
    patchlib_free(main_class);

    TEKLOG_DEBUG("terraria_main_init completed");
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