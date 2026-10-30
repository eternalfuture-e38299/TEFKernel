/*******************************************************************************
 * tefkernel - tef_world_save
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

#include "internal/terraria/save/tef_world_save.h"

#include <stdio.h>

#include "internal/log.h"
#include "internal/terraria/content/item_manager.h"
#include "internal/terraria/content/prefix_manager.h"
#include "internal/terraria/save/tef_save_game_handles.h"
#include "internal/terraria/save/tef_save_format.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"

/* ================================================================
 * 写入
 * ================================================================ */
static char *world_tef_save_base(void) {
  const tef_save_game_handles_t *h = tef_save_game_handles();
  if (!patchlib_is_valid(h->f_main_active_world)) return NULL;
  patch_handle_t wfd = PATCH_NULL;
  patchlib_field_get_value(h->f_main_active_world, PATCH_NULL, &wfd);
  if (!patchlib_is_valid(wfd)) return NULL;
  char *p = tef_save_invoke_get_str(h->m_file_get_path, wfd);
  patchlib_free(wfd);
  return p;
}

static void write_chests_seg(tef_save_wbuf_t *seg) {
  const size_t arr = tef_save_wb_begin_array(seg, TEF_SAVE_CHESTS_ARRAY);

  const tef_save_game_handles_t *h = tef_save_game_handles();
  if (!patchlib_is_valid(h->f_main_chest) || !patchlib_is_valid(h->f_chest_item)) {
    tef_save_wb_end_array(seg, arr);
    return;
  }

  patch_handle_t chests = PATCH_NULL;
  patchlib_field_get_value(h->f_main_chest, PATCH_NULL, &chests);
  if (!patchlib_is_valid(chests)) { tef_save_wb_end_array(seg, arr); return; }

  const int base = terraria_item_id_get_count();
  const size_t chest_count = patchlib_array_length(chests);

  for (size_t ci = 0; ci < chest_count; ++ci) {
    patch_handle_t chest = PATCH_NULL;
    if (!patchlib_array_at(chests, ci, &chest) || !patchlib_is_valid(chest)) continue;

    int max_items = 0;
    if (patchlib_is_valid(h->f_chest_max_items))
      patchlib_field_get_value(h->f_chest_max_items, chest, &max_items);

    patch_handle_t items = PATCH_NULL;
    patchlib_field_get_value(h->f_chest_item, chest, &items);
    if (patchlib_is_valid(items)) {
      size_t lim = patchlib_array_length(items);
      if (max_items > 0 && (size_t)max_items < lim) lim = (size_t)max_items;

      for (size_t si = 0; si < lim; ++si) {
        patch_handle_t item = tef_save_container_get_item(items, si, true);
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
        const bool has_custom_prefix = (int) prefix >= terraria_prefix_id_get_count();
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
            TEKLOG_WARN("tef_save_world_save: custom item id=%d out of registry, store as-is", type);
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
            TEKLOG_WARN("tef_save_world_save: unknown custom prefix id=%d, store empty",
                        (int) prefix);
          }
        }

        const size_t e = tef_save_wb_begin_elem(seg);
        tef_save_wb_tlv_u32(seg, TEF_SAVE_CHEST_INDEX,       (uint32_t)ci);
        tef_save_wb_tlv_u32(seg, TEF_SAVE_CHEST_SLOT,        (uint32_t)si);
        tef_save_wb_tlv_u32(seg, TEF_SAVE_CHEST_ORIGINAL_ID, (uint32_t)type);
        tef_save_wb_tlv_str(seg, TEF_SAVE_CHEST_LOADER,      loader);
        tef_save_wb_tlv_str(seg, TEF_SAVE_CHEST_MOD,         mod);
        tef_save_wb_tlv_str(seg, TEF_SAVE_CHEST_NAME,        name);
        tef_save_wb_tlv_u32(seg, TEF_SAVE_CHEST_STACK,       (uint32_t)stack);
        tef_save_wb_tlv_u8 (seg, TEF_SAVE_CHEST_PREFIX,      prefix);
        tef_save_wb_tlv_str(seg, TEF_SAVE_CHEST_PREFIX_LOADER, p_loader);
        tef_save_wb_tlv_str(seg, TEF_SAVE_CHEST_PREFIX_MOD,    p_mod);
        tef_save_wb_tlv_str(seg, TEF_SAVE_CHEST_PREFIX_NAME,   p_name);
        tef_save_wb_end(seg, e);

        patchlib_free(item);
      }
      patchlib_free(items);
    }
    patchlib_free(chest);
  }
  patchlib_free(chests);
  tef_save_wb_end_array(seg, arr);
}


static void save_world_chests(void) {
  char *wld = world_tef_save_base();
  if (!wld || !wld[0]) return;
  char path[1400];
  if (!tef_save_make_path(wld, path, sizeof path)) return;

  tef_save_wbuf_t chests = { .ok = true };
  tef_save_wbuf_t res    = { .ok = true };

  write_chests_seg(&chests);

  const tef_save_seg_out_t segs[2] = {
    {.id = TEF_SAVE_SEG_CHESTS,   .data = chests.data, .len = (uint32_t)chests.len },
    {.id = TEF_SAVE_SEG_RESEARCH, .data = res.data,    .len = (uint32_t)res.len },
  };
  if (chests.ok && res.ok && tef_save_file_write(path, TEF_SAVE_KIND_WORLD, segs, 2))
    TEKLOG_DEBUG("tef_save_world_save: saved to %s", path);

  tef_save_wb_free(&chests);
  tef_save_wb_free(&res);
}

/* ================================================================
 * 读取
 * ================================================================ */
/* 解析自定义前缀身份：prefix >= base 时按 (loader, mod, name) 查找并覆盖，否则原样返回。 */
static int resolve_prefix(const uint8_t prefix,
                          const char *p_loader, const char *p_mod, const char *p_name) {
  int final_prefix = prefix;
  const int base = terraria_prefix_id_get_count();
  if ((int) prefix >= base) {
    const terraria_prefix_handle_t *ph = NULL;
    if (p_loader && p_loader[0] && p_mod && p_mod[0] && p_name && p_name[0])
      ph = terraria_prefix_manager_find_prefix(p_loader, p_mod, p_name);
    if (ph && ph->runtime_id >= 0) {
      final_prefix = ph->runtime_id;
    } else {
      TEKLOG_WARN("tef_save_world_save: prefix %s.%s.%s not found, drop",
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

static void apply_world_item(patch_handle_t chests, const uint32_t ci, const uint32_t slot,
                             const uint32_t original_id,
                             const char *loader, const char *mod, const char *name,
                             const uint32_t stack, const uint8_t prefix,
                             const char *p_loader, const char *p_mod, const char *p_name) {
  const tef_save_game_handles_t *h = tef_save_game_handles();
  if (ci >= patchlib_array_length(chests)) return;
  patch_handle_t chest = PATCH_NULL;
  if (!patchlib_array_at(chests, ci, &chest) || !patchlib_is_valid(chest)) return;

  patch_handle_t items = PATCH_NULL;
  patchlib_field_get_value(h->f_chest_item, chest, &items);
  if (!patchlib_is_valid(items) || slot >= patchlib_array_length(items)) {
    if (patchlib_is_valid(items)) patchlib_free(items);
    patchlib_free(chest);
    return;
  }

  const int type = resolve_item_type(original_id, loader, mod, name);
  if (type <= 0) {
    patchlib_free(items);
    patchlib_free(chest);
    return;
  }

  const int final_prefix = resolve_prefix(prefix, p_loader, p_mod, p_name);
  tef_save_container_set_custom(items, slot, true, type, (int) stack, final_prefix);

  patchlib_free(items);
  patchlib_free(chest);
}

static void load_chests_seg(tef_save_rd_t *seg) {
  const tef_save_game_handles_t *h = tef_save_game_handles();
  if (!patchlib_is_valid(h->f_main_chest) || !patchlib_is_valid(h->f_chest_item)) return;

  tef_save_tlv_t t;
  while (tef_save_rd_next(seg, &t)) {
    if (t.tag != TEF_SAVE_CHESTS_ARRAY) continue;
    tef_save_rd_t elems; uint32_t count;
    if (!tef_save_rd_array_begin(&t, &elems, &count)) continue;

    patch_handle_t chests = PATCH_NULL;
    patchlib_field_get_value(h->f_main_chest, PATCH_NULL, &chests);
    if (!patchlib_is_valid(chests)) return;

    for (uint32_t i = 0; i < count; ++i) {
      tef_save_tlv_t obj;
      if (!tef_save_rd_next(&elems, &obj)) break;
      if (obj.type != TEF_SAVE_T_OBJECT) continue;

      tef_save_rd_t os; tef_save_rd_enter(&obj, &os);
      uint32_t ci = 0, slot = 0, stack = 0, original_id = 0;
      uint8_t prefix = 0;
      const char *loader = NULL, *mod = NULL, *name = NULL;
      const char *p_loader = NULL, *p_mod = NULL, *p_name = NULL;

      tef_save_tlv_t f;
      while (tef_save_rd_next(&os, &f)) {
        switch (f.tag) {
          case TEF_SAVE_CHEST_INDEX:           ci          = tef_save_rd_u32(&f); break;
          case TEF_SAVE_CHEST_SLOT:            slot        = tef_save_rd_u32(&f); break;
          case TEF_SAVE_CHEST_ORIGINAL_ID:     original_id = tef_save_rd_u32(&f); break;
          case TEF_SAVE_CHEST_LOADER:          loader      = tef_save_rd_str(&f); break;
          case TEF_SAVE_CHEST_MOD:             mod         = tef_save_rd_str(&f); break;
          case TEF_SAVE_CHEST_NAME:            name        = tef_save_rd_str(&f); break;
          case TEF_SAVE_CHEST_STACK:           stack       = tef_save_rd_u32(&f); break;
          case TEF_SAVE_CHEST_PREFIX:          prefix      = tef_save_rd_u8(&f);  break;
          case TEF_SAVE_CHEST_PREFIX_LOADER:   p_loader    = tef_save_rd_str(&f); break;
          case TEF_SAVE_CHEST_PREFIX_MOD:      p_mod       = tef_save_rd_str(&f); break;
          case TEF_SAVE_CHEST_PREFIX_NAME:     p_name      = tef_save_rd_str(&f); break;
          default: break;
        }
      }
      if (stack > 0)
        apply_world_item(chests, ci, slot, original_id, loader, mod, name,
                         stack, prefix, p_loader, p_mod, p_name);
    }
    patchlib_free(chests);
  }
}

static void sanitize_world_chests(void) {
  const tef_save_game_handles_t *h = tef_save_game_handles();
  if (!patchlib_is_valid(h->f_main_chest)) return;

  patch_handle_t chests = PATCH_NULL;
  patchlib_field_get_value(h->f_main_chest, PATCH_NULL, &chests);
  if (!patchlib_is_valid(chests)) return;

  const int max_valid = terraria_item_manager_get_count();
  int changed = 0;
  const size_t count = patchlib_array_length(chests);
  for (size_t i = 0; i < count; ++i) {
    patch_handle_t c = PATCH_NULL;
    if (!patchlib_array_at(chests, i, &c) || !patchlib_is_valid(c)) continue;
    changed += tef_save_sanitize_chest(c, max_valid);
    patchlib_free(c);
  }
  patchlib_free(chests);
  if (changed > 0)
    TEKLOG_INFO("tef_save_world_save: replaced %d stale item(s) in world chests", changed);
}

static void load_world_chests(void) {
  char *wld = world_tef_save_base();
  if (!wld || !wld[0]) return;
  char path[1400];
  if (!tef_save_make_path(wld, path, sizeof path)) return;

  tef_save_file_t f;
  if (!tef_save_file_open(path, &f)) return;
  if (f.kind != TEF_SAVE_KIND_WORLD) { tef_save_file_close(&f); return; }

  for (int i = 0; i < f.seg_count; ++i) {
    tef_save_rd_t seg = tef_save_file_seg(&f, i);
    switch (f.segs[i].id) {
      case TEF_SAVE_SEG_CHESTS:   load_chests_seg(&seg);   break;
      default: break;
    }
  }
  tef_save_file_close(&f);
}

/* ================================================================
 * 钩子
 * ================================================================ */
static void chest_save_postfix(patch_handle_t this, void **args, void *result,
                               const patch_method_signature_t *sig) {
  (void)this; (void)args; (void)result; (void)sig;
  save_world_chests();
}

static void chest_load_postfix(patch_handle_t this, void **args, void *result,
                               const patch_method_signature_t *sig) {
  (void)this; (void)args; (void)result; (void)sig;
  load_world_chests();
  sanitize_world_chests();
}

/* ================================================================
 * 初始化
 * ================================================================ */
void tef_world_save_init(void) {
  tef_save_game_handles_init();

  patch_handle_t worldfile_class = patchlib_type_get_type("Terraria.IO", "WorldFile");
  if (!patchlib_is_valid(worldfile_class)) {
    TEKLOG_WARN("tef_save_world_save: Terraria.IO.WorldFile not found");
    return;
  }

  patch_handle_t save_chests = patchlib_type_get_method_by_param_count(worldfile_class, "SaveChests", 1);
  if (patchlib_is_valid(save_chests)) {
    patchlib_install_prepost_hook(save_chests, NULL, chest_save_postfix);
    TEKLOG_INFO("tef_save_world_save: hooked WorldFile.SaveChests");
    patchlib_free(save_chests);
  }
  patch_handle_t load_chests = patchlib_type_get_method_by_param_count(worldfile_class, "LoadChests", 1);
  if (patchlib_is_valid(load_chests)) {
    patchlib_install_prepost_hook(load_chests, NULL, chest_load_postfix);
    TEKLOG_INFO("tef_save_world_save: hooked WorldFile.LoadChests");
    patchlib_free(load_chests);
  }

  patchlib_free(worldfile_class);
}