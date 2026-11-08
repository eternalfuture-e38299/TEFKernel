/*******************************************************************************
 * tefkernel - buff_registry
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

#include "terraria/content/buff_registry.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "tef/core/log.hpp"
#include "tef/patchlib/property.hpp"
#include "tef/patchlib/struct/array.hpp"
#include "tef/patchlib/struct/string.hpp"
#include "terraria/content/language_manager.hpp"
#include "terraria/content/gui_overlay.hpp"

#if defined(__ANDROID__)
#include "abi_impl/patchlib/il2cpp_api.h"
#endif

namespace terraria::content {
    using namespace tef;

    namespace {
        std::string make_key(const terraria_buff_handle_t *h, const std::string_view prefix) {
            std::string s;
            s.reserve(prefix.size() + 64);
            s += prefix;
            s += '_';
            s += h->parent_modloader_id ? h->parent_modloader_id : "?";
            s += '_';
            s += h->parent_id ? h->parent_id : "?";
            s += '.';
            s += h->internal_name ? h->internal_name : "?";
            return s;
        }
    }

    buff_registry &buff_registry::instance() noexcept {
        static buff_registry inst;
        return inst;
    }

    // -----------------------------------------------------------------------
    // 生命周期
    // -----------------------------------------------------------------------
    void buff_registry::early_init() {
        if (const auto main = patchlib::type("Terraria", "Main")) {
            f_main_player_ = main.get_field("player");
            f_sprite_batch_ = main.get_field("spriteBatch");
            f_main_my_player_ = main.get_field("myPlayer");
#if !defined(__ANDROID__)
            f_main_mouse_x_ = main.get_field("mouseX");
            f_main_mouse_y_ = main.get_field("mouseY");
            f_main_mouse_left_ = main.get_field("mouseLeft");
            f_main_mouse_right_ = main.get_field("mouseRight");
#else
            f_main_inst_ = main.get_field("instance");
            m_pe_mouse_text_ = main.get_method("MouseText", 10);
            if (auto p = main.get_property("mouseX")) m_get_mouse_x_ = p.get_get_method();
            if (auto p = main.get_property("mouseY")) m_get_mouse_y_ = p.get_get_method();
            if (auto p = main.get_property("mouseLeft")) m_get_mouse_left_ = p.get_get_method();
            if (auto p = main.get_property("mouseRight")) m_get_mouse_right_ = p.get_get_method();
#endif
            m_get_my_player_ = main.get_method("get_myPlayer", 0);

#if !defined(__ANDROID__)
            if (const auto m = main.get_method("DrawInterface_Resources_Buffs", 0))
                patchlib_install_prepost_hook(m.raw(),
                                              pc_buffs_draw_prefix,
                                              pc_buffs_draw_postfix);
            if (const auto m = main.get_method("DrawBuffIcon", 4))
                patchlib_install_prepost_hook(m.raw(),
                                              pc_draw_buff_icon_prefix,
                                              nullptr);
#endif
        }

        if (const auto player = patchlib::type("Terraria", "Player")) {
            f_player_buff_type_ = player.get_field("buffType");
            f_player_who_am_i_ = player.get_field("whoAmI");
            if (const auto m = player.get_method("UpdateBuffs", 1))
                patchlib_install_prepost_hook(m.raw(), nullptr,
                                              update_buffs_postfix);
        }

        if (const auto lay = patchlib::type("", "Buffs_Layout")) {
            if (const auto p = lay.get_property("Instance")) m_layout_instance_ = p.get_get_method();
            f_layout_item_scale_ = lay.get_field("ItemScale");
            f_layout_selected_scale_ = lay.get_field("SelectedScale");
            f_layout_normal_colour_ = lay.get_field("NormalColour");
            f_layout_text_scale_ = lay.get_field("TextScale");
            if (!f_layout_text_scale_) f_layout_text_scale_ = lay.get_field("ItemTextScale");
            f_layout_text_offset_ = lay.get_field("TextOffset");
        }

        // 护士治疗
        if (const auto t = patchlib::type("Terraria.GameContent.Achievements", "AchievementsHelper")) {
            if (const auto m = t.get_method("HandleNurseService", 1))
                patchlib_install_prepost_hook(m.raw(), nullptr,
                                              nurse_service_postfix);
        }

        // 原生 Buff 栏
        if (const auto gui = patchlib::type("", "GUIBuffs")) {
            if (const auto m = gui.get_method("Draw", 0))
                patchlib_install_prepost_hook(m.raw(),
                                              buffs_draw_prefix,
                                              buffs_draw_postfix);
            if (const auto m = gui.get_method("ItemDraw", 4))
                patchlib_install_prepost_hook(m.raw(),
                                              buff_item_draw_prefix,
                                              buff_item_draw_postfix);
        }
        if (const auto grid = patchlib::type("", "GUIItemGrid")) {
            if (const auto m = grid.get_method("Draw", 6))
                patchlib_install_prepost_hook(m.raw(),
                                              item_grid_draw_prefix,
                                              nullptr);
        }

        if (const auto lang = patchlib::type("Terraria", "Lang")) {
            f_buff_name_cache_ = lang.get_field("_buffNameCache");
            f_buff_desc_cache_ = lang.get_field("_buffDescriptionCache");
#if defined(__ANDROID__)
            if (auto lt = patchlib::type("Terraria.Localization", "LocalizedText")) {
                m_from_language_key_ = lt.get_method("FromLanguageKey", 2);
                if (!m_from_language_key_) m_from_language_key_ = lt.get_method("FromLanguageKey", 1);
            }
#else
            if (const auto lt = patchlib::type("Terraria.Localization", "LocalizedText"))
                m_from_language_key_ = lt.get_method("FromLanguageKey", 1);
#endif
        }
    }

    void buff_registry::on_main_initialize_prefix() {
        if (registry_.empty()) return;
        ids_assigned_ = assign_ids(id_count());
        for (auto *h: registry_.all())
            if (h->buff_ops.early_init) h->buff_ops.early_init(h);
    }

    void buff_registry::on_main_initialize_postfix() {
        if (registry_.empty()) return;
        init_texture2d();
        init_localized_text();
        for (auto *h: registry_.all())
            if (h->buff_ops.init_static) h->buff_ops.init_static(h);
    }

    void buff_registry::shutdown() {
#if defined(__ANDROID__)
        for (auto *h: registry_.all()) {
            if (h && h->texture_pin && il2cpp_gchandle_free) {
                il2cpp_gchandle_free(h->texture_pin);
                h->texture_pin = nullptr;
            }
        }
        if (pe_pn_ && il2cpp_gchandle_free) {
            il2cpp_gchandle_free(pe_pn_);
            pe_pn_ = nullptr;
        }
        if (pe_pd_ && il2cpp_gchandle_free) {
            il2cpp_gchandle_free(pe_pd_);
            pe_pd_ = nullptr;
        }
        pe_sn_.reset();
        pe_sd_.reset();
#endif
        font_.reset();
        registry_.all().clear();
        active_count_ = 0;
        ids_assigned_ = false;
    }

    // -----------------------------------------------------------------------
    // 注册表
    // -----------------------------------------------------------------------
    bool buff_registry::register_buff(terraria_buff_handle_t *h) {
        if (!h || !h->parent_modloader_id || !h->parent_id || !h->internal_name) return false;
        // 热重载时保留 texture
        if (const auto *existing = registry_.find(h->parent_modloader_id, h->parent_id, h->internal_name))
            h->texture = existing->texture;
        const bool ok = registry_.register_entity(h);
        if (ok && ids_assigned_)
            log::warn("buff registered after IDs assigned: {}.{}", h->parent_id, h->internal_name);
        return ok;
    }

    bool buff_registry::unregister_by_id(const int runtime_id) {
        if (runtime_id < 0) return false;
        clear_active_of(runtime_id);
        const bool ok = registry_.unregister_by_id(runtime_id);
        if (ok && ids_assigned_) assign_ids(id_count());
        return ok;
    }

    bool buff_registry::unregister(terraria_buff_handle_t *h) {
        if (!h) return false;
        if (h->runtime_id >= 0) clear_active_of(h->runtime_id);
        const bool ok = registry_.unregister(h);
        if (ok && ids_assigned_) assign_ids(id_count());
        return ok;
    }

    bool buff_registry::assign_ids(const int base) {
        if (registry_.empty()) {
            ids_assigned_ = true;
            return true;
        }
        return registry_.assign_ids(base);
    }

    terraria_buff_handle_t *buff_registry::get(const int runtime_id) const { return registry_.get(runtime_id); }
    tefstd_vector_t *buff_registry::get_buffs() { return registry_.raw_tefstd_vector(); }

    terraria_buff_handle_t *buff_registry::find(const std::string_view l, const std::string_view m,
                                                const std::string_view n) const {
        return registry_.find(l, m, n);
    }

    terraria_buff_handle_t *buff_registry::find_by_runtime_id(const int runtime_id) const {
        for (auto *h: registry_.all())
            if (h && h->runtime_id == runtime_id) return h;
        return nullptr;
    }

    void buff_registry::clear_active_of(const int runtime_id) {
        for (int i = 0; i < active_count_;) {
            if (active_[i].buff_id == runtime_id) active_[i] = active_[--active_count_];
            else ++i;
        }
    }

    // -----------------------------------------------------------------------
    // 玩家身份
    // -----------------------------------------------------------------------
    int buff_registry::player_who(patch_handle_t player) const {
        if (!player || !f_player_who_am_i_) return -1;
        return f_player_who_am_i_.get_value<int>(player);
    }

    int buff_registry::main_my_player() const {
        if (m_get_my_player_) return m_get_my_player_.invoke<int>();
        if (f_main_my_player_) return f_main_my_player_.get_value<int>();
        return -1;
    }

    patchlib::handle buff_registry::local_player_obj() const {
        if (!f_main_player_) return {};
        const int my = main_my_player();
        if (my < 0) return {};
        const auto players_w = f_main_player_.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!players_w || static_cast<size_t>(my) >= players_w.length()) return {};
        return players_w.at(static_cast<size_t>(my));
    }

    int buff_registry::local_custom_count() const {
        const auto local = local_player_obj();
        if (!local) return 0;
        const int who = player_who(local.raw());
        int n = 0;
        for (int i = 0; i < active_count_; ++i)
            if (active_[i].who == who && active_[i].time > 0) n++;
        return n;
    }

    // -----------------------------------------------------------------------
    // 活跃列表
    // -----------------------------------------------------------------------
    void buff_registry::clear_debuffs(patch_handle_t player) {
        const int who = player_who(player);
        if (who < 0) return;
        for (int i = 0; i < active_count_;) {
            const auto *h = find_by_runtime_id(active_[i].buff_id);
            if (active_[i].who == who && h && h->debuff) {
                active_[i] = active_[--active_count_];
            } else ++i;
        }
    }

    void buff_registry::visit_active(patch_handle_t player,
                                     const terraria_buff_active_visitor_t visitor, void *user) const {
        if (!visitor || !player) return;
        const int who = player_who(player);
        for (int i = 0; i < active_count_; ++i) {
            if (active_[i].who != who || active_[i].time <= 0) continue;
            if (auto *h = find_by_runtime_id(active_[i].buff_id))
                visitor(user, h, active_[i].time);
        }
    }

    int buff_registry::add(terraria_buff_handle_t *h, patch_handle_t player, const int time) {
        if (!h || h->runtime_id < 0 || !player || time <= 0) return -1;
        const int who = player_who(player);
        active_buff *a = nullptr;
        for (int i = 0; i < active_count_; ++i)
            if (active_[i].who == who && active_[i].buff_id == h->runtime_id) {
                a = &active_[i];
                break;
            }
        if (!a) {
            if (active_count_ >= kMaxActive) return -1;
            a = &active_[active_count_++];
            a->who = who;
            a->buff_id = h->runtime_id;
            a->time = 0;
        }
        if (time > a->time) a->time = time;
        if (h->buff_ops.apply) h->buff_ops.apply(h, player, a->time);
        return a->time;
    }

    int buff_registry::time_of(terraria_buff_handle_t *h, patch_handle_t player) const {
        if (!h || !player) return 0;
        const int who = player_who(player);
        for (int i = 0; i < active_count_; ++i)
            if (active_[i].who == who && active_[i].buff_id == h->runtime_id) return active_[i].time;
        return 0;
    }

    void buff_registry::remove(terraria_buff_handle_t *h, patch_handle_t player) {
        if (!h || !player) return;
        const int who = player_who(player);
        for (int i = 0; i < active_count_;) {
            if (active_[i].who == who && active_[i].buff_id == h->runtime_id)
                active_[i] = active_[--active_count_];
            else ++i;
        }
    }

    void buff_registry::player_tick(patch_handle_t inst) {
        if (!inst || active_count_ == 0) return;
        const int who = player_who(inst);
        for (int i = 0; i < active_count_;) {
            auto &a = active_[i];
            if (a.who != who) {
                ++i;
                continue;
            }
            auto *h = find_by_runtime_id(a.buff_id);
            if (!h) {
                a = active_[--active_count_];
                continue;
            }
            if (a.time > 0) a.time--;
            if (a.time <= 0) {
                a = active_[--active_count_];
                continue;
            }
            if (h->buff_ops.update) h->buff_ops.update(h, inst, i);
            ++i;
        }
    }

    void buff_registry::update_buffs_postfix(patch_handle_t inst, void **, void *,
                                             const patch_method_signature_t *) {
        instance().player_tick(inst);
    }

    void buff_registry::nurse_service_postfix(patch_handle_t, void **, void *,
                                              const patch_method_signature_t *) {
        auto &self = instance();
        if (const auto local = self.local_player_obj()) self.clear_debuffs(local.raw());
    }

    // -----------------------------------------------------------------------
    // 图标 / 本地化
    // -----------------------------------------------------------------------
    void buff_registry::init_texture2d() {
        int done = 0;
        for (auto *h: registry_.all()) {
            if (!h) continue;
            if (!h->texture && h->buff_ops.get_texture)
                h->texture = patchlib::handle(h->buff_ops.get_texture(h), true).raw();
#if defined(__ANDROID__)
            if (h->texture && !h->texture_pin && il2cpp_gchandle_new)
                h->texture_pin = il2cpp_gchandle_new(h->texture, false);
#endif
            if (h->texture) done++;
        }
        log::info("buff textures: {}/{} set", done, registry_.size());
    }

    void buff_registry::init_localized_text() {
        const int base = id_count();
        const int custom = static_cast<int>(registry_.size());
        if (custom <= 0) return;

        if (const auto lt_cls = patchlib::type("Terraria.Localization", "LocalizedText"); !lt_cls) return;

        auto expand = [&](const patchlib::field &f) -> patchlib::handle {
            if (!f) return {};
            auto arr_w = f.get_value<patchlib::structs::array<patchlib::handle> >();
            if (!arr_w) return {};
            const auto need = base + custom;
            if (arr_w.length() >= need) return {arr_w};
            auto bigger = arr_w.resized(need, patchlib::handle{});
            if (!bigger) return {};
            f.set_value(patchlib::handle(bigger));
            return {bigger};
        };

        const auto name_cache = expand(f_buff_name_cache_);
        const auto desc_cache = expand(f_buff_desc_cache_);

        for (const auto *h: registry_.all()) {
            if (!h || h->runtime_id < 0) continue;
            const int target = h->runtime_id;

            if (name_cache) {
                const std::string key = make_key(h, "buff_name");
                if (auto lt = language_manager::instance().get_localized_text(key.c_str())) {
                    auto w = patchlib::structs::array<patchlib::handle>(name_cache, false);
                    w.set(static_cast<size_t>(target), lt);
                }
            }
            if (desc_cache && h->has_desc && m_from_language_key_) {
                const std::string key = make_key(h, "buff_tooltip");
                const patchlib::structs::string ks(key);
                if (auto lt = m_from_language_key_.invoke<patchlib::handle>(ks)) {
                    auto w = patchlib::structs::array<patchlib::handle>(desc_cache, false);
                    w.set(static_cast<size_t>(target), lt);
                }
            }
        }
        log::info("buff l10n: {} custom buffs localized (base={})", custom, base);
    }

    // -----------------------------------------------------------------------
    // 原生 Buff 栏
    // -----------------------------------------------------------------------
    void buff_registry::update_mouse_state() {
        int x = -1, y = -1;
        if (m_get_mouse_x_) x = m_get_mouse_x_.invoke<int>();
        else if (f_main_mouse_x_) x = f_main_mouse_x_.get_value<int>();
        if (m_get_mouse_y_) y = m_get_mouse_y_.invoke<int>();
        else if (f_main_mouse_y_) y = f_main_mouse_y_.get_value<int>();
        if (x >= 0 && y >= 0) {
            mx_ = x;
            my_ = y;
        }

        bool left = false, right = false;
        if (m_get_mouse_left_) left = m_get_mouse_left_.invoke<bool>();
        else if (f_main_mouse_left_) left = f_main_mouse_left_.get_value<bool>();
        if (m_get_mouse_right_) right = m_get_mouse_right_.invoke<bool>();
        else if (f_main_mouse_right_) right = f_main_mouse_right_.get_value<bool>();
        left_tap_ = left && !left_down_;
        right_tap_ = right && !right_down_;
        left_down_ = left;
        right_down_ = right;
    }

    void buff_registry::draw_buff_tooltip(const terraria_buff_handle_t *h) const {
        if (!h) return;
        // 简化：桌面用 GUI 文字，Android 用 MouseText（原 C 版逻辑）
        const std::string name_key = make_key(h, "buff_name");
        auto name_lt = language_manager::instance().get_localized_text(name_key.c_str());
        if (!name_lt) return;
#if defined(__ANDROID__)
        if (!m_pe_mouse_text_ || !f_main_inst_) return;
        auto inst = f_main_inst_.get_value<patchlib::handle>();
        if (!inst) return;
        int rare = 0, diff = 0, mx = -1, my = -1, sw = -1, sh = -1, push = 0;
        bool no_override = false;
        const patchlib::handle sn = std::move(name_lt);
        patchlib::handle sd;
        if (h->has_desc) {
            const std::string desc_key = make_key(h, "buff_tooltip");
            sd = language_manager::instance().get_localized_text(desc_key.c_str());
        }
        m_pe_mouse_text_.invoke<void>(inst, sn, sd, rare, diff, mx, my, sw, sh, push, no_override);
#else
        gui_overlay::instance().string_text(name_lt.raw(), static_cast<float>(mx_ + 12), static_cast<float>(my_ + 12),
                                            1.0f, 0xFFFFFFFFu);
        if (h->has_desc) {
            const std::string desc_key = make_key(h, "buff_tooltip");
            if (const auto desc_lt = language_manager::instance().get_localized_text(desc_key.c_str()))
                gui_overlay::instance().string_text(desc_lt.raw(), static_cast<float>(mx_ + 12),
                                                    static_cast<float>(my_ + 36), 1.0f, 0xFFD8D8D8u);
        }
#endif
    }

    bool buff_registry::buffs_draw_prefix(patch_handle_t, void **, const patch_method_signature_t *, void *) {
        auto &self = instance();
        self.in_bar_ = 1;
        self.bar_base_ = -1;
        self.bar_epoch_++;
        self.frame_no_++;
        self.update_mouse_state();
        return false;
    }

    void buff_registry::buffs_draw_postfix(patch_handle_t, void **, void *, const patch_method_signature_t *) {
        auto &self = instance();
        self.in_bar_ = 0;
        self.bar_base_ = -1;
    }

    void buff_registry::buff_item_draw_postfix(patch_handle_t, void **, void *, const patch_method_signature_t *) {
        // 仅诊断
    }

    bool buff_registry::item_grid_draw_prefix(patch_handle_t, void **args, const patch_method_signature_t *, void *) {
        auto &self = instance();
        if (!self.in_bar_ || !args || !args[4]) return false;
        if (self.bar_base_ >= 0) return false;
        const int extra = self.local_custom_count();
        if (extra <= 0) return false;
        auto *count = static_cast<int *>(args[4]);
        self.bar_base_ = *count;
        *count += extra;
        return false;
    }

    bool buff_registry::buff_item_draw_prefix(patch_handle_t, void **args, const patch_method_signature_t *, void *) {
        auto &self = instance();
        if (!self.in_bar_ || self.bar_base_ < 0 || !args || !args[1] || !args[2] || !args[3])
            return false;
        const int index = *static_cast<int *>(args[1]);
        if (index < self.bar_base_) return false;

        const int slot = index - self.bar_base_;
        if (slot >= 0 && slot < 64) {
            if (self.bar_mask_epoch_ != self.bar_epoch_) {
                self.bar_mask_epoch_ = self.bar_epoch_;
                self.bar_mask_ = 0;
            }
            const unsigned long long bit = 1ull << slot;
            if (self.bar_mask_ & bit) return true;
            self.bar_mask_ |= bit;
        }

        // 取第 slot 个自定义活跃 buff
        const auto local = self.local_player_obj();
        if (!local) return true;
        const int who = self.player_who(local.raw());
        const active_buff *ab = nullptr;
        int n = slot;
        for (int i = 0; i < self.active_count_; ++i) {
            if (self.active_[i].who != who || self.active_[i].time <= 0) continue;
            if (n-- == 0) {
                ab = &self.active_[i];
                break;
            }
        }
        if (!ab) return true;
        const auto *h = self.find_by_runtime_id(ab->buff_id);
        if (!h) return true;

        const auto *pos = static_cast<const float *>(args[2]);
        // 简化：直接用 GUI 覆盖层画图标 + 倒计时
        gui_overlay::instance().begin_external(nullptr); // 借用当前批次
        if (h->texture)
            gui_overlay::instance().image_raw(h->texture, pos[0], pos[1], 32.0f, 32.0f, 0x66FFFFFFu);
        else
            gui_overlay::instance().rect(pos[0], pos[1], 32.0f, 32.0f, 0xFF00FF00u);

        if (!h->no_time_display) {
            char label[24];
            if (ab->time < 3600) {
                int v = (ab->time + 30) / 60;
                if (v < 1) v = 1;
                std::snprintf(label, sizeof label, "%d s", v);
            } else if (ab->time < 216000) std::snprintf(label, sizeof label, "%d m", (ab->time + 1800) / 3600);
            else std::snprintf(label, sizeof label, "%d h", (ab->time + 108000) / 216000);
            gui_overlay::instance().text(label, pos[0] + 4, pos[1] + 28, 0.8f, 0x66FFFFFFu);
        }
        gui_overlay::instance().end_external();
        return true;
    }

    // -----------------------------------------------------------------------
    // PC Buff 栏
    // -----------------------------------------------------------------------
    bool buff_registry::pc_draw_buff_icon_prefix(patch_handle_t, void **args,
                                                 const patch_method_signature_t *, void *) {
        auto &self = instance();
        if (!args || !args[2] || !args[3]) return false;
        const int x = *static_cast<int *>(args[2]);
        const int y = *static_cast<int *>(args[3]);
        if (self.pc_icon_count_ == 0) {
            self.pc_first_x_ = x;
            self.pc_first_y_ = y;
        } else {
            self.pc_icon_prev_x_ = self.pc_icon_x_;
            self.pc_icon_prev_y_ = self.pc_icon_y_;
        }
        self.pc_icon_x_ = x;
        self.pc_icon_y_ = y;
        self.pc_icon_count_++;
        return false;
    }

    bool buff_registry::pc_buffs_draw_prefix(patch_handle_t, void **, const patch_method_signature_t *, void *) {
        auto &self = instance();
        self.pc_icon_count_ = 0;
        self.pc_icon_x_ = self.pc_icon_y_ = -1;
        self.pc_icon_prev_x_ = self.pc_icon_prev_y_ = -1;
        self.pc_first_x_ = self.pc_first_y_ = -1;
        self.update_mouse_state();
        self.frame_no_++;
        return false;
    }

    void buff_registry::pc_buffs_draw_postfix(patch_handle_t, void **, void *, const patch_method_signature_t *) {
        const auto &self = instance();
        if (self.active_count_ <= 0 || !self.f_player_buff_type_) return;
        const auto local = self.local_player_obj();
        if (!local) return;
        const int who = self.player_who(local.raw());

        int n = 0;
        if (const auto types_w = self.f_player_buff_type_.get_value<patchlib::structs::array<int> >(local)) {
            for (size_t i = 0; i < types_w.length(); ++i)
                if (types_w.at(i) > 0) n++;
        }

        int base_x = 32, base_y = 76, step = 38;
        if (self.pc_icon_count_ > 0 && self.pc_first_x_ >= 0) {
            base_x = self.pc_first_x_;
            base_y = self.pc_first_y_;
            if (self.pc_icon_count_ > 1) {
                const int dx = self.pc_icon_x_ - self.pc_icon_prev_x_;
                if (const int dy = self.pc_icon_y_ - self.pc_icon_prev_y_; dx > 0 && dy == 0) step = dx;
            }
        }

        gui_overlay::instance().begin_external(nullptr);
        for (int i = 0; i < self.active_count_; ++i) {
            const auto &[whoi, buff_id, time] = self.active_[i];
            if (whoi != who || time <= 0) continue;
            const auto *h = self.find_by_runtime_id(buff_id);
            if (!h) continue;
            const auto x = static_cast<float>(base_x + n % 11 * step);
            const auto y = static_cast<float>(base_y + n / 11 * 50);
            if (h->texture)
                gui_overlay::instance().image_raw(h->texture, x, y, 32.0f, 32.0f, 0x66FFFFFFu);
            else
                gui_overlay::instance().rect(x, y, 32.0f, 32.0f, 0xFF00FF00u);
            if (!h->no_time_display) {
                char label[24];
                if (time < 3600) {
                    int v = (time + 30) / 60;
                    if (v < 1) v = 1;
                    std::snprintf(label, sizeof label, "%d s", v);
                } else if (time < 216000) std::snprintf(label, sizeof label, "%d m", (time + 1800) / 3600);
                else std::snprintf(label, sizeof label, "%d h", (time + 108000) / 216000);
                gui_overlay::instance().text(label, x, y + 32.0f, 0.8f, 0x66FFFFFFu);
            }
            n++;
        }
        gui_overlay::instance().end_external();
    }
}
