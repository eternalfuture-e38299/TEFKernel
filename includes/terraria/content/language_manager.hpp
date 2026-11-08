/*******************************************************************************
 * tefkernel - language_manager
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

#include <string_view>
#include <vector>

#include "../init/bootstrap.hpp"
#include "tef/patchlib/handle.hpp"
#include "tef/patchlib/type.hpp"
#include "tef/patchlib/field.hpp"
#include "tef/patchlib/method.hpp"
#include "terraria/language_manager.h"   // language_culture_t / 回调类型（ABI）

namespace terraria::content {
    class language_manager final : public init::subsystem {
    public:
        static language_manager &instance();

        [[nodiscard]] std::string_view name() const override { return "language"; }

        void early_init() override;
        void shutdown() override;

        // ===== 便捷 API（abi_impl 转发） =====
        [[nodiscard]] tef::patchlib::handle create_localized_text(const char *key,
                                                                  const char *value) const;
        [[nodiscard]] tef::patchlib::handle get_localized_text(const char *key) const;

        void register_callback(terraria_languager_manager_get_localized_text_t cb);
        bool unregister_callback(terraria_languager_manager_get_localized_text_t cb);

        // Windows：确保 LanguageManager 先于 LocalizedText.Empty 初始化
        static void ensure_initialized();

    private:
        language_manager() = default;

        static void load_language_postfix(patch_handle_t this_, void **args,
                                          void *result,
                                          const patch_method_signature_t *sig);

        void run_callbacks(patch_handle_t instance, int legacy_id) const;

        // ---- 句柄 ----
        tef::patchlib::type   localized_text_type_;
        tef::patchlib::method localized_text_ctor_;
        tef::patchlib::method language_get_text_;
        tef::patchlib::method load_text_json_;
        tef::patchlib::method process_copy_commands_;
        tef::patchlib::field  legacy_id_;

        // 回调列表
        std::vector<terraria_languager_manager_get_localized_text_t> callbacks_;

        patch_hook_id_t h_load_language_ = PATCH_HOOK_INVALID_ID;
    };
}