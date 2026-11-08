/*******************************************************************************
 * tefkernel - prefix_registry
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

#include <array>
#include <string>
#include <string_view>

#include "entity_registry.hpp"
#include "../init/bootstrap.hpp"
#include "tef/patchlib/field.hpp"
#include "tef/patchlib/handle.hpp"
#include "tef/patchlib/method.hpp"
#include "terraria/content/prefix_manager.h"   // ABI

namespace terraria::content {
    using namespace tef;

    class prefix_registry final : public init::subsystem {
    public:
        static prefix_registry &instance() noexcept;

        [[nodiscard]] std::string_view name() const noexcept override { return "prefix"; }

        void early_init() override;
        void on_main_initialize_prefix() override;
        void on_main_initialize_postfix() override;
        void shutdown() override;

        // ===== ABI =====
        bool register_prefix(terraria_prefix_handle_t *h);
        bool unregister_by_id(int runtime_id);
        [[nodiscard]] terraria_prefix_handle_t *get(int runtime_id) const;
        [[nodiscard]] tefstd_vector_t *get_prefixes();
        [[nodiscard]] terraria_prefix_handle_t *find(std::string_view loader,
                                                      std::string_view mod,
                                                      std::string_view name) const;
        [[nodiscard]] int id_count() const;
        [[nodiscard]] int remaining_slots() const;

        // 内部
        bool assign_ids(int base);
        void resize() const;
        void init_localized_text();
        [[nodiscard]] int count() const { return int(registry_.size()); }

    private:
        prefix_registry() = default;

        // ---- resize ----
        void resize_prefix_id_sets() const;

        // ---- 本地化 ----
        static std::string build_name_key(const terraria_prefix_handle_t *h);

        // ---- 钩子（static 自由函数，安装直接传名字） ----
        static bool item_try_get_stats_prefix(patch_handle_t inst, void **args,
                                              const patch_method_signature_t *sig, void *result);
        static void item_prefix_postfix(patch_handle_t inst, void **args, void *result,
                                        const patch_method_signature_t *sig);
        static void item_rollable_postfix(patch_handle_t inst, void **args, void *result,
                                          const patch_method_signature_t *sig);

        void set_item_prefix_value(patch_handle_t inst, int v);

        // ---- 状态 ----
        entity_registry<terraria_prefix_handle_t> registry_;
        bool ids_assigned_ = false;
        int prefix_base_ = 98;

        // 引擎句柄
        patchlib::field  f_prefix_id_count_;
        patchlib::field  f_lang_prefix_;
        patchlib::field  f_item_prefix_;
        patchlib::field  f_item_type_;
        patchlib::method m_item_prefix_;
        patchlib::method m_item_reset_;
        patchlib::method m_item_roll_;
        patchlib::method m_item_try_stats_;

        // 每个自定义 id 的 LocalizedText（Android 需 gchandle 钉住）
        std::array<patchlib::handle, 256> prefix_text_{};
#if defined(__ANDROID__)
        std::array<void *, 256> prefix_text_pin_{};
#endif
    };
} // namespace terraria::content