/*******************************************************************************
 * tefkernel - tef_save_game_handles
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
 * Created: 2026/10/28
 *******************************************************************************/

#include "internal/terraria/save/tef_save_game_handles.h"

#include <string.h>

#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"
#include "patchlib/struct/string.h"

static tef_save_game_handles_t g_h;
static bool g_inited = false;

tef_save_game_handles_t *tef_save_game_handles(void) { return &g_h; }

char *tef_save_invoke_get_str(patch_handle_t method, patch_handle_t instance) {
  if (!patchlib_is_valid(method) || !patchlib_is_valid(instance)) return NULL;
  patch_handle_t s = PATCH_NULL;
  patchlib_method_invoke_args(method, instance, &s, NULL);
  if (!patchlib_is_valid(s)) return NULL;
  char *c = patchlib_string_cstr(s);
  patchlib_free(s);
  return c;
}

patch_handle_t tef_save_container_array(patch_handle_t player, const uint8_t cont, bool *out_is_chest) {
  struct { patch_handle_t *field; bool is_chest; } m[9];
  m[TEF_SAVE_CONT_INV]     = (typeof(m[0])){.field = &g_h.f_player_inventory, .is_chest = false};
  m[TEF_SAVE_CONT_MISC]    = (typeof(m[0])){.field = &g_h.f_player_misc_equips, .is_chest = false};
  m[TEF_SAVE_CONT_MISCDYE] = (typeof(m[0])){.field = &g_h.f_player_misc_dyes, .is_chest = false};
  m[TEF_SAVE_CONT_ARMOR]   = (typeof(m[0])){.field = &g_h.f_player_armor, .is_chest = false};
  m[TEF_SAVE_CONT_DYE]     = (typeof(m[0])){.field = &g_h.f_player_dye, .is_chest = false};
  m[TEF_SAVE_CONT_BANK]    = (typeof(m[0])){.field = &g_h.f_player_bank, .is_chest = true};
  m[TEF_SAVE_CONT_BANK2]   = (typeof(m[0])){.field = &g_h.f_player_bank2, .is_chest = true};
  m[TEF_SAVE_CONT_BANK3]   = (typeof(m[0])){.field = &g_h.f_player_bank3, .is_chest = true};
  m[TEF_SAVE_CONT_BANK4]   = (typeof(m[0])){.field = &g_h.f_player_bank4, .is_chest = true};

  if (cont > 8) return PATCH_NULL;
  if (!patchlib_is_valid(*m[cont].field)) return PATCH_NULL;
  if (out_is_chest) *out_is_chest = m[cont].is_chest;

  patch_handle_t v = PATCH_NULL;
  patchlib_field_get_value(*m[cont].field, player, &v);
  if (m[cont].is_chest && patchlib_is_valid(v)) {
    patch_handle_t items = PATCH_NULL;
    patchlib_field_get_value(g_h.f_chest_item, v, &items);
    patchlib_free(v);
    return items;
  }
  return v;
}

patch_handle_t tef_save_container_get_item(patch_handle_t arr, const size_t i, const bool is_chest) {
#if defined(__ANDROID__)
  if (is_chest && patchlib_is_valid(g_h.m_chestitem_expand)) {
    unsigned char buf[256] = {0};
    if (!patchlib_array_at(arr, i, buf)) return PATCH_NULL;
    patch_handle_t item = PATCH_NULL;
    patchlib_method_invoke_args(g_h.m_chestitem_expand, buf, &item, NULL);
    return item;
  }
#else
  (void)is_chest;
#endif
  patch_handle_t item = PATCH_NULL;
  if (!patchlib_array_at(arr, i, &item)) return PATCH_NULL;
  return item;
}

void tef_save_container_set_custom(patch_handle_t arr, size_t slot, bool is_chest,
                              int id, int stack, int prefix) {
#if defined(__ANDROID__)
  if (is_chest) {
    if (!patchlib_is_valid(g_h.m_item_ctor) || !patchlib_is_valid(g_h.m_chestitem_set_to_item)) return;
    patch_handle_t item = PATCH_NULL, variant = PATCH_NULL;
    void *ca[2] = {&id, &variant};
    patchlib_constructor_invoke(g_h.m_item_ctor, &item, ca);
    if (!patchlib_is_valid(item)) return;
    if (stack > 0) patchlib_field_set_value(g_h.ops.f_stack, item, &stack);
    if (prefix > 0 && patchlib_is_valid(g_h.ops.m_prefix)) {
      void *pa[1] = {&prefix};
      patchlib_method_invoke_args(g_h.ops.m_prefix, item, NULL, pa);
    }
    unsigned char buf[256] = {0};
    if (!patchlib_array_at(arr, slot, buf)) { patchlib_free(item); return; }
    void *sa[1] = {&item};
    patchlib_method_invoke_args(g_h.m_chestitem_set_to_item, buf, NULL, sa);
    patchlib_array_set(arr, slot, buf);
    patchlib_free(item);
    return;
  }
#else
  (void)is_chest;
#endif
  patch_handle_t item = PATCH_NULL;
  if (!patchlib_array_at(arr, slot, &item) || !patchlib_is_valid(item)) return;
  int iid = id;
  void *a1[1] = {&iid};
  patchlib_method_invoke_args(g_h.ops.m_net_defaults, item, NULL, a1);
  if (stack > 0) patchlib_field_set_value(g_h.ops.f_stack, item, &stack);
  if (prefix > 0 && patchlib_is_valid(g_h.ops.m_prefix)) {
    void *pa[1] = {&prefix};
    patchlib_method_invoke_args(g_h.ops.m_prefix, item, NULL, pa);
  }
}

int tef_save_sanitize_chest(patch_handle_t chest, const int max_valid) {
  if (!patchlib_is_valid(chest) || !patchlib_is_valid(g_h.f_chest_item) ||
      !patchlib_is_valid(g_h.ops.f_type)) return 0;

  const terraria_item_handle_t *ph = terraria_item_manager_unknown_item();
  const int ph_id = ph ? ph->runtime_id : -1;
  if (ph_id < 0) return 0;

  int max_items = 0;
  if (patchlib_is_valid(g_h.f_chest_max_items))
    patchlib_field_get_value(g_h.f_chest_max_items, chest, &max_items);

  patch_handle_t items = PATCH_NULL;
  patchlib_field_get_value(g_h.f_chest_item, chest, &items);
  if (!patchlib_is_valid(items)) return 0;

  size_t lim = patchlib_array_length(items);
  if (max_items > 0 && (size_t)max_items < lim) lim = (size_t)max_items;

  int changed = 0;
  for (size_t i = 0; i < lim; ++i) {
    patch_handle_t item = tef_save_container_get_item(items, i, true);
    if (!patchlib_is_valid(item)) continue;
    int type = 0;
    patchlib_field_get_value(g_h.ops.f_type, item, &type);
    if (type >= max_valid) {
      tef_save_container_set_custom(items, i, true, ph_id, 1, 0);
      changed++;
    }
    patchlib_free(item);
  }
  patchlib_free(items);
  return changed;
}

void tef_save_game_handles_init(void) {
  if (g_inited) return;
  g_inited = true;

  memset(&g_h, 0, sizeof g_h);
  terraria_item_manager_get_item_ops(&g_h.ops);

  patch_handle_t player_class = patchlib_type_get_type("Terraria", "Player");
  if (patchlib_is_valid(player_class)) {
    g_h.f_player_inventory   = patchlib_type_get_field(player_class, "inventory");
    g_h.f_player_misc_equips = patchlib_type_get_field(player_class, "miscEquips");
    g_h.f_player_misc_dyes   = patchlib_type_get_field(player_class, "miscDyes");
    g_h.f_player_armor       = patchlib_type_get_field(player_class, "armor");
    g_h.f_player_dye         = patchlib_type_get_field(player_class, "dye");
    g_h.f_player_bank        = patchlib_type_get_field(player_class, "bank");
    g_h.f_player_bank2       = patchlib_type_get_field(player_class, "bank2");
    g_h.f_player_bank3       = patchlib_type_get_field(player_class, "bank3");
    g_h.f_player_bank4       = patchlib_type_get_field(player_class, "bank4");
    patchlib_free(player_class);
  }

  patch_handle_t chest_class = patchlib_type_get_type("Terraria", "Chest");
  if (patchlib_is_valid(chest_class)) {
    g_h.f_chest_item      = patchlib_type_get_field(chest_class, "item");
    g_h.f_chest_max_items = patchlib_type_get_field(chest_class, "maxItems");
    patchlib_free(chest_class);
  }

  patch_handle_t item_class = patchlib_type_get_type("Terraria", "Item");
  if (patchlib_is_valid(item_class)) {
    g_h.m_item_ctor = patchlib_type_get_method_by_param_count(item_class, ".ctor", 2);
    patchlib_free(item_class);
  }
  patch_handle_t chestitem_class = patchlib_type_get_type("Terraria", "ChestItem");
  if (patchlib_is_valid(chestitem_class)) {
    g_h.m_chestitem_expand      = patchlib_type_get_method_by_param_count(chestitem_class, "ExpandItem", 0);
    g_h.m_chestitem_set_to_item = patchlib_type_get_method_by_param_count(chestitem_class, "SetToItem", 1);
    patchlib_free(chestitem_class);
  }

  patch_handle_t filedata_class = patchlib_type_get_type("Terraria.IO", "FileData");
  if (patchlib_is_valid(filedata_class)) {
    g_h.m_file_get_path = patchlib_type_get_method_by_param_count(filedata_class, "get_Path", 0);
    patchlib_free(filedata_class);
  }
  patch_handle_t pfd_class = patchlib_type_get_type("Terraria.IO", "PlayerFileData");
  if (patchlib_is_valid(pfd_class)) {
    g_h.m_pfd_get_player = patchlib_type_get_method_by_param_count(pfd_class, "get_Player", 0);
    patchlib_free(pfd_class);
  }

  patch_handle_t main_class = patchlib_type_get_type("Terraria", "Main");
  if (patchlib_is_valid(main_class)) {
    g_h.f_main_chest        = patchlib_type_get_field(main_class, "chest");
    g_h.f_main_active_world = patchlib_type_get_field(main_class, "ActiveWorldFileData");
    patchlib_free(main_class);
  }
}