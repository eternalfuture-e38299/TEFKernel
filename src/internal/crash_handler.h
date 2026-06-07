/*******************************************************************************
 * tefkernel - crash_handler
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
 * Created: 2026/5/30
 *******************************************************************************/

#ifndef TEFKERNEL_CRASH_HANDLER_H
#define TEFKERNEL_CRASH_HANDLER_H
#ifdef __cplusplus
extern "C" {
#endif

// 初始化崩溃处理器
void tefkernel_crash_handler_init(void);

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_CRASH_HANDLER_H