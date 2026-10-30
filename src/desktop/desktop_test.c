/*******************************************************************************
 * tefkernel - desktop_test
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
 * Created: 2026/1/3
 *******************************************************************************/

#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/type.h"
#include "terraria/asset.h"
#include "terraria/language_manager.h"
#include "terraria/content/prefix_manager.h"

/*
/* ---------- 语言 JSON ----------
 * key 格式与 prefix_manager.c 的 build_prefix_name_key 一致：
 *   prefix_name_<loader>_<mod>.<name>
 * 即 prefix_name_tefkernel_tefkernel.furious
 #1#

static const char *test_prefix_text(language_culture_t l) {
    if (l == LANGUAGE_ENGLISH)
        return "{"
               "\"prefix_name_tefkernel_tefkernel\": {"
               "\"furious\": \"Furious\""
               "}"
               "}";

    /* 默认（简体中文） #1#
    return "{"
           "\"prefix_name_tefkernel_tefkernel\": {"
           "\"furious\": \"狂暴的\""
           "}"
           "}";
}

/* ---------- 回调 ---------- #1#

static void test_early_init(terraria_prefix_handle_t *h) {
    (void) h;
    /* 与 item 一样：在此注册语言 JSON，供后续 init_localized_text 查 key。 #1#
    terraria_language_manager_register_callback(test_prefix_text);
    TEKLOG_INFO("prefix_test: early_init %s (language json registered)",
                h->internal_name);
}

static void test_init_static(terraria_prefix_handle_t *h) {
    TEKLOG_INFO("prefix_test: init_static %s id=%d",
                h->internal_name, h->runtime_id);
}

static bool test_can_roll(terraria_prefix_handle_t *h, patch_handle_t item) {
    (void) h;
    /* 默认允许；类别已由 category=MELEE 在内核侧过滤。 #1#
    return patchlib_is_valid(item);
}

static float test_roll_chance(terraria_prefix_handle_t *h, patch_handle_t item) {
    (void) h; (void) item;
    return 5.0f;   /* 调高便于测试 #1#
}

static bool test_apply_stats(terraria_prefix_handle_t *h,
                             patch_handle_t item,
                             float *damage_mult,
                             float *knockback_mult,
                             float *use_time_mult,
                             float *scale_mult,
                             float *shoot_speed_mult,
                             float *mana_mult,
                             int   *crit_bonus,
                             int   *tag_damage,
                             int   *armor_penetration) {
    (void) h; (void) item;
    *damage_mult       *= 1.20f;   /* 伤害 +20% #1#
    *use_time_mult     *= 0.85f;   /* 攻速 +15% #1#
    *crit_bonus        += 4;
    *knockback_mult    *= 1.10f;   /* 击退 +10% #1#
    *tag_damage        += 3;       /* 测试：+3 标记伤害 #1#
    *armor_penetration += 5;       /* 测试：+5 穿甲 #1#

    return true;
}

static void test_modify_value(terraria_prefix_handle_t *h,
                              patch_handle_t item,
                              float *value_mult) {
    (void) h; (void) item;
    *value_mult *= 1.05f;          /* 价值 +5% #1#
}

/* ---------- 句柄 ---------- #1#

static terraria_prefix_handle_t g_test_furious = {
    .parent_modloader_id = "tefkernel",
    .parent_id           = "tefkernel",
    .internal_name       = "furious",
    .runtime_id          = -1,
    .category            = TERRARIA_PREFIX_CAT_MELEE,
    .prefix_ops = {
        .early_init   = test_early_init,
        .init_static  = test_init_static,
        .can_roll     = test_can_roll,
        .roll_chance  = test_roll_chance,
        .apply_stats  = test_apply_stats,
        .modify_value = test_modify_value,
    },
};
*/

void Test(void) {
    /*if (!terraria_prefix_manager_register_prefix(&g_test_furious)) {
        TEKLOG_ERROR("prefix_test: register failed (slots full?)");
    }*/
}