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

#include "internal/terraria/content/item_manager.h"
#include "internal/terraria/content/projectile_manager.h"
#include "internal/terraria/content/buff_manager.h"
#include "internal/terraria/content/npc_manager.h"
#include "internal/terraria/content/prefix_manager.h"
#include "patchlib/property.h"

static patch_handle_t get_graphics_device = PATCH_NULL;
static patch_handle_t cur_release = PATCH_NULL;
static patch_handle_t instance = PATCH_NULL;
static patch_handle_t local_player = PATCH_NULL;

static bool initialize_prefix(patch_handle_t this, void **args,
                               const patch_method_signature_t *sig_info, void *result) {
    const int base_count = terraria_item_id_get_count();
    const size_t custom_count = tefstd_vector_size(&g_terraria_item_registry);
    const size_t custom_proj = tefstd_vector_size(&g_terraria_projectile_registry);
    const size_t custom_buff = tefstd_vector_size(&g_terraria_buff_registry);
    const size_t custom_npc = tefstd_vector_size(&g_terraria_npc_registry);
    const size_t custom_prefix = tefstd_vector_size(&g_terraria_prefix_registry);

    TEKLOG_INFO("Main.Initialize: %d base + %zu custom items, %d base + %zu custom projectiles, %d base + %zu custom buffs, %d base + %zu custom npcs",
                base_count, custom_count, terraria_projectile_id_get_count(), custom_proj,
                terraria_buff_id_get_count(), custom_buff,
                terraria_npc_id_get_count(), custom_npc);

    if (custom_count > 0) {
        terraria_item_manager_assign_ids(base_count);
        /* 前缀阶段只扩容已存在的数组；Lang 缓存等还没初始化，
         * 创建了也会被游戏初始化覆盖，留到后面的兜底 pass。 */
        terraria_item_manager_resize(false);
        TEKLOG_INFO("Assigned item IDs and resized arrays");
    }

    if (custom_proj > 0) {
        terraria_projectile_manager_assign_ids(terraria_projectile_id_get_count());
        terraria_projectile_manager_resize();
        TEKLOG_INFO("Assigned projectile IDs and resized arrays");
    }

    if (custom_buff > 0) {
        terraria_buff_manager_assign_ids(terraria_buff_id_get_count());
        TEKLOG_INFO("Assigned buff IDs (不占用原版 BuffID)");
    }

    if (custom_npc > 0) {
        terraria_npc_manager_assign_ids(terraria_npc_id_get_count());
        TEKLOG_INFO("Assigned NPC IDs");
    }

    if (custom_prefix > 0) {
        if (terraria_prefix_manager_assign_ids(terraria_prefix_id_get_count())) {
            TEKLOG_INFO("Assigned %zu custom prefix IDs (base=%d)",
                        custom_prefix, terraria_prefix_id_get_count());
        } else {
            TEKLOG_ERROR("Failed to assign custom prefix IDs (slots full?)");
        }
    }

    int prefix_initialized = 0;
    for (size_t i = 0; i < custom_prefix; ++i) {
        terraria_prefix_handle_t** prefix_handle = tefstd_vector_at(&g_terraria_prefix_registry, i);
        if (*prefix_handle && (*prefix_handle)->prefix_ops.early_init) {
            (*prefix_handle)->prefix_ops.early_init(*prefix_handle);
            prefix_initialized++;
        }
    }
    TEKLOG_INFO("Initialized %d custom prefixes", prefix_initialized);

    int initialized = 0;
    for (size_t i = 0; i < custom_count; ++i) {
        terraria_item_handle_t** item_handle = tefstd_vector_at(&g_terraria_item_registry, i);
        if (*item_handle && (*item_handle)->item_ops.early_init) {
            (*item_handle)->item_ops.early_init(*item_handle);
            initialized++;
        }
    }
    TEKLOG_INFO("Initialized %d custom items", initialized);

    int proj_initialized = 0;
    for (size_t i = 0; i < custom_proj; ++i) {
        terraria_projectile_handle_t** proj_handle =
                tefstd_vector_at(&g_terraria_projectile_registry, i);
        if (*proj_handle && (*proj_handle)->projectile_ops.early_init) {
            (*proj_handle)->projectile_ops.early_init(*proj_handle);
            proj_initialized++;
        }
    }
    TEKLOG_INFO("Initialized %d custom projectiles", proj_initialized);

    const size_t buff_count = tefstd_vector_size(&g_terraria_buff_registry);
    int buff_initialized = 0;
    for (size_t i = 0; i < buff_count; ++i) {
        terraria_buff_handle_t** buff_handle =
                tefstd_vector_at(&g_terraria_buff_registry, i);
        if (*buff_handle && (*buff_handle)->buff_ops.early_init) {
            (*buff_handle)->buff_ops.early_init(*buff_handle);
            buff_initialized++;
        }
    }
    TEKLOG_INFO("Initialized %d custom buffs", buff_initialized);

    int npc_initialized = 0;
    for (size_t i = 0; i < custom_npc; ++i) {
        terraria_npc_handle_t** npc_handle =
                tefstd_vector_at(&g_terraria_npc_registry, i);
        if (*npc_handle && (*npc_handle)->npc_ops.early_init) {
            (*npc_handle)->npc_ops.early_init(*npc_handle);
            npc_initialized++;
        }
    }
    TEKLOG_INFO("Initialized %d custom npcs", npc_initialized);

    return false;
}

// Main.Initialize 结束时做最后一次扩容兜底。
// Initialize_AlmostEverything 之后仍可能有内容（贴图/配方/内容采样）重建
// 某些按 type 索引的静态表；这里再确认一次，避免自定义物品在
// WorldItem.UpdateItem 中越界而被 Main.ignoreErrors 静默移除。
static void initialize_postfix(patch_handle_t this, void **args, void *result,
                               const patch_method_signature_t *sig_info) {
    const size_t custom_count = tefstd_vector_size(&g_terraria_item_registry);
    const size_t custom_proj = tefstd_vector_size(&g_terraria_projectile_registry);
    const size_t custom_buff = tefstd_vector_size(&g_terraria_buff_registry);
    const size_t custom_npc = tefstd_vector_size(&g_terraria_npc_registry);
    const size_t custom_prefix = tefstd_vector_size(&g_terraria_prefix_registry);

    if (custom_count == 0 && custom_proj == 0 && custom_buff == 0 && custom_npc == 0)
        return;
    TEKLOG_INFO("Main.Initialize completed, final resizing pass...");
    if (custom_count > 0)
        terraria_item_manager_resize(true);
    if (custom_proj > 0)
        terraria_projectile_manager_resize();
    if (custom_npc > 0)
        terraria_npc_manager_resize();
    if (custom_prefix > 0)
        terraria_prefix_manager_resize();
}


static void initialize_almost_everything_postfix(patch_handle_t this, void **args, void *result,
                                 const patch_method_signature_t *sig_info) {
    TEKLOG_INFO("Main.Initialize_AlmostEverything completed, resizing arrays...");

    const size_t custom_count = tefstd_vector_size(&g_terraria_item_registry);
    const size_t custom_proj = tefstd_vector_size(&g_terraria_projectile_registry);
    const size_t custom_buff = tefstd_vector_size(&g_terraria_buff_registry);
    const size_t custom_npc = tefstd_vector_size(&g_terraria_npc_registry);
    const size_t custom_prefix = tefstd_vector_size(&g_terraria_prefix_registry);

    if (custom_count > 0) {
        terraria_item_manager_resize(true);
        terraria_item_manager_init_texture2d();
        TEKLOG_INFO("Arrays resized and textures initialized");

        // 填充自定义物品的名称/介绍缓存。此时语言已加载（Program.RunGame
        // 早于 Main.Initialize），early_init 注册的语言回调也已就绪；
        // 若文本尚未载入，Language.GetText 会创建占位对象，后续语言加载时
        // 会原地更新同一个对象，因此这里填充是安全且可自愈的。
        terraria_item_manager_init_localized_text();
        TEKLOG_INFO("Custom item localized text initialized");

    }

    if (custom_proj > 0) {
        terraria_projectile_manager_resize();
        terraria_projectile_manager_init_texture2d();
        TEKLOG_INFO("Custom projectile arrays resized and textures initialized");
    }

    if (custom_buff > 0) {
        terraria_buff_manager_init_texture2d();
        TEKLOG_INFO("Custom buff textures initialized");
    }

    if (custom_npc > 0) {
        terraria_npc_manager_resize();
        terraria_npc_manager_init_texture2d();
        TEKLOG_INFO("Custom NPC arrays resized and textures initialized");
    }


    if (custom_prefix > 0) {
        terraria_prefix_manager_resize();
        terraria_prefix_manager_init_localized_text();
        TEKLOG_INFO("Applied %zu custom prefix names to Lang.prefix",
                    custom_prefix);
    }

    int prefix_initialized = 0;
    for (size_t i = 0; i < custom_prefix; ++i) {
        terraria_prefix_handle_t** prefix_handle = tefstd_vector_at(&g_terraria_prefix_registry, i);
        if (*prefix_handle && (*prefix_handle)->prefix_ops.init_static) {
            (*prefix_handle)->prefix_ops.init_static(*prefix_handle);
            prefix_initialized++;
        }
    }
    TEKLOG_INFO("Initialized %d custom prefixes", prefix_initialized);

    // 初始化静态数据
    int initialized = 0;
    for (size_t i = 0; i < custom_count; ++i) {
        terraria_item_handle_t** item_handle = tefstd_vector_at(&g_terraria_item_registry, i);
        if (*item_handle && (*item_handle)->item_ops.init_static) {
            (*item_handle)->item_ops.init_static(*item_handle);
            initialized++;
        }
    }
    TEKLOG_INFO("Initialized %d custom items", initialized);

    int proj_initialized = 0;
    for (size_t i = 0; i < custom_proj; ++i) {
        terraria_projectile_handle_t** proj_handle =
                tefstd_vector_at(&g_terraria_projectile_registry, i);
        if (*proj_handle && (*proj_handle)->projectile_ops.init_static) {
            (*proj_handle)->projectile_ops.init_static(*proj_handle);
            proj_initialized++;
        }
    }
    TEKLOG_INFO("Initialized %d custom projectiles", proj_initialized);

    int buff_initialized = 0;
    for (size_t i = 0; i < custom_buff; ++i) {
        terraria_buff_handle_t** buff_handle =
                tefstd_vector_at(&g_terraria_buff_registry, i);
        if (*buff_handle && (*buff_handle)->buff_ops.init_static) {
            (*buff_handle)->buff_ops.init_static(*buff_handle);
            buff_initialized++;
        }
    }
    TEKLOG_INFO("Initialized %d custom buffs", buff_initialized);

    int npc_initialized = 0;
    for (size_t i = 0; i < custom_npc; ++i) {
        terraria_npc_handle_t** npc_handle =
                tefstd_vector_at(&g_terraria_npc_registry, i);
        if (*npc_handle && (*npc_handle)->npc_ops.init_static) {
            (*npc_handle)->npc_ops.init_static(*npc_handle);
            npc_initialized++;
        }
    }
    TEKLOG_INFO("Initialized %d custom npcs", npc_initialized);

    /* 归类"可重铸类别"：必须放在 init_static 之后 ——
     * 加载器是在 init_static 里通过 terraria_item_manager_set_prefix_class()
     * 用真实的 runtime_id 声明类别的。 */
    terraria_item_manager_apply_prefix_classes();
}

static bool g_is_server = false;

void terraria_main_init(const bool is_server) {
    g_is_server = is_server;
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

    patch_handle_t local_player_property = patchlib_type_get_property(main_class, "LocalPlayer");
    local_player = patchlib_property_get_get_method(local_player_property);
    patchlib_free(local_player_property);

    patch_handle_t initialize_method = patchlib_type_get_method_by_param_count(main_class, "Initialize", 0);
    if (!patchlib_is_valid(initialize_method)) {
        TEKLOG_ERROR("Failed to get Initialize method");
    } else {
        TEKLOG_DEBUG("Got Initialize method: %p, installing hook", initialize_method);
        patch_hook_id_t hook_id = patchlib_install_prepost_hook(initialize_method, initialize_prefix, initialize_postfix);
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

patch_handle_t terraria_main_get_local_player() {
#if defined(__ANDROID__)
    return ((void*(*)())patchlib_method_get_pointer(local_player))();
#else
    patch_handle_t player = PATCH_NULL;
    patchlib_method_invoke_args(local_player, NULL, &player, NULL);
    return player;
#endif
}

bool terraria_main_is_server(void) {
    return g_is_server;
}
