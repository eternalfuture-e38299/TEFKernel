/*******************************************************************************
 * tefkernel - prefix_manager (internal)
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
 * Created: 2026/10/30
 *******************************************************************************/

#ifndef TEFKERNEL_INTERNAL_PREFIX_MANAGER_H
#define TEFKERNEL_INTERNAL_PREFIX_MANAGER_H

#include "terraria/content/prefix_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 注册表：terraria_prefix_handle_t*，按 (loader, mod, name) 排序后分配 runtime_id */
extern tefstd_vector_t g_terraria_prefix_registry;

/**
 * @brief 初始化：解析 PrefixID.Count / Lang.prefix / Item 字段与方法，
 *        安装 Prefix / ResetStats / GetRollablePrefixes / SetLanguage 钩子，
 *        并把 Lang.prefix 扩容到 256。
 */
void terraria_prefix_manager_init(void);

/**
 * @brief 销毁：卸载钩子、释放 Android 侧 LocalizedText pin、销毁注册表。
 */
void terraria_prefix_manager_destroy(void);

void terraria_prefix_manager_resize();

/**
 * @brief 分配 runtime_id（base_id 起紧凑分配，按 loader/mod/name 排序）
 * @param base_id 通常传 terraria_prefix_id_get_count()
 * @return 成功 true；超出 byte 255 上限返回 false
 */
bool terraria_prefix_manager_assign_ids(int base_id);

/**
 * @brief 填充 Lang.prefix（参照 item_manager 的 init_localized_text）
 * @note 语言就绪后调用；语言切换后可再次调用。幂等。
 */
void terraria_prefix_manager_init_localized_text(void);


#ifdef __cplusplus
}
#endif

#endif /* TEFKERNEL_INTERNAL_PREFIX_MANAGER_H */