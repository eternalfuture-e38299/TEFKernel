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
 * Created: 2026/11/8
 *******************************************************************************/

#pragma once

#include <string_view>

#include "../init/bootstrap.hpp"
#include "patchlib/method.h"

namespace terraria::save {
    class player_save final : public init::subsystem {
    public:
        static player_save &instance() noexcept;

        [[nodiscard]] std::string_view name() const noexcept override { return "player_save"; }

        void early_init() override;        // 装 SavePlayer / LoadPlayer / ErasePlayer / EraseWorld 钩子
        void shutdown() override;

    private:
        player_save() = default;

        static void save_postfix(patch_handle_t inst, void **args, void *result,
                                 const patch_method_signature_t *sig);
        static void load_postfix(patch_handle_t inst, void **args, void *result,
                                 const patch_method_signature_t *sig);
        static bool erase_player_prefix(patch_handle_t inst, void **args,
                                        const patch_method_signature_t *sig, void *result);
        static bool erase_world_prefix(patch_handle_t inst, void **args,
                                       const patch_method_signature_t *sig, void *result);

        patch_hook_id_t h_save_ = PATCH_HOOK_INVALID_ID;
        patch_hook_id_t h_load_ = PATCH_HOOK_INVALID_ID;
        patch_hook_id_t h_erase_player_ = PATCH_HOOK_INVALID_ID;
        patch_hook_id_t h_erase_world_ = PATCH_HOOK_INVALID_ID;
    };
}