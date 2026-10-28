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

#ifndef TEFKERNEL_TEF_PLAYER_SAVE_H
#define TEFKERNEL_TEF_PLAYER_SAVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 安装 SavePlayer / LoadPlayer 钩子（幂等） */
void tef_player_save_init(void);

#ifdef __cplusplus
}
#endif

#endif //TEFKERNEL_TEF_PLAYER_SAVE_H
