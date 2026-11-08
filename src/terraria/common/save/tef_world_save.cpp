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

#include "terraria/save/tef_world_save.hpp"

#include <cstdio>
#include <string>

#include "tef/core/log.hpp"
#include "tef/patchlib/struct/array.hpp"
#include "terraria/save/tef_save_format.hpp"
#include "terraria/save/tef_save_game_handles.hpp"
#include "terraria/content/item_registry.hpp"
#include "terraria/content/prefix_registry.hpp"

namespace terraria::save {
    using namespace tef;

    world_save &world_save::instance() noexcept {
        static world_save inst;
        return inst;
    }

    namespace {
        std::string world_tef_save_base() {
            const auto &h = handles();
            if (!h.f_main_active_world) return {};
            const auto wfd = h.f_main_active_world.get_value<patchlib::handle>();
            if (!wfd) return {};
            return invoke_get_str(h.m_file_get_path, wfd);
        }

        void write_chests_seg(wbuf &seg) {
            const auto arr_mark = wb_begin_array(seg, k_chests_array);
            const auto &h = handles();
            if (!h.f_main_chest || !h.f_chest_item) {
                wb_end_array(seg, arr_mark);
                return;
            }

            const auto chests = h.f_main_chest.get_value<patchlib::handle>();
            if (!chests) {
                wb_end_array(seg, arr_mark);
                return;
            }

            const int base = content::item_registry::instance().base_count();
            const auto chests_w = patchlib::structs::array<patchlib::handle>(chests, false);

            for (size_t ci = 0; ci < chests_w.length(); ++ci) {
                auto chest = chests_w.at(ci);
                if (!chest) continue;

                const int max_items = h.f_chest_max_items ? h.f_chest_max_items.get_value<int>(chest) : 0;
                auto items = h.f_chest_item.get_value<patchlib::handle>(chest);
                if (!items) continue;
                auto items_w = patchlib::structs::array<patchlib::handle>(items, false);
                size_t lim = items_w.length();
                if (max_items > 0 && static_cast<size_t>(max_items) < lim) lim = static_cast<size_t>(max_items);

                for (size_t si = 0; si < lim; ++si) {
                    auto item = container_get_item(items, si, true);
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
                    wb_tlv_u32(seg, k_chest_index, static_cast<uint32_t>(ci));
                    wb_tlv_u32(seg, k_chest_slot, static_cast<uint32_t>(si));
                    wb_tlv_u32(seg, k_chest_orig_id, static_cast<uint32_t>(type));
                    wb_tlv_str(seg, k_chest_loader, loader.c_str());
                    wb_tlv_str(seg, k_chest_mod, mod.c_str());
                    wb_tlv_str(seg, k_chest_name, name.c_str());
                    wb_tlv_u32(seg, k_chest_stack, static_cast<uint32_t>(stack));
                    wb_tlv_u8(seg, k_chest_prefix, prefix);
                    wb_tlv_str(seg, k_chest_prefix_loader, p_loader.c_str());
                    wb_tlv_str(seg, k_chest_prefix_mod, p_mod.c_str());
                    wb_tlv_str(seg, k_chest_prefix_name, p_name.c_str());
                    wb_end(seg, e);
                }
            }
            wb_end_array(seg, arr_mark);
        }

        void save_world_chests() {
            const auto wld = world_tef_save_base();
            if (wld.empty()) return;
            char path[1400];
            if (!make_path(wld.c_str(), path, sizeof path)) return;

            wbuf chests;
            wbuf res;
            write_chests_seg(chests);

            const seg_out segs[2] = {
                {.id = k_seg_chests, .data = chests.data.data(), .len = static_cast<uint32_t>(chests.data.size())},
                {.id = k_seg_research, .data = res.data.data(), .len = static_cast<uint32_t>(res.data.size())},
            };
            if (chests.ok && res.ok && file_write(path, k_kind_world, segs, 2))
                log::debug("world_save: saved to {}", path);
        }

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
                    ih && ih->runtime_id >= 0) return ih->runtime_id;
                const auto *unk = content::item_registry::instance().unknown_item();
                return unk ? unk->runtime_id : -1;
            }
            if (static_cast<int>(original_id) > 0 && static_cast<int>(original_id) < base) return static_cast<int>(
                original_id);
            if (static_cast<int>(original_id) >= base) {
                if (const auto *ih = content::item_registry::instance().get_item(static_cast<int>(original_id));
                    ih && ih->runtime_id == static_cast<int>(original_id)) return static_cast<int>(original_id);
            }
            const auto *unk = content::item_registry::instance().unknown_item();
            return unk ? unk->runtime_id : -1;
        }

        void apply_world_item(const patchlib::handle &chests, const uint32_t ci, const uint32_t slot,
                              const uint32_t original_id, const char *loader, const char *mod,
                              const char *name, const uint32_t stack, const uint8_t prefix,
                              const char *p_loader, const char *p_mod, const char *p_name) {
            const auto &h = handles();
            const auto chests_w = patchlib::structs::array<patchlib::handle>(chests, false);
            if (ci >= chests_w.length()) return;
            const auto chest = chests_w.at(ci);
            if (!chest) return;

            const auto items = h.f_chest_item.get_value<patchlib::handle>(chest);
            if (!items) return;
            if (const auto items_w = patchlib::structs::array<patchlib::handle>(items, false); slot >= items_w.length())
                return;

            const int type = resolve_item_type(original_id, loader, mod, name);
            if (type <= 0) return;
            const int final_prefix = resolve_prefix(prefix, p_loader, p_mod, p_name);
            container_set_custom(items, slot, true, type, static_cast<int>(stack), final_prefix);
        }

        void load_chests_seg(rd &seg) {
            const auto &h = handles();
            if (!h.f_main_chest || !h.f_chest_item) return;

            tlv t;
            while (rd_next(seg, t)) {
                if (t.tag != k_chests_array) continue;
                rd elems;
                uint32_t count;
                if (!rd_array_begin(t, elems, &count)) continue;

                auto chests = h.f_main_chest.get_value<patchlib::handle>();
                if (!chests) return;

                for (uint32_t i = 0; i < count; ++i) {
                    tlv obj;
                    if (!rd_next(elems, obj)) break;
                    if (obj.type != static_cast<uint8_t>(tlv_type::object)) continue;

                    rd os;
                    rd_enter(obj, os);
                    uint32_t ci = 0, slot = 0, stack = 0, original_id = 0;
                    uint8_t prefix = 0;
                    const char *loader = nullptr, *mod = nullptr, *name = nullptr;
                    const char *p_loader = nullptr, *p_mod = nullptr, *p_name = nullptr;

                    tlv f;
                    while (rd_next(os, f)) {
                        switch (f.tag) {
                            case k_chest_index: ci = rd_u32(f);
                                break;
                            case k_chest_slot: slot = rd_u32(f);
                                break;
                            case k_chest_orig_id: original_id = rd_u32(f);
                                break;
                            case k_chest_loader: loader = rd_str(f);
                                break;
                            case k_chest_mod: mod = rd_str(f);
                                break;
                            case k_chest_name: name = rd_str(f);
                                break;
                            case k_chest_stack: stack = rd_u32(f);
                                break;
                            case k_chest_prefix: prefix = rd_u8(f);
                                break;
                            case k_chest_prefix_loader: p_loader = rd_str(f);
                                break;
                            case k_chest_prefix_mod: p_mod = rd_str(f);
                                break;
                            case k_chest_prefix_name: p_name = rd_str(f);
                                break;
                            default: break;
                        }
                    }
                    if (stack > 0)
                        apply_world_item(chests, ci, slot, original_id, loader, mod, name,
                                         stack, prefix, p_loader, p_mod, p_name);
                }
            }
        }

        void sanitize_world_chests() {
            const auto &h = handles();
            if (!h.f_main_chest) return;
            const auto chests = h.f_main_chest.get_value<patchlib::handle>();
            if (!chests) return;
            const int max_valid = terraria::content::item_registry::instance().count();
            int changed = 0;
            const auto chests_w = patchlib::structs::array<patchlib::handle>(chests, false);
            for (size_t i = 0; i < chests_w.length(); ++i) {
                if (auto c = chests_w.at(i)) changed += sanitize_chest(c, max_valid);
            }
            if (changed > 0)
                log::info("world_save: replaced {} stale item(s) in world chests", changed);
        }

        void load_world_chests() {
            const auto wld = world_tef_save_base();
            if (wld.empty()) return;
            char path[1400];
            if (!make_path(wld.c_str(), path, sizeof path)) return;

            file f;
            if (!file_open(path, f)) return;
            if (f.kind != k_kind_world) {
                file_close(f);
                return;
            }

            for (int i = 0; i < f.seg_count; ++i) {
                rd seg = file_seg(f, i);
                if (f.segs[i].id == k_seg_chests) load_chests_seg(seg);
            }
            file_close(f);
        }
    } // namespace

    void world_save::early_init() {
        handles_init();

        const auto worldfile = patchlib::type("Terraria.IO", "WorldFile");
        if (!worldfile) {
            log::warn("world_save: Terraria.IO.WorldFile not found");
            return;
        }

        if (const auto m = worldfile.get_method("SaveChests", 1))
            h_save_ = patchlib_install_prepost_hook(m.raw(), nullptr, chest_save_postfix);
        if (const auto m = worldfile.get_method("LoadChests", 1))
            h_load_ = patchlib_install_prepost_hook(m.raw(), nullptr, chest_load_postfix);
    }

    void world_save::shutdown() {
        // hook 不卸载
    }

    void world_save::chest_save_postfix(patch_handle_t, void **, void *, const patch_method_signature_t *) {
        save_world_chests();
    }

    void world_save::chest_load_postfix(patch_handle_t, void **, void *, const patch_method_signature_t *) {
        load_world_chests();
        sanitize_world_chests();
    }
}
