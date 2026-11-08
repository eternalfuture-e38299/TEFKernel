/*******************************************************************************
 * tefkernel - netmanager
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
 * Created: 2026/11/8
 *******************************************************************************/

#include "terraria/netmanager.h"                       // include_abi，不许改
#include "terraria/content/net_engine.hpp"
#include "terraria/abi/guard.hpp"

namespace {
    using terraria::abi::guard_void;
    using terraria::content::net_engine;
} // namespace

extern "C" {

void terraria_net_send_tile_action(int x, int y, int action) {
    guard_void([&] { net_engine::instance().send_tile_action(x, y, action); });
}

void terraria_net_send_tile_square(int x, int y, int width, int height) {
    guard_void([&] { net_engine::instance().send_tile_square(x, y, width, height); });
}

void terraria_net_set_mod_message_handler(terraria_net_mod_message_cb cb) {
    guard_void([&] { net_engine::instance().set_mod_message_handler(cb); });
}

void terraria_net_send_mod_message(const char *mod_id, const char *channel,
                                   const void *data, int len) {
    guard_void([&] { net_engine::instance().send_mod_message(mod_id, channel, data, len); });
}

} // extern "C"