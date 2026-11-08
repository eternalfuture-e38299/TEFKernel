/*******************************************************************************
 * tefkernel - net_engine
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

#include "terraria/init/bootstrap.hpp"
#include "tef/patchlib/field.hpp"
#include "tef/patchlib/handle.hpp"
#include "tef/patchlib/method.hpp"
#include "terraria/netmanager.h"   // ABI：terraria_net_mod_message_cb

namespace terraria::content {
    using namespace tef;

    enum class connection_type : int {
        none = 0,
        vanilla = 1,
        tefkernel = 2,
        bad_hash = 3,
        version_mismatch = 4,
    };

    enum class net_error : int {
        none = 0,
        kick = 2,
        banned = 3,
        version = 4,
        mod_required = 5,
    };

    class net_engine final : public init::subsystem {
    public:
        static net_engine &instance() noexcept;

        [[nodiscard]] std::string_view name() const noexcept override { return "net"; }

        void early_init() override;
        void shutdown() noexcept override;

        // ===== ABI =====
        void set_mod_message_handler(terraria_net_mod_message_cb cb) noexcept;
        void send_tile_action(int x, int y, int action) const;
        void send_tile_square(int x, int y, int width, int height) const;
        void send_mod_message(const char *mod_id, const char *channel,
                              const void *data, int len) const;

        // Android 强类型 detour 的转发入口
        static bool dispatch_process_data(void *self, void **args);

        static void dispatch_send_data(void **args);

        static void dispatch_client_send(void **args);

        // 每个客户端的连接状态（原 internal/netmanager.h 的 extern 数组收进单例）
        connection_type client_connections[256] = {};
        net_error       client_errors[256] = {};
        char            client_error_details[256][1024] = {};

    private:
        net_engine() = default;

        // 版本握手
        static bool get_all_versions(std::vector<uint8_t> &out);

        static bool compare_versions(const uint8_t *remote, uint32_t remote_size,
                                     char *error_msg, size_t error_msg_size);

        // 包头
        static patchlib::handle create_tefkernel_pack();

        static patchlib::handle create_vanilla_connection_packet(int *out_size);

        static patchlib::handle create_null_connection_packet();

        // 解析
        connection_type parse_tefconnection_packet(const uint8_t *data, int len,
                                                    int client);
        static bool is_vanilla_connection_packet(const uint8_t *data, int len);

        void send_raw_packet(const uint8_t *data, int len) const;
        [[nodiscard]] int net_mode_now() const noexcept;
        void resolve_sync_methods() const;

        bool handle_mod_message(const uint8_t *data, int length, int start,
                                patch_handle_t instance) const;

        // 钩子（C ABI 签名，static 便于 patchlib_install_prepost_hook）
        static bool get_data_hook(patch_handle_t instance, void **args,
                                  const patch_method_signature_t *sig, void *result);
        static bool send_data_hook(patch_handle_t instance, void **args,
                                   const patch_method_signature_t *sig, void *result);
        static bool send_hook(patch_handle_t instance, void **args,
                              const patch_method_signature_t *sig, void *result);

        // 句柄
        patchlib::field  f_net_mode_;
        patchlib::method m_network_text_from_literal_;
        patchlib::field  f_who_am_i_;
#if !defined(__ANDROID__)
        patchlib::field  f_read_buffer_;
        patchlib::field  f_read_buffer_max_;
#endif
        mutable patchlib::method m_send_data11_;
        mutable patchlib::method m_send_tile_square_;
        mutable patchlib::method m_send_raw_;
#if defined(__ANDROID__)
        mutable patchlib::handle g_telepathy_client_;
        patchlib::method m_process_data_;
        patchlib::method m_client_send_;
#endif

        terraria_net_mod_message_cb mod_cb_ = nullptr;

        patch_hook_id_t h_send_data_ = PATCH_HOOK_INVALID_ID;
        patch_hook_id_t h_send_      = PATCH_HOOK_INVALID_ID;
        patch_hook_id_t h_get_data_  = PATCH_HOOK_INVALID_ID;
    };
} // namespace terraria::content