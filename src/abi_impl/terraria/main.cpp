/*******************************************************************************
 * tefkernel - main
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
 * Created: 2026/11/7
 *******************************************************************************/

#include "terraria/main.h"              // include_abi，不许改
#include "terraria/main_state.hpp"
#include "terraria/abi/guard.hpp"

namespace {
    using terraria::main_state;
    using terraria::abi::guard;
} // namespace

extern "C" {
bool terraria_main_is_server(void) {
    return guard([] {
        return main_state::instance().is_server();
    }, false);
}

int terraria_main_get_cur_release(void) {
    return guard([] {
        return main_state::instance().cur_release();
    }, -1);
}

patch_handle_t terraria_main_get_graphics_device(void) {
    return guard([] {
        // graphics_device() 返回 owning handle；ABI 返回裸 handle，
        // 语义是调用方接管所有权，这里 release() 交出。
        return main_state::instance().graphics_device().release();
    }, static_cast<patch_handle_t>(nullptr));
}

patch_handle_t terraria_main_get_local_player(void) {
    return guard([] {
        return main_state::instance().local_player().release();
    }, static_cast<patch_handle_t>(nullptr));
}
} // extern "C"
