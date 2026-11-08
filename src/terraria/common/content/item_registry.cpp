/*******************************************************************************
 * tefkernel - item_registry
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

#include "terraria/content/item_registry.hpp"

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "tef/core/log.hpp"
#include "tef/patchlib/struct/array.hpp"
#include "tef/patchlib/struct/dictionary.hpp"
#include "tef/patchlib/struct/list.hpp"
#include "tef/patchlib/struct/string.hpp"
#include "terraria/content/asset_factory.hpp"
#include "terraria/content/language_manager.hpp"
#include "terraria/content/texture2d.hpp"
#include "abi_impl/patchlib/il2cpp_api.h"

namespace terraria::content {
    using namespace tef;

    namespace {
        // 统一的 key 构造：item_name_<loader>_<mod>.<name> / item_tooltip_...
        std::string make_key(const terraria_item_handle_t *h, const std::string_view prefix) {
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

        // 持久化 id 字符串：<loader>_<mod>_<name>
        std::string persist_id(const terraria_item_handle_t *h) {
            std::string s;
            s += h->parent_modloader_id ? h->parent_modloader_id : "?";
            s += '_';
            s += h->parent_id ? h->parent_id : "?";
            s += '_';
            s += h->internal_name ? h->internal_name : "?";
            return s;
        }
    } // namespace

    item_registry &item_registry::instance() noexcept {
        static item_registry inst;
        return inst;
    }

    // ============================================================
    // 未知物品
    // ============================================================
    namespace {
        patchlib::handle unknown_item_texture_impl() {
            constexpr int k_size = 64;
            constexpr int k_cell = k_size / 2;
            constexpr size_t k_data = static_cast<size_t>(k_size) * k_size * 4;

            std::vector<uint8_t> pixels(k_data);
            auto *row_px = reinterpret_cast<uint32_t *>(pixels.data());
            for (int y = 0; y < k_size; ++y) {
#if defined(__ANDROID__)
                const int src_y = k_size - 1 - y;
#else
                const int src_y = y;
#endif
                uint32_t *row = row_px + static_cast<size_t>(y) * k_size;
                for (int x = 0; x < k_size; ++x) {
                    if (x == k_cell || src_y == k_cell)
                        row[x] = 0xFF323232u;
                    else {
                        const bool left = x < k_cell;
                        const bool top = src_y < k_cell;
                        row[x] = (top == left) ? 0xFF000000u : 0xFFA45067u;
                    }
                }
            }

            const auto tex = texture2d::instance().create(k_size, k_size,
                                                          TEXTURE_FORMAT_RGBA32,
                                                          pixels.data(), pixels.size());
            if (!tex) return {};
            const auto tex_type = patchlib::type("Microsoft.Xna.Framework.Graphics",
                                                 "Texture2D");
            return asset_factory::instance().create(tex_type, tex);
        }
    } // namespace

    // ============================================================
    // 生命周期
    // ============================================================
    void item_registry::early_init() {
        unknown_item_.parent_modloader_id = "tefkernel";
        unknown_item_.parent_id = "tefkernel";
        unknown_item_.internal_name = "unknown";
        unknown_item_.has_tooltip = false;
        unknown_item_.runtime_id = -1;
        unknown_item_.sacrifice_count = 9999;
        unknown_item_.item_ops.set_defaults = nullptr;
        unknown_item_.item_ops.get_texture = [](terraria_item_handle_t *) -> patch_handle_t {
            auto tex = unknown_item_texture_impl();
            return tex.release();
        };

        registry_.register_entity(&unknown_item_);

        if (const auto cs = patchlib::type("Terraria.ID", "ContentSamples"))
            if (const auto m = cs.get_method("Initialize", 0))
                patchlib_install_prepost_hook(m.raw(), nullptr, content_samples_postfix);

        if (const auto c = patchlib::type("Terraria.GameContent.Creative",
                                          "CreativeItemSacrificesCatalog"))
            if (const auto m = c.get_method("Initialize", 0))
                patchlib_install_prepost_hook(m.raw(), nullptr, creative_sacrifices_postfix);

        if (const auto id_cls = patchlib::type("Terraria.ID", "ItemID"))
            f_item_id_count_ = id_cls.get_field("Count");

        const auto item_cls = patchlib::type("Terraria", "Item");
        if (!item_cls) {
            log::error("item_registry: Terraria.Item not found");
            return;
        }

        engine_.f_type = item_cls.get_field("type");
        engine_.f_stack = item_cls.get_field("stack");
        engine_.f_item_prefix = item_cls.get_field("prefix");
        engine_.m_net_defaults = item_cls.get_method("netDefaults", 1);
        engine_.m_prefix = item_cls.get_method("Prefix", 1);

        m_reset_stats_ = item_cls.get_method("ResetStats", 1);
        m_rebuild_tooltip_ = item_cls.get_method("RebuildTooltip", 0);
        m_get_rollable_ = item_cls.get_method("GetRollablePrefixes", 0);

        if (const auto m = item_cls.get_method("SetDefaults", 2))
            patchlib_install_prepost_hook(m.raw(), nullptr, set_defaults_postfix);

        if (const auto player = patchlib::type("Terraria", "Player")) {
            if (const auto m = player.get_method("ItemCheck_StartActualUse", 1))
                patchlib_install_prepost_hook(m.raw(), nullptr, start_actual_use_postfix);
            if (const auto m = player.get_method("ItemCheck_CheckCanUse_Inner", 2))
                patchlib_install_prepost_hook(m.raw(), check_can_use_prefix, nullptr);
        }

        if (const auto main = patchlib::type("Terraria", "Main"))
            if (const auto m = main.get_method("InitializeItemAnimations", 0))
                patchlib_install_prepost_hook(m.raw(), nullptr, init_item_animations_postfix);

        if (const auto lang = patchlib::type("Terraria", "Lang")) {
            f_name_cache_ = lang.get_field("_itemNameCache");
            f_tooltip_cache_ = lang.get_field("_itemTooltipCache");
            if (const auto m = lang.get_method("GetItemName", 1))
                patchlib_install_prepost_hook(m.raw(), nullptr, get_item_name_postfix);
        }

        if (const auto sorting = patchlib::type("Terraria.UI", "ItemSorting")) {
            f_sorting_whitelists_ = sorting.get_field("_layerWhiteLists");
            if (const auto m = sorting.get_method("SetupWhiteLists", 0))
                patchlib_install_prepost_hook(m.raw(), nullptr, setup_white_lists_postfix);
        }

        install_instance_array_hooks();

        if (const auto ta = patchlib::type("Terraria.GameContent", "TextureAssets")) {
            f_texture_item_ = ta.get_field("Item");
            f_texture_item_flame_ = ta.get_field("ItemFlame");
        }

        if (const auto tt = patchlib::type("Terraria.UI", "ItemTooltip")) {
#if defined(__ANDROID__)
            m_from_language_key_ = tt.get_method("FromLanguageKey", 2);
#else
            m_from_language_key_ = tt.get_method("FromLanguageKey", 1);
#endif
        }

        // PC：LoadTextures 之后重新写入自定义贴图
#if !defined(__ANDROID__)
        if (const auto ai = patchlib::type("Terraria.Initializers", "AssetInitializer"))
            if (const auto m = ai.get_method("LoadTextures", 1))
                patchlib_install_prepost_hook(m.raw(), nullptr, load_textures_postfix);
#endif
    }

    void item_registry::on_main_initialize_prefix() {
        base_count_ = f_item_id_count_ ? f_item_id_count_.get_value<short>() : 0;
        if (registry_.empty()) return;

        ids_assigned_ = assign_ids(base_count_);
        resize(false);

        for (auto *h: registry_.all())
            if (h->item_ops.early_init) h->item_ops.early_init(h);
    }

    void item_registry::on_main_initialize_postfix() {
        if (registry_.empty()) {
            log::warn("on_main_initialize_postfix: registry empty, skip");
            return;
        }

        resize(true);
        log::info("on_main_initialize_postfix: resize done");

        init_texture2d();
        log::info("on_main_initialize_postfix: init_texture2d done");

        init_localized_text();
        log::info("on_main_initialize_postfix: init_localized_text done");

        for (auto *h: registry_.all())
            if (h->item_ops.init_static) h->item_ops.init_static(h);
        log::info("on_main_initialize_postfix: init_static done");

        apply_prefix_classes_impl();
        log::info("on_main_initialize_postfix: done");
    }

    void item_registry::shutdown() {
        pending_accessories_.clear();
        registry_.all().clear();
        engine_ = {};
        base_count_ = 0;
        ids_assigned_ = false;
        prefix_decl_count_ = 0;
        prefix_sets_state_ = -1;
    }

    // ============================================================
    // 注册 / 注销 / ID
    // ============================================================
    bool item_registry::register_item(terraria_item_handle_t *h) {
        if (!h || !h->parent_modloader_id || !h->parent_id || !h->internal_name)
            return false;
        const bool ok = registry_.register_entity(h);
        if (ok && ids_assigned_)
            log::warn("item registered after IDs assigned: {}.{}.{}",
                      h->parent_modloader_id, h->parent_id, h->internal_name);
        return ok;
    }

    bool item_registry::unregister_item_by_id(const int runtime_id) {
        if (runtime_id < 0) return false;
        for (const auto *h: registry_.all()) {
            if (h->runtime_id == runtime_id) {
                prefix_decl_remove(h);
                const bool ok = registry_.unregister_by_id(runtime_id);
                if (ok && ids_assigned_) assign_ids(base_count_);
                return ok;
            }
        }
        return false;
    }

    bool item_registry::unregister_item(terraria_item_handle_t *h) {
        if (!h) return false;
        prefix_decl_remove(h);
        const bool ok = registry_.unregister(h);
        if (ok && ids_assigned_) assign_ids(base_count_);
        return ok;
    }

    terraria_item_handle_t *item_registry::get_item(const int runtime_id) const {
        return registry_.get(runtime_id);
    }

    tefstd_vector_t *item_registry::get_items() {
        return registry_.raw_tefstd_vector();
    }

    void item_registry::apply_prefix_classes() {
        apply_prefix_classes_impl();
    }

    void item_registry::set_prefix_class(terraria_item_handle_t *h, const int cls) {
        if (!h || cls < 0 || cls > 7) return;
        for (size_t i = 0; i < prefix_decl_count_; ++i) {
            if (prefix_decls_[i].handle == h) {
                prefix_decls_[i].cls = static_cast<uint8_t>(cls);
                return;
            }
        }
        if (prefix_decl_count_ >= k_max_prefix_decls) {
            log::warn("prefix: too many declarations");
            return;
        }
        prefix_decls_[prefix_decl_count_++] = {.handle = h, .cls = static_cast<uint8_t>(cls)};
    }

    int item_registry::reserve_accessory_slot(const char *slot) {
        const accessory_slot *s = nullptr;
        for (auto &x: k_accessory_slots)
            if (std::strcmp(x.slot, slot) == 0) {
                s = &x;
                break;
            }
        if (!s) return -1;

        const auto armor = patchlib::type("Terraria.ID", "ArmorIDs");
        if (!armor) return -1;
        const auto cls = armor.get_inner_type(s->armor_class);
        if (!cls) return -1;

        const auto f_count = cls.get_field("Count");
        const int old = f_count ? f_count.get_value<int>() : -1;
        if (old <= 0) return -1;

        const int new_count = old + 1;
        f_count.set_value(new_count);

        if (const auto sets = cls.get_inner_type("Sets")) {
            for (auto &f: sets.get_fields()) {
                auto ft = patchlib::type(il2cpp_field_get_parent(f.raw()), true);
                if (!ft) continue;
                if (auto tn = ft.get_name(); tn.find("[]") == std::string::npos) continue;

                auto a = f.get_value<patchlib::handle>();
                if (!a) continue;
                auto aw = patchlib::structs::array<patchlib::handle>(a, false);
                if (aw.length() != static_cast<size_t>(old)) continue;
                if (auto b = aw.resized(static_cast<size_t>(new_count), patchlib::handle{}))
                    f.set_value(
                        patchlib::handle(b));
            }

            // 复制模板 WingStats
            if (s->template_index >= 0) {
                if (const auto f_stats = sets.get_field("Stats")) {
                    if (const auto stats = f_stats.get_value<patchlib::handle>()) {
                        const size_t elem = patchlib_array_element_size(stats.raw());
                        if (const auto sw = patchlib::structs::array<uint8_t>(stats, false);
                            elem > 0 && sw.length() > static_cast<size_t>(s->template_index)) {
                            std::vector<uint8_t> buf(elem);
                            if (patchlib_array_at(stats.raw(), static_cast<size_t>(s->template_index), buf.data()))
                                patchlib_array_set(stats.raw(), static_cast<size_t>(new_count) - 1, buf.data());
                        }
                    }
                }
            }

            // SetFactory._size
            if (const auto f_factory = sets.get_field("Factory")) {
                if (const auto factory = f_factory.get_value<patchlib::handle>()) {
                    if (const auto sf = patchlib::type("Terraria.ID", "SetFactory"))
                        if (auto f_size = sf.get_field("_size"))
                            f_size.set_value(factory, new_count);
                }
            }
        }
        return old;
    }

    bool item_registry::set_accessory_texture(const char *slot, const int index, patch_handle_t asset) {
        if (!slot || index < 0 || !asset) return false;
        const patchlib::handle a(asset, false);
        if (accessory_assets_loaded_) return apply_accessory_texture(slot, index, a);
        for (auto &x: k_accessory_slots) {
            if (std::strcmp(x.slot, slot) == 0) {
                pending_accessories_.push_back({.slot = &x, .index = index, .asset = a});
                return true;
            }
        }
        return false;
    }

    bool item_registry::accessory_texture_ready(const char *slot, const int index) {
        if (index < 0) return false;
        const auto arr = accessory_texture_array(slot);
        if (!arr) return false;
        const auto arr_w = patchlib::structs::array<patchlib::handle>(arr, false);
        if (arr_w.length() <= static_cast<size_t>(index)) return false;
        return static_cast<bool>(arr_w.at(static_cast<size_t>(index)));
    }

    bool item_registry::set_wing_stats(const int index, const int fly_time, const float fly_speed,
                                       const float accel, const bool hover,
                                       const float hover_speed, const float hover_accel) {
        if (index < 0) return false;
        const auto armor = patchlib::type("Terraria.ID", "ArmorIDs");
        if (!armor) return false;
        const auto wing = armor.get_inner_type("Wing");
        if (!wing) return false;
        const auto sets = wing.get_inner_type("Sets");
        if (!sets) return false;
        const auto f_stats = sets.get_field("Stats");
        if (!f_stats) return false;
        const auto stats = f_stats.get_value<patchlib::handle>();
        if (!stats) return false;

        struct wing_stats {
            int fly_time;
            float acc_run_speed_override;
            float acc_run_acceleration_mult;
            bool has_down_hover_stats;
            float down_hover_speed_override;
            float down_hover_acceleration_mult;
        };
        wing_stats v{
            .fly_time = fly_time, .acc_run_speed_override = fly_speed, .acc_run_acceleration_mult = accel,
            .has_down_hover_stats = hover, .down_hover_speed_override = hover_speed,
            .down_hover_acceleration_mult = hover_accel
        };
        return patchlib_array_set(stats.raw(), static_cast<size_t>(index), &v);
    }

    int item_registry::count() const {
        return static_cast<int>(registry_.size()) + base_count_;
    }

    terraria_item_handle_t *item_registry::find(const std::string_view loader,
                                                const std::string_view mod,
                                                const std::string_view name) const {
        return registry_.find(loader, mod, name);
    }

    // ============================================================
    // 纹理 / 本地化
    // ============================================================
    void item_registry::init_texture2d() {
        if (!f_texture_item_ || !f_texture_item_flame_) return;

        const auto tex_type = patchlib::type("Microsoft.Xna.Framework.Graphics", "Texture2D");
        const auto asset_type = asset_factory::instance().get_generic_class(tex_type);
        resize_array("Terraria.GameContent", "TextureAssets", "Item",
                     asset_type, count(), nullptr, true);
        resize_array("Terraria.GameContent", "TextureAssets", "ItemFlame",
                     asset_type, count(), nullptr, true);

        auto arr = f_texture_item_.get_value<patchlib::structs::array<patchlib::handle>>();
        auto flame_arr = f_texture_item_flame_.get_value<patchlib::structs::array<patchlib::handle>>();
        if (!arr || !flame_arr) return;

        const auto fallback = unknown_item_texture_impl();

        const size_t custom = registry_.size();
        for (size_t i = 0; i < custom; ++i) {
            auto *h = registry_.all()[i];
            if (!h) continue;
            const size_t index = static_cast<size_t>(base_count_) + i;
            patchlib::handle tex;
            if (h->item_ops.get_texture)
                tex = patchlib::handle(h->item_ops.get_texture(h), true);
            if (!tex) tex = fallback;
            arr.set(index, tex);
            flame_arr.set(index, tex);
        }

        accessory_assets_loaded_ = true;
        apply_pending_accessories_now();
    }

void item_registry::init_localized_text() {
    const auto arr_name = f_name_cache_
                              ? f_name_cache_.get_value<patchlib::handle>()
                              : patchlib::handle{};
    const auto arr_tip = f_tooltip_cache_
                             ? f_tooltip_cache_.get_value<patchlib::handle>()
                             : patchlib::handle{};
    if (!arr_name) {
        log::warn("init_localized_text: name cache null, skip");
        return;
    }

    auto name_w = patchlib::structs::array<patchlib::handle>(arr_name, false);
    auto tip_w = arr_tip
                     ? patchlib::structs::array<patchlib::handle>(arr_tip, false)
                     : patchlib::structs::array<patchlib::handle>{};

    const int base = base_count_;
    for (size_t i = 0; i < registry_.size(); ++i) {
        const auto *h = registry_.all()[i];
        if (!h) continue;
        const int target = base + static_cast<int>(i);

        const std::string name_key = make_key(h, "item_name");
        auto lt = language_manager::instance().get_localized_text(name_key.c_str());
        if (!lt) {
            log::warn("init_localized_text: LocalizedText null for {}", name_key);
            continue;
        }
        if (target < 0 || static_cast<size_t>(target) >= name_w.length()) {
            log::error("init_localized_text: target {} >= name.len {}", target, name_w.length());
            continue;
        }

        // ★ 用底层 API 绕过写屏障
        if (!patchlib_array_set(arr_name.raw(), static_cast<size_t>(target), lt.raw_ptr())) {
            log::error("init_localized_text: array_set failed target={}", target);
            continue;
        }

        if (!h->has_tooltip || !m_from_language_key_) continue;

        const std::string tip_key = make_key(h, "item_tooltip");
        const patchlib::structs::string key_str(tip_key);
        patchlib::handle tooltip;
#if defined(__ANDROID__)
        const auto tid = static_cast<int16_t>(target);
        tooltip = m_from_language_key_.invoke<patchlib::handle>(tid, key_str);
#else
        tooltip = m_from_language_key_.invoke<patchlib::handle>(key_str);
#endif
        if (tooltip && tip_w && static_cast<size_t>(target) < tip_w.length()) {
            patchlib_array_set(arr_tip.raw(), static_cast<size_t>(target), tooltip.raw_ptr());
        }
    }
}

    // ============================================================
    // resize 工具
    // ============================================================
    void item_registry::fill_new_slots_with_modal(const patchlib::handle &old_arr,
                                                  const patchlib::handle &new_arr,
                                                  const size_t old_len,
                                                  const size_t new_len) {
        enum { k_max_uniq = 64, k_buf = 256 };
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
            else if (nu < k_max_uniq) {
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

    void item_registry::resize_array(const std::string_view ns,
                                     const std::string_view cls,
                                     const std::string_view fid,
                                     const patchlib::handle &type,
                                     const int nsize, void *dvalue,
                                     const bool allow_create) {
        const auto class_h = patchlib::type(std::string(ns), std::string(cls));
        if (!class_h) return;
        const auto field = class_h.get_field(std::string(fid));
        if (!field) return;

        const auto arr = field.get_value<patchlib::handle>();
        if (!arr) {
            if (!allow_create) return;
            auto narray = patchlib::structs::array<uint8_t>(static_cast<size_t>(nsize), type);
            if (!narray) return;
            if (dvalue) patchlib_array_fill(narray.raw(), dvalue);
            field.set_value(patchlib::handle(narray));
            return;
        }

        const auto arr_w = patchlib::structs::array<uint8_t>(arr, false);
        if (arr_w.length() >= static_cast<size_t>(nsize)) return;

        auto narray = patchlib::structs::array<uint8_t>(static_cast<size_t>(nsize), type);
        if (!narray) return;
        if (dvalue) patchlib_array_fill(narray.raw(), dvalue);
        if (arr_w.length() > 0)
            patchlib_array_copy(narray.raw(), arr.raw(), arr_w.length());
        field.set_value(patchlib::handle(narray));
    }

    void item_registry::resize_all_class_sets(const std::string_view ns,
                                              const std::string_view cls,
                                              const std::string_view inner,
                                              const int base, const int new_size) {
        auto sets = patchlib::type(std::string(ns), std::string(cls));
        if (!sets) return;
        if (!inner.empty()) {
            const auto nested = sets.get_inner_type(std::string(inner));
            if (!nested) return;
            sets = nested;
        }

        static const char *k_excluded[] = {
            "Factory", "DD2BannerEffect", "DefaultKillsForBannerNeeded", "Count",
            "ItemsThatAreProcessedAfterNormalContentSample", "NonColorfulDyeItems",
            "SpecificLauncherAmmoProjectileMatches", "ColorfulDyeValues",
            "ItemsForStuffCannon", "Workbenches",
        };

        for (auto &f: sets.get_fields()) {
            const std::string name = f.get_name();
            if (name.empty()) continue;
            bool excluded = false;
            for (auto *e: k_excluded)
                if (name == e) {
                    excluded = true;
                    break;
                }
            if (excluded) continue;

            const auto arr = f.get_value<patchlib::handle>();
            if (!arr) continue;
            const auto arr_w = patchlib::structs::array<uint8_t>(arr, false);
            const size_t len = arr_w.length();
            if (!(len + 8 >= static_cast<size_t>(base) && len < static_cast<size_t>(new_size))) continue;

            auto bigger = arr_w.resized(static_cast<size_t>(new_size), 0);
            if (!bigger) continue;
            fill_new_slots_with_modal(arr, patchlib::handle(bigger), len, static_cast<size_t>(new_size));
            f.set_value(patchlib::handle(bigger));
        }
    }

    void item_registry::resize_all_itemid_sets(const int base, const int new_size) {
        resize_all_class_sets("Terraria.ID", "ItemID", "Sets", base, new_size);
        resize_all_class_sets("Terraria.GameContent.Prefixes", "PrefixLegacy",
                              "ItemSets", base, new_size);
        resize_all_class_sets("Terraria.ID", "AmmoID", "Sets", base, new_size);
    }

    void item_registry::resize_item_animations(const int new_size, const bool allow_create) {
        const auto da = patchlib::type("Terraria.DataStructures", "DrawAnimation");
        if (!da) return;
        resize_array("Terraria", "Main", "itemAnimations", da, new_size, nullptr, allow_create);
    }

    void item_registry::resize_lang_caches(int /*base*/, const int new_size, const bool allow_create) {
        const auto tt = patchlib::type("Terraria.UI", "ItemTooltip");
        const auto lt = patchlib::type("Terraria.Localization", "LocalizedText");
        if (!tt || !lt) return;

        const auto none_f = tt.get_field("None");
        auto none = none_f ? none_f.get_value<patchlib::handle>() : patchlib::handle{};
        resize_array("Terraria", "Lang", "_itemTooltipCache", tt, new_size,
                     none ? &none : nullptr, allow_create);

        const auto empty_f = lt.get_field("Empty");
        auto empty = empty_f ? empty_f.get_value<patchlib::handle>() : patchlib::handle{};
        resize_array("Terraria", "Lang", "_itemNameCache", lt, new_size,
                     empty ? &empty : nullptr, allow_create);

        const auto int_type = patchlib::type::get_basic_type(PATCH_INT32);
        resize_array("Terraria.UI", "ItemSorting", "_layerIndexForItemType",
                     int_type, new_size, nullptr, allow_create);
    }

    void item_registry::resize_armor_set_bonuses(const int base, const int new_size) {
        const auto cls = patchlib::type("Terraria.DataStructures", "ArmorSetBonuses");
        if (!cls) return;
        const auto f = cls.get_field("SetsContaining");
        if (!f) return;

        const auto arr_w = f.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!arr_w || arr_w.length() != static_cast<size_t>(base)) return;
        const auto empty = arr_w.at(0);
        if (!empty) return;

        auto bigger = arr_w.resized(static_cast<size_t>(new_size), patchlib::handle{});
        if (!bigger) return;
        for (int i = base; i < new_size; ++i) bigger.set(static_cast<size_t>(i), empty);
        f.set_value(patchlib::handle(bigger));
    }

    void item_registry::force_resize_itemid_set(const std::string_view name, int new_size) {
        const auto id_cls = patchlib::type("Terraria.ID", "ItemID");
        if (!id_cls) return;
        const auto sets = id_cls.get_inner_type("Sets");
        if (!sets) return;
        const auto f = sets.get_field(std::string(name));
        if (!f) return;

        auto arr_w = f.get_value<patchlib::structs::array<uint8_t> >();
        if (!arr_w) return;
        const size_t len = arr_w.length();
        if (len == 0 || len >= static_cast<size_t>(new_size)) return;

        auto bigger = arr_w.resized(static_cast<size_t>(new_size), 0);
        if (!bigger) return;
        fill_new_slots_with_modal(patchlib::handle(arr_w), patchlib::handle(bigger),
                                  len, static_cast<size_t>(new_size));
        f.set_value(patchlib::handle(bigger));
    }

    void item_registry::force_resize_critical_item_sets(const int new_size) {
        static const char *k_names[] = {
            "ItemNoGravity", "RecoverableImportantItem", "IsLavaImmuneRegardlessOfRarity",
            "BossBag", "Torches", "WaterTorches", "CommonCoin",
            "CanPassivelyStackInWorldOverride", "OnlyNeedOneInInventoryOverride",
            "IsAPickup", "IgnoresEncumberingStone", "NebulaPickup",
            "ItemsThatShouldNotBeInInventory", "IsFood", "AnimatesAsSoul",
            "TrapSigned", "DrawUnsafeIndicator", "ItemIconPulse", "TextureCopyLoad",
            "IsAMaterial", "OverflowProtectionTimeOffset",
            "DropWithGrabDelayForAllPlayers", "Deprecated",
            "CanBeQuickusedOnGamepad", "ForceConsumption", "ForcesBreaksSleeping",
            "LockOnAimCompensation", "NetUseSoundSync",
        };
        for (auto *n: k_names) force_resize_itemid_set(n, new_size);
    }

    void item_registry::resize(const bool allow_create) {
        const int new_size = count();
        emergency_lookup_checked_ = false;

        const auto bool_t = patchlib::type::get_basic_type(PATCH_BOOL);
        const auto int_t = patchlib::type::get_basic_type(PATCH_INT32);
        static int d_cached = -1;

        resize_array("Terraria", "Item", "claw", bool_t, new_size, nullptr, true);
        resize_array("Terraria", "Item", "staff", bool_t, new_size, nullptr, true);
        resize_array("Terraria", "Item", "cachedItemSpawnsByType", int_t,
                     new_size, &d_cached, true);

#if defined(__ANDROID__)
        resize_array("Terraria", "Player", "ItemUsesRightFire", bool_t, new_size, nullptr, true);
        resize_array("", "VirtualControllerInputState", "ItemCategories", int_t,
                     new_size, nullptr, true);
#endif

        resize_all_itemid_sets(base_count_, new_size);
        resize_item_animations(new_size, allow_create);
        resize_lang_caches(base_count_, new_size, allow_create);
        resize_armor_set_bonuses(base_count_, new_size);
        force_resize_critical_item_sets(new_size);
    }

    bool item_registry::assign_ids(const int base) {
        if (registry_.empty()) {
            ids_assigned_ = true;
            return true;
        }
        return registry_.assign_ids(base);
    }

    // ============================================================
    // 前缀
    // ============================================================
    int item_registry::prefix_decl_lookup(const terraria_item_handle_t *h) const {
        for (size_t i = 0; i < prefix_decl_count_; ++i)
            if (prefix_decls_[i].handle == h) return prefix_decls_[i].cls;
        return 0;
    }

    void item_registry::prefix_decl_remove(const terraria_item_handle_t *h) {
        for (size_t i = 0; i < prefix_decl_count_; ++i) {
            if (prefix_decls_[i].handle == h) {
                prefix_decls_[i] = prefix_decls_[--prefix_decl_count_];
                return;
            }
        }
    }

    bool item_registry::resolve_prefix_sets() {
        if (prefix_sets_state_ >= 0) return prefix_sets_state_ == 1;
        prefix_sets_state_ = 0;

        const auto pl = patchlib::type("Terraria.GameContent.Prefixes", "PrefixLegacy");
        if (!pl) return false;
        const auto sets = pl.get_inner_type("ItemSets");
        if (!sets) return false;

        f_pref_swords_ = sets.get_field("SwordsHammersAxesPicks");
        f_pref_spears_ = sets.get_field("SpearsMacesChainsawsDrillsPunchCannon");
        f_pref_guns_ = sets.get_field("GunsBows");
        f_pref_magic_ = sets.get_field("Magic");
        f_pref_summon_ = sets.get_field("Summon");
        f_pref_boomerang_ = sets.get_field("BoomerangsChakrams");
        f_pref_legendary2_ = sets.get_field("ItemsThatCanHaveLegendary2");

        if (const auto id = patchlib::type("Terraria.ID", "ItemID"))
            if (const auto idsets = id.get_inner_type("Sets"))
                f_can_get_prefixes_ = idsets.get_field("CanGetPrefixes");

        if (!f_pref_swords_ || !f_can_get_prefixes_) return false;
        prefix_sets_state_ = 1;
        return true;
    }

    void item_registry::prefix_set_entry(const patchlib::field &f, int type,
                                         std::string_view name) const {
        if (!f || type < 0) return;

        auto arr_w = f.get_value<patchlib::structs::array<uint8_t> >();
        if (!arr_w) return;

        if (const int need = count(); static_cast<int>(arr_w.length()) < need) {
            auto bigger = arr_w.resized(static_cast<size_t>(need), 0);
            if (!bigger) return;
            f.set_value(patchlib::handle(bigger));
            arr_w = bigger;
        }

        if (static_cast<size_t>(type) < arr_w.length()) {
            arr_w.set(static_cast<size_t>(type), 1);
            log::info("prefix: {}[{}] = true", name, type);
        }
    }

    void item_registry::prefix_set_by_class(const int cls, const int type,
                                            const std::string_view name) const {
        switch (cls) {
            case 1: prefix_set_entry(f_pref_swords_, type, "Swords");
                break;
            case 2: prefix_set_entry(f_pref_spears_, type, "Spears");
                break;
            case 3: prefix_set_entry(f_pref_guns_, type, "GunsBows");
                break;
            case 4: prefix_set_entry(f_pref_magic_, type, "Magic");
                break;
            case 5: prefix_set_entry(f_pref_summon_, type, "Summon");
                break;
            case 6: prefix_set_entry(f_pref_boomerang_, type, "Boomerang");
                break;
            case 7: prefix_set_entry(f_can_get_prefixes_, type, "CanGetPrefixes");
                break;
            default: prefix_set_entry(f_pref_swords_, type, name);
                break;
        }
    }

    void item_registry::apply_prefix_classes_impl() {
        static bool applied = false;
        if (applied) return;
        applied = true;

        if (prefix_decl_count_ == 0) return;
        if (!resolve_prefix_sets()) return;

        if (const auto item_cls = patchlib::type("Terraria", "Item")) {
            if (const auto ctor = item_cls.get_method(".ctor", 2)) {
                constexpr int any = 0;
                if (const auto item = ctor.invoke_ctor(any, static_cast<void*>(nullptr));
                    item && m_get_rollable_) {
                    auto ro = m_get_rollable_[item].invoke<patchlib::handle>();
                }
            }
        }

        int done = 0;
        for (const auto *h: registry_.all()) {
            if (!h || h->runtime_id < 0) continue;
            const int cls = prefix_decl_lookup(h);
            if (cls <= 0) continue;
            prefix_set_by_class(cls, h->runtime_id, h->internal_name);
            done++;
        }
        log::info("prefix: {} declared item(s) classified", done);
    }

    // ============================================================
    // 实例数组钩子安装
    // ============================================================
    void item_registry::install_instance_array_hooks() {
        if (const auto cls = patchlib::type("Terraria.GameContent", "QuickStacking")) {
            if (const auto nested = cls.get_inner_type("MatchingItemTypeDestinationList")) {
                f_quickstack_first_entry_ = nested.get_field("firstEntryForType");
                if (const auto m = nested.get_method("Reset", 0))
                    patchlib_install_prepost_hook(m.raw(), nullptr, quickstack_reset_postfix);
            }
        }
        if (const auto cls = patchlib::type("Terraria.GameContent", "EmergencyStacking")) {
            f_emergency_group_lookup_ = cls.get_field("_groupLookup");
            if (const auto g = cls.get_inner_type("Group"))
                f_emergency_group_default_ = g.get_field("Default");
            if (const auto m = cls.get_method("get_GroupLookup", 0))
                patchlib_install_prepost_hook(m.raw(), nullptr,
                                              emergency_group_lookup_postfix);
        }
        if (const auto filters = patchlib::type("Terraria.GameContent.Creative", "ItemFilters")) {
            if (const auto misc = filters.get_inner_type("MiscFallback")) {
                f_misc_fallback_fits_ = misc.get_field("_fitsFilterByItemType");
                if (const auto m = misc.get_method(".ctor", 1))
                    patchlib_install_prepost_hook(m.raw(), nullptr,
                                                  misc_fallback_ctor_postfix);
            }
        }
        if (const auto cls = patchlib::type("Terraria.GameContent.LootSimulation",
                                            "LootSimulationItemCounter")) {
            f_loot_counts_ = cls.get_field("_itemCountsObtained");
            f_loot_counts_expert_ = cls.get_field("_itemCountsObtainedExpert");
            if (const auto m = cls.get_method(".ctor", 0))
                patchlib_install_prepost_hook(m.raw(), nullptr, loot_counter_ctor_postfix);
        }
    }

    // ============================================================
    // 帧动画
    // ============================================================
    bool item_registry::ensure_item_animations_handle() {
        if (f_item_animations_) return true;
        if (const auto main = patchlib::type("Terraria", "Main"))
            f_item_animations_ = main.get_field("itemAnimations");
        return static_cast<bool>(f_item_animations_);
    }

    bool item_registry::ensure_item_animations_length(const int need) {
        if (!ensure_item_animations_handle()) return false;

        const auto arr_w = f_item_animations_.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!arr_w) return false;
        if (static_cast<size_t>(need) < arr_w.length()) return true;

        auto bigger = arr_w.resized(static_cast<size_t>(need) + 1, patchlib::handle{});
        if (!bigger) return false;
        f_item_animations_.set_value(patchlib::handle(bigger));
        return true;
    }

    bool item_registry::item_animation_registered(const int type) const {
        if (!f_item_animations_) return true;
        const auto arr_w = f_item_animations_.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!arr_w) return true;
        if (static_cast<size_t>(type) >= arr_w.length()) return true;
        return static_cast<bool>(arr_w.at(static_cast<size_t>(type)));
    }

    void item_registry::register_item_animation(terraria_item_handle_t *ih, int type) {
        if (!ih) return;
        if (!ensure_item_animations_length(type)) return;

        const auto anim_cls = patchlib::type("Terraria.DataStructures", "DrawAnimationVertical");
        if (!anim_cls) return;
        const auto ctor = anim_cls.get_method(".ctor", 3);
        if (!ctor) return;

        int ticks = ih->anim_ticks > 0 ? ih->anim_ticks : 8;
        int frames = ih->anim_frames;
        constexpr bool pingpong = false;
        const auto anim = ctor.invoke_ctor(ticks, frames, pingpong);
        if (!anim) return;

        const auto main = patchlib::type("Terraria", "Main");
        const auto reg = main
                             ? main.get_method("RegisterItemAnimation", 2)
                             : patchlib::method{};
        if (!reg) return;
        reg.invoke<void>(type, anim);
        ih->anim_registered = true;
        log::info("item anim: {} registered ({} frames / {} ticks, type={})",
                  ih->internal_name ? ih->internal_name : "?", frames, ticks, type);
    }

    // ============================================================
    // 饰品槽位
    // ============================================================
    patchlib::handle item_registry::accessory_texture_array(const char *slot) {
        for (auto &s: k_accessory_slots) {
            if (std::strcmp(s.slot, slot) != 0) continue;
            const auto ta = patchlib::type("Terraria.GameContent", "TextureAssets");
            if (!ta) return {};
            const auto f = ta.get_field(s.texture_field);
            return f ? f.get_value<patchlib::handle>() : patchlib::handle{};
        }
        return {};
    }

    bool item_registry::apply_accessory_texture(const char *slot, const int index,
                                                const patchlib::handle &asset) {
        if (index < 0 || !asset) return false;
        const accessory_slot *s = nullptr;
        for (auto &x: k_accessory_slots)
            if (std::strcmp(x.slot, slot) == 0) {
                s = &x;
                break;
            }
        if (!s) return false;

        const auto arr = accessory_texture_array(slot);
        if (!arr) return false;
        auto arr_w = patchlib::structs::array<patchlib::handle>(arr, false);
        if (arr_w.length() <= static_cast<size_t>(index)) {
            auto bigger = arr_w.resized(static_cast<size_t>(index) + 1, patchlib::handle{});
            if (!bigger) return false;
            if (const auto ta = patchlib::type("Terraria.GameContent", "TextureAssets"))
                if (const auto f = ta.get_field(s->texture_field))
                    f.set_value(patchlib::handle(bigger));
            arr_w = bigger;
        }
        arr_w.set(static_cast<size_t>(index), asset);
        return true;
    }

    void item_registry::apply_pending_accessories_now() {
        for (auto &[slot, index, asset]: pending_accessories_)
            apply_accessory_texture(slot->slot, index, asset);
        pending_accessories_.clear();
    }

    // ============================================================
    // 钩子实现
    // ============================================================
    void item_registry::set_defaults_postfix(patch_handle_t inst, void **args, void *,
                                             const patch_method_signature_t *) {
        if (!args || !args[0] || !inst) return;
        auto &self = instance();
        const int type = *static_cast<int *>(args[0]);
        const int nid = type - self.base_count_;
        if (nid < 0 || nid >= static_cast<int>(self.registry_.size())) return;

        self.engine_.f_type.set_value(inst, type);
        self.engine_.f_stack.set_value(inst, 1);

        auto *h = self.registry_.all()[nid];
        if (h && h->item_ops.set_defaults) h->item_ops.set_defaults(h, inst);
        if (h && h->anim_frames > 0 && !self.item_animation_registered(type))
            self.register_item_animation(h, type);
        // if (self.m_rebuild_tooltip_) self.m_rebuild_tooltip_.invoke<void>(inst);
    }

    void item_registry::start_actual_use_postfix(patch_handle_t inst, void **args, void *,
                                                 const patch_method_signature_t *) {
        if (!args || !args[0]) return;
        auto &self = instance();
        const patchlib::handle item(*static_cast<patch_handle_t *>(args[0]), false);
        if (!item) return;
        const int type = self.engine_.f_type.get_value<int>(item);
        const int nid = type - self.base_count_;
        if (nid < 0 || nid >= static_cast<int>(self.registry_.size())) return;
        if (auto *h = self.registry_.all()[nid]; h && h->item_ops.use)
            h->item_ops.use(h, inst, item.raw());
    }

    bool item_registry::check_can_use_prefix(patch_handle_t inst, void **args,
                                             const patch_method_signature_t *,
                                             void *result) {
        if (!args || !args[0] || !inst) return false;
        auto &self = instance();
        const bool ignore = args[1] ? *static_cast<bool *>(args[1]) : false;
        const patchlib::handle item(*static_cast<patch_handle_t *>(args[0]), false);
        if (!item) return false;
        const int type = self.engine_.f_type.get_value<int>(item);
        const int nid = type - self.base_count_;
        if (nid < 0 || nid >= static_cast<int>(self.registry_.size())) return false;
        auto *h = self.registry_.all()[nid];
        if (!h || !h->item_ops.can_use) return false;
        *static_cast<bool *>(result) = h->item_ops.can_use(h, inst, item.raw(), ignore);
        return true;
    }

    void item_registry::get_item_name_postfix(patch_handle_t, void **args, void *result,
                                              const patch_method_signature_t *) {
        if (!args || !args[0] || !result) return;
        const auto &self = instance();
        const int type = *static_cast<int *>(args[0]);
        if (const int nid = type - self.base_count_; nid < 0 || nid >= static_cast<int>(self.registry_.size())) return;

        if (!self.f_name_cache_) return;
        const auto arr_w = self.f_name_cache_.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!arr_w || arr_w.length() <= static_cast<size_t>(type)) return;
        if (auto cached = arr_w.at(static_cast<size_t>(type)))
            *static_cast<patch_handle_t *>(result) = cached.release();
    }

    // ============================================================
    // ContentSamples：字典统一用 get_value<dictionary<K,V>>
    // ============================================================
    void item_registry::content_samples_postfix(patch_handle_t, void **, void *,
                                                const patch_method_signature_t *) {
        const auto item_cls = patchlib::type("Terraria", "Item");
        const auto cs_cls = patchlib::type("Terraria.ID", "ContentSamples");
        if (!item_cls || !cs_cls) return;

        const auto f1 = cs_cls.get_field("ItemsByType");
        const auto f2 = cs_cls.get_field("ItemNetIdsByPersistentIds");
        const auto f3 = cs_cls.get_field("ItemPersistentIdsByNetIds");
        if (!f1 || !f2 || !f3) return;

        auto d_by_type = f1.get_value<patchlib::structs::dictionary<int, patchlib::handle> >();
        auto d_net_ids = f2.get_value<patchlib::structs::dictionary<patchlib::structs::string, int> >();
        auto d_persist = f3.get_value<patchlib::structs::dictionary<int, patchlib::structs::string> >();
        const auto m_ctor = item_cls.get_method(".ctor", 2);

        // ItemID.Search
        const auto item_id_cls = patchlib::type("Terraria.ID", "ItemID");
        const auto f_search = item_id_cls ? item_id_cls.get_field("Search") : patchlib::field{};
        const auto search_dict = f_search ? f_search.get_value<patchlib::handle>() : patchlib::handle{};
        auto d_nti = patchlib::structs::dictionary<patchlib::structs::string, int>{};
        auto d_itn = patchlib::structs::dictionary<int, patchlib::structs::string>{};
        if (const auto idc = patchlib::type("ReLogic.Reflection", "IdDictionary")) {
            if (const auto f_nti = idc.get_field("_nameToId"))
                if (search_dict)
                    d_nti = f_nti.get_value<
                        patchlib::structs::dictionary<patchlib::structs::string, int> >(search_dict);
            if (const auto f_itn = idc.get_field("_idToName"))
                if (search_dict)
                    d_itn = f_itn.get_value<
                        patchlib::structs::dictionary<int, patchlib::structs::string> >(search_dict);
        }

        int registered = 0;
        for (const auto *h: instance().registry_.all()) {
            if (!h || h->runtime_id < 0) continue;
            const int id = h->runtime_id;
            auto item = m_ctor
                            ? m_ctor.invoke_ctor(id, reinterpret_cast<void*>(0))
                            : patchlib::handle{};
            if (!item) continue;

            if (d_by_type) d_by_type.set(id, item);

            const std::string pid = persist_id(h);
            if (patchlib::structs::string str(pid); str) {
                if (d_net_ids) d_net_ids.set(str, id);
                if (d_persist) d_persist.set(id, str);
                if (d_nti) d_nti.set(str, id);
                if (d_itn) d_itn.set(id, str);
            }
            registered++;
        }
        log::info("registered {} custom items into ContentSamples", registered);
    }

    void item_registry::creative_sacrifices_postfix(patch_handle_t inst, void **, void *,
                                                    const patch_method_signature_t *) {
        if (!inst) return;
        const auto cls = patchlib::type("Terraria.GameContent.Creative",
                                        "CreativeItemSacrificesCatalog");
        if (!cls) return;
        const auto f = cls.get_field("_sacrificeCountNeededByItemId");
        if (!f) return;

        auto dict_w = f.get_value<patchlib::structs::dictionary<int, int> >(inst);
        if (!dict_w) return;

        int registered = 0;
        for (const auto *h: instance().registry_.all()) {
            if (!h || h->runtime_id < 0 || h->sacrifice_count <= 0) continue;
            dict_w.set(h->runtime_id, h->sacrifice_count);
            registered++;
        }
        log::info("registered {} sacrifice entries", registered);
    }

    void item_registry::init_item_animations_postfix(patch_handle_t, void **, void *,
                                                     const patch_method_signature_t *) {
        for (auto &self = instance(); auto *h: self.registry_.all()) {
            if (!h || h->anim_frames <= 0 || h->runtime_id < 0) continue;
            if (!self.item_animation_registered(h->runtime_id))
                self.register_item_animation(h, h->runtime_id);
        }
    }

    void item_registry::setup_white_lists_postfix(patch_handle_t, void **, void *,
                                                  const patch_method_signature_t *) {
        auto &self = instance();
        if (!self.f_sorting_whitelists_) return;

        // Dictionary<string, List<int>>
        const auto dict_w = self.f_sorting_whitelists_.get_value<
            patchlib::structs::dictionary<patchlib::handle, patchlib::handle> >();
        if (!dict_w) return;

        static const char *k_names[] = {"Last - Not Trash", "Last - Trash"};
        for (auto *name: k_names) {
            patchlib::structs::string key(name);
            patchlib::handle lh;
            if (!dict_w.try_get(key, lh)) continue;
            if (!lh) continue;
            for (auto *h: self.registry_.all()) {
                if (!h || h->runtime_id < 0) continue;
                patchlib_list_add(lh.raw(), &h->runtime_id);
            }
        }
    }

    void item_registry::quickstack_reset_postfix(patch_handle_t inst, void **, void *,
                                                 const patch_method_signature_t *) {
        auto &self = instance();
        if (!self.f_quickstack_first_entry_) return;
        const auto arr_w = self.f_quickstack_first_entry_.get_value<
            patchlib::structs::array<patchlib::handle> >(inst);
        if (!arr_w) return;
        if (arr_w.length() >= static_cast<size_t>(self.count())) return;
        if (const auto bigger = arr_w.resized(static_cast<size_t>(self.count()), patchlib::handle{}))
            self.
                    f_quickstack_first_entry_.set_value(inst, bigger);
    }

    void item_registry::emergency_group_lookup_postfix(patch_handle_t, void **,
                                                       void *result,
                                                       const patch_method_signature_t *) {
        auto &self = instance();
        if (self.emergency_lookup_checked_) return;
        if (!result || !self.f_emergency_group_lookup_) return;

        const patchlib::handle arr(*static_cast<patch_handle_t *>(result), false);
        if (!arr) return;
        const auto arr_w = patchlib::structs::array<patchlib::handle>(arr, false);

        const int need = self.count();
        if (arr_w.length() >= static_cast<size_t>(need)) {
            self.emergency_lookup_checked_ = true;
            return;
        }

        auto def = self.f_emergency_group_default_
                       ? self.f_emergency_group_default_.get_value<patchlib::handle>()
                       : patchlib::handle{};
        if (!def) {
            for (size_t i = arr_w.length(); i-- > 0;) {
                if (const auto c = arr_w.at(i)) {
                    def = c;
                    break;
                }
            }
        }
        if (!def) return;

        auto bigger = arr_w.resized(static_cast<size_t>(need), patchlib::handle{});
        if (!bigger) return;
        for (size_t i = arr_w.length(); i < static_cast<size_t>(need); ++i) bigger.set(i, def);
        self.f_emergency_group_lookup_.set_value(bigger);
        *static_cast<patch_handle_t *>(result) = bigger.release();
        self.emergency_lookup_checked_ = true;
    }

    void item_registry::misc_fallback_ctor_postfix(patch_handle_t inst, void **, void *,
                                                   const patch_method_signature_t *) {
        if (!inst) return;
        auto &self = instance();
        if (!self.f_misc_fallback_fits_) return;

        const auto arr_w = self.f_misc_fallback_fits_.get_value<
            patchlib::structs::array<patchlib::handle> >(inst);
        if (!arr_w) return;
        if (arr_w.length() >= static_cast<size_t>(self.count())) return;
        if (const auto bigger = arr_w.resized(static_cast<size_t>(self.count()), patchlib::handle{}))
            self.
                    f_misc_fallback_fits_.set_value(inst, bigger);
    }

    void item_registry::loot_counter_ctor_postfix(patch_handle_t inst, void **, void *,
                                                  const patch_method_signature_t *) {
        if (!inst) return;
        auto &self = instance();
        const auto need = static_cast<size_t>(self.count());

        auto grow = [&](patchlib::field &f) {
            if (!f) return;
            const auto arr_w = f.get_value<patchlib::structs::array<patchlib::handle> >(inst);
            if (!arr_w || arr_w.length() >= need) return;
            if (const auto bigger = arr_w.resized(need, patchlib::handle{}))
                f.set_value(inst, bigger);
        };

        grow(self.f_loot_counts_);
        grow(self.f_loot_counts_expert_);
    }

    void item_registry::load_textures_postfix(patch_handle_t, void **, void *,
                                              const patch_method_signature_t *) {
        auto &self = instance();
        self.init_texture2d();
        // NPC 贴图由 npc_registry 负责，这里按原逻辑调用：
        // npc_registry::instance().init_texture2d();
    }
}