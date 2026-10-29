/*******************************************************************************
 * tefkernel - netmanager (public)
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
 *
 * Mod 联机：客户端 → 服务端的 Mod 消息通道，以及服务端权威的图格同步辅助。
 * 实现位于 src/terraria/common/netmanager.c；这里只声明通过 TPF 导出的 API。
 *******************************************************************************/

#ifndef TEFKERNEL_TERRARIA_NETMANAGER_H
#define TEFKERNEL_TERRARIA_NETMANAGER_H

#include <stdint.h>

#include "tef_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 发送一次图格操作请求（原版 MessageID 17，客户端 → 服务端）
 *
 * 服务端权威执行（KillTile / PlaceTile / ...）后广播给所有客户端。
 * 客户端调用后应立即返回，不要假设本地已生效。
 *
 * @param action 0=KillTile（掉物品） 1=PlaceTile 2=KillWall 3=PlaceWall
 *               4=KillTile（不掉物品）
 */
DEFINE_FUNCTION(void, terraria_net_send_tile_action, int x, int y, int action)

/** @brief 请求服务端同步一块区域的图格（NetMessage.SendTileSquare） */
DEFINE_FUNCTION(void, terraria_net_send_tile_square, int x, int y, int width, int height)

/**
 * @brief Mod 消息回调（只在服务端触发）
 *
 * @param mod_id  发送方 Mod 的 id（注册时用的 parent_id）
 * @param channel 业务频道名（Mod 自定义）
 * @param data    负载字节
 * @param len     负载长度
 * @param player  发送方玩家索引（whoAmI）
 */
typedef void (*terraria_net_mod_message_cb)(const char *mod_id, const char *channel,
                                            const uint8_t *data, int len, int player);

/** @brief 注册 Mod 消息回调（扩展/加载器初始化时调用一次） */
DEFINE_FUNCTION(void, terraria_net_set_mod_message_handler, terraria_net_mod_message_cb cb)

/**
 * @brief 客户端向服务端发送一条 Mod 消息（服务端权威架构的入口）
 *
 * 只有 Main.netMode == 1（客户端）时有效；服务端收到后会调用已注册的回调。
 *
 * @param mod_id   发送方 Mod 的 id
 * @param channel  业务频道名（Mod 自定义，服务端按 channel 分发）
 * @param data     负载字节（可为 NULL，len=0）
 * @param len      负载长度
 */
DEFINE_FUNCTION(void, terraria_net_send_mod_message,
                const char *mod_id, const char *channel, const void *data, int len)

#ifdef __cplusplus
}
#endif

#endif /* TEFKERNEL_TERRARIA_NETMANAGER_H */
