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

#ifndef TEFKERNEL_TEF_SAVE_GAME_HANDLES_H
#define TEFKERNEL_TEF_SAVE_GAME_HANDLES_H

#include <stdint.h>
#include <stdbool.h>

#include "internal/terraria/item_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 玩家容器枚举（存入 ITEMS 段 TEF_SAVE_ITEM_CONTAINER） */
typedef enum : uint8_t {
  TEF_SAVE_CONT_INV      = 0,
  TEF_SAVE_CONT_MISC     = 1,
  TEF_SAVE_CONT_MISCDYE  = 2,
  TEF_SAVE_CONT_ARMOR    = 3,
  TEF_SAVE_CONT_DYE      = 4,
  TEF_SAVE_CONT_BANK     = 5,
  TEF_SAVE_CONT_BANK2    = 6,
  TEF_SAVE_CONT_BANK3    = 7,
  TEF_SAVE_CONT_BANK4    = 8,
} tef_save_container_t;

/* 句柄结构（由 init 填充） */
typedef struct {
  terraria_item_handles_t ops;

  patch_handle_t f_player_inventory;
  patch_handle_t f_player_misc_equips;
  patch_handle_t f_player_misc_dyes;
  patch_handle_t f_player_armor;
  patch_handle_t f_player_dye;
  patch_handle_t f_player_bank;
  patch_handle_t f_player_bank2;
  patch_handle_t f_player_bank3;
  patch_handle_t f_player_bank4;

  patch_handle_t f_chest_item;
  patch_handle_t f_chest_max_items;

  patch_handle_t m_pfd_get_player;
  patch_handle_t m_file_get_path;

  patch_handle_t f_main_chest;
  patch_handle_t f_main_active_world;

  patch_handle_t m_item_ctor;
  patch_handle_t m_chestitem_expand;
  patch_handle_t m_chestitem_set_to_item;
} tef_save_game_handles_t;

/* 单例 */
tef_save_game_handles_t *tef_save_game_handles(void);

/* 初始化（player/world save init 都会调用；内部幂等） */
void tef_save_game_handles_init(void);

/* 通用：调用无参 get_xxx() 返回 string，转 C 字符串（loader 分配，内核不释放） */
char *tef_save_invoke_get_str(patch_handle_t method, patch_handle_t instance);

/* 通用：玩家容器 → 数组句柄（调用方负责 patchlib_free） */
patch_handle_t tef_save_container_array(patch_handle_t player, uint8_t cont, bool *out_is_chest);

/* 通用：取槽位 Item（勿释放） */
patch_handle_t tef_save_container_get_item(patch_handle_t arr, size_t i, bool is_chest);

/* 通用：设置槽位为自定义物品 */
void tef_save_container_set_custom(patch_handle_t arr, size_t slot, bool is_chest,
                              int id, int stack, int prefix);

/* 通用：清理箱子中越界物品（返回改动数） */
int tef_save_sanitize_chest(patch_handle_t chest, int max_valid);

#ifdef __cplusplus
}
#endif

#endif //TEFKERNEL_TEF_SAVE_GAME_HANDLES_H
