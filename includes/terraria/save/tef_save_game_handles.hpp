/*******************************************************************************
 * tefkernel - tef_save_game_handles
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

#include <cstdint>

#include "terraria/content/item_registry.hpp"   // engine_handles / find
#include "terraria/content/prefix_registry.hpp"

namespace terraria::save {
    using namespace tef;

    enum class container : uint8_t {
        inv = 0, misc, misc_dye, armor, dye,
        bank, bank2, bank3, bank4,
    };

    struct game_handles {
        content::item_registry::engine_handles ops;

        patchlib::field f_player_inventory, f_player_misc_equips, f_player_misc_dyes,
                        f_player_armor, f_player_dye,
                        f_player_bank, f_player_bank2, f_player_bank3, f_player_bank4;
        patchlib::field f_chest_item, f_chest_max_items;
        patchlib::field f_bank_item, f_bank_max_items;

        patchlib::method m_pfd_get_player, m_file_get_path;
        patchlib::field f_main_chest, f_main_active_world, f_main_world_list;
        patchlib::method m_main_get_player_list;
        patchlib::method m_item_ctor, m_chestitem_expand, m_chestitem_set_to_item;
        patchlib::field f_item_max_stack;

        bool inited = false;
    };

    game_handles& handles();
    void handles_init();

    // 调用无参 get_xxx() 返回 string，转 C 字符串（malloc 分配，调用方 free）
    std::string invoke_get_str(const patchlib::method& m, const patchlib::handle &inst);

    // 玩家容器 → 数组句柄（借用，调用方不要 free）
    patchlib::handle container_array(const patchlib::handle &player, container c, bool* out_is_chest);

    // 取槽位 Item（借用）
    patchlib::handle container_get_item(const patchlib::handle& arr, size_t i, bool is_chest);

    // 设置槽位为自定义物品
    void container_set_custom(const patchlib::handle& arr, size_t slot, bool is_chest,
                              int id, int stack, int prefix);

    int sanitize_chest(const patchlib::handle& chest, int max_valid);
    int sanitize_bank(const patchlib::handle& storage, int max_valid);

}