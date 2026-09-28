/*******************************************************************************
 * tefkernel - item_manager
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
 * Created: 2026/8/26
 *******************************************************************************/

#include "internal/terraria/item_manager.h"
#include "terraria/language_manager.h"
#include "terraria/recipe_manager.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"
#include "terraria/asset.h"
#include "terraria/texture2d.h"

tefstd_vector_t g_terraria_item_registry; // 存储 terraria_item_handle_t*
static bool g_ids_assigned = false;

static patch_handle_t item_id_count;
static terraria_item_handle_t unknown_item;

static const char *unknown_item_text(language_culture_t l) {
  if (l == LANGUAGE_ENGLISH)
    return "{"
           "\"item_name_tefkernel_tefkernel\": {"
           "\"unknown\": \"英文\""
           "}"
           "}";


  return "{"
      "\"item_name_tefkernel_tefkernel\": {"
      "\"unknown\": \"不知道什么语言所以UNKNOWN\""
      "},"
      "item_tooltip_tefkernel_tefkernel: {"
      "\"unknown\": \"傻逼泰拉代码\""
      "}"
      "}";
}

static void unknown_item_recipe() {
  terraria_recipe_set_result(unknown_item.runtime_id, 9999);
  terraria_recipe_set_material((int[2]){9, 1}, 1);
  terraria_recipe_add();
}

static void unknown_item_early_init(terraria_item_handle_t* current) {
  terraria_recipe_manager_register_callback(unknown_item_recipe);
  terraria_language_manager_register_callback(unknown_item_text);
}

// 未知物品的属性设置
static void unknown_item_set_defaults(terraria_item_handle_t *current,
                                      patch_handle_t instance) {
  /*
    patch_handle_t item_class = patchlib_type_get_type("Terraria", "Item");
    if (patchlib_is_valid(item_class)) {
      patch_handle_t f_name_override =
          patchlib_type_get_field(item_class, "_nameOverride");
      patch_handle_t f_bestiary_notes =
          patchlib_type_get_field(item_class, "BestiaryNotes");
      if (patchlib_is_valid(f_name_override)) {
        patch_handle_t name = patchlib_string_create("UNKNOWN");
        patchlib_field_set_value(f_name_override, instance, &name);
        patchlib_field_set_value(f_bestiary_notes, instance, &name);

        patchlib_free(name);
        patchlib_free(f_name_override);
        patchlib_free(f_bestiary_notes);
      }
      patchlib_free(item_class);
    }*/
}

// 生成未知物品纹理
static patch_handle_t
unknown_item_get_texture(terraria_item_handle_t *current) {
  const int SIZE = 64;
  const size_t PIXEL_COUNT = SIZE * SIZE;
  const size_t DATA_SIZE = PIXEL_COUNT * 4;

  uint8_t *pixels = malloc(DATA_SIZE);
  if (!pixels) {
    return 0;
  }

  // 预定义颜色值（直接使用 32-bit 整数，减少 memcpy 开销）
  const uint32_t COLOR_DARK_PURPLE = 0xFFA45067; // RGBA: 103,80,164,255
  const uint32_t COLOR_BLACK = 0xFF000000;
  const uint32_t DIVIDER_COLOR = 0xFF323232;

  const int CELL_SIZE = SIZE / 2;
  const int ROW_SIZE = SIZE * 4;

#if defined(__ANDROID__) || defined(ANDROID)
  // Android: Y 轴翻转
  for (int y = 0; y < SIZE; y++) {
    int src_y = SIZE - 1 - y;
    uint32_t *row = (uint32_t *)(pixels + y * ROW_SIZE);

    for (int x = 0; x < SIZE; x++) {
      if (x == CELL_SIZE || src_y == CELL_SIZE) {
        row[x] = DIVIDER_COLOR;
      } else {
        const bool isLeft = x < CELL_SIZE;
        const bool isTop = src_y < CELL_SIZE;
        row[x] = (isTop && isLeft) || (!isTop && !isLeft) ? COLOR_BLACK
                                                          : COLOR_DARK_PURPLE;
      }
    }
  }
#else
  // 桌面: 正常坐标
  for (int y = 0; y < SIZE; y++) {
    uint32_t *row = (uint32_t *)(pixels + y * ROW_SIZE);

    for (int x = 0; x < SIZE; x++) {
      if (x == CELL_SIZE || y == CELL_SIZE) {
        row[x] = DIVIDER_COLOR;
      } else {
        const bool isLeft = x < CELL_SIZE;
        const bool isTop = y < CELL_SIZE;
        row[x] = (isTop && isLeft) || (!isTop && !isLeft) ? COLOR_BLACK
                                                          : COLOR_DARK_PURPLE;
      }
    }
  }
#endif

  patch_handle_t texture = terraria_texture2d_create(
      SIZE, SIZE, TEXTURE_FORMAT_RGBA32, pixels, DATA_SIZE);

  free(pixels);

  patch_handle_t texture_type = terraria_texture2d_get_class();
  patch_handle_t asset = terraria_asset_create(texture_type, texture);

  patchlib_free(texture_type);
  patchlib_free(texture);

  return asset;
}

// 创建未知物品实例
static terraria_item_handle_t unknown_item = {
    .parent_modloader_id = "tefkernel",
    .parent_id = "tefkernel",
    .internal_name = "unknown",
    .has_tooltip = false,
    .runtime_id = -1,
    .item_ops = {.early_init = unknown_item_early_init,
                 .init_static = NULL,
                 .set_defaults = unknown_item_set_defaults,
                 .can_use = NULL,
                 .get_texture = unknown_item_get_texture}};

static patch_handle_t f_type = PATCH_NULL;
static patch_handle_t f_stack = PATCH_NULL;
static patch_handle_t m_reset_stats = PATCH_NULL;
static patch_handle_t m_rebuild_tooltip = PATCH_NULL; // void RebuildTooltip()

static patch_handle_t f_texture_item = PATCH_NULL;
static patch_handle_t f_texture_item_flame = PATCH_NULL;

static patch_handle_t f_item_name_cache = PATCH_NULL;
static patch_handle_t f_item_tooltip_cache = PATCH_NULL;

static patch_handle_t m_from_language_key = PATCH_NULL; // static ItemTooltip FromLanguageKey(short id, string key)

static void set_defaults_postfix(patch_handle_t this, void **args, void *result,
                                 const patch_method_signature_t *sig_info) {
  // 检查 args 是否有效
  if (!args || !args[0] || !this) {
    return; // 非原版物品不打印日志
  }

  const int args_type = *(int *)args[0];
  const int base_count = terraria_item_id_get_count();
  const size_t custom_count = tefstd_vector_size(&g_terraria_item_registry);

  // 只处理自定义物品（超出原版范围的 ID）
  const int nid = args_type - base_count;
  if (nid < 0 || nid >= (int)custom_count) {
    return; // 非原版物品不打印日志
  }

  // ⭐ 关键：强制恢复 type（SetDefaults 内部可能把它清空了）
  patchlib_field_set_value(f_type, this, args[0]);

  // 设置堆叠数
  int stack = 1;
  patchlib_field_set_value(f_stack, this, &stack);

  // 调用自定义物品的 set_defaults
  terraria_item_handle_t **item_handle_ptr =
      tefstd_vector_at(&g_terraria_item_registry, nid);
  if (item_handle_ptr && *item_handle_ptr) {
    terraria_item_handle_t *item_handle = *item_handle_ptr;
    if (item_handle->item_ops.set_defaults) {
      item_handle->item_ops.set_defaults(item_handle, this);
    }
  }

  patchlib_method_invoke_args(m_rebuild_tooltip, this, PATCH_NULL, NULL);
}

/*
static bool
item_check_check_can_use_inner_prefix(patch_handle_t this, void **args,
                                      const patch_method_signature_t *sig_info,
                                      void *result) {

  if (!args || !args[0] || !this) {
    return false; // 非原版物品不打印日志
  }

  bool ignore_cursed = false;
  if (args[1]) {
    ignore_cursed = *(bool *)args[1];
  }

  patch_handle_t args_item = *(patch_handle_t *)args[0];
  int current_item_type = 0;
  patchlib_field_get_value(f_type, args_item, &current_item_type);

  const int base_count = terraria_item_id_get_count();
  const size_t custom_count = tefstd_vector_size(&g_terraria_item_registry);

  // 只处理自定义物品（超出原版范围的 ID）
  const int nid = current_item_type - base_count;
  if (nid < 0 || nid >= (int)custom_count) {
    return false; // 非原版物品不打印日志
  }

  // 调用自定义物品的 can_use
  terraria_item_handle_t **item_handle_ptr =
      tefstd_vector_at(&g_terraria_item_registry, nid);
  if (item_handle_ptr && *item_handle_ptr) {
    terraria_item_handle_t *item_handle = *item_handle_ptr;
    if (item_handle->item_ops.can_use) {
      *(bool *)result = item_handle->item_ops.can_use(item_handle, this,
                                                      args_item, ignore_cursed);
      return true;
    }
  }

  return false;
}
*/

static void get_item_name_postfix(patch_handle_t this, void **args,
                                  void *result,
                                  const patch_method_signature_t *sig_info) {

  const int current_item_type = *(int *)args[0];
  const int base_count = terraria_item_id_get_count();
  const size_t custom_count = tefstd_vector_size(&g_terraria_item_registry);

  const int nid = current_item_type - base_count;
  if (nid < 0 || nid >= (int)custom_count) {
    return;
  }

  patch_handle_t array_item_name_cache = PATCH_NULL;
  patchlib_field_get_value(f_item_name_cache, NULL, &array_item_name_cache);

  patchlib_array_at(array_item_name_cache, current_item_type, result);

  patchlib_free(array_item_name_cache);
}

// 比较函数（用于 qsort）
static int compare_item(const void *a, const void *b) {
  const terraria_item_handle_t *item_a = *(const terraria_item_handle_t **)a;
  const terraria_item_handle_t *item_b = *(const terraria_item_handle_t **)b;

  /* 先按 modloader 字典序 */
  int cmp = strcmp(item_a->parent_modloader_id, item_b->parent_modloader_id);
  if (cmp != 0)
    return cmp;

  /* 同一 modloader 内按 mod id 字典序 */
  cmp = strcmp(item_a->parent_id, item_b->parent_id);
  if (cmp != 0)
    return cmp;

  /* 同一 mod 内按 internal_name 字典序 */
  return strcmp(item_a->internal_name, item_b->internal_name);
}

// 初始化

static void resize_array(const char *ns, const char *cls, const char *fid,
                         patch_handle_t type, int nsize);

#if !__ANDROID__
// PC端：AssetInitializer.LoadTextures 会按 TextureAssets.Item 的新长度遍历，
// 并读取 ItemID.Sets.TextureCopyLoad[i]（原版长度只有 ItemID.Count），
// 导致 IndexOutOfRangeException 中断整个贴图加载（表现为游戏无贴图）。
// 同时该函数会用原版 Images/Item_x 覆盖自定义物品贴图。
// 因此：1) resize 时同步扩充 TextureCopyLoad；2) LoadTextures
// 之后重新写入自定义贴图。
static void load_textures_postfix(patch_handle_t this, void **args,
                                  void *result,
                                  const patch_method_signature_t *sig_info) {
  TEKLOG_INFO("AssetInitializer.LoadTextures postfix: re-applying custom item "
              "textures");
  terraria_item_manager_init_texture2d();
}
#endif

// 一次性扩容 ItemID.Sets 中所有长度为 ItemID.Count 的静态数组。
// 游戏代码大量按 item.type
// 索引这些集合（ItemIconPulse、TrapSigned、TextureCopyLoad 等），
// 任何一个没扩容都会导致绘制自定义物品时
// IndexOutOfRangeException（表现为图标"透明"或崩溃）。 一次性扩容 ItemID.Sets
// 中所有长度为 ItemID.Count 的静态数组。 游戏代码大量按 item.type
// 索引这些集合（ItemIconPulse、TrapSigned、TextureCopyLoad 等），
// 任何一个没扩容都会导致绘制自定义物品时
// IndexOutOfRangeException（表现为图标"透明"或崩溃）。
static void resize_all_class_sets(const char *ns, const char *cls,
                                  const char *inner, const int base_count,
                                  const int new_size) {

  patch_handle_t sets_class = patchlib_type_get_type(ns, cls);
  if (inner) {
    patch_handle_t new_sets_class =
        patchlib_type_get_inner_type(sets_class, inner);
    patchlib_free(sets_class);
    sets_class = new_sets_class;
  }

  if (!patchlib_is_valid(sets_class)) {
    TEKLOG_ERROR("resize_all_class_sets: Failed to get %s.%s", ns, cls);
    return;
  }

  tefstd_vector_t fields;
  tefstd_vector_init(&fields, sizeof(patch_handle_t));
  patchlib_type_get_fields(sets_class, false, &fields);
  if (!fields.data || fields.size <= 0) {
    TEKLOG_ERROR("resize_all_class_sets: Failed to get fields of %s.%s", ns,
                 cls);
    patchlib_free(sets_class);
    tefstd_vector_destroy(&fields);
    return;
  }

  // ⭐ 需要排除的字段名称（非数组、多维数组、引用类型数组、List等）
  const char *EXCLUDED_FIELDS[] = {
      // 非数组
      "Factory", "DD2BannerEffect", "DefaultKillsForBannerNeeded", "Count",

      // List<T> 类型
      "ItemsThatAreProcessedAfterNormalContentSample", "NonColorfulDyeItems",

      // 多维数组 (Color[][])
      "FoodParticleColors", "DrinkParticleColors",

      /*
      // 引用类型数组 (FlowerPacketInfo[], BannerEffect[], PlacementDetails[],
      UniqueTagEffect[]) "DerivedPlacementDetails", "flowerPacketInfo",
      "BannerStrength",
      "UniqueTagEffects",
      "ColorfulDyeValues",*/

      // 其他特殊类型
      "ItemsForStuffCannon", "Workbenches", "CanBeQuickusedOnGamepad",
      "ForcesBreaksSleeping", "NetUseSoundSync", "ForceConsumption",
      "OnlyNeedOneInInventoryOverride", "CanPassivelyStackInWorldOverride",
      "LockOnAimCompensation"};

  int resized = 0;
  int skipped = 0;
  int error = 0;

  for (size_t i = 0; i < fields.size; i++) {
    patch_handle_t field = *(patch_handle_t *)tefstd_vector_at(&fields, i);
    if (!patchlib_is_valid(field)) {
      error++;
      continue;
    }

    const char *name = patchlib_field_get_name(field);

    // ⭐ 检查是否在排除列表中
    bool excluded = false;
    for (size_t j = 0; j < sizeof(EXCLUDED_FIELDS) / sizeof(EXCLUDED_FIELDS[0]);
         j++) {
      if (name && strcmp(name, EXCLUDED_FIELDS[j]) == 0) {
        excluded = true;
        break;
      }
    }

    if (excluded) {
      TEKLOG_DEBUG("resize_all_itemid_sets: skipping excluded field: %s",
                   name ? name : "?");
      patchlib_free(field);
      skipped++;
      continue;
    }

    // 获取字段值（数组实例）
    patch_handle_t array = PATCH_NULL;
    patchlib_field_get_value(field, NULL, &array);

    if (!patchlib_is_valid(array)) {
      TEKLOG_DEBUG("resize_all_itemid_sets: invalid array for %s",
                   name ? name : "?");
      patchlib_free(field);
      skipped++;
      continue;
    }

    // 检查数组长度是否匹配 base_count
    const size_t len = patchlib_array_length(array);
    if (len != (size_t)base_count) {
      TEKLOG_DEBUG("resize_all_itemid_sets: skipping %s (len=%zu, expected=%d)",
                   name ? name : "?", len, base_count);
      patchlib_free(field);
      patchlib_free(array);
      skipped++;
      continue;
    }

    // ⭐ 执行扩容（传入默认值）
    patch_handle_t new_array = patchlib_array_resize(array, new_size, NULL);

    if (patchlib_is_valid(new_array)) {
      patchlib_field_set_value(field, NULL, &new_array);
      TEKLOG_DEBUG(
          "resize_all_itemid_sets: ✅ resized ItemID.Sets.%s (%zu -> %d)",
          name ? name : "?", len, new_size);
      patchlib_free(new_array);
      resized++;
    } else {
      TEKLOG_ERROR("resize_all_itemid_sets: failed to resize %s",
                   name ? name : "?");
      error++;
    }

    patchlib_free(field);
    patchlib_free(array);
  }

  tefstd_vector_destroy(&fields);

  TEKLOG_INFO(
      "resize_all_class_sets: %s.%s %d arrays resized, %d skipped, %d errors",
      ns, cls, resized, skipped, error);
  patchlib_free(sets_class);
}

// 一次性扩容所有按 ItemID.Count 定长、按物品 type 索引的静态集合类：
// - ItemID.Sets：116 个集合（ItemIconPulse、TrapSigned、TextureCopyLoad 等）
// - PrefixLegacy+ItemSets：7 个 bool[]（Item.Prefix 掷前缀路径，无边界检查，
//   合成产物 Prefix(-1) 时越界导致材料消耗但物品消失）
// - AmmoID+Sets：3 个 bool[]（弹药类型判断，无边界检查）
static void resize_all_itemid_sets(const int base_count, const int new_size) {
  resize_all_class_sets("Terraria.ID", "ItemID", "Sets", base_count, new_size);
  resize_all_class_sets("Terraria.GameContent.Prefixes", "PrefixLegacy",
                        "ItemSets", base_count, new_size);
  resize_all_class_sets("Terraria.ID", "AmmoID", "Sets", base_count, new_size);
}

// Main.itemAnimations 是 DrawAnimation[ItemID.Count]，
// 绘制自定义物品图标（ItemSlot.DrawItemIcon / Main.LoadItem /
// 更新动画等）时会按 type 索引，必须同步扩容。 新增区域为
// null，表示自定义物品没有动画帧，符合预期。
static void resize_item_animations(const int new_size) {
  patch_handle_t draw_animation_class =
      patchlib_type_get_type("Terraria.DataStructures", "DrawAnimation");
  if (!patchlib_is_valid(draw_animation_class)) {
    TEKLOG_ERROR("resize_item_animations: Failed to get DrawAnimation class");
    return;
  }

  patch_handle_t main_class = patchlib_type_get_type("Terraria", "Main");
  if (!patchlib_is_valid(main_class)) {
    TEKLOG_ERROR("resize_item_animations: Failed to get Main class");
    patchlib_free(draw_animation_class);
    return;
  }

  patch_handle_t field = patchlib_type_get_field(main_class, "itemAnimations");
  if (patchlib_is_valid(field)) {
    resize_array("Terraria", "Main", "itemAnimations", draw_animation_class,
                 new_size);
    patchlib_free(field);
  } else {
    TEKLOG_ERROR(
        "resize_item_animations: Failed to get Main.itemAnimations field");
  }

  patchlib_free(main_class);
  patchlib_free(draw_animation_class);
}

// Lang._itemTooltipCache / _itemNameCache 按 ItemID.Count 定长，且
// GetTooltip 无边界检查，自定义物品悬停取 tooltip 时直接越界。
// 扩容后 _itemTooltipCache 新槽位填充 ItemTooltip.None，避免返回 null 引发
// NRE。
static void resize_lang_caches(const int base_count, const int new_size) {
  patch_handle_t tooltip_class =
      patchlib_type_get_type("Terraria.UI", "ItemTooltip");
  patch_handle_t name_class =
      patchlib_type_get_type("Terraria.Localization", "LocalizedText");
  if (!patchlib_is_valid(tooltip_class) || !patchlib_is_valid(name_class)) {
    TEKLOG_ERROR(
        "resize_lang_caches: Failed to get ItemTooltip/LocalizedText class");
    if (patchlib_is_valid(tooltip_class))
      patchlib_free(tooltip_class);
    if (patchlib_is_valid(name_class))
      patchlib_free(name_class);
    return;
  }

  resize_array("Terraria", "Lang", "_itemTooltipCache", tooltip_class,
               new_size);
  {
    patch_handle_t lang_class = patchlib_type_get_type("Terraria", "Lang");
    patch_handle_t f_cache =
        patchlib_type_get_field(lang_class, "_itemTooltipCache");
    patch_handle_t f_none = patchlib_type_get_field(tooltip_class, "None");
    patch_handle_t cache = PATCH_NULL;
    patch_handle_t none = PATCH_NULL;
    if (patchlib_is_valid(f_cache))
      patchlib_field_get_value(f_cache, NULL, &cache);
    if (patchlib_is_valid(f_none))
      patchlib_field_get_value(f_none, NULL, &none);
    if (patchlib_is_valid(cache) && patchlib_is_valid(none)) {
      for (int i = base_count; i < new_size; i++) {
        patchlib_array_set(cache, (size_t)i, &none);
      }
    }
    if (patchlib_is_valid(cache))
      patchlib_free(cache);
    if (patchlib_is_valid(none))
      patchlib_free(none);
    if (patchlib_is_valid(f_cache))
      patchlib_free(f_cache);
    if (patchlib_is_valid(f_none))
      patchlib_free(f_none);
    patchlib_free(lang_class);
  }

  // _itemNameCache: LocalizedText[]（GetItemName 对 null 槽位有判断，无需填充）
  resize_array("Terraria", "Lang", "_itemNameCache", name_class, new_size);

  // _layerIndexForItemType: int[]（GetSortingLayerIndex 无边界检查，
  // 物品进背包触发排序时按 type 索引，越界导致物品"消失"）
  {
    patch_handle_t int32_type = patchlib_get_basic_type(PATCH_INT32);
    resize_array("Terraria.UI", "ItemSorting", "_layerIndexForItemType",
                 int32_type, new_size);
    patchlib_free(int32_type);
  }

  patchlib_free(tooltip_class);
  patchlib_free(name_class);
}

// ArmorSetBonuses.SetsContaining 是锯齿数组 ArmorSetBonus[ItemID.Count][]，
// tooltip 生成直接按 item.type 索引且无边界检查。
// 扩容后新槽位填充 vanilla BuildLookup 使用的共享空数组（取 [0] 复用），否则
// foreach null 会 NRE。
static void resize_armor_set_bonuses(const int base_count, const int new_size) {
  patch_handle_t bonus_class =
      patchlib_type_get_type("Terraria.DataStructures", "ArmorSetBonuses");
  if (!patchlib_is_valid(bonus_class)) {
    TEKLOG_ERROR(
        "resize_armor_set_bonuses: Failed to get ArmorSetBonuses class");
    return;
  }

  patch_handle_t f_sets =
      patchlib_type_get_field(bonus_class, "SetsContaining");
  if (!patchlib_is_valid(f_sets)) {
    TEKLOG_ERROR(
        "resize_armor_set_bonuses: Failed to get SetsContaining field");
    patchlib_free(bonus_class);
    return;
  }

  patch_handle_t old_array = PATCH_NULL;
  patchlib_field_get_value(f_sets, NULL, &old_array);
  if (patchlib_is_valid(old_array)) {
    if ((size_t)base_count == patchlib_array_length(old_array)) {
      patch_handle_t empty = PATCH_NULL;
      if (patchlib_array_at(old_array, 0, &empty) && patchlib_is_valid(empty)) {
        patch_handle_t new_array =
            patchlib_array_resize(old_array, (size_t)new_size, NULL);
        if (patchlib_is_valid(new_array)) {
          for (int i = base_count; i < new_size; i++) {
            patchlib_array_set(new_array, (size_t)i, &empty);
          }
          patchlib_field_set_value(f_sets, NULL, &new_array);
          TEKLOG_INFO("resize_armor_set_bonuses: ✅ SetsContaining resized to "
                      "%d (empty slots filled)",
                      new_size);
          patchlib_free(new_array);
        }
        patchlib_free(empty);
      } else {
        TEKLOG_ERROR(
            "resize_armor_set_bonuses: Failed to read SetsContaining[0]");
      }
    }
    patchlib_free(old_array);
  }

  patchlib_free(f_sets);
  patchlib_free(bonus_class);
}

void terraria_item_manager_init() {
  if (!tefstd_vector_init(&g_terraria_item_registry,
                          sizeof(terraria_item_handle_t *)))
    return;

  terraria_item_manager_register_item(&unknown_item);

  patch_handle_t item_id_class =
      patchlib_type_get_type("Terraria.ID", "ItemID");
  item_id_count = patchlib_type_get_field(item_id_class, "Count");

  patch_handle_t item_class = patchlib_type_get_type("Terraria", "Item");
  f_type = patchlib_type_get_field(item_class, "type");
  f_stack = patchlib_type_get_field(item_class, "stack");
  m_rebuild_tooltip = patchlib_type_get_method_by_param_count(item_class, "RebuildTooltip", 0);
  m_reset_stats =
      patchlib_type_get_method_by_param_count(item_class, "ResetStats", 1);
  patch_handle_t set_defaults =
      patchlib_type_get_method_by_param_count(item_class, "SetDefaults", 2);

  patchlib_install_prepost_hook(set_defaults, NULL, set_defaults_postfix);

  patch_handle_t texture_asset_class =
      patchlib_type_get_type("Terraria.GameContent", "TextureAssets");

  f_texture_item = patchlib_type_get_field(texture_asset_class, "Item");
  f_texture_item_flame =
      patchlib_type_get_field(texture_asset_class, "ItemFlame");

  patch_handle_t lang_cls = patchlib_type_get_type("Terraria", "Lang");
  f_item_name_cache = patchlib_type_get_field(lang_cls, "_itemNameCache");
  f_item_tooltip_cache = patchlib_type_get_field(lang_cls, "_itemTooltipCache");
  patch_handle_t get_item_name =
      patchlib_type_get_method_by_param_count(lang_cls, "GetItemName", 1);

  patchlib_install_prepost_hook(get_item_name, NULL, get_item_name_postfix);

  patch_handle_t item_tooltip_cls = patchlib_type_get_type("Terraria.UI", "ItemTooltip");
  m_from_language_key = patchlib_type_get_method_by_param_count(item_tooltip_cls, "FromLanguageKey", 2);

  patchlib_free(item_tooltip_cls);
  patchlib_free(get_item_name);
  patchlib_free(lang_cls);
  patchlib_free(texture_asset_class);
  patchlib_free(item_class);
  patchlib_free(set_defaults);
  patchlib_free(item_id_class);

#if !__ANDROID__
  // PC端：在原版贴图加载完成后重新写入自定义物品贴图（原版会覆盖整个数组）
  patch_handle_t asset_initializer_class =
      patchlib_type_get_type("Terraria.Initializers", "AssetInitializer");
  if (patchlib_is_valid(asset_initializer_class)) {
    patch_handle_t load_textures = patchlib_type_get_method_by_param_count(
        asset_initializer_class, "LoadTextures", 1);
    if (patchlib_is_valid(load_textures)) {
      const patch_hook_id_t hook_id = patchlib_install_prepost_hook(
          load_textures, NULL, load_textures_postfix);
      if (hook_id != PATCH_HOOK_INVALID_ID) {
        TEKLOG_INFO(
            "AssetInitializer.LoadTextures hook installed successfully");
      } else {
        TEKLOG_ERROR("Failed to install AssetInitializer.LoadTextures hook");
      }
      patchlib_free(load_textures);
    } else {
      TEKLOG_ERROR("Failed to get AssetInitializer.LoadTextures method");
    }
    patchlib_free(asset_initializer_class);
  } else {
    TEKLOG_ERROR("Failed to get AssetInitializer class");
  }
#endif
}

void terraria_item_manager_destroy() {
  // 注意：只释放 vector，不释放 item 本身（由调用者管理）
  tefstd_vector_destroy(&g_terraria_item_registry);
  g_ids_assigned = false;
}

bool terraria_item_manager_register_item(terraria_item_handle_t *item_handle) {
  if (!item_handle || !item_handle->parent_modloader_id ||
      !item_handle->parent_id || !item_handle->internal_name)
    return false;

  /* 检查是否已存在 */
  const size_t count = tefstd_vector_size(&g_terraria_item_registry);
  for (size_t i = 0; i < count; i++) {
    terraria_item_handle_t **existing =
        tefstd_vector_at(&g_terraria_item_registry, i);
    if (!existing || !*existing)
      continue;

    if (strcmp((*existing)->parent_modloader_id,
               item_handle->parent_modloader_id) == 0 &&
        strcmp((*existing)->parent_id, item_handle->parent_id) == 0 &&
        strcmp((*existing)->internal_name, item_handle->internal_name) == 0) {
      return false; /* 重复注册 */
    }
  }

  /* 加入 registry */
  return tefstd_vector_push_back(&g_terraria_item_registry, &item_handle);
}

bool terraria_item_manager_unregister_item_by_id(const int runtime_id) {
  if (runtime_id == 0)
    return false;

  const size_t count = tefstd_vector_size(&g_terraria_item_registry);
  for (size_t i = 0; i < count; i++) {
    return tefstd_vector_erase(&g_terraria_item_registry, runtime_id, NULL);
  }
  return false;
}

bool terraria_item_manager_assign_ids(const int base_id) {
  const size_t count = tefstd_vector_size(&g_terraria_item_registry);
  if (count == 0)
    return true;

  // 获取底层数组指针进行排序
  terraria_item_handle_t **items = g_terraria_item_registry.data;

  // 排序
  qsort(items, count, sizeof(terraria_item_handle_t *), compare_item);

  // 分配 ID（从 base_id 开始紧凑分配）
  for (size_t i = 0; i < count; i++)
    items[i]->runtime_id = base_id + (int)i;

  g_ids_assigned = true;
  return true;
}

int terraria_item_id_get_count() {
  short count = 0;
  patchlib_field_get_value(item_id_count, NULL, &count);
  return count;
}

void terraria_item_manager_init_texture2d() {
  TEKLOG_DEBUG("terraria_item_manager_init_texture2d: starting");

  // 检查字段是否有效
  if (!patchlib_is_valid(f_texture_item) ||
      !patchlib_is_valid(f_texture_item_flame)) {
    TEKLOG_ERROR("Texture fields not initialized");
    return;
  }

  // 扩容数组
  patch_handle_t texture_type = terraria_texture2d_get_class();
  patch_handle_t asset_type = terraria_asset_get_generic_class(texture_type);

  resize_array("Terraria.GameContent", "TextureAssets", "Item", asset_type,
               terraria_item_manager_get_count());
  resize_array("Terraria.GameContent", "TextureAssets", "ItemFlame", asset_type,
               terraria_item_manager_get_count());

  patchlib_free(texture_type);
  patchlib_free(asset_type);

  // 初始化
  patch_handle_t texture_item_array = PATCH_NULL;
  patch_handle_t texture_item_flame_array = PATCH_NULL;
  patchlib_field_get_value(f_texture_item, NULL, &texture_item_array);
  patchlib_field_get_value(f_texture_item_flame, NULL,
                           &texture_item_flame_array);

  if (!patchlib_is_valid(texture_item_array) ||
      !patchlib_is_valid(texture_item_flame_array)) {
    TEKLOG_ERROR("Failed to get texture arrays");
    return;
  }

  int base_count = terraria_item_id_get_count();
  size_t custom_count = tefstd_vector_size(&g_terraria_item_registry);
  size_t array_len = patchlib_array_length(texture_item_array);

  TEKLOG_INFO("init_texture2d: array_len=%zu, base_count=%d, custom_count=%zu, "
              "start_index=%d",
              array_len, base_count, custom_count, base_count);

  // 创建默认纹理
  patch_handle_t unknow_texture = unknown_item_get_texture(NULL);
  if (!patchlib_is_valid(unknow_texture)) {
    TEKLOG_ERROR("Failed to create default texture");
    return;
  }
  TEKLOG_DEBUG("Created default texture: %p", unknow_texture);

  int success_count = 0;
  int fail_count = 0;

  for (size_t i = 0; i < custom_count; ++i) {
    const size_t index = (size_t)base_count + i;

    terraria_item_handle_t **item_handle_ptr =
        tefstd_vector_at(&g_terraria_item_registry, i);
    if (!item_handle_ptr || !*item_handle_ptr) {
      TEKLOG_WARN("init_texture2d: invalid item_handle at index %zu", i);
      fail_count++;
      continue;
    }

    terraria_item_handle_t *item_handle = *item_handle_ptr;
    TEKLOG_DEBUG("init_texture2d: processing item %zu: %s.%s (index=%zu)", i,
                 item_handle->parent_id, item_handle->internal_name, index);

    patch_handle_t itx = PATCH_NULL;
    if (item_handle->item_ops.get_texture) {
      itx = item_handle->item_ops.get_texture(item_handle);
      TEKLOG_DEBUG("init_texture2d: get_texture returned %p for %s.%s", itx,
                   item_handle->parent_id, item_handle->internal_name);
    }

    if (patchlib_is_valid(itx)) {
      if (patchlib_array_set(texture_item_array, index, &itx)) {
        patchlib_array_set(texture_item_flame_array, index, &itx);
        success_count++;
        TEKLOG_DEBUG(
            "init_texture2d: set custom texture for %s.%s at index %zu",
            item_handle->parent_id, item_handle->internal_name, index);
      } else {
        TEKLOG_ERROR("init_texture2d: failed to set texture at index %zu",
                     index);
        fail_count++;
      }
      patchlib_free(itx);
    } else {
      // 使用默认纹理
      if (patchlib_array_set(texture_item_array, index, &unknow_texture)) {
        patchlib_array_set(texture_item_flame_array, index, &unknow_texture);
        TEKLOG_DEBUG(
            "init_texture2d: set default texture for %s.%s at index %zu",
            item_handle->parent_id, item_handle->internal_name, index);
      } else {
        TEKLOG_ERROR(
            "init_texture2d: failed to set default texture at index %zu",
            index);
        fail_count++;
      }
    }
  }

  TEKLOG_INFO("init_texture2d: completed: %d success, %d failed, %zu total "
              "custom items",
              success_count, fail_count, custom_count);

  patchlib_free(unknow_texture);
  patchlib_free(texture_item_array);
  patchlib_free(texture_item_flame_array);
}

static void resize_array(const char *ns, const char *cls, const char *fid,
                         patch_handle_t type, const int nsize) {
  // 获取类型和字段
  patch_handle_t class_handle = patchlib_type_get_type(ns, cls);
  if (!patchlib_is_valid(class_handle)) {
    TEKLOG_ERROR("resize_array: Failed to get class: %s.%s", ns, cls);
    return;
  }

  patch_handle_t field = patchlib_type_get_field(class_handle, fid);
  if (!patchlib_is_valid(field)) {
    TEKLOG_ERROR("resize_array: Failed to get field: %s.%s.%s", ns, cls, fid);
    patchlib_free(class_handle);
    return;
  }

  // 获取旧数组
  patch_handle_t array = PATCH_NULL;
  patchlib_field_get_value(field, NULL, &array);

  if (!patchlib_is_valid(array)) {
    TEKLOG_WARN(
        "resize_array: Array %s.%s.%s is NULL, creating new array of size %d",
        ns, cls, fid, nsize);
    patch_handle_t narray = patchlib_array_create(nsize, type);
    if (patchlib_is_valid(narray)) {
      patchlib_field_set_value(field, NULL, &narray);
      TEKLOG_INFO("resize_array: Created new array %s.%s.%s of size %d", ns,
                  cls, fid, nsize);
      patchlib_free(narray);
    }
    patchlib_free(field);
    patchlib_free(class_handle);
    return;
  }

  const uint32_t old_len = patchlib_array_length(array);

  if (old_len >= (uint32_t)nsize) {
    TEKLOG_DEBUG("resize_array: %s.%s.%s already large enough: %u >= %d", ns,
                 cls, fid, old_len, nsize);
    patchlib_free(array);
    patchlib_free(field);
    patchlib_free(class_handle);
    return;
  }

  TEKLOG_DEBUG("resize_array: Resizing %s.%s.%s from %u to %d", ns, cls, fid,
               old_len, nsize);

  // 创建新数组
  patch_handle_t narray = patchlib_array_create(nsize, type);
  if (!patchlib_is_valid(narray)) {
    TEKLOG_ERROR(
        "resize_array: Failed to create new array of size %d for %s.%s.%s",
        nsize, ns, cls, fid);
    patchlib_free(array);
    patchlib_free(field);
    patchlib_free(class_handle);
    return;
  }

  // 复制所有旧数据到新数组
  if (old_len > 0) {
    if (!patchlib_array_copy(narray, array, old_len)) {
      TEKLOG_ERROR("resize_array: Failed to copy data for %s.%s.%s", ns, cls,
                   fid);
    }
  }

  // 更新字段
  patchlib_field_set_value(field, NULL, &narray);
  TEKLOG_INFO("resize_array: ✅ %s.%s.%s resized from %u to %d", ns, cls, fid,
              old_len, nsize);

  // 清理资源
  patchlib_free(narray);
  patchlib_free(array);
  patchlib_free(field);
  patchlib_free(class_handle);
}

void terraria_item_manager_resize() {
  const int new_size = terraria_item_manager_get_count();

  patch_handle_t bool_type = patchlib_get_basic_type(PATCH_BOOL);

  // 注意：这些数组在 Terraria 中用于存储物品相关的数据
  resize_array("Terraria", "Item", "claw", bool_type, new_size);
  resize_array("Terraria", "Item", "staff", bool_type, new_size);

#if __ANDROID__
  resize_array("Terraria", "Player", "ItemUsesRightFire", bool_type, new_size);
  resize_array("", "VirtualControllerInputState", "ItemCategories",
               patchlib_get_basic_type(PATCH_INT32), new_size);
#endif
  // 一次性扩容 ItemID.Sets 所有按 ItemID.Count 定长的数组
  resize_all_itemid_sets(terraria_item_id_get_count(), new_size);
  // 绘制物品图标时按 type 索引 Main.itemAnimations，必须同步扩充
  resize_item_animations(new_size);
  // tooltip/名称缓存与物品排序索引同步扩容（无边界检查，越界会导致物品"消失"）
  resize_lang_caches(terraria_item_id_get_count(), new_size);
  // 护甲套装查询锯齿数组同步扩容（tooltip 生成路径）
  resize_armor_set_bonuses(terraria_item_id_get_count(), new_size);

  patchlib_free(bool_type);
}

int terraria_item_manager_get_count() {
  return (int)tefstd_vector_size(&g_terraria_item_registry) +
         terraria_item_id_get_count();
}

tefstd_vector_t *terraria_item_manager_get_items() {
  return &g_terraria_item_registry;
}

static char* generate_key(size_t key_size, const char* prefix, terraria_item_handle_t* current) {

  // 计算所需缓冲区大小
  if (current->parent_modloader_id)
    key_size += strlen(current->parent_modloader_id);
  else
    key_size += 1;

  if (current->parent_id)
    key_size += strlen(current->parent_id);
  else
    key_size += 1;

  if (current->internal_name)
    key_size += strlen(current->internal_name);
  else
    key_size += 1;

  // 分配并拼接键名
  char *key_buffer = malloc(key_size);
  snprintf(key_buffer, key_size, "%s_%s_%s.%s", prefix,
           current->parent_modloader_id ? current->parent_modloader_id : "?",
           current->parent_id ? current->parent_id : "?",
           current->internal_name ? current->internal_name : "?");

  return key_buffer;
}

void terraria_item_manager_init_localized_text() {
  patch_handle_t array_item_name_cache = PATCH_NULL;
  patch_handle_t array_item_tooltip_cache = PATCH_NULL;
  patchlib_field_get_value(f_item_name_cache, PATCH_NULL, &array_item_name_cache);
  patchlib_field_get_value(f_item_tooltip_cache, PATCH_NULL, &array_item_tooltip_cache);

  const int org_count = terraria_item_id_get_count();

  for (int i = 0; i < tefstd_vector_size(&g_terraria_item_registry); ++i) {
    // vector 存的是 terraria_item_handle_t*，所以先取指针，再解引用
    terraria_item_handle_t **pp_current =
        tefstd_vector_at(&g_terraria_item_registry,
                         i);

    if (!pp_current || !*pp_current)
      continue;

    terraria_item_handle_t *current = *pp_current;

    // 分配并拼接键名
    char *item_name_key_buffer = generate_key(13, "item_name", current);
    int target_index = org_count + i;

    patch_handle_t item_name_key =
        terraria_language_manager_get_localized_text(item_name_key_buffer);

    patchlib_array_set(array_item_name_cache, target_index, &item_name_key);
    patchlib_free(item_name_key);
    free(item_name_key_buffer);

    if (current->has_tooltip) {
      char *item_tooltip_key_buffer = generate_key(15, "item_tooltip", current);
      patch_handle_t item_tooltip_key =
          terraria_language_manager_get_localized_text(item_tooltip_key_buffer);

#if __ANDROID__
      patch_handle_t item_tooltip = ((void*(*)(short, void*))patchlib_method_get_pointer(m_from_language_key))((short)target_index, item_tooltip_key);
#else
      void* args[2] = { &target_index, &item_tooltip_key };
      patch_handle_t item_tooltip = PATCH_NULL;
      patchlib_method_invoke_args(m_from_language_key, NULL, args, &item_tooltip);
# endif

      patchlib_array_set(array_item_tooltip_cache, target_index, &item_tooltip);

      patchlib_free(item_tooltip);
      patchlib_free(item_tooltip_key);
      free(item_tooltip_key_buffer);
    }
  }

  patchlib_free(array_item_tooltip_cache);
  patchlib_free(array_item_name_cache);
  patchlib_free(f_item_name_cache);
  patchlib_free(lang_cls);
}
