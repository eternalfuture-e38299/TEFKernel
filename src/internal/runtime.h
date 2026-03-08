/*******************************************************************************
* tefkernel - runtime
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
 * Created: 2026/3/8
 *******************************************************************************/

#ifndef TEFKERNEL_RUNTIME_H
#define TEFKERNEL_RUNTIME_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {

#endif

void tefkernel_init(void);

void tefkernel_load(void);

void tefkernel_hot_reload(void);

void tefkernel_start(void);

void tefkernel_cleanup(void);

/**
 * @brief 启动热重载线程
 */
void tefkernel_start_hotreload_thread(void);

/**
 * @brief 停止热重载线程
 */
void tefkernel_stop_hotreload_thread(void);

/**
 * @brief 启用/禁用热重载
 * @param enabled 是否启用
 */
void tefkernel_set_hotreload_enabled(bool enabled);

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_RUNTIME_H
