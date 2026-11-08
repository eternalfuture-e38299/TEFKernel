/*******************************************************************************
 * tefkernel - prefix_registry
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

#include "terraria/content/prefix_registry.hpp"

#include <string>
#include <vector>

#include "tef/core/log.hpp"
#include "tef/patchlib/struct/array.hpp"
#include "terraria/content/language_manager.hpp"

namespace terraria::content {
    using namespace tef;

    prefix_registry &prefix_registry::instance() noexcept {
        static prefix_registry inst;
        return inst;
    }

    // -----------------------------------------------------------------------
    // 生命周期
    // -----------------------------------------------------------------------

    void prefix_registry::early_init() {
        if (const auto id_cls = patchlib::type("Terraria.ID", "PrefixID"))
            f_prefix_id_count_ = id_cls.get_field("Count");

        if (const auto lang = patchlib::type("Terraria", "Lang"))
            f_lang_prefix_ = lang.get_field("prefix");

        const auto item_cls = patchlib::type("Terraria", "Item");
        if (!item_cls) {
            log::error("prefix_registry: Terraria.Item not found");
            return;
        }

        f_item_prefix_ = item_cls.get_field("prefix");
        f_item_type_ = item_cls.get_field("type");

        m_item_prefix_ = item_cls.get_method("Prefix", 1);
        m_item_reset_ = item_cls.get_method("ResetStats", 1);
        m_item_roll_ = item_cls.get_method("GetRollablePrefixes", 0);
        m_item_try_stats_ = item_cls.get_method("TryGetPrefixStatMultipliersForItem", 11);

        if (m_item_prefix_)
            patchlib_install_prepost_hook(m_item_prefix_.raw(), nullptr,
                                          item_prefix_postfix);
        if (m_item_roll_)
            patchlib_install_prepost_hook(m_item_roll_.raw(), nullptr,
                                          item_rollable_postfix);
        if (m_item_try_stats_) {
            patchlib_install_prepost_hook(m_item_try_stats_.raw(),
                                          item_try_get_stats_prefix,
                                          nullptr);
            log::info("prefix: hooked TryGetPrefixStatMultipliersForItem");
        } else {
            log::warn("prefix: TryGetPrefixStatMultipliersForItem not found");
        }
    }

    void prefix_registry::on_main_initialize_prefix() {
        if (registry_.empty()) return;
        assign_ids(id_count());
        for (auto *h: registry_.all())
            if (h->prefix_ops.early_init) h->prefix_ops.early_init(h);
    }

    void prefix_registry::on_main_initialize_postfix() {
        if (registry_.empty()) return;
        resize();
        init_localized_text();
        for (auto *h: registry_.all())
            if (h->prefix_ops.init_static) h->prefix_ops.init_static(h);
    }

    void prefix_registry::shutdown() {
        for (auto &h: prefix_text_) h.reset();
        registry_.all().clear();
        ids_assigned_ = false;
    }

    // -----------------------------------------------------------------------
    // 注册表
    // -----------------------------------------------------------------------

    bool prefix_registry::register_prefix(terraria_prefix_handle_t *h) {
        if (!h || !h->parent_modloader_id || !h->parent_id || !h->internal_name)
            return false;
        const bool ok = registry_.register_entity(h);
        if (ok && ids_assigned_)
            log::warn("prefix registered after IDs assigned: {}.{}",
                      h->parent_id, h->internal_name);
        return ok;
    }

    bool prefix_registry::unregister_by_id(const int runtime_id) {
        if (runtime_id < 0) return false;
        const bool ok = registry_.unregister_by_id(runtime_id);
        if (ok && ids_assigned_) assign_ids(id_count());
        return ok;
    }

    bool prefix_registry::assign_ids(int base) {
        if (registry_.empty()) {
            ids_assigned_ = true;
            return true;
        }
        if (base + static_cast<int>(registry_.size()) > 256) {
            log::error("prefix: too many prefixes ({} + {} > 256)",
                       base, registry_.size());
            return false;
        }
        if (!registry_.assign_ids(base)) return false;
        ids_assigned_ = true;
        log::info("prefix: assigned {} ids from {}", registry_.size(), base);
        return true;
    }

    terraria_prefix_handle_t *prefix_registry::get(const int runtime_id) const {
        return registry_.get(runtime_id);
    }

    tefstd_vector_t *prefix_registry::get_prefixes() {
        return registry_.raw_tefstd_vector();
    }

    terraria_prefix_handle_t *prefix_registry::find(const std::string_view loader,
                                                    const std::string_view mod,
                                                    const std::string_view name) const {
        return registry_.find(loader, mod, name);
    }

    int prefix_registry::id_count() const {
        if (f_prefix_id_count_) {
            if (const int c = f_prefix_id_count_.get_value<int>(); c > 0) return c;
        }
        return prefix_base_;
    }

    int prefix_registry::remaining_slots() const {
        const int base = id_count();
        const int used = static_cast<int>(registry_.size());
        return 256 - base - used;
    }

    // -----------------------------------------------------------------------
    // PrefixID.Sets 扩容
    // -----------------------------------------------------------------------

    void prefix_registry::resize_prefix_id_sets() const {
        const auto id_cls = patchlib::type("Terraria.ID", "PrefixID");
        if (!id_cls) return;
        const auto sets = id_cls.get_inner_type("Sets");
        if (!sets) return;

        const int base = id_count();
        int resized = 0, skipped = 0, error = 0;

        for (auto &f: sets.get_fields()) {
            constexpr int k_new_size = 256;
            const std::string name = f.get_name();
            if (name.empty() || name == "Factory" || name == "Count") {
                skipped++;
                continue;
            }

            if (!f.is_static() || f.is_const()) {
                skipped++;
                continue;
            }

            auto arr_w = f.get_value<patchlib::structs::array<uint8_t> >();
            if (!arr_w) {
                skipped++;
                continue;
            }

            const size_t len = arr_w.length();
            if (const bool near_base = len + 8 >= static_cast<size_t>(base) && len < static_cast<size_t>(k_new_size); !
                near_base) {
                skipped++;
                continue;
            }

            auto bigger = arr_w.resized(static_cast<size_t>(k_new_size), 0);
            if (!bigger) {
                error++;
                continue;
            }
            f.set_value(patchlib::handle(bigger));
            log::info("prefix: PrefixID.Sets.{} {} -> {}", name, len, k_new_size);
            resized++;
        }
        log::info("prefix: PrefixID.Sets resized={} skipped={} errors={}",
                  resized, skipped, error);
    }

    void prefix_registry::resize() const {
        resize_prefix_id_sets();
        if (!f_lang_prefix_) {
            log::warn("prefix: Lang.prefix field not resolved");
            return;
        }

        const auto arr_w = f_lang_prefix_.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!arr_w) {
            const auto text_cls = patchlib::type("Terraria.Localization", "LocalizedText");
            if (!text_cls) return;
            auto narray = patchlib::structs::array<patchlib::handle>(256, text_cls);
            if (!narray) return;
            f_lang_prefix_.set_value(patchlib::handle(narray));
            log::info("prefix: Lang.prefix created (256)");
            return;
        }

        const size_t len = arr_w.length();
        if (len >= 256) return;
        auto bigger = arr_w.resized(256, patchlib::handle{});
        if (!bigger) {
            log::error("prefix: Lang.prefix resize failed (len={})", len);
            return;
        }
        f_lang_prefix_.set_value(patchlib::handle(bigger));
        log::info("prefix: Lang.prefix {} -> 256", len);
    }

    // -----------------------------------------------------------------------
    // 本地化
    // -----------------------------------------------------------------------

    std::string prefix_registry::build_name_key(const terraria_prefix_handle_t *h) {
        std::string s = "prefix_name_";
        s += h->parent_modloader_id ? h->parent_modloader_id : "?";
        s += '_';
        s += h->parent_id ? h->parent_id : "?";
        s += '.';
        s += h->internal_name ? h->internal_name : "?";
        return s;
    }

    void prefix_registry::init_localized_text() {
        resize();
        if (!f_lang_prefix_) return;

        auto arr_w = f_lang_prefix_.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!arr_w) return;

        for (const auto *h: registry_.all()) {
            if (!h || h->runtime_id < 0 || h->runtime_id > 255) continue;
            const std::string key = build_name_key(h);
            auto text = language_manager::instance().get_localized_text(key.c_str());
            if (!text) {
                log::warn("prefix: get_localized_text('{}') failed", key);
                continue;
            }
            arr_w.set(static_cast<size_t>(h->runtime_id), text);
            prefix_text_[h->runtime_id] = text;
        }
        log::info("prefix: Lang.prefix filled ({} custom)", registry_.size());
    }

    // -----------------------------------------------------------------------
    // 字段写
    // -----------------------------------------------------------------------

    void prefix_registry::set_item_prefix_value(patch_handle_t inst, int v) {
        if (!f_item_prefix_ || !inst) return;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        const auto b = static_cast<unsigned char>(v);
        f_item_prefix_.set_value(inst, b);
    }

    // -----------------------------------------------------------------------
    // 钩子：TryGetPrefixStatMultipliersForItem（prefix）
    // -----------------------------------------------------------------------

    bool prefix_registry::item_try_get_stats_prefix(patch_handle_t inst, void **args,
                                                    const patch_method_signature_t *, void *result) {
        if (!args || !args[0] || !args[1] || !args[2] || !args[3] || !args[4] ||
            !args[5] || !args[6] || !args[7] || !args[8] || !args[9] || !args[10] ||
            !result)
            return false;

        const int pre = *static_cast<int *>(args[0]);
        const auto &self = instance();
        if (const int base = self.id_count(); pre < base) return false;

        auto *h = self.get(pre);
        if (!h) return false;

        float dmg = 1.0f, kb = 1.0f, spd = 1.0f, size = 1.0f, shtspd = 1.0f, mcst = 1.0f;
        int crt = 0, tagdmg = 0, arpen = 0;

        float d_dmg = 1.0f, d_kb = 1.0f, d_spd = 1.0f, d_size = 1.0f, d_shtspd = 1.0f, d_mcst = 1.0f;
        int d_crt = 0, d_tagdmg = 0, d_arpen = 0;
        if (h->prefix_ops.apply_stats)
            *static_cast<bool *>(result) = h->prefix_ops.apply_stats(
                h, inst, &d_dmg, &d_kb, &d_spd, &d_size, &d_shtspd, &d_mcst,
                &d_crt, &d_tagdmg, &d_arpen);

        dmg *= d_dmg;
        kb *= d_kb;
        spd *= d_spd;
        size *= d_size;
        shtspd *= d_shtspd;
        mcst *= d_mcst;
        crt += d_crt;
        tagdmg += d_tagdmg;
        arpen += d_arpen;

        float value = 1.0f * dmg * (2.0f - spd) * (2.0f - mcst) * size * kb * shtspd
                      * (1.0f + static_cast<float>(crt) * 0.02f)
                      * (1.0f + static_cast<float>(arpen) * 0.015f)
                      * (1.0f + static_cast<float>(tagdmg) * 0.03f);
        if (h->prefix_ops.modify_value) {
            float vm = 1.0f;
            h->prefix_ops.modify_value(h, inst, &vm);
            value *= vm;
        }

        **static_cast<float **>(args[1]) = dmg;
        **static_cast<float **>(args[2]) = kb;
        **static_cast<float **>(args[3]) = spd;
        **static_cast<float **>(args[4]) = size;
        **static_cast<float **>(args[5]) = shtspd;
        **static_cast<float **>(args[6]) = mcst;
        **static_cast<int **>(args[7]) = crt;
        **static_cast<int **>(args[8]) = tagdmg;
        **static_cast<int **>(args[9]) = arpen;
        **static_cast<float **>(args[10]) = value;

        return true;
    }

    // -----------------------------------------------------------------------
    // 钩子：Item.Prefix(int)（postfix）
    // -----------------------------------------------------------------------

    void prefix_registry::item_prefix_postfix(patch_handle_t inst, void **args, void *,
                                              const patch_method_signature_t *) {
        if (!inst || !args || !args[0]) return;
        const int pre = *static_cast<int *>(args[0]);
        auto &self = instance();
        if (const int base = self.id_count(); pre < base) return;

        auto *h = self.get(pre);
        if (!h) {
            self.set_item_prefix_value(inst, 0);
            return;
        }
        self.set_item_prefix_value(inst, pre);
        log::debug("prefix: applied {} (id={})", h->internal_name, pre);
    }

    // -----------------------------------------------------------------------
    // 钩子：GetRollablePrefixes（postfix）
    // -----------------------------------------------------------------------

    void prefix_registry::item_rollable_postfix(patch_handle_t inst, void **, void *result,
                                                const patch_method_signature_t *) {
        if (!inst || !result) return;
        auto &self = instance();
        if (self.registry_.empty()) return;

        const int base = self.id_count();
        const int max_extra = 256 - base;
        int extra[256];
        int extra_n = 0;

        for (auto *h: self.registry_.all()) {
            if (!h || h->runtime_id < base || h->runtime_id > 255) continue;
            if (extra_n >= max_extra) break;
            if (h->prefix_ops.can_roll && !h->prefix_ops.can_roll(h, inst)) continue;
            if (h->prefix_ops.roll_chance) {
                const float c = h->prefix_ops.roll_chance(h, inst);
                if (c <= 0.0f) continue;
            }
            extra[extra_n++] = h->runtime_id;
        }
        if (extra_n == 0) return;

        const patchlib::handle orig(*static_cast<patch_handle_t *>(result), false);
        size_t orig_len = 0;
        if (orig && patchlib_array_element_size(orig.raw()) != sizeof(int)) {
            log::warn("prefix: GetRollablePrefixes returned non-int[] (elem_size={})",
                      patchlib_array_element_size(orig.raw()));
            return;
        }
        if (orig) orig_len = patchlib_array_length(orig.raw());

        const size_t new_len = orig_len + static_cast<size_t>(extra_n);
        const auto int_type = patchlib::type::get_basic_type(PATCH_INT32);
        if (!int_type) return;

        auto merged = patchlib::structs::array<int>(new_len, int_type);
        if (!merged) {
            log::warn("prefix: create rollable array failed (len={})", new_len);
            return;
        }

        if (orig_len > 0 && !patchlib_array_copy(merged.raw(), orig.raw(), orig_len)) {
            log::warn("prefix: copy {} rollable prefixes failed", orig_len);
            return;
        }
        for (int i = 0; i < extra_n; ++i)
            merged.set(orig_len + static_cast<size_t>(i), extra[i]);

        *static_cast<patch_handle_t *>(result) = merged.release();
    }
}
