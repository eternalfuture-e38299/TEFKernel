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

#include "terraria/main_state.hpp"

#include "tef/patchlib/property.hpp"
#include "tef/patchlib/type.hpp"
#include "terraria/init/bootstrap.hpp"
#include "terraria/init/register_all.hpp"

namespace terraria {
    main_state &main_state::instance() {
        static main_state inst;
        return inst;
    }

    void main_state::initialize(const bool is_server) {
        if (initialized_) return;
        initialized_ = true;
        is_server_ = is_server;

        const auto main_cls = patchlib::type("Terraria", "Main");
        if (!main_cls) return;

        f_cur_release_ = main_cls.get_field("curRelease");
        f_instance_ = main_cls.get_field("instance");

        const auto game_cls = is_server
                                  ? patchlib::type("Terraria.Server", "Game")
                                  : patchlib::type("Microsoft.Xna.Framework", "Game");
        if (game_cls)
            m_get_graphics_device_ = game_cls.get_method("get_GraphicsDevice", 0);

        if (const auto lp_prop = main_cls.get_property("LocalPlayer"))
            m_get_local_player_ = lp_prop.get_get_method();

        if (const auto m = main_cls.get_method("Initialize", 0)) {
            h_initialize_ = patchlib_install_prepost_hook(
                m.raw(),
                initialize_prefix,
                initialize_postfix);
        }

        /* 内容/贴图/静态表在 Main.Initialize 结束时还没准备好，必须等
         * Initialize_AlmostEverything 之后再注入；否则会黑屏或贴图丢失。
         * 这与重构前的 hook 时机保持一致。 */
        if (const auto m = main_cls.get_method("Initialize_AlmostEverything", 0)) {
            h_almost_everything_ = patchlib_install_prepost_hook(
                m.raw(),
                nullptr,
                almost_everything_postfix);
        }

        init::register_all(is_server);
    }

    bool main_state::initialize_prefix(patch_handle_t, void **, const patch_method_signature_t *, void *) {
        init::bootstrap::instance().run_main_initialize_prefix();
        return false;
    }

    void main_state::initialize_postfix(patch_handle_t, void **, void *, const patch_method_signature_t *) {
        /* Main.Initialize 结束时贴图/内容表还没就绪，这里不做注入。
         * 真正的 resize / init_texture2d / init_static 放到
         * Initialize_AlmostEverything 之后执行。 */
    }

    void main_state::almost_everything_postfix(patch_handle_t, void **, void *, const patch_method_signature_t *) {
        init::bootstrap::instance().run_main_initialize_postfix();
    }

    void main_state::shutdown() {
        init::bootstrap::instance().run_shutdown();
        f_cur_release_.reset();
        f_instance_.reset();
        m_get_graphics_device_.reset();
        m_get_local_player_.reset();
        initialized_ = false;
    }

    // 供 ABI 转发读取
    int main_state::cur_release() const {
        const int v = f_cur_release_;
        return v;
    }

    patchlib::handle main_state::graphics_device() const {
        if (!m_get_graphics_device_) return {};
        const auto inst = f_instance_.get_value<patchlib::handle>();
        if (!inst) return {};
        auto gd = m_get_graphics_device_[inst].invoke<patchlib::handle>();
        return gd;
    }

    patchlib::handle main_state::local_player() const {
        if (!m_get_local_player_) return {};
        auto p = m_get_local_player_.invoke<patchlib::handle>();
        return p;
    }
}
