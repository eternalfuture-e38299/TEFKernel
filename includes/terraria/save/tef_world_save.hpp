/*******************************************************************************
 * tefkernel - tef_world_save
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

#include "patchlib/method.h"
#include "../init/bootstrap.hpp"

namespace terraria::save {
    class world_save final : public init::subsystem {
    public:
        static world_save &instance() noexcept;

        [[nodiscard]] std::string_view name() const noexcept override { return "world_save"; }

        void early_init() override;
        void shutdown() override;

    private:
        world_save() = default;

        static void chest_save_postfix(patch_handle_t inst, void **args, void *result,
                                       const patch_method_signature_t *sig);
        static void chest_load_postfix(patch_handle_t inst, void **args, void *result,
                                       const patch_method_signature_t *sig);

        patch_hook_id_t h_save_ = PATCH_HOOK_INVALID_ID;
        patch_hook_id_t h_load_ = PATCH_HOOK_INVALID_ID;
    };
} // namespace terraria::save