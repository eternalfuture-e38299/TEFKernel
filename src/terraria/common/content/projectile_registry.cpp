/*******************************************************************************
 * tefkernel - projectile_registry
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

#include "terraria/content/projectile_registry.hpp"

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
#endif

namespace terraria::content {
    using namespace tef;

    namespace {
        constexpr size_t k_max_uniq = 64;
        constexpr size_t k_buf = 64;

        std::string make_key(const terraria_projectile_handle_t *h, const std::string_view prefix) {
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

        patchlib::handle make_placeholder_texture() {
            constexpr int k_size = 32;
            constexpr size_t k_data = static_cast<size_t>(k_size) * k_size * 4;

            std::vector<uint8_t> px(k_data);
            for (int y = 0; y < k_size; ++y) {
                for (int x = 0; x < k_size; ++x) {
                    auto *p = px.data() + (static_cast<size_t>(y) * k_size + x) * 4;
                    const bool border = (x < 2 || y < 2 || x >= k_size - 2 || y >= k_size - 2);
                    p[0] = border ? 40 : 190;
                    p[1] = border ? 20 : 90;
                    p[2] = border ? 60 : 230;
                    p[3] = 255;
                }
            }
            const auto tex = texture2d::instance().create(k_size, k_size, TEXTURE_FORMAT_RGBA32,
                                                          px.data(), px.size());
            if (!tex) return {};

            const auto tex_type = patchlib::type("Microsoft.Xna.Framework.Graphics", "Texture2D");
            return asset_factory::instance().create(tex_type, tex);
        }
    } // namespace

    projectile_registry &projectile_registry::instance() noexcept {
        static projectile_registry inst;
        return inst;
    }

    // -----------------------------------------------------------------------
    // 生命周期
    // -----------------------------------------------------------------------
    void projectile_registry::early_init() {
        projectile_class_ = patchlib::type("Terraria", "Projectile");
        if (!projectile_class_) {
            log::error("projectile_registry: Terraria.Projectile not found");
            return;
        }

        f_type_ = projectile_class_.get_field("type");
        f_active_ = projectile_class_.get_field("active");
        f_velocity_ = projectile_class_.get_field("velocity");

#if defined(__ANDROID__)
        if (f_type_) {
            const size_t off = il2cpp_field_get_offset(f_type_.raw());
            if (off > 0) type_offset_ = int(off);
        }
#endif

        f_uses_id_static_ = projectile_class_.get_field("usesIDStaticNPCImmunity");
        f_immunity_identity_ = projectile_class_.get_field("immunityIdentity");

        if (const auto main_cls = patchlib::type("Terraria", "Main")) {
            f_main_players_ = main_cls.get_field("player");
            f_proj_frames_ = main_cls.get_field("projFrames");
        }
        if (const auto player_cls = patchlib::type("Terraria", "Player"))
            f_owned_counts_ = player_cls.get_field("ownedProjectileCounts");
        if (const auto id_cls = patchlib::type("Terraria.ID", "ProjectileID"))
            f_projectile_id_count_ = id_cls.get_field("Count");
        if (const auto tex_cls = patchlib::type("Terraria.GameContent", "TextureAssets"))
            f_texture_projectile_ = tex_cls.get_field("Projectile");
        if (const auto lang_cls = patchlib::type("Terraria.Localization", "Lang"))
            f_proj_name_cache_ = lang_cls.get_field("_projectileNameCache");

        m_set_defaults_ = projectile_class_.get_method("SetDefaults", 1);
        m_ai_ = projectile_class_.get_method("AI", 0);
        m_kill_ = projectile_class_.get_method("Kill", 0);
        m_damage_ = projectile_class_.get_method("Damage", 0);
        m_move_ = projectile_class_.get_method("HandleMovement", 1);

        if (m_set_defaults_)
            patchlib_install_prepost_hook(m_set_defaults_.raw(), nullptr,
                                          set_defaults_postfix);
        if (m_ai_)
            patchlib_install_prepost_hook(m_ai_.raw(),
                                          ai_prefix,
                                          ai_postfix);
        if (m_kill_)
            patchlib_install_prepost_hook(m_kill_.raw(), nullptr,
                                          kill_postfix);
        if (m_damage_)
            patchlib_install_prepost_hook(m_damage_.raw(),
                                          damage_prefix,
                                          damage_postfix);
        if (m_move_)
            patchlib_install_prepost_hook(m_move_.raw(),
                                          handle_movement_prefix,
                                          handle_movement_postfix);

        if (const auto npc_cls = patchlib::type("Terraria", "NPC")) {
            if (const auto m = npc_cls.get_method("StrikeNPC", 6))
                patchlib_install_prepost_hook(m.raw(), nullptr,
                                              strike_npc_postfix);
        }
        if (const auto player_cls = patchlib::type("Terraria", "Player")) {
            if (const auto m = player_cls.get_method("Hurt", 8))
                patchlib_install_prepost_hook(m.raw(), nullptr,
                                              hurt_postfix);
        }
    }

    void projectile_registry::on_main_initialize_prefix() {
        if (registry_.empty()) return;
        base_count_ = id_count();
        ids_assigned_ = assign_ids(base_count_);
        for (auto *h: registry_.all())
            if (h->projectile_ops.early_init) h->projectile_ops.early_init(h);
    }

    void projectile_registry::on_main_initialize_postfix() {
        if (registry_.empty()) return;
        resize();
        init_texture2d();
        init_localized_text();
        for (auto *h: registry_.all())
            if (h->projectile_ops.init_static) h->projectile_ops.init_static(h);
    }

    void projectile_registry::shutdown() {
        registry_.all().clear();
        damage_stack_.fill({});
        move_projectile_.reset();
        ids_assigned_ = false;
    }

    // -----------------------------------------------------------------------
    // 注册 / 注销 / ID
    // -----------------------------------------------------------------------
    bool projectile_registry::register_projectile(terraria_projectile_handle_t *h) {
        if (!h || !h->parent_modloader_id || !h->parent_id || !h->internal_name)
            return false;
        const bool ok = registry_.register_entity(h);
        if (ok && ids_assigned_)
            log::warn("projectile registered after IDs assigned: {}.{}",
                      h->parent_id, h->internal_name);
        return ok;
    }

    bool projectile_registry::unregister_by_id(const int runtime_id) {
        if (runtime_id < 0) return false;
        const bool ok = registry_.unregister_by_id(runtime_id);
        if (ok && ids_assigned_) assign_ids(base_count_);
        return ok;
    }

    bool projectile_registry::unregister(terraria_projectile_handle_t *h) {
        if (!h) return false;
        const bool ok = registry_.unregister(h);
        if (ok && ids_assigned_) assign_ids(base_count_);
        return ok;
    }

    bool projectile_registry::assign_ids(const int base) {
        if (registry_.empty()) {
            ids_assigned_ = true;
            return true;
        }
        base_count_ = base;
        return registry_.assign_ids(base);
    }

    terraria_projectile_handle_t *projectile_registry::get(const int runtime_id) const {
        return registry_.get(runtime_id);
    }

    tefstd_vector_t *projectile_registry::get_projectiles() {
        return registry_.raw_tefstd_vector();
    }

    terraria_projectile_handle_t *projectile_registry::find(const std::string_view loader,
                                                            const std::string_view mod,
                                                            const std::string_view name) const {
        return registry_.find(loader, mod, name);
    }

    int projectile_registry::id_count() const {
        if (f_projectile_id_count_) {
            if (const auto c = f_projectile_id_count_.get_value<short>(); c > 0) return c;
        }
        return 0;
    }

    // -----------------------------------------------------------------------
    // type 读取
    // -----------------------------------------------------------------------
    int projectile_registry::get_projectile_type(patch_handle_t inst) const {
        if (!inst) return -1;
#if defined(__ANDROID__)
        if (type_offset_ >= 0)
            return *reinterpret_cast<const int *>(reinterpret_cast<const char *>(inst) + type_offset_);
#endif
        int type = -1;
        if (f_type_) type = f_type_.get_value<int>(inst);
        return type;
    }

    // -----------------------------------------------------------------------
    // 帧图 / 玩家数组 / AI 数组
    // -----------------------------------------------------------------------
    void projectile_registry::register_item_animation(const terraria_projectile_handle_t *h, const int type) const {
        if (!h || !f_proj_frames_) return;
        auto arr_w = f_proj_frames_.get_value<patchlib::structs::array<int> >();
        if (!arr_w) return;
        if (static_cast<size_t>(type) >= arr_w.length()) return;
        const int frames = h->frames > 0 ? h->frames : 1;
        arr_w.set(static_cast<size_t>(type), frames);
    }

    void projectile_registry::ensure_player_owned_counts(int new_size) {
        if (!f_main_players_ || !f_owned_counts_) return;
        const auto players_w = f_main_players_.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!players_w) return;

        int patched = 0;
        for (size_t i = 0; i < players_w.length(); ++i) {
            auto player = players_w.at(i);
            if (!player) continue;
            auto counts_w = f_owned_counts_.get_value<patchlib::structs::array<int> >(player);
            if (!counts_w) continue;
            if (static_cast<int>(counts_w.length()) >= new_size) continue;
            if (auto bigger = counts_w.resized(static_cast<size_t>(new_size), 0)) {
                f_owned_counts_.set_value(player, patchlib::handle(bigger));
                patched++;
            }
        }
        if (patched > 0)
            log::info("projectile: ownedProjectileCounts patched {} players -> {}",
                      patched, new_size);
    }

    void projectile_registry::ensure_projectile_ai_arrays(patch_handle_t inst) {
        static patchlib::field f_ai, f_local_ai, f_max_ai;
        static bool resolved = false;
        if (!resolved) {
            resolved = true;
            if (const auto cls = patchlib::type("Terraria", "Projectile")) {
                f_ai = cls.get_field("ai");
                f_local_ai = cls.get_field("localAI");
                f_max_ai = cls.get_field("maxAI");
            }
        }
        if (!f_ai || !f_local_ai) return;
        if (f_ai.get_type() != PATCH_OBJECT) return;

        int n = 3;
        if (f_max_ai) n = f_max_ai.get_value<int>();
        if (n < 1) n = 3;

        const auto float_t = patchlib::type::get_basic_type(PATCH_FLOAT);
        if (!float_t) return;

        auto arr = f_ai.get_value<patchlib::handle>(inst);
        if (!arr) {
            if (auto created = patchlib::structs::array<float>(static_cast<size_t>(n), float_t))
                f_ai.set_value(
                    inst, patchlib::handle(created));
        }
        arr = f_local_ai.get_value<patchlib::handle>(inst);
        if (!arr) {
            if (auto created = patchlib::structs::array<float>(static_cast<size_t>(n), float_t))
                f_local_ai.set_value(
                    inst, patchlib::handle(created));
        }
    }

    void projectile_registry::clamp_static_immunity_identity(patch_handle_t inst) {
        if (!inst || !f_immunity_identity_ || !f_uses_id_static_) return;
        if (!f_uses_id_static_.get_value<bool>(inst)) return;
        int identity = f_immunity_identity_.get_value<int>(inst);
        if (const int base = id_count(); base <= 0 || (identity >= 0 && identity < base)) return;
        static bool logged = false;
        f_immunity_identity_.set_value(inst, 0);
        if (!logged) {
            logged = true;
            log::debug("projectile: immunityIdentity {} -> 0 (no 2D resize)", identity);
        }
    }

    // -----------------------------------------------------------------------
    // resize
    // -----------------------------------------------------------------------
    void projectile_registry::fill_new_slots_with_modal(const patchlib::handle &old_arr,
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
            for (int j = 0; j < nu; ++j)
                if (keys[j] == key) {
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

    void projectile_registry::resize_static_array(const patchlib::field &f,
                                                  std::string_view name,
                                                  const int base, int new_size,
                                                  const bool modal, const int int_fill,
                                                  const bool fill_with_int) {
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
                fill_new_slots_with_modal(patchlib::handle(arr_w), patchlib::handle(bigger),
                                          len, static_cast<size_t>(new_size));
            f.set_value(patchlib::handle(bigger));
        }
        log::info("projectile resize: {} {} -> {}", name, len, new_size);
    }

    void projectile_registry::resize_projectile_set_factory(int new_size) {
        const auto pid = patchlib::type("Terraria.ID", "ProjectileID");
        if (!pid) return;
        const auto sets = pid.get_inner_type("Sets");
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
        log::info("projectile resize: ProjectileID.Sets.Factory._size -> {}", new_size);
    }

    void projectile_registry::resize() {
        const int base = id_count();
        if (base <= 0) return;
        const int new_size = base + static_cast<int>(registry_.size());

        // TextureAssets.Projectile[]（引用数组，不做众数填充）
        resize_static_array(f_texture_projectile_, "TextureAssets.Projectile",
                            base, new_size, false, 0, false);

        // ProjectileID.Sets.*
        if (const auto id_cls = patchlib::type("Terraria.ID", "ProjectileID")) {
            if (const auto sets = id_cls.get_inner_type("Sets")) {
                for (auto &f: sets.get_fields()) {
                    if (const std::string name = f.get_name(); name.empty() || name == "Factory" || name == "Count")
                        continue;
                    if (!f.is_static() || f.is_const()) continue;
                    auto arr_w = f.get_value<patchlib::structs::array<uint8_t> >();
                    if (!arr_w) continue;
                    const size_t len = arr_w.length();
                    if (!(len + 8 >= static_cast<size_t>(base) && len < static_cast<size_t>(new_size))) continue;
                    auto bigger = arr_w.resized(static_cast<size_t>(new_size), 0);
                    if (!bigger) continue;
                    fill_new_slots_with_modal(patchlib::handle(arr_w), patchlib::handle(bigger),
                                              len, static_cast<size_t>(new_size));
                    f.set_value(patchlib::handle(bigger));
                }
            }
            resize_projectile_set_factory(new_size);
        }

        // Main.projFrames/projPet/projHook/projHostile
        if (const auto main_cls = patchlib::type("Terraria", "Main")) {
            if (const auto f = main_cls.get_field("projFrames"))
                resize_static_array(f, "Main.projFrames", base, new_size, false, 1, true);
            if (const auto f = main_cls.get_field("projPet"))
                resize_static_array(f, "Main.projPet", base, new_size, true, 0, false);
            if (const auto f = main_cls.get_field("projHook"))
                resize_static_array(f, "Main.projHook", base, new_size, true, 0, false);
            if (const auto f = main_cls.get_field("projHostile"))
                resize_static_array(f, "Main.projHostile", base, new_size, true, 0, false);
        }

        // Player.ownedProjectileCounts
        ensure_player_owned_counts(new_size);
    }

    // -----------------------------------------------------------------------
    // 贴图 / 本地化
    // -----------------------------------------------------------------------
    void projectile_registry::init_texture2d() {
        if (!f_texture_projectile_) return;
        auto arr_w = f_texture_projectile_.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!arr_w) return;

        const int base = id_count();
        patchlib::handle placeholder;
        int done = 0;
        for (auto *h: registry_.all()) {
            if (!h || h->runtime_id < 0 || static_cast<size_t>(h->runtime_id) >= arr_w.length()) continue;
            patchlib::handle tex;
            if (h->projectile_ops.get_texture)
                tex = patchlib::handle(h->projectile_ops.get_texture(h), true);
            if (!tex) {
                if (!placeholder) placeholder = make_placeholder_texture();
                if (!placeholder) break;
                tex = placeholder;
            }
            arr_w.set(static_cast<size_t>(h->runtime_id), tex);
            done++;
        }
        log::info("projectile textures: {} set (base={})", done, base);
    }

    void projectile_registry::init_localized_text() {
        const int base = id_count();
        const int custom = static_cast<int>(registry_.size());
        if (custom <= 0 || base <= 0) return;
        if (!f_proj_name_cache_) return;

        auto arr_w = f_proj_name_cache_.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!arr_w) {
            const auto lt = patchlib::type("Terraria.Localization", "LocalizedText");
            if (!lt) return;
            auto created = patchlib::structs::array<patchlib::handle>(static_cast<size_t>(base + custom), lt);
            if (!created) return;
            f_proj_name_cache_.set_value(patchlib::handle(created));
            arr_w = created;
        }
        if (arr_w.length() < base + custom) {
            auto bigger = arr_w.resized(base + custom, patchlib::handle{});
            if (!bigger) return;
            f_proj_name_cache_.set_value(patchlib::handle(bigger));
            arr_w = bigger;
        }

        for (size_t i = 0; i < registry_.size(); ++i) {
            const auto *h = registry_.all()[i];
            if (!h) continue;
            const int target = base + static_cast<int>(i);
            const std::string key = make_key(h, "proj_name");
            auto lt = language_manager::instance().get_localized_text(key.c_str());
            if (!lt) continue;
            arr_w.set(static_cast<size_t>(target), lt);
        }
        log::info("projectile l10n: {} custom projectiles localized (base={})",
                  custom, base);
    }

    // -----------------------------------------------------------------------
    // 钩子实现
    // -----------------------------------------------------------------------
    void projectile_registry::set_defaults_postfix(patch_handle_t inst, void **args, void *,
                                                   const patch_method_signature_t *) {
        if (!inst || !args || !args[0]) return;
        auto &self = instance();
        const int type = *static_cast<int *>(args[0]);
        const int nid = type - self.base_count_;
        if (nid < 0 || nid >= static_cast<int>(self.registry_.size())) return;

        int current_type = 0;
        if (self.f_type_) current_type = self.f_type_.get_value<int>(inst);
        if (current_type != type)
            log::warn("projectile SetDefaults rewrote type: {} -> {}", type, current_type);
        if (self.f_type_) self.f_type_.set_value(inst, type);
        if (self.f_active_) self.f_active_.set_value(inst, true);

        ensure_projectile_ai_arrays(inst);

        auto *h = self.registry_.all()[nid];
        if (h) {
            if (self.f_proj_frames_) self.register_item_animation(h, type);
        }

        self.ensure_player_owned_counts(self.base_count_ + static_cast<int>(self.registry_.size()));

        if (h && h->projectile_ops.set_defaults) h->projectile_ops.set_defaults(h, inst);
        self.clamp_static_immunity_identity(inst);
    }

    bool projectile_registry::ai_prefix(patch_handle_t inst, void **,
                                        const patch_method_signature_t *, void *) {
        if (!inst) return false;
        auto &self = instance();
        const int type = self.get_projectile_type(inst);
        const int nid = type - self.base_count_;
        if (nid < 0 || nid >= static_cast<int>(self.registry_.size())) return false;
        auto *h = self.registry_.all()[nid];
        if (!h || h->no_ai_hook || !h->projectile_ops.ai) return false;
        if (h->ai_after_vanilla) return false;
        h->projectile_ops.ai(h, inst);
        return true;
    }

    void projectile_registry::ai_postfix(patch_handle_t inst, void **, void *,
                                         const patch_method_signature_t *) {
        if (!inst) return;
        auto &self = instance();
        const int type = self.get_projectile_type(inst);
        const int nid = type - self.base_count_;
        if (nid < 0 || nid >= static_cast<int>(self.registry_.size())) return;
        auto *h = self.registry_.all()[nid];
        if (!h || h->no_ai_hook || !h->ai_after_vanilla || !h->projectile_ops.ai) return;
        h->projectile_ops.ai(h, inst);
    }

    void projectile_registry::kill_postfix(patch_handle_t inst, void **, void *,
                                           const patch_method_signature_t *) {
        if (!inst) return;
        auto &self = instance();
        const int type = self.get_projectile_type(inst);
        const int nid = type - self.base_count_;
        if (nid < 0 || nid >= static_cast<int>(self.registry_.size())) return;
        if (auto *h = self.registry_.all()[nid]; h && h->projectile_ops.on_kill) h->projectile_ops.on_kill(h, inst);
    }

    bool projectile_registry::damage_prefix(patch_handle_t inst, void **,
                                            const patch_method_signature_t *, void *) {
        auto &self = instance();
        self.clamp_static_immunity_identity(inst);
        if (self.damage_depth_ < k_max_damage_depth) {
            self.damage_stack_[self.damage_depth_] = patchlib::handle(inst, false);
        }
        self.damage_depth_++;
        return false;
    }

    void projectile_registry::damage_postfix(patch_handle_t, void **, void *,
                                             const patch_method_signature_t *) {
        auto &self = instance();
        if (self.damage_depth_ > 0) {
            self.damage_depth_--;
            if (self.damage_depth_ < k_max_damage_depth)
                self.damage_stack_[self.damage_depth_].reset();
        }
    }

    void projectile_registry::strike_npc_postfix(patch_handle_t inst, void **args, void *,
                                                 const patch_method_signature_t *) {
        auto &self = instance();
        if (self.damage_depth_ <= 0 || self.damage_depth_ > k_max_damage_depth) return;
        const auto proj = self.damage_stack_[self.damage_depth_ - 1];
        if (!proj) return;
        const int type = self.get_projectile_type(proj.raw());
        const int nid = type - self.base_count_;
        if (nid < 0 || nid >= static_cast<int>(self.registry_.size())) return;
        auto *h = self.registry_.all()[nid];
        if (!h || !h->projectile_ops.on_hit_npc) return;
        const int damage = args && args[0] ? *static_cast<int *>(args[0]) : 0;
        const int dir = args && args[2] ? *static_cast<int *>(args[2]) : 0;
        const bool crit = args && args[3] ? *static_cast<bool *>(args[3]) : false;
        h->projectile_ops.on_hit_npc(h, proj.raw(), inst, damage, dir, crit);
    }

    void projectile_registry::hurt_postfix(patch_handle_t inst, void **args, void *,
                                           const patch_method_signature_t *) {
        auto &self = instance();
        if (self.damage_depth_ <= 0 || self.damage_depth_ > k_max_damage_depth) return;
        const auto proj = self.damage_stack_[self.damage_depth_ - 1];
        if (!proj) return;
        const int type = self.get_projectile_type(proj.raw());
        const int nid = type - self.base_count_;
        if (nid < 0 || nid >= static_cast<int>(self.registry_.size())) return;
        auto *h = self.registry_.all()[nid];
        if (!h || !h->projectile_ops.on_hit_player) return;
        const int damage = args && args[1] ? *static_cast<int *>(args[1]) : 0;
        const int dir = args && args[2] ? *static_cast<int *>(args[2]) : 0;
        const bool pvp = args && args[3] ? *static_cast<bool *>(args[3]) : false;
        const bool crit = args && args[5] ? *static_cast<bool *>(args[5]) : false;
        h->projectile_ops.on_hit_player(h, proj.raw(), inst, damage, dir, pvp, crit);
    }

    bool projectile_registry::handle_movement_prefix(patch_handle_t inst, void **,
                                                     const patch_method_signature_t *, void *) {
        auto &self = instance();
        self.move_projectile_.reset();
        self.move_pending_ = false;
        if (!inst) return false;
        const int type = self.get_projectile_type(inst);
        if (const int nid = type - self.base_count_; nid < 0 || nid >= static_cast<int>(self.registry_.size()))
            return
                    false;

        self.move_pending_ = true;
        self.move_vx_ = 0.0f;
        self.move_vy_ = 0.0f;
        if (self.f_velocity_) {
            if (auto v = self.f_velocity_.get_value<patchlib::handle>(inst)) {
                float arr[2] = {0.0f, 0.0f};
                patchlib_field_get_value(self.f_velocity_.raw(), inst, arr);
                self.move_vx_ = arr[0];
                self.move_vy_ = arr[1];
            }
            self.move_projectile_ = patchlib::handle(inst, false);
        }
        return false;
    }

    void projectile_registry::handle_movement_postfix(patch_handle_t inst, void **, void *,
                                                      const patch_method_signature_t *) {
        auto &self = instance();
        if (!self.move_pending_) return;
        self.move_pending_ = false;
        const auto proj = self.move_projectile_;
        self.move_projectile_.reset();
        if (!proj) return;
        const int type = self.get_projectile_type(proj.raw());
        const int nid = type - self.base_count_;
        if (nid < 0 || nid >= static_cast<int>(self.registry_.size())) return;
        auto *h = self.registry_.all()[nid];
        if (!h || !h->projectile_ops.on_tile_collide) return;
        if (!self.f_velocity_) return;
        float arr[2] = {0.0f, 0.0f};
        patchlib_field_get_value(self.f_velocity_.raw(), inst, arr);
        if (arr[0] != self.move_vx_ || arr[1] != self.move_vy_)
            h->projectile_ops.on_tile_collide(h, proj.raw(), self.move_vx_, self.move_vy_);
    }
}
