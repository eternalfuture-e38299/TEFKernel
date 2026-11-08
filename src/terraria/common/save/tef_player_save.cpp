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

#include <cstdio>
#include <string>

#include "tef/core/log.hpp"
#include "tef/patchlib/struct/array.hpp"
#include "terraria/save/tef_save_format.hpp"
#include "terraria/save/tef_player_save.hpp"
#include "terraria/save/tef_save_game_handles.hpp"
#include "terraria/content/item_registry.hpp"
#include "terraria/content/prefix_registry.hpp"
#include "terraria/content/buff_registry.hpp"

namespace terraria::save {
    using namespace tef;

    player_save &player_save::instance() noexcept {
        static player_save inst;
        return inst;
    }

    // ================= 写 =================
    namespace {
        void write_items_seg(wbuf &seg, const patchlib::handle &player) {
            const auto arr_mark = wb_begin_array(seg, k_items_array);
            const auto &h = handles();

            constexpr container conts[9] = {
                container::inv, container::misc, container::misc_dye, container::armor,
                container::dye, container::bank, container::bank2, container::bank3,
                container::bank4,
            };
            const int base = terraria::content::item_registry::instance().base_count();

            for (auto c: conts) {
                bool is_chest = false;
                auto arrh = container_array(player, c, &is_chest);
                if (!arrh) continue;
                auto arr_w = patchlib::structs::array<patchlib::handle>(arrh, false);

                for (size_t i = 0; i < arr_w.length(); ++i) {
                    auto item = container_get_item(arrh, i, is_chest);
                    if (!item) continue;

                    const int type = h.ops.f_type ? h.ops.f_type.get_value<int>(item) : 0;
                    const int stack = h.ops.f_stack ? h.ops.f_stack.get_value<int>(item) : 0;
                    if (type <= 0 || stack <= 0) continue;

                    unsigned char prefix = 0;
                    if (h.ops.f_item_prefix) prefix = h.ops.f_item_prefix.get_value<unsigned char>(item);

                    const bool is_custom_item = type >= base;
                    const bool has_custom_prefix =
                            static_cast<int>(prefix) >= content::prefix_registry::instance().id_count();
                    if (!is_custom_item && !has_custom_prefix) continue;

                    std::string loader, mod, name;
                    if (is_custom_item) {
                        if (const auto *ih = content::item_registry::instance().get_item(type)) {
                            loader = ih->parent_modloader_id ? ih->parent_modloader_id : "";
                            mod = ih->parent_id ? ih->parent_id : "";
                            name = ih->internal_name ? ih->internal_name : "";
                        }
                    }
                    std::string p_loader, p_mod, p_name;
                    if (static_cast<int>(prefix) >= content::prefix_registry::instance().id_count()) {
                        if (const auto *ph = content::prefix_registry::instance().get(prefix)) {
                            p_loader = ph->parent_modloader_id ? ph->parent_modloader_id : "";
                            p_mod = ph->parent_id ? ph->parent_id : "";
                            p_name = ph->internal_name ? ph->internal_name : "";
                        }
                    }

                    const auto e = wb_begin_elem(seg);
                    wb_tlv_u8(seg, k_item_container, static_cast<uint8_t>(c));
                    wb_tlv_u32(seg, k_item_slot, static_cast<uint32_t>(i));
                    wb_tlv_u32(seg, k_item_orig_id, static_cast<uint32_t>(type));
                    wb_tlv_str(seg, k_item_loader, loader.c_str());
                    wb_tlv_str(seg, k_item_mod, mod.c_str());
                    wb_tlv_str(seg, k_item_name, name.c_str());
                    wb_tlv_u32(seg, k_item_stack, static_cast<uint32_t>(stack));
                    wb_tlv_u8(seg, k_item_prefix, prefix);
                    wb_tlv_str(seg, k_item_prefix_loader, p_loader.c_str());
                    wb_tlv_str(seg, k_item_prefix_mod, p_mod.c_str());
                    wb_tlv_str(seg, k_item_prefix_name, p_name.c_str());
                    wb_end(seg, e);
                }
            }
            wb_end_array(seg, arr_mark);
        }

        struct buff_ctx {
            wbuf *seg;
            int count;
        };

        void save_buff_line(void *user, terraria_buff_handle_t *buff, int time) {
            auto *ctx = static_cast<buff_ctx *>(user);
            if (!ctx || !ctx->seg || !buff || time <= 0) return;
            const auto e = wb_begin_elem(*ctx->seg);
            wb_tlv_str(*ctx->seg, k_buff_loader, buff->parent_modloader_id ? buff->parent_modloader_id : "?");
            wb_tlv_str(*ctx->seg, k_buff_mod, buff->parent_id ? buff->parent_id : "?");
            wb_tlv_str(*ctx->seg, k_buff_name, buff->internal_name ? buff->internal_name : "?");
            wb_tlv_u32(*ctx->seg, k_buff_time, static_cast<uint32_t>(time));
            wb_end(*ctx->seg, e);
            ctx->count++;
        }

        void write_buffs_seg(wbuf &seg, const patchlib::handle &player) {
            const auto arr_mark = wb_begin_array(seg, k_buffs_array);
            buff_ctx ctx{.seg = &seg, .count = 0};
            content::buff_registry::instance().visit_active(player.raw(), save_buff_line, &ctx);
            wb_end_array(seg, arr_mark);
        }

        void save_player_items(const patchlib::handle &player, const patchlib::handle &pfd) {
            if (!player) return;
            const auto &h = handles();
            const std::string plr = invoke_get_str(h.m_file_get_path, pfd);
            if (plr.empty()) return;
            char path[1400];
            if (!make_path(plr.c_str(), path, sizeof path)) return;

            wbuf items;
            wbuf buffs;
            write_items_seg(items, player);
            write_buffs_seg(buffs, player);

            const seg_out segs[2] = {
                {.id = k_seg_items, .data = items.data.data(), .len = static_cast<uint32_t>(items.data.size())},
                {.id = k_seg_buffs, .data = buffs.data.data(), .len = static_cast<uint32_t>(buffs.data.size())},
            };
            if (items.ok && buffs.ok && file_write(path, k_kind_player, segs, 2))
                log::debug("player_save: saved to {}", path);
        }

        // ================= 读 =================
        int resolve_prefix(const uint8_t prefix, const char *p_loader, const char *p_mod, const char *p_name) {
            int final_prefix = prefix;
            if (const int base = content::prefix_registry::instance().id_count(); static_cast<int>(prefix) >= base) {
                const terraria_prefix_handle_t *ph = nullptr;
                if (p_loader && p_loader[0] && p_mod && p_mod[0] && p_name && p_name[0])
                    ph = content::prefix_registry::instance().find(p_loader, p_mod, p_name);
                if (ph && ph->runtime_id >= 0) final_prefix = ph->runtime_id;
                else final_prefix = 0;
            }
            return final_prefix;
        }

        int resolve_item_type(const uint32_t original_id,
                              const char *loader, const char *mod, const char *name) {
            const int base = content::item_registry::instance().base_count();
            if (loader && loader[0] && mod && mod[0] && name && name[0]) {
                if (const auto *ih = content::item_registry::instance().find(loader, mod, name);
                    ih && ih->runtime_id >= 0)
                    return ih->runtime_id;
                const auto *unk = content::item_registry::instance().unknown_item();
                return unk ? unk->runtime_id : -1;
            }
            if (static_cast<int>(original_id) > 0 && static_cast<int>(original_id) < base)
                return static_cast<int>(
                    original_id);
            if (static_cast<int>(original_id) >= base) {
                if (const auto *ih = content::item_registry::instance().get_item(static_cast<int>(original_id));
                    ih && ih->runtime_id == static_cast<int>(original_id))
                    return static_cast<int>(original_id);
            }
            const auto *unk = content::item_registry::instance().unknown_item();
            return unk ? unk->runtime_id : -1;
        }

        void apply_item(const patchlib::handle &player, uint8_t cont, const uint32_t slot,
                        const uint32_t original_id, const char *loader, const char *mod,
                        const char *name, const uint32_t stack, const uint8_t prefix,
                        const char *p_loader, const char *p_mod, const char *p_name) {
            bool is_chest = false;
            const auto arr = container_array(player, static_cast<container>(cont), &is_chest);
            if (!arr) return;
            if (const auto arr_w = patchlib::structs::array<patchlib::handle>(arr, false); slot >= arr_w.length())
                return;

            const int type = resolve_item_type(original_id, loader, mod, name);
            if (type <= 0) return;
            const int final_prefix = resolve_prefix(prefix, p_loader, p_mod, p_name);
            container_set_custom(arr, slot, is_chest, type, static_cast<int>(stack), final_prefix);
        }

        void load_items_seg(rd &seg, const patchlib::handle &player) {
            tlv t;
            while (rd_next(seg, t)) {
                if (t.tag != k_items_array) continue;
                rd elems;
                uint32_t count;
                if (!rd_array_begin(t, elems, &count)) continue;

                for (uint32_t i = 0; i < count; ++i) {
                    tlv obj;
                    if (!rd_next(elems, obj)) break;
                    if (obj.type != static_cast<uint8_t>(tlv_type::object)) continue;

                    rd os;
                    rd_enter(obj, os);
                    uint8_t cont = 0, prefix = 0;
                    uint32_t slot = 0, stack = 0, original_id = 0;
                    const char *loader = nullptr, *mod = nullptr, *name = nullptr;
                    const char *p_loader = nullptr, *p_mod = nullptr, *p_name = nullptr;

                    tlv f;
                    while (rd_next(os, f)) {
                        switch (f.tag) {
                            case k_item_container: cont = rd_u8(f);
                                break;
                            case k_item_slot: slot = rd_u32(f);
                                break;
                            case k_item_orig_id: original_id = rd_u32(f);
                                break;
                            case k_item_loader: loader = rd_str(f);
                                break;
                            case k_item_mod: mod = rd_str(f);
                                break;
                            case k_item_name: name = rd_str(f);
                                break;
                            case k_item_stack: stack = rd_u32(f);
                                break;
                            case k_item_prefix: prefix = rd_u8(f);
                                break;
                            case k_item_prefix_loader: p_loader = rd_str(f);
                                break;
                            case k_item_prefix_mod: p_mod = rd_str(f);
                                break;
                            case k_item_prefix_name: p_name = rd_str(f);
                                break;
                            default: break;
                        }
                    }
                    if (stack > 0)
                        apply_item(player, cont, slot, original_id, loader, mod, name,
                                   stack, prefix, p_loader, p_mod, p_name);
                }
            }
        }

        void load_buffs_seg(rd &seg, const patchlib::handle &player) {
            tlv t;
            while (rd_next(seg, t)) {
                if (t.tag != k_buffs_array) continue;
                rd elems;
                uint32_t count;
                if (!rd_array_begin(t, elems, &count)) continue;

                for (uint32_t i = 0; i < count; ++i) {
                    tlv obj;
                    if (!rd_next(elems, obj)) break;
                    if (obj.type != static_cast<uint8_t>(tlv_type::object)) continue;

                    rd os;
                    rd_enter(obj, os);
                    const char *loader = nullptr, *mod = nullptr, *name = nullptr;
                    uint32_t time = 0;
                    tlv f;
                    while (rd_next(os, f)) {
                        switch (f.tag) {
                            case k_buff_loader: loader = rd_str(f);
                                break;
                            case k_buff_mod: mod = rd_str(f);
                                break;
                            case k_buff_name: name = rd_str(f);
                                break;
                            case k_buff_time: time = rd_u32(f);
                                break;
                            default: break;
                        }
                    }
                    if (!loader || !mod || !name || time == 0) continue;
                    if (auto *bh = content::buff_registry::instance().find(loader, mod, name))
                        content::buff_registry::instance().add(bh, player.raw(), static_cast<int>(time));
                }
            }
        }

        void sanitize_player_banks(const patchlib::handle &player) {
            if (!player) return;
            const auto &h = handles();
            patchlib::field banks[4] = {
                h.f_player_bank, h.f_player_bank2,
                h.f_player_bank3, h.f_player_bank4
            };
            const int max_valid = content::item_registry::instance().count();
            int changed = 0;
            for (auto &b: banks) {
                if (!b) continue;
                auto chest = b.get_value<patchlib::handle>(player);
                if (!chest) continue;
                if (h.f_bank_item) changed += sanitize_bank(chest, max_valid);
                else changed += sanitize_chest(chest, max_valid);
            }
            if (changed > 0)
                log::info("player_save: replaced {} stale item(s) in banks", changed);
        }

        void load_player_items(const patchlib::handle &player, const patchlib::handle &pfd) {
            if (!player) return;
            const auto &h = handles();
            const std::string plr = invoke_get_str(h.m_file_get_path, pfd);
            if (plr.empty()) return;
            char path[1400];
            if (!make_path(plr.c_str(), path, sizeof path)) return;

            file f;
            if (!file_open(path, f)) return;
            if (f.kind != k_kind_player) {
                file_close(f);
                return;
            }

            for (int i = 0; i < f.seg_count; ++i) {
                rd seg = file_seg(f, i);
                switch (f.segs[i].id) {
                    case k_seg_items: load_items_seg(seg, player);
                        break;
                    case k_seg_buffs: load_buffs_seg(seg, player);
                        break;
                    default: break;
                }
            }
            file_close(f);
        }

        void delete_tef_for_list(const patchlib::handle &list, const int index) {
            if (!list || index < 0) return;
            const auto arr_w = patchlib::structs::array<patchlib::handle>(list, false);
            if (static_cast<size_t>(index) >= arr_w.length()) return;
            const auto data = arr_w.at(static_cast<size_t>(index));
            if (!data) return;
            const auto &h = handles();
            const std::string path = invoke_get_str(h.m_file_get_path, data);
            if (path.empty()) return;
            char tef[1400];
            if (make_path(path.c_str(), tef, sizeof tef)) std::remove(tef);
        }
    } // namespace

    // ================= 生命周期 =================
    void player_save::early_init() {
        handles_init();

        const auto player_cls = patchlib::type("Terraria", "Player");
        if (!player_cls) {
            log::warn("player_save: Terraria.Player not found");
            return;
        }

        if (const auto m = player_cls.get_method("SavePlayer", 3))
            h_save_ = patchlib_install_prepost_hook(m.raw(), nullptr, save_postfix);
        if (const auto m = player_cls.get_method("LoadPlayer", 2))
            h_load_ = patchlib_install_prepost_hook(m.raw(), nullptr, load_postfix);

        if (const auto main_cls = patchlib::type("Terraria", "Main")) {
            if (const auto m = main_cls.get_method("ErasePlayer", 1))
                h_erase_player_ = patchlib_install_prepost_hook(m.raw(), erase_player_prefix, nullptr);
            if (const auto m = main_cls.get_method("EraseWorld", 1))
                h_erase_world_ = patchlib_install_prepost_hook(m.raw(), erase_world_prefix, nullptr);
        }
    }

    void player_save::shutdown() {
        // hook 不卸载（进程生命周期）
    }

    // ================= 钩子 =================
    void player_save::save_postfix(patch_handle_t, void **args, void *, const patch_method_signature_t *) {
        if (!args || !args[0]) return;
        const auto &h = handles();
        const patchlib::handle pfd(*static_cast<patch_handle_t *>(args[0]), false);
        if (!pfd || !h.m_pfd_get_player) return;
        const auto player = h.m_pfd_get_player.invoke<patchlib::handle>(pfd);
        save_player_items(player, pfd);
    }

    void player_save::load_postfix(patch_handle_t, void **, void *result, const patch_method_signature_t *) {
        if (!result) return;
        const auto &h = handles();
        const patchlib::handle pfd(*static_cast<patch_handle_t *>(result), false);
        if (!pfd || !h.m_pfd_get_player) return;
        const auto player = h.m_pfd_get_player.invoke<patchlib::handle>(pfd);
        load_player_items(player, pfd);
        sanitize_player_banks(player);
    }

    bool player_save::erase_player_prefix(patch_handle_t, void **args, const patch_method_signature_t *, void *) {
        if (!args || !args[0]) return false;
        const auto &h = handles();
        if (!h.m_main_get_player_list) return false;
        const auto list = h.m_main_get_player_list.invoke<patchlib::handle>();
        delete_tef_for_list(list, *static_cast<int *>(args[0]));
        return false;
    }

    bool player_save::erase_world_prefix(patch_handle_t, void **args, const patch_method_signature_t *, void *) {
        if (!args || !args[0]) return false;
        const auto &h = handles();
        if (!h.f_main_world_list) return false;
        const auto list = h.f_main_world_list.get_value<patchlib::handle>();
        delete_tef_for_list(list, *static_cast<int *>(args[0]));
        return false;
    }
}
