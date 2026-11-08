/*******************************************************************************
 * tefkernel - register_all
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

#include "terraria/init/register_all.hpp"

#include "terraria/init/bootstrap.hpp"
#include "terraria/content/texture2d.hpp"
#include "terraria/content/asset_factory.hpp"
#include "terraria/content/language_manager.hpp"
#include "terraria/content/item_registry.hpp"
#include "terraria/content/npc_registry.hpp"
#include "terraria/content/projectile_registry.hpp"
#include "terraria/content/buff_registry.hpp"
#include "terraria/content/prefix_registry.hpp"
#include "terraria/content/gui_overlay.hpp"
#include "terraria/content/net_engine.hpp"
#include "terraria/content/recipe_manager.hpp"

#include "terraria/save/tef_player_save.hpp"
#include "terraria/save/tef_world_save.hpp"

namespace terraria::init {

    void register_all(const bool is_server) {
        auto &b = bootstrap::instance();
        b.set_server(is_server);

        // 顺序 = shutdown 逆序。子系统之间没有强依赖，这里按"基础设施 → 内容 → 表现 → 持久化"排。

        // texture2d 需要在 early_init 前知道 server 模式
        content::texture2d::instance().set_server(is_server);

        b.register_subsystem(&content::texture2d::instance());
        b.register_subsystem(&content::asset_factory::instance());
        b.register_subsystem(&content::language_manager::instance());
        b.register_subsystem(&content::item_registry::instance());
        b.register_subsystem(&content::recipe_manager::instance());
        // b.register_subsystem(&content::npc_registry::instance());
        // b.register_subsystem(&content::projectile_registry::instance());
        // b.register_subsystem(&content::buff_registry::instance());
        b.register_subsystem(&content::prefix_registry::instance());
        // b.register_subsystem(&content::gui_overlay::instance());
        b.register_subsystem(&content::net_engine::instance());
        b.register_subsystem(&save::player_save::instance());
        b.register_subsystem(&save::world_save::instance());

        b.run_early_init();
    }

}