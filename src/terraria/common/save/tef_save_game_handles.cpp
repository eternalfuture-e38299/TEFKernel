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

#include "terraria/save/tef_save_game_handles.hpp"

#include "tef/patchlib/struct/array.hpp"
#include "tef/patchlib/struct/string.hpp"

#if defined(__ANDROID__)
#include "abi_impl/patchlib/il2cpp_api.h"
#endif

namespace terraria::save {
    using namespace tef;

    namespace {
        game_handles g_h;
    }

    game_handles &handles() { return g_h; }

    std::string invoke_get_str(const patchlib::method &m, const patchlib::handle &inst) {
        if (!m || !inst) return {};
        const auto s = m.invoke<patchlib::handle>(inst);
        if (!s) return {};
        auto cstr = patchlib::structs::string(s).to_string();
        return cstr;
    }

    patchlib::handle container_array(const patchlib::handle &player, container c, bool *out_is_chest) {
        patchlib::field *fields[9] = {
            &g_h.f_player_inventory, &g_h.f_player_misc_equips, &g_h.f_player_misc_dyes,
            &g_h.f_player_armor, &g_h.f_player_dye,
            &g_h.f_player_bank, &g_h.f_player_bank2, &g_h.f_player_bank3, &g_h.f_player_bank4,
        };
        const int idx = static_cast<int>(c);
        if (idx < 0 || idx > 8) return {};
        const auto *f = fields[idx];
        if (!*f) return {};
        auto v = f->get_value<patchlib::handle>(player);
        if (!v) return {};

        if (idx < static_cast<int>(container::bank)) {
            if (out_is_chest) *out_is_chest = false;
            return v;
        }
        if (g_h.f_bank_item) {
            auto items = g_h.f_bank_item.get_value<patchlib::handle>(v);
            if (out_is_chest) *out_is_chest = false;
            return items;
        }
        if (out_is_chest) *out_is_chest = true;
        return g_h.f_chest_item.get_value<patchlib::handle>(v);
    }

    patchlib::handle container_get_item(const patchlib::handle &arr, const size_t i, const bool is_chest) {
#if defined(__ANDROID__)
        if (is_chest && g_h.m_chestitem_expand) {
            unsigned char buf[256] = {};
            if (!patchlib_array_at(arr.raw(), i, buf)) return {};
            patchlib::handle item;
            g_h.m_chestitem_expand.invoke<void>(item); // 实际需要 dvalue，见下
            return item;
        }
#else
        (void) is_chest;
#endif
        const auto arr_w = patchlib::structs::array<patchlib::handle>(arr, false);
        if (i >= arr_w.length()) return {};
        return arr_w.at(i);
    }

    namespace {
        void clamp_stack(const patchlib::handle &item, int *stack) {
            if (*stack < 1 || !g_h.f_item_max_stack) return;
            if (const int maxs = g_h.f_item_max_stack.get_value<int>(item); maxs > 0 && *stack > maxs) *stack = maxs;
        }
    }

    void container_set_custom(const patchlib::handle &arr, const size_t slot, const bool is_chest,
                              const int id, int stack, const int prefix) {
#if defined(__ANDROID__)
        if (is_chest && g_h.m_item_ctor && g_h.m_chestitem_set_to_item) {
            patchlib::handle item = g_h.m_item_ctor.invoke_ctor(id, patchlib::handle{});
            if (!item) return;
            clamp_stack(item, &stack);
            if (stack > 0) g_h.ops.f_stack.set_value(item, stack);
            if (prefix > 0 && g_h.ops.m_prefix) g_h.ops.m_prefix.invoke<void>(item, prefix);

            unsigned char buf[256] = {};
            if (!patchlib_array_at(arr.raw(), slot, buf)) return;
            // SetToItem(buf, item)
            void *sa[1] = {&item};

            patchlib_method_invoke_args(g_h.m_chestitem_set_to_item.raw(), buf, nullptr, sa);
            patchlib_array_set(arr.raw(), slot, buf);
            return;
        }
#else
        (void) is_chest;
#endif
        const auto arr_w = patchlib::structs::array<patchlib::handle>(arr, false);
        if (slot >= arr_w.length()) return;
        const auto item = arr_w.at(slot);
        if (!item) return;
        const int iid = id;
        g_h.ops.m_net_defaults.invoke<void>(item, iid);
        clamp_stack(item, &stack);
        if (stack > 0) g_h.ops.f_stack.set_value(item, stack);
        if (prefix > 0 && g_h.ops.m_prefix) g_h.ops.m_prefix.invoke<void>(item, prefix);
    }

    int sanitize_chest(const patchlib::handle &chest, const int max_valid) {
        if (!chest || !g_h.f_chest_item || !g_h.ops.f_type) return 0;
        const auto *unk = content::item_registry::instance().unknown_item();
        const int unk_id = unk ? unk->runtime_id : -1;
        if (unk_id < 0) return 0;

        const int max_items = g_h.f_chest_max_items ? g_h.f_chest_max_items.get_value<int>(chest) : 0;
        const auto items = g_h.f_chest_item.get_value<patchlib::handle>(chest);
        if (!items) return 0;
        const auto items_w = patchlib::structs::array<patchlib::handle>(items, false);
        size_t lim = items_w.length();
        if (max_items > 0 && static_cast<size_t>(max_items) < lim) lim = static_cast<size_t>(max_items);

        int changed = 0;
        for (size_t i = 0; i < lim; ++i) {
            auto item = items_w.at(i);
            if (!item) continue;
            if (const int type = g_h.ops.f_type.get_value<int>(item); type >= max_valid) {
                container_set_custom(items, i, true, unk_id, 1, 0);
                changed++;
            }
        }
        return changed;
    }

    int sanitize_bank(const patchlib::handle &storage, const int max_valid) {
        if (!storage || !g_h.f_bank_item || !g_h.ops.f_type) return 0;
        const auto *unk = content::item_registry::instance().unknown_item();
        const int unk_id = unk ? unk->runtime_id : -1;
        if (unk_id < 0) return 0;

        const int max_items = g_h.f_bank_max_items ? g_h.f_bank_max_items.get_value<int>(storage) : 0;
        const auto items = g_h.f_bank_item.get_value<patchlib::handle>(storage);
        if (!items) return 0;
        const auto items_w = patchlib::structs::array<patchlib::handle>(items, false);
        size_t lim = items_w.length();
        if (max_items > 0 && static_cast<size_t>(max_items) < lim) lim = static_cast<size_t>(max_items);

        int changed = 0;
        for (size_t i = 0; i < lim; ++i) {
            auto item = items_w.at(i);
            if (!item) continue;
            if (const int type = g_h.ops.f_type.get_value<int>(item); type >= max_valid) {
                container_set_custom(items, i, false, unk_id, 1, 0);
                changed++;
            }
        }
        return changed;
    }

    void handles_init() {
        if (g_h.inited) return;
        g_h.inited = true;

        g_h.ops = content::item_registry::instance().engine();

        if (const auto player = patchlib::type("Terraria", "Player")) {
            g_h.f_player_inventory = player.get_field("inventory");
            g_h.f_player_misc_equips = player.get_field("miscEquips");
            g_h.f_player_misc_dyes = player.get_field("miscDyes");
            g_h.f_player_armor = player.get_field("armor");
            g_h.f_player_dye = player.get_field("dye");
            g_h.f_player_bank = player.get_field("bank");
            g_h.f_player_bank2 = player.get_field("bank2");
            g_h.f_player_bank3 = player.get_field("bank3");
            g_h.f_player_bank4 = player.get_field("bank4");
        }
        if (const auto chest = patchlib::type("Terraria", "Chest")) {
            g_h.f_chest_item = chest.get_field("item");
            g_h.f_chest_max_items = chest.get_field("maxItems");
        }
        if (const auto storage = patchlib::type("Terraria", "InventoryStorage")) {
            g_h.f_bank_item = storage.get_field("item");
            g_h.f_bank_max_items = storage.get_field("maxItems");
        }
        if (const auto item = patchlib::type("Terraria", "Item")) {
            g_h.m_item_ctor = item.get_method(".ctor", 2);
            g_h.f_item_max_stack = item.get_field("maxStack");
        }
        if (const auto ci = patchlib::type("Terraria", "ChestItem")) {
            g_h.m_chestitem_expand = ci.get_method("ExpandItem", 0);
            g_h.m_chestitem_set_to_item = ci.get_method("SetToItem", 1);
        }
        if (const auto fd = patchlib::type("Terraria.IO", "FileData"))
            g_h.m_file_get_path = fd.get_method("get_Path", 0);
        if (const auto pfd = patchlib::type("Terraria.IO", "PlayerFileData"))
            g_h.m_pfd_get_player = pfd.get_method("get_Player", 0);
        if (const auto main = patchlib::type("Terraria", "Main")) {
            g_h.f_main_chest = main.get_field("chest");
            g_h.f_main_active_world = main.get_field("ActiveWorldFileData");
            g_h.f_main_world_list = main.get_field("WorldList");
            g_h.m_main_get_player_list = main.get_method("get_PlayerList", 0);
        }
    }
}
