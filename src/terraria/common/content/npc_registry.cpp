/*******************************************************************************
 * tefkernel - npc_registry
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

#include "terraria/content/npc_registry.hpp"

#include <cstring>
#include <string>
#include <vector>

#include "tef/core/log.hpp"
#include "tef/patchlib/struct/array.hpp"
#include "terraria/content/asset_factory.hpp"
#include "terraria/content/language_manager.hpp"
#include "terraria/content/texture2d.hpp"

#if defined(__ANDROID__)
#include "abi_impl/patchlib/il2cpp_api.h"
#include "tef/patchlib/property.hpp"
#endif

namespace terraria::content {
    using namespace tef;

    namespace {
        constexpr size_t k_max_uniq = 64;
        constexpr size_t k_buf = 64;

        std::string make_name_key(const terraria_npc_handle_t *h) {
            std::string s = "npc_name_";
            s += h->parent_modloader_id ? h->parent_modloader_id : "?";
            s += '_';
            s += h->parent_id ? h->parent_id : "?";
            s += '.';
            s += h->internal_name ? h->internal_name : "?";
            return s;
        }

        patchlib::handle make_placeholder_texture() {
            constexpr int k_size = 16;
            constexpr size_t k_data = static_cast<size_t>(k_size) * k_size * 4;
            std::vector<uint8_t> px(k_data);
            for (size_t i = 0; i < k_size * k_size; ++i) {
                px[i * 4 + 0] = 255;
                px[i * 4 + 1] = 0;
                px[i * 4 + 2] = 255;
                px[i * 4 + 3] = 255;
            }
            const auto tex = texture2d::instance().create(k_size, k_size, TEXTURE_FORMAT_RGBA32,
                                                          px.data(), px.size());
            if (!tex) return {};
            const auto tex_type = patchlib::type("Microsoft.Xna.Framework.Graphics", "Texture2D");
            return asset_factory::instance().create(tex_type, tex);
        }
    } // namespace

    npc_registry &npc_registry::instance() noexcept {
        static npc_registry inst;
        return inst;
    }

    // -----------------------------------------------------------------------
    // 生命周期
    // -----------------------------------------------------------------------
    void npc_registry::early_init() {
        npc_class_ = patchlib::type("Terraria", "NPC");
        if (!npc_class_) {
            log::error("npc_registry: Terraria.NPC not found");
            return;
        }

        f_type_ = npc_class_.get_field("type");
        f_active_ = npc_class_.get_field("active");
        f_npc_boss_ = npc_class_.get_field("boss");
#if defined(__ANDROID__)
        f_npc_hide_ = npc_class_.get_field("hide");
        f_npc_behind_tiles_ = npc_class_.get_field("behindTiles");
        if (f_type_) {
            const size_t off = il2cpp_field_get_offset(f_type_.raw());
            if (off > 0) type_offset_ = int(off);
        }
#endif

        if (const auto id_cls = patchlib::type("Terraria.ID", "NPCID")) {
            f_npc_id_count_ = id_cls.get_field("Count");
            if (f_npc_id_count_) {
                if (const auto c = f_npc_id_count_.get_value<short>(); c > 0) base_count_ = c;
            }
            if (const auto sets = id_cls.get_inner_type("Sets"))
                f_boss_head_textures_ = sets.get_field("BossHeadTextures");
        }

        if (const auto ta = patchlib::type("Terraria.GameContent", "TextureAssets")) {
            f_texture_npc_ = ta.get_field("Npc");
            f_npc_head_boss_ = ta.get_field("NpcHeadBoss");
        }

        if (const auto main_cls = patchlib::type("Terraria", "Main")) {
            f_npc_frame_count_ = main_cls.get_field("npcFrameCount");
#if defined(__ANDROID__)
            f_main_sprite_batch_ = main_cls.get_field("spriteBatch");
            if (auto prop = main_cls.get_property("screenPosition"))
                m_screen_pos_get_ = prop.get_get_method();
            f_main_npc_arr_ = main_cls.get_field("npc");
            m_draw_direct_ = main_cls.get_method("DrawNPCDirect", 4);
#endif
        }

        m_set_defaults_ = npc_class_.get_method("SetDefaults", 2);
        m_loot_ = npc_class_.get_method("NPCLoot", 0);
        m_ai_ = npc_class_.get_method("AI", 0);

        if (m_set_defaults_)
            patchlib_install_prepost_hook(m_set_defaults_.raw(), nullptr,
                                          set_defaults_postfix);
        if (m_loot_)
            patchlib_install_prepost_hook(m_loot_.raw(), nullptr,
                                          npc_loot_postfix);
        if (m_ai_)
            patchlib_install_prepost_hook(m_ai_.raw(),
                                          ai_prefix,
                                          ai_postfix);

        if (const auto trm = patchlib::type("Terraria.GameContent", "TownRoomManager")) {
            f_has_room_ = trm.get_field("_hasRoom");
            if (const auto m = trm.get_method(".ctor", 0))
                patchlib_install_prepost_hook(m.raw(), nullptr,
                                              town_room_ctor_postfix);
        }

        // Android: DrawNPCs 后置补画
#if defined(__ANDROID__)
        if (auto main_cls = patchlib::type("Terraria", "Main")) {
            if (auto m = main_cls.get_method("DrawNPCs", 1)) {
                patchlib_install_prepost_hook(m.raw(), nullptr, draw_npcs_postfix);
            }
        }
#endif
    }

    void npc_registry::on_main_initialize_prefix() {
        if (registry_.empty()) return;
        ids_assigned_ = assign_ids(base_count_);
        for (auto *h: registry_.all())
            if (h->npc_ops.early_init) h->npc_ops.early_init(h);
    }

    void npc_registry::on_main_initialize_postfix() {
        if (registry_.empty()) return;
        resize();
        init_texture2d();
        init_localized_text();
        for (auto *h: registry_.all())
            if (h->npc_ops.init_static) h->npc_ops.init_static(h);
    }

    void npc_registry::shutdown() {
        registry_.all().clear();
        ids_assigned_ = false;
    }

    // -----------------------------------------------------------------------
    // 注册 / 注销 / ID
    // -----------------------------------------------------------------------
    bool npc_registry::register_npc(terraria_npc_handle_t *h) {
        if (!h || !h->parent_modloader_id || !h->parent_id || !h->internal_name) return false;
        const bool ok = registry_.register_entity(h);
        if (ok && ids_assigned_)
            log::warn("npc registered after IDs assigned: {}.{}", h->parent_id, h->internal_name);
        return ok;
    }

    bool npc_registry::unregister_by_id(const int runtime_id) {
        if (runtime_id < 0) return false;
        const bool ok = registry_.unregister_by_id(runtime_id);
        if (ok && ids_assigned_) assign_ids(base_count_);
        return ok;
    }

    bool npc_registry::unregister(terraria_npc_handle_t *h) {
        if (!h) return false;
        const bool ok = registry_.unregister(h);
        if (ok && ids_assigned_) assign_ids(base_count_);
        return ok;
    }

    bool npc_registry::assign_ids(const int base) {
        if (registry_.empty()) {
            ids_assigned_ = true;
            return true;
        }
        base_count_ = base;
        return registry_.assign_ids(base);
    }

    terraria_npc_handle_t *npc_registry::get(const int runtime_id) const { return registry_.get(runtime_id); }
    tefstd_vector_t *npc_registry::get_npcs() { return registry_.raw_tefstd_vector(); }

    terraria_npc_handle_t *npc_registry::find(const std::string_view loader, const std::string_view mod,
                                              const std::string_view name) const {
        return registry_.find(loader, mod, name);
    }

    int npc_registry::id_count() const { return base_count_ > 0 ? base_count_ : 0; }

    // -----------------------------------------------------------------------
    // type
    // -----------------------------------------------------------------------
    int npc_registry::get_npc_type(patch_handle_t inst) const {
        if (!inst) return -1;
#if defined(__ANDROID__)
        if (type_offset_ >= 0)
            return *reinterpret_cast<const int *>(reinterpret_cast<const char *>(inst) + type_offset_);
#endif
        int type = -1;
        if (f_type_) type = f_type_.get_value<int>(inst);
        return type;
    }

    terraria_npc_handle_t *npc_registry::custom_from_instance(patch_handle_t inst) const {
        const int type = get_npc_type(inst);
        const int nid = type - base_count_;
        if (nid < 0 || nid >= static_cast<int>(registry_.size())) return nullptr;
        return registry_.all()[nid];
    }

    // -----------------------------------------------------------------------
    // resize 工具
    // -----------------------------------------------------------------------
    void npc_registry::fill_new_slots_with_modal(const patchlib::handle &old_arr,
                                                 const patchlib::handle &new_arr,
                                                 const size_t old_len, const size_t new_len) {
        if (!old_arr || !new_arr || old_len == 0 || new_len <= old_len) return;
        uint64_t keys[k_max_uniq]{};
        int counts[k_max_uniq]{};
        uint8_t vals[k_max_uniq][k_buf]{};
        int nu = 0;
        uint8_t buf[k_buf]{};
        for (size_t i = 0; i < old_len; ++i) {
            std::memset(buf, 0, k_buf);
            if (!patchlib_array_at(old_arr.raw(), i, buf)) continue;
            uint64_t key = 0;
            std::memcpy(&key, buf, sizeof(key));
            int found = -1;
            for (int j = 0; j < nu; ++j) if (keys[j] == key) {
                found = j;
                break;
            }
            if (found >= 0) counts[found]++;
            else if (static_cast<size_t>(nu) < k_max_uniq) {
                keys[nu] = key;
                std::memcpy(vals[nu], buf, k_buf);
                counts[nu] = 1;
                nu++;
            }
        }
        if (nu == 0) return;
        int best = 0;
        for (int j = 1; j < nu; ++j) if (counts[j] > counts[best]) best = j;
        for (size_t i = old_len; i < new_len; ++i)
            if (!patchlib_array_set(new_arr.raw(), i, vals[best])) break;
    }

    void npc_registry::resize_static_array(const patchlib::field &f, std::string_view name,
                                           const int base, const int new_size, const bool modal,
                                           const int int_fill, const bool fill_with_int) {
        if (!f) return;
        auto arr_w = f.get_value<patchlib::structs::array<uint8_t> >();
        if (!arr_w) return;
        const size_t len = arr_w.length();
        if (len + 8 < static_cast<size_t>(base) || len >= static_cast<size_t>(new_size)) return;
        if (fill_with_int) {
            auto bigger = arr_w.resized(static_cast<size_t>(new_size), static_cast<uint8_t>(int_fill));
            if (!bigger) return;
            f.set_value(patchlib::handle(bigger));
        } else {
            auto bigger = arr_w.resized(static_cast<size_t>(new_size), 0);
            if (!bigger) return;
            if (modal)
                fill_new_slots_with_modal(patchlib::handle(arr_w),
                                          patchlib::handle(bigger),
                                          len, static_cast<size_t>(new_size));
            f.set_value(patchlib::handle(bigger));
        }
        log::debug("npc resize: {} {} -> {}", name, len, new_size);
    }

    int npc_registry::resize_sets_arrays(const std::string_view ns, const std::string_view cls,
                                         const std::string_view inner, const int base, const int new_size,
                                         const bool exclude_town_groups) {
        const auto cls_h = patchlib::type(std::string(ns), std::string(cls));
        if (!cls_h) return 0;
        const auto sets = cls_h.get_inner_type(std::string(inner));
        if (!sets) return 0;
        int resized = 0;
        for (auto &f: sets.get_fields()) {
            const std::string name = f.get_name();
            if (name.empty() || name == "Factory" || name == "Count") continue;
            if (!f.is_static() || f.is_const()) continue;
            if (exclude_town_groups && name == "TownNPCsFramingGroups") continue;
            auto ft = patchlib::type(patchlib_field_get_type_handle(f.raw()), true);
            if (!ft) continue;
            if (const auto tn = ft.get_name(); tn.find("[]") == std::string::npos) continue;
            auto arr_w = f.get_value<patchlib::structs::array<uint8_t> >();
            if (!arr_w) continue;
            const size_t len = arr_w.length();
            if (!(len + 8 >= static_cast<size_t>(base) && len < static_cast<size_t>(new_size))) continue;
            auto bigger = arr_w.resized(static_cast<size_t>(new_size), 0);
            if (!bigger) continue;
            fill_new_slots_with_modal(patchlib::handle(arr_w), patchlib::handle(bigger),
                                      len, static_cast<size_t>(new_size));
            f.set_value(patchlib::handle(bigger));
            resized++;
        }
        return resized;
    }

    int npc_registry::resize_all_static_arrays(const std::string_view ns, const std::string_view cls,
                                               const int base, const int new_size,
                                               const char *const *excludes,
                                               const size_t n_excludes, const bool modal) {
        auto cls_h = patchlib::type(std::string(ns), std::string(cls));
        if (!cls_h) return 0;
        int done = 0;
        for (auto &f: cls_h.get_fields()) {
            const std::string name = f.get_name();
            if (name.empty()) continue;
            if (!f.is_static() || f.is_const()) continue;
            auto ft = patchlib::type(patchlib_field_get_type_handle(f.raw()), true);
            if (!ft) continue;
            if (ft.get_name().find("[]") == std::string::npos) continue;
            bool skip = false;
            for (size_t j = 0; j < n_excludes; ++j)
                if (excludes && excludes[j] && name == excludes[j]) {
                    skip = true;
                    break;
                }
            if (skip) continue;
            auto arr_w = f.get_value<patchlib::structs::array<uint8_t> >();
            if (!arr_w) continue;
            if (arr_w.length() != static_cast<size_t>(base)) continue;
            auto bigger = arr_w.resized(static_cast<size_t>(new_size), 0);
            if (!bigger) continue;
            if (modal)
                fill_new_slots_with_modal(patchlib::handle(arr_w),
                                          patchlib::handle(bigger),
                                          static_cast<size_t>(base), static_cast<size_t>(new_size));
            f.set_value(patchlib::handle(bigger));
            done++;
        }
        return done;
    }

    void npc_registry::resize_player_arrays(const int base, const int new_size) {
        const auto main_cls = patchlib::type("Terraria", "Main");
        const auto player_cls = patchlib::type("Terraria", "Player");
        if (!main_cls || !player_cls) return;
        const auto f_players = main_cls.get_field("player");
        auto f_no_aggro = player_cls.get_field("npcTypeNoAggro");
        if (!f_players || !f_no_aggro) return;

        const auto players_w = f_players.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!players_w) return;
        int patched = 0;
        for (size_t i = 0; i < players_w.length(); ++i) {
            auto player = players_w.at(i);
            if (!player) continue;
            if (auto arr_w = f_no_aggro.get_value<patchlib::structs::array<uint8_t> >(player);
                arr_w && arr_w.length() == static_cast<size_t>(base)) {
                if (auto bigger = arr_w.resized(static_cast<size_t>(new_size), 0)) {
                    f_no_aggro.set_value(player, patchlib::handle(bigger));
                    patched++;
                }
            }
        }
        log::info("npc resize: npcTypeNoAggro {}/{} players", patched, players_w.length());
    }

    void npc_registry::resize_npc_name_cache(int base, int new_size) {
        const auto lang_cls = patchlib::type("Terraria", "Lang");
        if (!lang_cls) return;
        const auto f = lang_cls.get_field("_npcNameCache");
        if (!f) return;
        const auto arr_w = f.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!arr_w || arr_w.length() != static_cast<size_t>(base)) return;

        // 新槽填一个自建 LocalizedText（不要用旧元素当 fill，GCHandle 会踩坏）
        const auto text = language_manager::instance().create_localized_text("NPCName.ModNPC", "Mod NPC");
        auto bigger = arr_w.resized(static_cast<size_t>(new_size), patchlib::handle{});
        if (!bigger) return;
        f.set_value(patchlib::handle(bigger));
        if (text) {
            auto b_w = patchlib::structs::array<patchlib::handle>(patchlib::handle(bigger), false);
            for (int i = base; i < new_size; ++i) b_w.set(static_cast<size_t>(i), text);
        }
        log::info("npc resize: Lang._npcNameCache {} -> {}", base, new_size);
    }

    void npc_registry::resize_scene_metrics(int base, int new_size) {
        auto sm_cls = patchlib::type("Terraria", "SceneMetrics");
        auto main_cls = patchlib::type("Terraria", "Main");
        if (!sm_cls || !main_cls) return;
        auto f_pos = sm_cls.get_field("ClosestNPCPosition");
        if (!f_pos) return;
        for (auto *inst_field: {"_playerSceneMetrics", "_cameraSceneMetrics"}) {
            auto f_inst = main_cls.get_field(inst_field);
            if (!f_inst) continue;
            auto inst = f_inst.get_value<patchlib::handle>();
            if (!inst) continue;
            auto arr_w = f_pos.get_value<patchlib::structs::array<uint8_t> >(inst);
            if (!arr_w || arr_w.length() != static_cast<size_t>(base)) continue;
            if (auto bigger = arr_w.resized(static_cast<size_t>(new_size), 0)) f_pos.set_value(
                inst, patchlib::handle(bigger));
        }
    }

    void npc_registry::resize_town_room_instance(const int base, int new_size) {
        if (!f_has_room_) return;
        const auto wg_cls = patchlib::type("Terraria", "WorldGen");
        if (!wg_cls) return;
        const auto f_tm = wg_cls.get_field("TownManager");
        if (!f_tm) return;
        const auto tm = f_tm.get_value<patchlib::handle>();
        if (!tm) return;
        const auto arr_w = f_has_room_.get_value<patchlib::structs::array<uint8_t> >(tm);
        if (!arr_w || arr_w.length() != static_cast<size_t>(base)) return;
        if (auto bigger = arr_w.resized(static_cast<size_t>(new_size), 0)) {
            f_has_room_.set_value(tm, patchlib::handle(bigger));
            log::info("npc resize: WorldGen.TownManager._hasRoom -> {}", new_size);
        }
    }

    void npc_registry::resize_npc_set_factory(int new_size) {
        const auto npcid = patchlib::type("Terraria.ID", "NPCID");
        if (!npcid) return;
        const auto sets = npcid.get_inner_type("Sets");
        if (!sets) return;
        const auto f_factory = sets.get_field("Factory");
        if (!f_factory) return;
        const auto factory = f_factory.get_value<patchlib::handle>();
        if (!factory) return;
        const auto sf = patchlib::type("Terraria.ID", "SetFactory");
        if (!sf) return;
        auto f_size = sf.get_field("_size");
        if (!f_size) return;
        f_size.set_value(factory, new_size);
        log::info("npc resize: NPCID.Sets.Factory._size -> {}", new_size);
    }

    void npc_registry::resize() {
        const int base = base_count_;
        if (base <= 0) return;
        const int new_size = base + static_cast<int>(registry_.size());

        resize_static_array(f_texture_npc_, "TextureAssets.Npc", base, new_size, false, 0, false);
        resize_static_array(f_npc_frame_count_, "Main.npcFrameCount", base, new_size, false, 1, true);

        int resized = resize_sets_arrays("Terraria.ID", "NPCID", "Sets", base, new_size, true);
        log::info("npc resize: NPCID.Sets {} arrays ({} -> {})", resized, base, new_size);

        static const char *main_excludes[] = {"npcFrameCount"};
        int main_done = resize_all_static_arrays("Terraria", "Main", base, new_size,
                                                 main_excludes, 1, false);
        int npc_done = resize_all_static_arrays("Terraria", "NPC", base, new_size,
                                                nullptr, 0, false);
        int emote_done = resize_all_static_arrays("Terraria.GameContent.UI", "EmoteBubble",
                                                  base, new_size, nullptr, 0, true);
        int dialogue_done = resize_all_static_arrays("Terraria.GameContent", "ConditionalDialogue",
                                                     base, new_size, nullptr, 0, true);
        int critter_done = resize_all_static_arrays("Terraria.GameContent.Tile_Entities",
                                                    "TECritterAnchor", base, new_size,
                                                    nullptr, 0, true);
        int dmg_done = resize_all_static_arrays("Terraria.GameContent", "NPCDamageTracker",
                                                base, new_size, nullptr, 0, true);
        log::info("npc resize: Main={} NPC={} Emote={} Dialog={} TECritter={} NPCDmg={}",
                  main_done, npc_done, emote_done, dialogue_done, critter_done, dmg_done);

        resize_player_arrays(base, new_size);
        resize_npc_name_cache(base, new_size);
        resize_scene_metrics(base, new_size);
        resize_town_room_instance(base, new_size);
        resize_npc_set_factory(new_size);

#if !defined(__ANDROID__)
        if (f_npc_id_count_) {
            const auto total = static_cast<short>(new_size);
            f_npc_id_count_.set_value(total);
            log::info("npc resize: NPCID.Count {} -> {} (desktop)", base, new_size);
        }
#endif
    }

    // -----------------------------------------------------------------------
    // 贴图
    // -----------------------------------------------------------------------
    void npc_registry::register_frames(const terraria_npc_handle_t *h, const int type) const {
        if (!h || !f_npc_frame_count_) return;
        auto arr_w = f_npc_frame_count_.get_value<patchlib::structs::array<int> >();
        if (!arr_w) return;
        if (static_cast<size_t>(type) >= arr_w.length()) return;
        int n = h->frames > 0 ? h->frames : 1;
        arr_w.set(static_cast<size_t>(type), n);
    }

    void npc_registry::init_texture2d() {
        if (!f_texture_npc_) return;
        auto arr_w = f_texture_npc_.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!arr_w) return;
        patchlib::handle placeholder;
        int done = 0;
        for (auto *h: registry_.all()) {
            if (!h || h->runtime_id < 0 || static_cast<size_t>(h->runtime_id) >= arr_w.length()) continue;
            patchlib::handle tex;
            if (h->npc_ops.get_texture) tex = patchlib::handle(h->npc_ops.get_texture(h), true);
            if (!tex) {
                if (!placeholder) placeholder = make_placeholder_texture();
                if (!placeholder) break;
                tex = placeholder;
            }
            arr_w.set(static_cast<size_t>(h->runtime_id), tex);
            register_frames(h, h->runtime_id);
            done++;
        }
        log::info("npc textures: {} set", done);
    }

    void npc_registry::init_localized_text() {
        const int base = base_count_;
        const int custom = static_cast<int>(registry_.size());
        if (custom <= 0 || base <= 0) return;
        const auto lang_cls = patchlib::type("Terraria", "Lang");
        if (!lang_cls) return;
        const auto f = lang_cls.get_field("_npcNameCache");
        if (!f) return;

        auto arr_w = f.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!arr_w) {
            const auto lt = patchlib::type("Terraria.Localization", "LocalizedText");
            if (!lt) return;
            auto created = patchlib::structs::array<patchlib::handle>(base + custom, lt);
            if (!created) return;
            f.set_value(patchlib::handle(created));
            arr_w = created;
        }
        if (arr_w.length() < base + custom) {
            auto bigger = arr_w.resized(base + custom, patchlib::handle{});
            if (!bigger) return;
            f.set_value(patchlib::handle(bigger));
            arr_w = bigger;
        }

        for (size_t i = 0; i < registry_.size(); ++i) {
            const auto *h = registry_.all()[i];
            if (!h) continue;
            const int target = base + static_cast<int>(i);
            const std::string key = make_name_key(h);
            auto lt = language_manager::instance().get_localized_text(key.c_str());
            if (!lt) continue;
            arr_w.set(static_cast<size_t>(target), lt);
        }
        log::info("npc l10n: {} custom NPCs localized (base={})", custom, base);
    }

    // -----------------------------------------------------------------------
    // 钩子
    // -----------------------------------------------------------------------
    void npc_registry::set_defaults_postfix(patch_handle_t inst, void **args, void *,
                                            const patch_method_signature_t *) {
        if (!inst || !args || !args[0] || !instance().f_type_) return;
        auto &self = instance();
        const int type = *static_cast<int *>(args[0]);
        const int nid = type - self.base_count_;
        if (nid < 0 || nid >= static_cast<int>(self.registry_.size())) return;

        auto *h = self.registry_.all()[nid];
        if (h && h->npc_ops.set_defaults) h->npc_ops.set_defaults(h, inst);
        self.register_frames(h, type);
    }

    void npc_registry::town_room_ctor_postfix(patch_handle_t inst, void **, void *,
                                              const patch_method_signature_t *) {
        auto &self = instance();
        if (!inst || !self.f_has_room_) return;
        const auto arr_w = self.f_has_room_.get_value<patchlib::structs::array<uint8_t> >(inst);
        if (!arr_w) return;
        const int base = self.base_count_;
        const int need = base + static_cast<int>(self.registry_.size());
        if (static_cast<int>(arr_w.length()) >= need) return;
        if (auto bigger = arr_w.resized(static_cast<size_t>(need), 0)) self.f_has_room_.set_value(
            inst, patchlib::handle(bigger));
    }

    void npc_registry::npc_loot_postfix(patch_handle_t inst, void **, void *,
                                        const patch_method_signature_t *) {
        const auto &self = instance();
        if (auto *h = self.custom_from_instance(inst); h && h->npc_ops.on_death) h->npc_ops.on_death(h, inst);
    }

    bool npc_registry::ai_prefix(patch_handle_t inst, void **,
                                 const patch_method_signature_t *, void *) {
        const auto &self = instance();
        auto *h = self.custom_from_instance(inst);
        if (!h || !h->npc_ops.ai) return false;
        if (h->ai_after_vanilla) return false;
        h->npc_ops.ai(h, inst);
        return true;
    }

    void npc_registry::ai_postfix(patch_handle_t inst, void **, void *,
                                  const patch_method_signature_t *) {
        const auto &self = instance();
        auto *h = self.custom_from_instance(inst);
        if (!h || !h->npc_ops.ai || !h->ai_after_vanilla) return;
        h->npc_ops.ai(h, inst);
    }

#if defined(__ANDROID__)
    // Android: NPCID.Count 是 const 697，Main.DrawNPCs 的 type < NPCID.Count
    // 会把自定义 NPC 跳过。这里在 DrawNPCs 后置里手动补画。
    void npc_registry::draw_npcs_postfix(patch_handle_t inst, void **args, void *,
                                          const patch_method_signature_t *) {
        if (!inst || !args || !args[0] || !instance().m_draw_direct_)
            return;

        const auto &self = instance();
        if (!self.f_main_sprite_batch_ || !self.m_screen_pos_get_ ||
            !self.f_main_npc_arr_ || !self.f_npc_hide_ ||
            !self.f_npc_behind_tiles_ || !self.f_active_ || !self.f_type_)
            return;

        const bool behind = *static_cast<bool *>(args[0]);

        // Main.spriteBatch / screenPosition 是静态字段，instance 传 nullptr
        const auto sb = self.f_main_sprite_batch_.get_value<patchlib::handle>();
        if (!sb) return;

        // screenPosition 是静态属性 getter，返回 Vector2
        struct vec2 { float x, y; };
        const auto screen_pos = self.m_screen_pos_get_.invoke<vec2>();

        const auto arr_w = self.f_main_npc_arr_.get_value<
            patchlib::structs::array<patchlib::handle>>();
        if (!arr_w) return;

        const int custom = static_cast<int>(self.registry_.size());
        const int base = self.base_count_;

        for (size_t i = 0; i < arr_w.length(); ++i) {
            auto npc = arr_w.at(i);
            if (!npc) continue;

            const bool active = self.f_active_.get_value<bool>(npc);
            const bool hide   = self.f_npc_hide_.get_value<bool>(npc);
            const bool npc_behind = self.f_npc_behind_tiles_.get_value<bool>(npc);
            const int type = self.f_type_.get_value<int>(npc);

            if (const int nid = type - base; active && !hide && type > 0 && nid >= 0 && nid < custom &&
                                             npc_behind == behind) {
                self.m_draw_direct_.invoke<void>(sb, npc, behind, screen_pos);
                }
        }
    }
#endif

    // -----------------------------------------------------------------------
    // 生成 / 掉落
    // -----------------------------------------------------------------------
    void npc_registry::ensure_local_player_aggro_array() const {
        static patchlib::field f_main, f_players, f_my, f_no_aggro;
        if (!f_main) {
            f_main = patchlib::type("Terraria", "Main").get_field("player");
            if (const auto main = patchlib::type("Terraria", "Main"))
                f_my = main.get_field("myPlayer");
            if (const auto pcls = patchlib::type("Terraria", "Player"))
                f_no_aggro = pcls.get_field("npcTypeNoAggro");
        }
        if (!f_main || !f_my || !f_no_aggro) return;

        int my = 0;
        if (f_my) my = f_my.get_value<int>();
        const auto players_w = f_main.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!players_w || static_cast<size_t>(my) >= players_w.length()) return;
        const auto player = players_w.at(static_cast<size_t>(my));
        if (!player) return;
        const auto arr_w = f_no_aggro.get_value<patchlib::structs::array<uint8_t> >(player);
        if (!arr_w) return;
        const int need = base_count_ + static_cast<int>(registry_.size());
        if (static_cast<int>(arr_w.length()) >= need) return;
        if (auto bigger = arr_w.resized(static_cast<size_t>(need), 0)) f_no_aggro.set_value(
            player, patchlib::handle(bigger));
    }

    int npc_registry::spawn(const int runtime_id, const float x, const float y) const {
        ensure_local_player_aggro_array();
        if (runtime_id < 0) return -1;

        static patchlib::method new_npc;
        static patchlib::type source_cls;
        if (!new_npc) {
            if (const auto cls = patchlib::type("Terraria", "NPC"))
                new_npc = cls.get_method("NewNPC", 10);
            if (!new_npc) {
                log::error("npc spawn: NPC.NewNPC(10) not found");
                return -1;
            }
        }
        if (!source_cls) {
            source_cls = patchlib::type("Terraria.DataStructures", "EntitySource_SpawnNPC");
            if (!source_cls) {
                log::error("npc spawn: EntitySource_SpawnNPC not found");
                return -1;
            }
        }

        const auto source = source_cls.new_instance();
        const int xi = static_cast<int>(x), yi = static_cast<int>(y), type = runtime_id, start = 0;
        constexpr float ai0 = 0, ai1 = 0, ai2 = 0, ai3 = 0;
        constexpr int target = 255;
        constexpr int result = -1;
        new_npc.invoke<void>(source, xi, yi, type, start, ai0, ai1, ai2, ai3, target);
        return result;
    }

    int npc_registry::drop_item(const int item_id, const float x, const float y, const int count) {
        if (item_id <= 0) return -1;
        // 与 C 版一致：Android 走 9 参旧版 NewItem，桌面走 12 参
        const int stack = count > 0 ? count : 1;
#if defined(__ANDROID__)
        static patchlib::method m_legacy;
        if (!m_legacy) {
            if (auto cls = patchlib::type("Terraria", "Item"))
                m_legacy = cls.get_method("NewItem", 9);
            if (!m_legacy) {
                log::error("npc drop: NewItem(9) not found");
                return -1;
            }
        }
        int x_arg = int(x), y_arg = int(y), width = 0, height = 0;
        int type = item_id, st = stack, prefix = 0, ownership = 0;
        bool no_broadcast = false;
        int result = -1;
        m_legacy.invoke<void>(x_arg, y_arg, width, height, type, st, no_broadcast, prefix, ownership);
        return result;
#else
        static patchlib::method m_new_item;
        static patchlib::type src_cls;
        if (!m_new_item) {
            if (const auto cls = patchlib::type("Terraria", "Item"))
                m_new_item = cls.get_method("NewItem", 12);
            if (!m_new_item) {
                log::error("npc drop: NewItem(12) not found");
                return -1;
            }
        }
        if (!src_cls) {
            src_cls = patchlib::type("Terraria.DataStructures", "EntitySource_SpawnNPC");
            if (!src_cls) {
                log::error("npc drop: EntitySource_SpawnNPC not found");
                return -1;
            }
        }
        const auto source = src_cls.new_instance();
        const int xi = static_cast<int>(x), yi = static_cast<int>(y), w = 0, h = 0;
        const int type = item_id, st = stack, prefix = 0, ownership = 0;
        constexpr bool no_broadcast = false;
        constexpr unsigned char velocity[16] = {};
        constexpr void *modifier = nullptr;
        constexpr int result = -1;
        m_new_item.invoke<void>(source, xi, yi, w, h, type, st, no_broadcast,
                                prefix, ownership, velocity, modifier);
        return result;
#endif
    }
} // namespace terraria::content
