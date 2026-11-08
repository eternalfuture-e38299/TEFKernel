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

#pragma once

#include <array>
#include <string_view>
#include <vector>

#include "terraria/content/entity_registry.hpp"
#include "terraria/init/bootstrap.hpp"
#include "tef/patchlib/field.hpp"
#include "tef/patchlib/handle.hpp"
#include "tef/patchlib/method.hpp"
#include "terraria/content/item_manager.h"   // ABI

namespace terraria::content {
    using namespace tef;

    class item_registry final : public init::subsystem {
    public:
        static item_registry &instance() noexcept;

        [[nodiscard]] std::string_view name() const noexcept override { return "item"; }

        void early_init() override;
        void on_main_initialize_prefix() override;
        void on_main_initialize_postfix() override;
        void shutdown() override;

        // ===== ABI =====
        bool register_item(terraria_item_handle_t *h);
        bool unregister_item_by_id(int runtime_id);
        bool unregister_item(terraria_item_handle_t *h);
        [[nodiscard]] terraria_item_handle_t *get_item(int runtime_id) const;
        [[nodiscard]] tefstd_vector_t *get_items();
        void apply_prefix_classes();
        void set_prefix_class(terraria_item_handle_t *h, int cls);

        static int reserve_accessory_slot(const char *slot);
        bool set_accessory_texture(const char *slot, int index, patch_handle_t asset);
        static bool accessory_texture_ready(const char *slot, int index);

        static bool set_wing_stats(int index, int fly_time, float fly_speed,
                                   float acceleration, bool hover,
                                   float hover_speed, float hover_acceleration);

        // ===== 内部 =====
        [[nodiscard]] int count() const;
        [[nodiscard]] int base_count() const { return base_count_; }
        [[nodiscard]] terraria_item_handle_t *find(std::string_view loader,
                                                    std::string_view mod,
                                                    std::string_view name) const;
        [[nodiscard]] terraria_item_handle_t *unknown_item() { return &unknown_item_; }

        struct engine_handles {
            patchlib::field  f_type, f_stack, f_item_prefix;
            patchlib::method m_net_defaults, m_prefix;
        };
        [[nodiscard]] const engine_handles &engine() const { return engine_; }

        void init_texture2d();
        void init_localized_text();
        void resize(bool allow_create);

    private:
        item_registry() = default;

        // ---- 注册表 / ID ----
        bool assign_ids(int base_id);
        void apply_prefix_classes_impl();

        // ---- 前缀 ----
        [[nodiscard]] int  prefix_decl_lookup(const terraria_item_handle_t *h) const;
        void prefix_decl_remove(const terraria_item_handle_t *h);
        bool resolve_prefix_sets();
        void prefix_set_entry(const patchlib::field &f, int type, std::string_view name) const;
        void prefix_set_by_class(int cls, int type, std::string_view name) const;

        // ---- resize 工具 ----
        static void resize_array(std::string_view ns, std::string_view cls, std::string_view fid,
                                 const patchlib::handle &type, int nsize,
                                 void *dvalue, bool allow_create);

        static void resize_all_class_sets(std::string_view ns, std::string_view cls,
                                          std::string_view inner, int base, int new_size);
        static void resize_all_itemid_sets(int base, int new_size);
        static void resize_item_animations(int new_size, bool allow_create);
        static void resize_lang_caches(int base, int new_size, bool allow_create);
        static void resize_armor_set_bonuses(int base, int new_size);
        static void force_resize_itemid_set(std::string_view name, int new_size);
        static void force_resize_critical_item_sets(int new_size);

        static void fill_new_slots_with_modal(const patchlib::handle &old_arr,
                                              const patchlib::handle &new_arr,
                                              size_t old_len, size_t new_len);

        // ---- 帧动画 ----
        bool ensure_item_animations_handle();
        bool ensure_item_animations_length(int need);
        [[nodiscard]] bool item_animation_registered(int type) const;
        void register_item_animation(terraria_item_handle_t *ih, int type);

        // ---- 饰品 ----
        [[nodiscard]] static patchlib::handle accessory_texture_array(const char *slot);

        static bool apply_accessory_texture(const char *slot, int index, const patchlib::handle &asset);
        void apply_pending_accessories_now();

        void install_instance_array_hooks();

        // ---- 钩子（全部 static 自由函数，安装时直接传函数名） ----
        static void set_defaults_postfix(patch_handle_t inst, void **args, void *result,
                                         const patch_method_signature_t *sig);
        static void start_actual_use_postfix(patch_handle_t inst, void **args, void *result,
                                             const patch_method_signature_t *sig);
        static bool check_can_use_prefix(patch_handle_t inst, void **args,
                                         const patch_method_signature_t *sig, void *result);
        static void get_item_name_postfix(patch_handle_t inst, void **args, void *result,
                                          const patch_method_signature_t *sig);
        static void content_samples_postfix(patch_handle_t inst, void **args, void *result,
                                            const patch_method_signature_t *sig);
        static void creative_sacrifices_postfix(patch_handle_t inst, void **args, void *result,
                                                const patch_method_signature_t *sig);
        static void init_item_animations_postfix(patch_handle_t inst, void **args, void *result,
                                                 const patch_method_signature_t *sig);
        static void setup_white_lists_postfix(patch_handle_t inst, void **args, void *result,
                                              const patch_method_signature_t *sig);
        static void quickstack_reset_postfix(patch_handle_t inst, void **args, void *result,
                                             const patch_method_signature_t *sig);
        static void emergency_group_lookup_postfix(patch_handle_t inst, void **args, void *result,
                                                   const patch_method_signature_t *sig);
        static void misc_fallback_ctor_postfix(patch_handle_t inst, void **args, void *result,
                                               const patch_method_signature_t *sig);
        static void loot_counter_ctor_postfix(patch_handle_t inst, void **args, void *result,
                                              const patch_method_signature_t *sig);
        static void load_textures_postfix(patch_handle_t inst, void **args, void *result,
                                          const patch_method_signature_t *sig);

        // ---- 状态 ----
        entity_registry<terraria_item_handle_t> registry_;
        engine_handles engine_;
        int base_count_ = 0;
        bool ids_assigned_ = false;
        bool accessory_assets_loaded_ = false;
        bool emergency_lookup_checked_ = false;

        // 前缀
        static constexpr size_t k_max_prefix_decls = 512;
        struct prefix_decl { terraria_item_handle_t *handle; unsigned char cls; };
        std::array<prefix_decl, k_max_prefix_decls> prefix_decls_{};
        size_t prefix_decl_count_ = 0;
        int prefix_sets_state_ = -1;

        patchlib::field f_pref_swords_, f_pref_spears_, f_pref_guns_;
        patchlib::field f_pref_magic_, f_pref_summon_, f_pref_boomerang_;
        patchlib::field f_pref_legendary2_, f_can_get_prefixes_;

        // 引擎句柄
        patchlib::field  f_item_id_count_;
        patchlib::field  f_item_animations_;
        patchlib::field  f_texture_item_, f_texture_item_flame_;
        patchlib::field  f_name_cache_, f_tooltip_cache_;
        patchlib::field  f_sorting_whitelists_;
        patchlib::method m_reset_stats_, m_rebuild_tooltip_, m_get_rollable_;
        patchlib::method m_from_language_key_;

        // 实例数组
        patchlib::field f_quickstack_first_entry_;
        patchlib::field f_emergency_group_lookup_, f_emergency_group_default_;
        patchlib::field f_misc_fallback_fits_;
        patchlib::field f_loot_counts_, f_loot_counts_expert_;

        // 未知物品
        terraria_item_handle_t unknown_item_{};

        // 饰品槽位
        struct accessory_slot {
            const char *slot;
            const char *texture_field;
            const char *armor_class;
            int template_index;
        };
        static constexpr accessory_slot k_accessory_slots[] = {
            {.slot = "back",    .texture_field = "AccBack",    .armor_class = "Back",    .template_index = -1},
            {.slot = "front",   .texture_field = "AccFront",   .armor_class = "Front",   .template_index = -1},
            {.slot = "neck",    .texture_field = "AccNeck",    .armor_class = "Neck",    .template_index = -1},
            {.slot = "face",    .texture_field = "AccFace",    .armor_class = "Face",    .template_index = -1},
            {.slot = "handOn",  .texture_field = "AccHandsOn", .armor_class = "HandOn",  .template_index = -1},
            {.slot = "handOff", .texture_field = "AccHandsOff",.armor_class = "HandOff", .template_index = -1},
            {.slot = "waist",   .texture_field = "AccWaist",   .armor_class = "Waist",   .template_index = -1},
            {.slot = "shield",  .texture_field = "AccShield",  .armor_class = "Shield",  .template_index = -1},
            {.slot = "wing",    .texture_field = "Wings",      .armor_class = "Wing",    .template_index = 46},
        };
        struct pending_accessory {
            const accessory_slot *slot;
            int index;
            patchlib::handle asset;
        };
        std::vector<pending_accessory> pending_accessories_;
    };
}