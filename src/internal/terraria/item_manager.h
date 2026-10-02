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


#ifndef TEFKERNEL_INTERNAL_ITEM_MANAGER_H
#define TEFKERNEL_INTERNAL_ITEM_MANAGER_H

#include "terraria/item_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

extern tefstd_vector_t g_terraria_item_registry; // 存储 terraria_item_handle_t*

void terraria_item_manager_init();

void terraria_item_manager_destroy();

bool terraria_item_manager_assign_ids(int base_id);

void terraria_item_manager_init_localized_text();

/**
 * @brief 获取物品数量
 * @return 原版物品数量
 */
int terraria_item_id_get_count();

/**
 * @brief 自定义物品常用的 Item 字段/方法句柄（供存档等模块使用）
 */
typedef struct {
  patch_handle_t f_type;         // Item.type (int)
  patch_handle_t f_stack;        // Item.stack (int)
  patch_handle_t f_item_prefix;  // Item.prefix (byte)
  patch_handle_t m_net_defaults; // Item.netDefaults(int)
  patch_handle_t m_prefix;       // Item.Prefix(int)
} terraria_item_handles_t;

void terraria_item_manager_get_item_ops(terraria_item_handles_t *out);

/**
 * @brief 按稳定字符串键 (loader/mod/internal_name) 查找自定义物品
 */
terraria_item_handle_t *terraria_item_manager_find_item(const char *loader,
                                                        const char *mod,
                                                        const char *name);

/**
 * @brief 获取内核预留的“未知物品”占位句柄
 */
terraria_item_handle_t *terraria_item_manager_unknown_item();

void terraria_item_manager_init_texture2d();

// 扩容游戏中的数组以支持物品
void terraria_item_manager_resize();

/**
 * @brief 获取原版物品+新物品的数量
 */
int terraria_item_manager_get_count();

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_INTERNAL_ITEM_MANAGER_H