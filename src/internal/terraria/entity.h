/*******************************************************************************
 * tefkernel - entity
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
 * Created: 2026/11/5
 *******************************************************************************/

#ifndef TEFKERNEL_INTERNAL_ENTITY_H
#define TEFKERNEL_INTERNAL_ENTITY_H

#include <stddef.h>
#include "terraria/entity.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 计算 entity key 所需缓冲区大小
 * @param e       实体句柄
 * @param prefix  前缀（不可为 NULL）
 * @return 包含结尾 '\0' 的字节数
 * @note 格式: "<prefix>_<loader>_<mod>.<name>"，缺失字段用 "?" 占位
 */
size_t terraria_entity_key_size(const terraria_entity_handle_t *e,
                                         const char *prefix);

/**
 * @brief 生成 entity key（malloc 分配，调用方 free）
 * @param e       实体句柄
 * @param prefix  前缀（不可为 NULL）
 * @return 成功返回 key，失败返回 NULL
 * @note 格式: "<prefix>_<loader>_<mod>.<name>"，缺失字段用 "?" 占位
 */
char *terraria_entity_make_key(const terraria_entity_handle_t *e,
                                        const char *prefix);


#ifdef __cplusplus
}
#endif

#endif //TEFKERNEL_INTERNAL_ENTITY_H
