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
 *******************************************************************************/

#ifndef TEFKERNEL_INTERNAL_ITEM_SAVE_H
#define TEFKERNEL_INTERNAL_ITEM_SAVE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化自定义物品存档（解析 Player 字段并挂载 SavePlayer/LoadPlayer）
 * @note 需在 terraria_item_manager_init() 之后调用
 */
void terraria_item_save_init();

#ifdef __cplusplus
}
#endif

#endif // TEFKERNEL_INTERNAL_ITEM_SAVE_H
