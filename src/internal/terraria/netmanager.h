/*******************************************************************************
 * tefkernel - manager
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
 * Created: 2026/4/6
 *******************************************************************************/
 
#ifndef TEFKERNEL_MANAGER_H
#define TEFKERNEL_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

// 全局状态
typedef enum {
    CONNECTION_TYPE_NONE = 0,      // 无连接
    CONNECTION_TYPE_VANILLA = 1,   // 原版客户端
    CONNECTION_TYPE_TEFKERNEL = 2, // TEFKernel客户端
    CONNECTION_TYPE_BAD_HASH = 3,  // Hash错误
    CONNECTION_TYPE_VERSION_MISMATCH = 4, // 版本不匹配
} connection_type_t;

typedef enum {
    ERROR_NONE = 0,
    ERROR_KICK = 2,      // Lang.mp[1] - "Invalid operation at this state."
    ERROR_BANNED = 3,    // Lang.mp[3] - "Banned."
    ERROR_VERSION = 4,   // Lang.mp[4] - "Wrong version."
    ERROR_MOD_REQUIRED = 5, // 自定义错误：需要TEFKernel
} error_type_t;

#define TEFKERNEL_MAGIC_STRING "Terraria With TEFKernel"
#define TEFKERNEL_VERSION_CODE 2026080800ULL

extern connection_type_t terraria_netmanager_client_connections[256];
extern error_type_t terraria_netmanager_client_errors[256];
extern char terraria_netmanager_error_details[256][1024]; // 错误信息

void terraria_netmanager_init();

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_MANAGER_H
