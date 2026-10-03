/*******************************************************************************
 * tefkernel - item_save
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
 * Created: 2026/10/2
 *
 * 自定义物品存档（额外文件，不改游戏存档格式）
 *
 * 游戏读档时会把 type >= ItemID.Count 的物品当作“未知物品”直接丢弃
 * （Player.Deserialize 里的 netDefaults(0)），所以背包里的 Mod 物品退出重进
 * 就没了。这里把背包中的自定义物品按“稳定字符串键”(loader/mod/name)另存到
 * 一个额外文件，读档后再按槽位塞回去；数字 ID 变化也不影响。
 *******************************************************************************/

#include "internal/terraria/item_save.h"

#include <stdio.h>
#include <string.h>

#include "internal/kernel_state.h"
#include "internal/log.h"
#include "internal/terraria/item_manager.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"
#include "patchlib/struct/string.h"

// ---- 自定义物品存档所需句柄 ----
static terraria_item_handles_t g_ops = {0};               // Item 字段/方法（来自 item_manager）
static patch_handle_t f_player_name = PATCH_NULL;     // Player.name (string)
static patch_handle_t f_player_inventory = PATCH_NULL; // Player.inventory (Item[])
static patch_handle_t f_player_misc_equips = PATCH_NULL; // Player.miscEquips (Item[])
static patch_handle_t f_player_misc_dyes = PATCH_NULL;   // Player.miscDyes (Item[])
static patch_handle_t m_pfd_get_player = PATCH_NULL;  // PlayerFileData.get_Player()

static void sanitize_filename(char *s) {
  for (; *s; ++s) {
    const unsigned char c = (unsigned char)*s;
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok)
      *s = '_';
  }
}

static void player_save_path(const char *player_name, char *buf, size_t n) {
  char safe[160];
  snprintf(safe, sizeof(safe), "%s",
           player_name && player_name[0] ? player_name : "player");
  sanitize_filename(safe);
  snprintf(buf, n, "%s/tefkernel_item_save_%s.txt",
           tefkernel_working_dir ? tefkernel_working_dir : ".", safe);
}

// 把某个容器里所有“自定义物品”写进文件，返回写入条数
static int save_one_container(FILE *f, const char *tag, patch_handle_t field,
                              patch_handle_t player) {
  if (!patchlib_is_valid(field))
    return 0;
  patch_handle_t arr = PATCH_NULL;
  patchlib_field_get_value(field, player, &arr);
  if (!patchlib_is_valid(arr))
    return 0;

  const int base = terraria_item_id_get_count();
  const size_t custom = tefstd_vector_size(&g_terraria_item_registry);
  const size_t len = patchlib_array_length(arr);
  int saved = 0;

  for (size_t i = 0; i < len; ++i) {
    patch_handle_t item = PATCH_NULL;
    if (!patchlib_array_at(arr, i, &item) || !patchlib_is_valid(item))
      continue;

    int type = 0;
    patchlib_field_get_value(g_ops.f_type, item, &type);
    const int nid = type - base;
    if (nid < 0 || nid >= (int)custom)
      continue;

    terraria_item_handle_t **pp =
        tefstd_vector_at(&g_terraria_item_registry, nid);
    if (!pp || !*pp)
      continue;

    int stack = 0;
    patchlib_field_get_value(g_ops.f_stack, item, &stack);
    if (stack <= 0)
      continue;

    unsigned char prefix = 0;
    if (patchlib_is_valid(g_ops.f_item_prefix))
      patchlib_field_get_value(g_ops.f_item_prefix, item, &prefix);

    fprintf(f, "%s\t%zu\t%s\t%s\t%s\t%d\t%d\n", tag, i,
            (*pp)->parent_modloader_id ? (*pp)->parent_modloader_id : "?",
            (*pp)->parent_id ? (*pp)->parent_id : "?",
            (*pp)->internal_name ? (*pp)->internal_name : "?", stack,
            (int)prefix);
    saved++;
  }
  return saved;
}

static void save_player_items(patch_handle_t player) {
  if (!patchlib_is_valid(player) || !patchlib_is_valid(f_player_name))
    return;

  patch_handle_t name_obj = PATCH_NULL;
  patchlib_field_get_value(f_player_name, player, &name_obj);
  char *name =
      patchlib_is_valid(name_obj) ? patchlib_string_cstr(name_obj) : NULL;
  if (!name)
    return;

  char path[1200];
  player_save_path(name, path, sizeof(path));
  // 注：name 由 patchlib_string_cstr 分配，跨平台释放语义不一致，这里不释放

  FILE *f = fopen(path, "w");
  if (!f) {
    TEKLOG_WARN("item_save: cannot write %s", path);
    return;
  }

  // 游戏读档时只会丢弃这三个容器里的自定义物品
  int saved = 0;
  saved += save_one_container(f, "inv", f_player_inventory, player);
  saved += save_one_container(f, "misc", f_player_misc_equips, player);
  saved += save_one_container(f, "miscdye", f_player_misc_dyes, player);

  fclose(f);
  TEKLOG_DEBUG("item_save: saved %d custom item(s) to %s", saved, path);
}

static patch_handle_t container_field_by_tag(const char *tag) {
  if (strcmp(tag, "inv") == 0)
    return f_player_inventory;
  if (strcmp(tag, "misc") == 0)
    return f_player_misc_equips;
  if (strcmp(tag, "miscdye") == 0)
    return f_player_misc_dyes;
  return PATCH_NULL;
}

static void load_player_items(patch_handle_t player) {
  if (!patchlib_is_valid(player) || !patchlib_is_valid(f_player_name))
    return;

  patch_handle_t name_obj = PATCH_NULL;
  patchlib_field_get_value(f_player_name, player, &name_obj);
  char *name =
      patchlib_is_valid(name_obj) ? patchlib_string_cstr(name_obj) : NULL;
  if (!name)
    return;

  char path[1200];
  player_save_path(name, path, sizeof(path));
  // 注：name 由 patchlib_string_cstr 分配，跨平台释放语义不一致，这里不释放

  FILE *f = fopen(path, "r");
  if (!f)
    return;

  char line[700];
  int restored = 0;
  while (fgets(line, sizeof(line), f)) {
    char tag[32] = {0};
    size_t slot = 0;
    char loader[160] = {0}, mod[160] = {0}, item_name[160] = {0};
    int stack = 0, prefix = 0;
    if (sscanf(line, "%31[^\t]\t%zu\t%159[^\t]\t%159[^\t]\t%159[^\t]\t%d\t%d",
               tag, &slot, loader, mod, item_name, &stack, &prefix) != 7)
      continue;

    patch_handle_t field = container_field_by_tag(tag);
    if (!patchlib_is_valid(field))
      continue;

    terraria_item_handle_t *h =
        terraria_item_manager_find_item(loader, mod, item_name);
    if (!h || h->runtime_id < 0) {
      // Mod 已被卸载：用内核预留的“未知物品”占位，保留槽位而不是直接丢掉
      TEKLOG_WARN("item_save: mod item %s.%s.%s not found, using placeholder",
                  loader, mod, item_name);
      h = terraria_item_manager_unknown_item();
    }
    if (!h || h->runtime_id < 0)
      continue;

    patch_handle_t arr = PATCH_NULL;
    patchlib_field_get_value(field, player, &arr);
    if (!patchlib_is_valid(arr))
      continue;

    patch_handle_t item = PATCH_NULL;
    if (!patchlib_array_at(arr, slot, &item) || !patchlib_is_valid(item))
      continue;

    // netDefaults(runtime_id) 会触发 set_defaults_postfix，套用自定义默认值
    int id = h->runtime_id;
    void *args1[1] = {&id};
    patchlib_method_invoke_args(g_ops.m_net_defaults, item, NULL, args1);

    if (stack > 0)
      patchlib_field_set_value(g_ops.f_stack, item, &stack);
    if (prefix > 0 && patchlib_is_valid(g_ops.m_prefix)) {
      void *pa[1] = {&prefix};
      patchlib_method_invoke_args(g_ops.m_prefix, item, NULL, pa);
    }
    restored++;
  }

  fclose(f);
  TEKLOG_DEBUG("item_save: restored %d custom item(s) from %s", restored, path);
}

static void player_save_postfix(patch_handle_t this, void **args, void *result,
                                const patch_method_signature_t *sig) {
  (void)this;
  (void)result;
  (void)sig;
  if (!args || !args[0])
    return;
  patch_handle_t pfd = *(patch_handle_t *)args[0];
  if (!patchlib_is_valid(pfd) || !patchlib_is_valid(m_pfd_get_player))
    return;
  patch_handle_t player = PATCH_NULL;
  patchlib_method_invoke_args(m_pfd_get_player, pfd, &player, NULL);
  save_player_items(player);
}

static void player_load_postfix(patch_handle_t this, void **args, void *result,
                                const patch_method_signature_t *sig) {
  (void)this;
  (void)args;
  (void)sig;
  if (!result)
    return;
  patch_handle_t pfd = *(patch_handle_t *)result;
  if (!patchlib_is_valid(pfd) || !patchlib_is_valid(m_pfd_get_player))
    return;
  patch_handle_t player = PATCH_NULL;
  patchlib_method_invoke_args(m_pfd_get_player, pfd, &player, NULL);
  load_player_items(player);
}

void terraria_item_save_init() {
  terraria_item_manager_get_item_ops(&g_ops);

  patch_handle_t player_class = patchlib_type_get_type("Terraria", "Player");
  if (!patchlib_is_valid(player_class)) {
    TEKLOG_WARN("item_save: Terraria.Player not found; item save disabled");
    return;
  }

  f_player_name = patchlib_type_get_field(player_class, "name");
  f_player_inventory = patchlib_type_get_field(player_class, "inventory");
  f_player_misc_equips = patchlib_type_get_field(player_class, "miscEquips");
  f_player_misc_dyes = patchlib_type_get_field(player_class, "miscDyes");

  patch_handle_t pfd_class = patchlib_type_get_type("Terraria.IO", "PlayerFileData");
  if (patchlib_is_valid(pfd_class)) {
    m_pfd_get_player =
        patchlib_type_get_method_by_param_count(pfd_class, "get_Player", 0);
    patchlib_free(pfd_class);
  }

  patch_handle_t save_player =
      patchlib_type_get_method_by_param_count(player_class, "SavePlayer", 3);
  if (patchlib_is_valid(save_player)) {
    patchlib_install_prepost_hook(save_player, NULL, player_save_postfix);
    TEKLOG_INFO("item_save: hooked Player.SavePlayer");
    patchlib_free(save_player);
  }

  patch_handle_t load_player =
      patchlib_type_get_method_by_param_count(player_class, "LoadPlayer", 2);
  if (patchlib_is_valid(load_player)) {
    patchlib_install_prepost_hook(load_player, NULL, player_load_postfix);
    TEKLOG_INFO("item_save: hooked Player.LoadPlayer");
    patchlib_free(load_player);
  }

  patchlib_free(player_class);
}
