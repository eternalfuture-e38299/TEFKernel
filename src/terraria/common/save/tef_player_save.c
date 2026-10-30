/*******************************************************************************
 * tefkernel - tef_player_save
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

#include "internal/terraria/save/tef_player_save.h"

#include <stdio.h>

#include "internal/log.h"
#include "internal/terraria/content/item_manager.h"
#include "internal/terraria/content/prefix_manager.h"
#include "internal/terraria/save/tef_save_game_handles.h"
#include "internal/terraria/save/tef_save_format.h"
#include "patchlib/field.h"
#include "terraria/content/buff_manager.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"

/* ================================================================
 * 写入
 * ================================================================ */
static void write_items_seg(tef_save_wbuf_t *seg, patch_handle_t player) {
  const size_t arr = tef_save_wb_begin_array(seg, TEF_SAVE_ITEMS_ARRAY);

  static const uint8_t conts[9] = {
    TEF_SAVE_CONT_INV, TEF_SAVE_CONT_MISC, TEF_SAVE_CONT_MISCDYE, TEF_SAVE_CONT_ARMOR, TEF_SAVE_CONT_DYE,
    TEF_SAVE_CONT_BANK, TEF_SAVE_CONT_BANK2, TEF_SAVE_CONT_BANK3, TEF_SAVE_CONT_BANK4
  };

  tef_save_game_handles_t *h = tef_save_game_handles();
  const int base = terraria_item_id_get_count();

  for (int ci = 0; ci < 9; ++ci) {
    bool is_chest = false;
    patch_handle_t arrh = tef_save_container_array(player, conts[ci], &is_chest);
    if (!patchlib_is_valid(arrh)) continue;

    const size_t n = patchlib_array_length(arrh);
    for (size_t i = 0; i < n; ++i) {
      patch_handle_t item = tef_save_container_get_item(arrh, i, is_chest);
      if (!patchlib_is_valid(item)) continue;

      int type = 0, stack = 0;
      patchlib_field_get_value(h->ops.f_type,  item, &type);
      patchlib_field_get_value(h->ops.f_stack, item, &stack);
      if (stack <= 0 || type <= 0) { patchlib_free(item); continue; }   /* 空槽跳过 */

      unsigned char prefix = 0;
      if (patchlib_is_valid(h->ops.f_item_prefix))
        patchlib_field_get_value(h->ops.f_item_prefix, item, &prefix);

      /* 过滤：
       *   - 自定义物品（type >= base）：无论有无前缀都存；
       *   - 原版物品（type < base）：只有带自定义前缀（prefix >= PrefixID.Count）才存；
       *   - 原版物品 + 原版前缀 / 无前缀：跳过（原版存档已覆盖）。 */
      const bool is_custom_item = (type >= base);
      const bool has_custom_prefix = ((int) prefix >= terraria_prefix_id_get_count());
      if (!is_custom_item && !has_custom_prefix) { patchlib_free(item); continue; }
      
      /* 自定义物品（type >= base）：用 registry 的 (loader, mod, name) 作为稳定身份；
       * 原版物品（type < base）：三个字符串写空串。 */
      const char *loader = "";
      const char *mod    = "";
      const char *name   = "";
      if (type >= base) {
        const int nid = type - base;
        const size_t custom = tefstd_vector_size(&g_terraria_item_registry);
        if (nid >= 0 && nid < (int)custom) {
          terraria_item_handle_t **pp = tefstd_vector_at(&g_terraria_item_registry, nid);
          if (pp && *pp) {
            loader = (*pp)->parent_modloader_id ? (*pp)->parent_modloader_id : "";
            mod    = (*pp)->parent_id           ? (*pp)->parent_id           : "";
            name   = (*pp)->internal_name       ? (*pp)->internal_name       : "";
          }
        } else {
          TEKLOG_WARN("tef_save_player_save: custom item id=%d out of registry, store as-is", type);
        }
      }

      /* 自定义前缀身份：prefix >= PrefixID.Count 时填 (loader, mod, name)。 */
      const char *p_loader = "";
      const char *p_mod    = "";
      const char *p_name   = "";
      if ((int) prefix >= terraria_prefix_id_get_count()) {
        const terraria_prefix_handle_t *ph = terraria_prefix_manager_get_prefix(prefix);
        if (ph) {
          p_loader = ph->parent_modloader_id ? ph->parent_modloader_id : "";
          p_mod    = ph->parent_id           ? ph->parent_id           : "";
          p_name   = ph->internal_name       ? ph->internal_name       : "";
        } else {
          TEKLOG_WARN("tef_save_player_save: unknown custom prefix id=%d, store empty",
                      (int) prefix);
        }
      }

      size_t e = tef_save_wb_begin_elem(seg);
      tef_save_wb_tlv_u8 (seg, TEF_SAVE_ITEM_CONTAINER, conts[ci]);
      tef_save_wb_tlv_u32(seg, TEF_SAVE_ITEM_SLOT,      (uint32_t)i);
      tef_save_wb_tlv_u32(seg, TEF_SAVE_ITEM_ORIGINAL_ID, (uint32_t)type);
      tef_save_wb_tlv_str(seg, TEF_SAVE_ITEM_LOADER,    loader);
      tef_save_wb_tlv_str(seg, TEF_SAVE_ITEM_MOD,       mod);
      tef_save_wb_tlv_str(seg, TEF_SAVE_ITEM_NAME,      name);
      tef_save_wb_tlv_u32(seg, TEF_SAVE_ITEM_STACK,     (uint32_t)stack);
      tef_save_wb_tlv_u8 (seg, TEF_SAVE_ITEM_PREFIX,    prefix);
      tef_save_wb_tlv_str(seg, TEF_SAVE_ITEM_PREFIX_LOADER, p_loader);
      tef_save_wb_tlv_str(seg, TEF_SAVE_ITEM_PREFIX_MOD,    p_mod);
      tef_save_wb_tlv_str(seg, TEF_SAVE_ITEM_PREFIX_NAME,   p_name);
      tef_save_wb_end(seg, e);

      patchlib_free(item);
    }
    patchlib_free(arrh);
  }
  tef_save_wb_end_array(seg, arr);
}

struct buff_save_ctx { tef_save_wbuf_t *seg; int count; };

static void save_buff_line(void *user, terraria_buff_handle_t *buff, int time) {
  struct buff_save_ctx *ctx = user;
  if (!ctx || !ctx->seg || !buff || time <= 0) return;
  const size_t e = tef_save_wb_begin_elem(ctx->seg);
  tef_save_wb_tlv_str(ctx->seg, TEF_SAVE_BUFF_LOADER, buff->parent_modloader_id ? buff->parent_modloader_id : "?");
  tef_save_wb_tlv_str(ctx->seg, TEF_SAVE_BUFF_MOD,    buff->parent_id ? buff->parent_id : "?");
  tef_save_wb_tlv_str(ctx->seg, TEF_SAVE_BUFF_NAME,   buff->internal_name ? buff->internal_name : "?");
  tef_save_wb_tlv_u32(ctx->seg, TEF_SAVE_BUFF_TIME,   (uint32_t)time);
  tef_save_wb_end(ctx->seg, e);
  ctx->count++;
}

static void write_buffs_seg(tef_save_wbuf_t *seg, patch_handle_t player) {
  const size_t arr = tef_save_wb_begin_array(seg, TEF_SAVE_BUFFS_ARRAY);
  struct buff_save_ctx ctx = { .seg = seg, .count = 0 };
  terraria_buff_manager_visit_active(player, save_buff_line, &ctx);
  tef_save_wb_end_array(seg, arr);
}

static void save_player_items(patch_handle_t player, patch_handle_t pfd) {
  if (!patchlib_is_valid(player)) return;
  const tef_save_game_handles_t *h = tef_save_game_handles();

  char *plr = tef_save_invoke_get_str(h->m_file_get_path, pfd);
  if (!plr) { TEKLOG_WARN("tef_save_player_save: cannot resolve player file path"); return; }

  char path[1400];
  if (!tef_save_make_path(plr, path, sizeof path)) return;

  tef_save_wbuf_t items = { .ok = true };
  tef_save_wbuf_t buffs = { .ok = true };

  write_items_seg(&items, player);
  write_buffs_seg(&buffs, player);

  tef_save_seg_out_t segs[2] = {
    {.id = TEF_SAVE_SEG_ITEMS, .data = items.data, .len = (uint32_t)items.len },
    {.id = TEF_SAVE_SEG_BUFFS, .data = buffs.data, .len = (uint32_t)buffs.len },
  };
  if (items.ok && buffs.ok && tef_save_file_write(path, TEF_SAVE_KIND_PLAYER, segs, 2))
    TEKLOG_DEBUG("tef_save_player_save: saved to %s", path);

  tef_save_wb_free(&items);
  tef_save_wb_free(&buffs);
}

/* ================================================================
 * 读取
 * ================================================================ */
/* 解析自定义前缀身份：prefix >= base 时按 (loader, mod, name) 查找并覆盖，否则原样返回。 */
static int resolve_prefix(const uint8_t prefix,
                          const char *p_loader, const char *p_mod, const char *p_name) {
  int final_prefix = (int) prefix;
  const int base = terraria_prefix_id_get_count();
  if ((int) prefix >= base) {
    const terraria_prefix_handle_t *ph = NULL;
    if (p_loader && p_loader[0] && p_mod && p_mod[0] && p_name && p_name[0])
      ph = terraria_prefix_manager_find_prefix(p_loader, p_mod, p_name);
    if (ph && ph->runtime_id >= 0) {
      final_prefix = ph->runtime_id;
    } else {
      TEKLOG_WARN("tef_save_player_save: prefix %s.%s.%s not found, drop",
                  p_loader ? p_loader : "?", p_mod ? p_mod : "?", p_name ? p_name : "?");
      final_prefix = 0;
    }
  }
  return final_prefix;
}

/* 解析物品身份：
 *   1) original_id >= base 且 registry 里有对应自定义物品 → 直接用该 runtime_id；
 *   2) 否则按 (loader, mod, name) 查找自定义物品；
 *   3) 否则按 original_id（原版 id）走原版 netDefaults；
 *   4) 都不行 → unknown 占位。 */
static int resolve_item_type(const uint32_t original_id,
                             const char *loader, const char *mod, const char *name) {
  const int base = terraria_item_id_get_count();

  if ((int) original_id >= base) {
    const terraria_item_handle_t *h = terraria_item_manager_get_item((int) original_id);
    if (h && h->runtime_id == (int) original_id)
      return (int) original_id;
  }

  if (loader && loader[0] && mod && mod[0] && name && name[0]) {
    const terraria_item_handle_t *h = terraria_item_manager_find_item(loader, mod, name);
    if (h && h->runtime_id >= 0)
      return h->runtime_id;
  }

  if ((int) original_id > 0 && (int) original_id < base)
    return (int) original_id;

  const terraria_item_handle_t *unk = terraria_item_manager_unknown_item();
  return unk ? unk->runtime_id : -1;
}

static void apply_item(patch_handle_t player,
                       const uint8_t cont, const uint32_t slot,
                       const uint32_t original_id,
                       const char *loader, const char *mod, const char *name,
                       const uint32_t stack, const uint8_t prefix,
                       const char *p_loader, const char *p_mod, const char *p_name) {
  bool is_chest = false;
  patch_handle_t arr = tef_save_container_array(player, cont, &is_chest);
  if (!patchlib_is_valid(arr)) return;
  if (slot >= patchlib_array_length(arr)) { patchlib_free(arr); return; }

  const int type = resolve_item_type(original_id, loader, mod, name);
  if (type <= 0) { patchlib_free(arr); return; }

  const int final_prefix = resolve_prefix(prefix, p_loader, p_mod, p_name);
  tef_save_container_set_custom(arr, slot, is_chest, type, (int) stack, final_prefix);
  patchlib_free(arr);
}

static void load_items_seg(tef_save_rd_t *seg, patch_handle_t player) {
  tef_save_tlv_t t;
  while (tef_save_rd_next(seg, &t)) {
    if (t.tag != TEF_SAVE_ITEMS_ARRAY) continue;
    tef_save_rd_t elems; uint32_t count;
    if (!tef_save_rd_array_begin(&t, &elems, &count)) continue;

    for (uint32_t i = 0; i < count; ++i) {
      tef_save_tlv_t obj;
      if (!tef_save_rd_next(&elems, &obj)) break;
      if (obj.type != TEF_SAVE_T_OBJECT) continue;

      tef_save_rd_t os; tef_save_rd_enter(&obj, &os);
      uint8_t cont = 0, prefix = 0;
      uint32_t slot = 0, stack = 0, original_id = 0;
      const char *loader = NULL, *mod = NULL, *name = NULL;
      const char *p_loader = NULL, *p_mod = NULL, *p_name = NULL;

      tef_save_tlv_t f;
      while (tef_save_rd_next(&os, &f)) {
        switch (f.tag) {
          case TEF_SAVE_ITEM_CONTAINER:       cont        = tef_save_rd_u8(&f);  break;
          case TEF_SAVE_ITEM_SLOT:            slot        = tef_save_rd_u32(&f); break;
          case TEF_SAVE_ITEM_ORIGINAL_ID:     original_id = tef_save_rd_u32(&f); break;
          case TEF_SAVE_ITEM_LOADER:          loader      = tef_save_rd_str(&f); break;
          case TEF_SAVE_ITEM_MOD:             mod         = tef_save_rd_str(&f); break;
          case TEF_SAVE_ITEM_NAME:            name        = tef_save_rd_str(&f); break;
          case TEF_SAVE_ITEM_STACK:           stack       = tef_save_rd_u32(&f); break;
          case TEF_SAVE_ITEM_PREFIX:          prefix      = tef_save_rd_u8(&f);  break;
          case TEF_SAVE_ITEM_PREFIX_LOADER:   p_loader    = tef_save_rd_str(&f); break;
          case TEF_SAVE_ITEM_PREFIX_MOD:      p_mod       = tef_save_rd_str(&f); break;
          case TEF_SAVE_ITEM_PREFIX_NAME:     p_name      = tef_save_rd_str(&f); break;
          default: break;
        }
      }
      if (stack > 0)
        apply_item(player, cont, slot, original_id, loader, mod, name,
                   stack, prefix, p_loader, p_mod, p_name);
    }
  }
}

static void load_buffs_seg(tef_save_rd_t *seg, patch_handle_t player) {
  tef_save_tlv_t t;
  while (tef_save_rd_next(seg, &t)) {
    if (t.tag != TEF_SAVE_BUFFS_ARRAY) continue;
    tef_save_rd_t elems; uint32_t count;
    if (!tef_save_rd_array_begin(&t, &elems, &count)) continue;

    for (uint32_t i = 0; i < count; ++i) {
      tef_save_tlv_t obj;
      if (!tef_save_rd_next(&elems, &obj)) break;
      if (obj.type != TEF_SAVE_T_OBJECT) continue;

      tef_save_rd_t os; tef_save_rd_enter(&obj, &os);
      const char *loader = NULL, *mod = NULL, *name = NULL;
      uint32_t time = 0;
      tef_save_tlv_t f;
      while (tef_save_rd_next(&os, &f)) {
        switch (f.tag) {
          case TEF_SAVE_BUFF_LOADER: loader = tef_save_rd_str(&f); break;
          case TEF_SAVE_BUFF_MOD:    mod    = tef_save_rd_str(&f); break;
          case TEF_SAVE_BUFF_NAME:   name   = tef_save_rd_str(&f); break;
          case TEF_SAVE_BUFF_TIME:   time   = tef_save_rd_u32(&f); break;
          default: break;
        }
      }
      if (!loader || !mod || !name || time == 0) continue;
      terraria_buff_handle_t *bh = terraria_buff_manager_find_buff(loader, mod, name);
      if (bh) terraria_buff_manager_add(bh, player, (int)time);
      else TEKLOG_WARN("tef_save_player_save: buff %s.%s.%s not found", loader, mod, name);
    }
  }
}

static void sanitize_player_banks(patch_handle_t player) {
  if (!patchlib_is_valid(player)) return;
  const tef_save_game_handles_t *h = tef_save_game_handles();

  patch_handle_t banks[4] = { h->f_player_bank, h->f_player_bank2, h->f_player_bank3, h->f_player_bank4 };
  const int max_valid = terraria_item_manager_get_count();
  int changed = 0;
  for (int i = 0; i < 4; ++i) {
    if (!patchlib_is_valid(banks[i])) continue;
    patch_handle_t chest = PATCH_NULL;
    patchlib_field_get_value(banks[i], player, &chest);
    if (!patchlib_is_valid(chest)) continue;
    changed += tef_save_sanitize_chest(chest, max_valid);
    patchlib_free(chest);
  }
  if (changed > 0)
    TEKLOG_INFO("tef_save_player_save: replaced %d stale item(s) in banks", changed);
}

static void load_player_items(patch_handle_t player, patch_handle_t pfd) {
  if (!patchlib_is_valid(player)) return;
  const tef_save_game_handles_t *h = tef_save_game_handles();

  char *plr = tef_save_invoke_get_str(h->m_file_get_path, pfd);
  if (!plr) return;
  char path[1400];
  if (!tef_save_make_path(plr, path, sizeof path)) return;

  tef_save_file_t f;
  if (!tef_save_file_open(path, &f)) return;
  if (f.kind != TEF_SAVE_KIND_PLAYER) { tef_save_file_close(&f); return; }

  for (int i = 0; i < f.seg_count; ++i) {
    tef_save_rd_t seg = tef_save_file_seg(&f, i);
    switch (f.segs[i].id) {
      case TEF_SAVE_SEG_ITEMS: load_items_seg(&seg, player); break;
      case TEF_SAVE_SEG_BUFFS: load_buffs_seg(&seg, player); break;
      default: break;
    }
  }
  tef_save_file_close(&f);
}

/* ================================================================
 * 钩子
 * ================================================================ */
static void player_save_postfix(patch_handle_t this, void **args, void *result,
                                const patch_method_signature_t *sig) {
  (void)this; (void)result; (void)sig;
  if (!args || !args[0]) return;
  const tef_save_game_handles_t *h = tef_save_game_handles();
  patch_handle_t pfd = *(patch_handle_t *)args[0];
  if (!patchlib_is_valid(pfd) || !patchlib_is_valid(h->m_pfd_get_player)) return;
  patch_handle_t player = PATCH_NULL;
  patchlib_method_invoke_args(h->m_pfd_get_player, pfd, &player, NULL);
  save_player_items(player, pfd);
}

static void player_load_postfix(patch_handle_t this, void **args, void *result,
                                const patch_method_signature_t *sig) {
  (void)this; (void)args; (void)sig;
  if (!result) return;
  tef_save_game_handles_t *h = tef_save_game_handles();
  patch_handle_t pfd = *(patch_handle_t *)result;
  if (!patchlib_is_valid(pfd) || !patchlib_is_valid(h->m_pfd_get_player)) return;
  patch_handle_t player = PATCH_NULL;
  patchlib_method_invoke_args(h->m_pfd_get_player, pfd, &player, NULL);
  load_player_items(player, pfd);
  sanitize_player_banks(player);
}

/* ================================================================
 * 初始化
 * ================================================================ */
void tef_player_save_init(void) {
  tef_save_game_handles_init();

  patch_handle_t player_class = patchlib_type_get_type("Terraria", "Player");
  if (!patchlib_is_valid(player_class)) {
    TEKLOG_WARN("tef_save_player_save: Terraria.Player not found");
    return;
  }

  patch_handle_t save_player = patchlib_type_get_method_by_param_count(player_class, "SavePlayer", 3);
  if (patchlib_is_valid(save_player)) {
    patchlib_install_prepost_hook(save_player, NULL, player_save_postfix);
    TEKLOG_INFO("tef_save_player_save: hooked Player.SavePlayer");
    patchlib_free(save_player);
  }
  patch_handle_t load_player = patchlib_type_get_method_by_param_count(player_class, "LoadPlayer", 2);
  if (patchlib_is_valid(load_player)) {
    patchlib_install_prepost_hook(load_player, NULL, player_load_postfix);
    TEKLOG_INFO("tef_save_player_save: hooked Player.LoadPlayer");
    patchlib_free(load_player);
  }

  patchlib_free(player_class);
}