/*******************************************************************************
 * tefkernel - main_state
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

#pragma once

#include "tef/patchlib/field.hpp"
#include "tef/patchlib/handle.hpp"
#include "tef/patchlib/method.hpp"

namespace terraria {
    using namespace tef;

    class main_state {
    public:
        static main_state &instance();

        void initialize(bool is_server);

        void shutdown();

        [[nodiscard]] bool is_server() const { return is_server_; }

        [[nodiscard]] int cur_release() const;

        [[nodiscard]] patchlib::handle graphics_device() const;

        [[nodiscard]] patchlib::handle local_player() const;

    private:
        main_state() = default;

        static bool initialize_prefix(patch_handle_t this_, void **args,
                                      const patch_method_signature_t *sig, void *result);

        static void initialize_postfix(patch_handle_t this_, void **args, void *result,
                                       const patch_method_signature_t *sig);

        // hook 不卸载，仅保留 id 供诊断
        patch_hook_id_t h_initialize_ = PATCH_HOOK_INVALID_ID;

        patchlib::field f_cur_release_;
        patchlib::field f_instance_;
        patchlib::method m_get_graphics_device_;
        patchlib::method m_get_local_player_;
        bool is_server_ = false;
        bool initialized_ = false;
    };
}
