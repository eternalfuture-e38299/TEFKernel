/*******************************************************************************
 * File: prefix_manager
 * Project: tefkernel
 * Created: 2026/10/30
 * Author: eternalfuture-e38299
 * Github: https://github.com/eternalfuture-e38299
 *
 * MIT License
 *
 * Copyright (c) 2026 eternalfuture-e38299
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *******************************************************************************/

#ifndef TEFKERNEL_PREFIX_MANAGER_H
#define TEFKERNEL_PREFIX_MANAGER_H

#include "../../tef_api.h"
#include "../../patchlib/type.h"
#include "../../tefstd/vector.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct terraria_prefix_handle_t terraria_prefix_handle_t;

/* 词缀池类别（与 PrefixLegacy.ItemSets 的 bool[] 对应） */
typedef enum terraria_prefix_category_t {
    TERRARIA_PREFIX_CAT_NONE      = 0,
    TERRARIA_PREFIX_CAT_MELEE     = 1,  /* SwordsHammersAxesPicks */
    TERRARIA_PREFIX_CAT_SPEAR     = 2,  /* SpearsMacesChainsawsDrillsPunchCannon */
    TERRARIA_PREFIX_CAT_RANGED    = 3,  /* GunsBows */
    TERRARIA_PREFIX_CAT_MAGIC     = 4,  /* Magic */
    TERRARIA_PREFIX_CAT_SUMMON    = 5,  /* Summon */
    TERRARIA_PREFIX_CAT_BOOMERANG = 6,  /* BoomerangsChakrams */
    TERRARIA_PREFIX_CAT_ACCESSORY = 7,  /* CanGetPrefixes */
    TERRARIA_PREFIX_CAT_ANY       = 8,  /* 任意可重铸物品 */
} terraria_prefix_category_t;

typedef struct terraria_prefix_ops_t {
    void (*early_init)(terraria_prefix_handle_t *current);
    void (*init_static)(terraria_prefix_handle_t *current);

    /**
     * @brief 判定该前缀能否附着到某物品（roll_chance 之前调用）
     */
    bool (*can_roll)(terraria_prefix_handle_t *current, patch_handle_t item_instance);

    /**
     * @brief 相对权重；<=0 表示不参与随机
     */
    float (*roll_chance)(terraria_prefix_handle_t *current, patch_handle_t item_instance);

    /**
     * @brief 属性倍率（在原版 Prefix 计算完之后叠加）
     */
    bool (*apply_stats)(terraria_prefix_handle_t *current,
                        patch_handle_t item_instance,
                        float *damage_mult,
                        float *knockback_mult,
                        float *use_time_mult,
                        float *scale_mult,
                        float *shoot_speed_mult,
                        float *mana_mult,
                        int   *crit_bonus,
                        int   *tag_damage,
                        int   *armor_penetration);

    void (*modify_value)(terraria_prefix_handle_t *current,
                         patch_handle_t item_instance,
                         float *value_mult);
} terraria_prefix_ops_t;

typedef struct terraria_prefix_handle_t {
    const char *parent_modloader_id;
    const char *parent_id;
    const char *internal_name;
    int runtime_id;                       /* 98..255 */

    terraria_prefix_category_t category;
    terraria_prefix_ops_t prefix_ops;
} terraria_prefix_handle_t;

/* ==================== 注册表 ==================== */

/**
 * @brief 注册前缀
 * @return 成功 true；槽位耗尽（>=256）返回 false
 * @warning 需在 Main.Initialize 之前调用（否则分配不到有效 runtime_id）
 */
DEFINE_FUNCTION(bool, terraria_prefix_manager_register_prefix,
                terraria_prefix_handle_t *prefix_handle)

DEFINE_FUNCTION(bool, terraria_prefix_manager_unregister_prefix_by_id, int runtime_id)

DEFINE_FUNCTION(terraria_prefix_handle_t*, terraria_prefix_manager_get_prefix, int runtime_id)

DEFINE_FUNCTION(tefstd_vector_t*, terraria_prefix_manager_get_prefixes)

DEFINE_FUNCTION(terraria_prefix_handle_t*, terraria_prefix_manager_find_prefix,
                const char* loader, const char* mod, const char* name)

/** 原版 PrefixID.Count（自定义 runtime_id 从它开始） */
DEFINE_FUNCTION(int, terraria_prefix_id_get_count)

/** 剩余可用自定义槽位（255 - Count + 1 - 已注册数） */
DEFINE_FUNCTION(int, terraria_prefix_manager_remaining_slots)


#ifdef __cplusplus
}
#endif

#endif //TEFKERNEL_PREFIX_MANAGER_H
