/*******************************************************************************
 * File: netmanager
 * Project: tefkernel
 * Created: 2026/10/29
 * Author: eternalfuture-e38299
 * Github: https://github.com/eternalfuture-e38299
 *
 * MIT License
 *
 * Copyright (c) 2026 eternalfuture-e38299
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *******************************************************************************/


#ifndef TEFKERNEL_NETMANAGER_H
#define TEFKERNEL_NETMANAGER_H

#include <stdint.h>

#include "../tef_api.h"

/**
 * @brief 发送一次图格操作请求（原版 MessageID 17，客户端 -> 服务端）
 *
 * 服务端权威执行（KillTile/PlaceTile/...）后广播给所有客户端。
 * 客户端调用后应立即返回，不要假设本地已生效。
 *
 * @param action 0=KillTile(掉物品) 1=PlaceTile 2=KillWall 3=PlaceWall
 *               4=KillTile(不掉物品)
 */
DEFINE_FUNCTION(void, terraria_net_send_tile_action, int x, int y, int action)

/** @brief 请求服务端同步一块区域的图格（NetMessage.SendTileSquare） */
DEFINE_FUNCTION(void, terraria_net_send_tile_square, int x, int y, int width, int height)

/**
 * @brief mod 消息回调（只在服务端触发）
 *
 * @param mod_id  发送方 Mod 的 id（注册时用的 parent_id）
 * @param channel 业务频道名（Mod 自定义）
 * @param data    负载字节
 * @param len     负载长度
 * @param player  发送方玩家索引（whoAmI）
 */
typedef void (*terraria_net_mod_message_cb)(const char *mod_id, const char *channel,
                                            const uint8_t *data, int len, int player);

/** @brief 注册 mod 消息回调（扩展/加载器初始化时调用一次） */
DEFINE_FUNCTION(void, terraria_net_set_mod_message_handler, terraria_net_mod_message_cb cb)

/**
 * @brief 客户端向服务端发送一条 Mod 消息（服务端权威架构的入口）
 *
 * 只有 Main.netMode == 1（客户端）时有效；服务端会在收到后调用已注册的
 * mod 消息回调，由对应 Mod 在服务端执行真正的逻辑。
 *
 * @param channel  业务频道名（Mod 自定义，服务端按 channel 分发）
 * @param data     负载字节（可为 NULL，len=0）
 * @param len      负载长度
 */
DEFINE_FUNCTION(void, terraria_net_send_mod_message,
                const char *mod_id, const char *channel, const void *data, int len)

#endif //TEFKERNEL_NETMANAGER_H
