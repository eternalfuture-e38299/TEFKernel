/*******************************************************************************
 * tefkernel - recipe_manager
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
 * Created: 2026/11/7
 *******************************************************************************/

#include "terraria/content/recipe_manager.hpp"

#include <algorithm>

#include "tef/core/log.hpp"
#include "tef/patchlib/property.hpp"
#include "tef/patchlib/struct/array.hpp"
#include "tef/patchlib/struct/string.hpp"

namespace terraria::content {
    using namespace tef;

    recipe_manager &recipe_manager::instance() {
        static recipe_manager inst;
        return inst;
    }

    // -----------------------------------------------------------------------
    // 初始化
    // -----------------------------------------------------------------------

    void recipe_manager::early_init() {
        recipe_cls_ = patchlib::type("Terraria", "Recipe");
        if (!recipe_cls_) {
            log::error("recipe_manager: Terraria.Recipe not found");
            return;
        }

        m_recipe_ctor_ = recipe_cls_.get_method(".ctor", 0);
        m_add_recipe_ = recipe_cls_.get_method("AddRecipe", 0);
        m_set_ingredients_ = recipe_cls_.get_method("SetIngredients", 1);
        m_set_station_ = recipe_cls_.get_method("SetCraftingStation", 1);

        f_current_recipe_ = recipe_cls_.get_field("currentRecipe");
        f_create_item_ = recipe_cls_.get_field("createItem");
        f_max_recipes_ = recipe_cls_.get_field("maxRecipes");
        f_num_recipes_ = recipe_cls_.get_field("numRecipes");

        if (const patchlib::type item_cls("Terraria", "Item"); item_cls) {
            f_stack_ = item_cls.get_field("stack");
            m_set_defaults_ = item_cls.get_method("SetDefaults", 2);
        }
        if (const auto id_cls = patchlib::type("Terraria.ID", "ItemID")) {
            if (const auto sets = id_cls.get_inner_type("Sets"))
                f_is_a_material_ = sets.get_field("IsAMaterial");
        }

        const patchlib::type main_cls("Terraria", "Main");
        if (main_cls) {
            f_recipe_ = main_cls.get_field("recipe");
#if defined(__ANDROID__)
            if (const auto prop = main_cls.get_property("availableRecipe")) {
                f_available_recipe_ = prop;
                m_get_available_recipe_ = prop.get_get_method();
                m_set_available_recipe_ = prop.get_set_method();
            }
#else
            f_available_recipe_ = main_cls.get_field("availableRecipe");
#endif
        }

#if defined(__ANDROID__)
        if (const auto f = main_cls.get_field("availableRecipeY"))
            f_available_recipe_y_ = f;

        if (const auto prop = main_cls.get_property("focusRecipe"))
            m_get_focus_recipe_ = prop.get_get_method();
        if (const auto prop = main_cls.get_property("LocalPlayer"))
            m_get_local_player_ = prop.get_get_method();
        if (const auto prop = main_cls.get_property("InGuideCraftMenu"))
            m_get_in_guide_menu_ = prop.get_get_method();

        m_collect_items_ = recipe_cls_.get_method("CollectItemsToCraftWithFrom");
        m_meets_tile_ = recipe_cls_.get_method("PlayerMeetsTileRequirements");
        m_meets_env_ = recipe_cls_.get_method("PlayerMeetsEnvironmentConditions"); {
            const std::vector<std::string> names = {"recipe"};
            m_enough_items_ = recipe_cls_.get_method("CollectedEnoughItemsToCraft", names);
            if (!m_enough_items_)
                m_enough_items_ = recipe_cls_.get_method("CollectedEnoughItemsToCraft", 1);
        }
        m_add_available_ = recipe_cls_.get_method("AddToAvailableRecipes");
        m_refocus_ = recipe_cls_.get_method("TryRefocusingRecipe");

        if (const auto max_field = recipe_cls_.get_field("maxRecipes")) {
            if (const int v = max_field.get_value<int>(); v > 0) vanilla_max_recipes_ = v;
        }
#endif
    }

    void recipe_manager::on_main_initialize_prefix() {
        if (!recipe_cls_) return;

        if (const auto setup = recipe_cls_.get_method("SetupRecipes", 0))
            h_setup_recipes_ = patchlib_install_prepost_hook(
                setup.raw(), setup_recipes_prefix, nullptr);

        if (m_add_recipe_)
            h_add_recipe_ = patchlib_install_prepost_hook(
                m_add_recipe_.raw(), recipe_add_prefix, nullptr);

#if defined(__ANDROID__)
        if (const auto find = recipe_cls_.get_method("FindRecipes", 1)) {
            if (m_get_available_recipe_ && m_get_local_player_ && m_collect_items_ &&
                m_meets_tile_ && m_meets_env_ && m_enough_items_ &&
                m_add_available_ &&m_refocus_) {
                h_find_recipes_ = patchlib_install_prepost_hook(
                    find.raw(),
                    find_recipes_prefix,
                    find_recipes_postfix);
                log::info("recipe_manager: hooked FindRecipes (mobile maxRecipes={})",
                          vanilla_max_recipes_);
            } else {
                log::warn("recipe_manager: mobile >maxRecipes fix unavailable");
            }
        }
#endif
    }

    void recipe_manager::shutdown() {
        callbacks_.clear();
        // hook 不卸载
    }

    // -----------------------------------------------------------------------
    // 便捷 API
    // -----------------------------------------------------------------------

    patchlib::handle recipe_manager::get_current_recipe() const {
        if (!f_current_recipe_) return {};
        return f_current_recipe_.get_value<patchlib::handle>();
    }

    void recipe_manager::set_result(const int item_id, const int stack) {
        const auto current = get_current_recipe();
        if (!current || !m_set_defaults_ || !f_create_item_ || !f_stack_) return;

        const auto create_item = f_create_item_.get_value<patchlib::handle>(current);
        if (!create_item) return;

        m_set_defaults_[create_item](item_id, patchlib::handle{});
        f_stack_.set_value<int>(create_item, stack);
    }

    void recipe_manager::set_material(const int *materials, const int count) const {
        if (!materials || count <= 0) return;
        const auto current = get_current_recipe();
        if (!current || !m_set_ingredients_) return;

        if (const auto int_type = patchlib::type::get_basic_type(PATCH_INT32); !int_type) return;

        const int array_size = count * 2;
        auto ingredients = patchlib::structs::array<int>(array_size);
        ingredients.copy_from_c(materials, array_size);
        m_set_ingredients_[current](ingredients);

        if (f_is_a_material_) {
            // ItemID.Sets.IsAMaterial 是静态 bool[]；读出的是借用句柄（_own = false），
            // 由游戏持有，不能 free。
            if (auto is_a_material =
                    f_is_a_material_.get_value<patchlib::structs::array<bool> >()) {
                for (int i = 0; i < count; ++i) {
                    const int item_id = materials[i * 2];
                    is_a_material.set(item_id, true);
                }
            }
        }
    }

    void recipe_manager::set_station(const int tile_id) const {
        const auto current = get_current_recipe();
        if (!current || !m_set_station_) return;
        m_set_station_[current](tile_id);
    }

    void recipe_manager::add() {
        // 兜底：hook 没触发时也保证容量
        ensure_capacity();
        if (m_add_recipe_) m_add_recipe_.invoke<void>();
    }

    void recipe_manager::register_callback(const terraria_recipe_manager_addrecipes_t cb) {
        if (cb) callbacks_.push_back(cb);
    }

    bool recipe_manager::unregister_callback(const terraria_recipe_manager_addrecipes_t cb) {
        const auto it = std::ranges::find(callbacks_, cb);
        if (it == callbacks_.end()) return false;
        callbacks_.erase(it);
        return true;
    }

    void recipe_manager::run_callbacks() const {
        // 遍历快照：回调内注册/注销不影响本轮
        for (const auto snapshot = callbacks_; const auto cb: snapshot) if (cb) cb();
    }

    // -----------------------------------------------------------------------
    // 钩子
    // -----------------------------------------------------------------------

    bool recipe_manager::setup_recipes_prefix(patch_handle_t, void **,
                                              const patch_method_signature_t *, void *) {
        instance().run_callbacks();
        return false; // 不跳过原方法
    }

    bool recipe_manager::recipe_add_prefix(patch_handle_t, void **,
                                           const patch_method_signature_t *, void *) {
        instance().ensure_capacity();
        return false;
    }

    // -----------------------------------------------------------------------
    // 容量兜底
    // -----------------------------------------------------------------------

    void recipe_manager::ensure_capacity() {
        if (!f_num_recipes_ || !f_recipe_) return;

        int num_recipes = f_num_recipes_.get_value<int>();

        // Main.recipe 是 Recipe[]（引用数组）→ array<handle>
        const auto recipe_array = f_recipe_.get_value<patchlib::structs::array<patchlib::handle> >();
        if (!recipe_array) return;

        const int max_recipes = static_cast<int>(recipe_array.length());
        if (max_recipes <= 0) return;

#if !defined(__ANDROID__)
        if (f_max_recipes_) {
            if (int configured = f_max_recipes_.get_value<int>(); configured < max_recipes) {
                f_max_recipes_.set_value<int>(max_recipes);
                resize_crafting_ui_recipe_y(max_recipes);
                resize_new_crafting_ui_lookup(max_recipes);
                log::info("recipe_manager: maxRecipes {} -> {} (synced)", configured, max_recipes);
            }
        }
#endif

        if (num_recipes < max_recipes) return;

        const int resize_recipes = max_recipes + 4096;

        auto new_recipe_obj = m_recipe_ctor_ ? m_recipe_ctor_.invoke_ctor() : patchlib::handle{};
        if (!new_recipe_obj) new_recipe_obj = recipe_cls_.new_instance();
        if (!new_recipe_obj) {
            log::error("recipe_manager: cannot create Recipe template, keep {}", max_recipes);
            return;
        }

        // resized 内部对 T=handle 已按平台处理 dvalue（见 array.hpp 的平台分支）
        const auto new_recipe_array = recipe_array.resized(resize_recipes, new_recipe_obj);

#if defined(__ANDROID__)
        if (const auto new_available = m_get_available_recipe_.invoke<patchlib::structs::array<int> >()) {
            if (const auto resized_available = new_available.resized(resize_recipes, 0))
                m_set_available_recipe_.invoke<
                    void>(resized_available);
        }
        f_recipe_.set_value(new_recipe_array);
#else
        f_recipe_.set_value(new_recipe_array);
        if (f_max_recipes_) f_max_recipes_.set_value<int>(resize_recipes);
        resize_crafting_ui_recipe_y(resize_recipes);
        resize_new_crafting_ui_lookup(resize_recipes);

        if (f_available_recipe_) {
            if (const auto avail = f_available_recipe_.get_value<patchlib::structs::array<int> >()) {
                if (const auto resized = avail.resized(resize_recipes, 0)) f_available_recipe_.set_value(resized);
            }
        }
#endif

        log::info("recipe_manager: resized to {} (was {}, used {})",
                  resize_recipes, max_recipes, num_recipes);
    }

#if !defined(__ANDROID__)
    void recipe_manager::resize_crafting_ui_recipe_y(int new_size) {
        static patchlib::field field;
        if (!field) {
            if (const auto cls = patchlib::type("Terraria.UI", "CraftingUI"))
                field = cls.get_field("availableRecipeY");
        }
        if (!field) return;

        const auto arr = field.get_value<patchlib::structs::array<float> >();
        if (!arr) return;

        const size_t len = arr.length();
        if (len >= static_cast<size_t>(new_size)) return;

        auto bigger = arr.resized(new_size, 0.0f);
        if (!bigger) {
            log::error("recipe_manager: CraftingUI.availableRecipeY resize failed");
            return;
        }
        for (size_t i = len; i < static_cast<size_t>(new_size); ++i) {
            const float y = 65.0f * static_cast<float>(i);
            bigger.set(i, y);
        }
        field.set_value(bigger);
        log::info("recipe_manager: CraftingUI.availableRecipeY {} -> {}", len, new_size);
    }

    void recipe_manager::resize_new_crafting_ui_lookup(int new_size) {
        static patchlib::field instance_f, lookup_f;
        static bool resolved = false;
        if (!resolved) {
            resolved = true;
            if (const auto cls = patchlib::type("Terraria.GameContent.UI", "NewCraftingUI")) {
                instance_f = cls.get_field("_instance");
                lookup_f = cls.get_field("_recipeListLookup");
            }
        }
        if (!instance_f || !lookup_f) return;

        const auto instance = instance_f.get_value<patchlib::handle>();
        if (!instance) return;

        const auto arr = lookup_f.get_value<patchlib::structs::array<patchlib::handle> >(instance);
        if (!arr) return;

        const size_t len = arr.length();
        if (len >= static_cast<size_t>(new_size)) return;

        if (const auto bigger = arr.resized(new_size, patchlib::handle{})) {
            lookup_f.set_value(instance, bigger);
            log::debug("recipe_manager: NewCraftingUI._recipeListLookup {} -> {}", len, new_size);
        } else {
            log::error("recipe_manager: NewCraftingUI._recipeListLookup resize failed");
        }
    }
#endif

#if defined(__ANDROID__)
    void recipe_manager::grow_available_recipe_arrays(const int total) const {
        if (m_get_available_recipe_ && m_set_available_recipe_) {
            if (const auto available = m_get_available_recipe_.invoke<patchlib::structs::array<int> >();
                available && static_cast<int>(available.length()) < total) {
                if (const auto bigger = available.resized(total, 0)) m_set_available_recipe_.invoke<void>(bigger);
            }
        }
        if (f_available_recipe_y_) {
            if (const auto y = f_available_recipe_y_.get_value<patchlib::structs::array<float> >();
                y && static_cast<int>(y.length()) < total) {
                if (const auto bigger = y.resized(total, 0.0f)) f_available_recipe_y_.set_value(bigger);
            }
        }
    }

    bool recipe_manager::find_recipes_prefix(patch_handle_t, void **args,
                                             const patch_method_signature_t *, void *) {
        auto &self = instance();
        self.scan_beyond_max_ = false;

        if (args && args[0] ? *static_cast<bool *>(args[0]) : true) return false;

        if (self.m_get_in_guide_menu_ && self.m_get_in_guide_menu_.invoke<bool>())
            return false;

        const int total = self.f_num_recipes_ ? self.f_num_recipes_.get_value<int>() : 0;
        if (total <= self.vanilla_max_recipes_) {
            self.last_added_beyond_ = -1;
            return false;
        }

        self.grow_available_recipe_arrays(total);

        const int focus = self.m_get_focus_recipe_ ? self.m_get_focus_recipe_.invoke<int>() : 0;
        int old_recipe = 0;
        if (const auto available = self.m_get_available_recipe_.invoke<patchlib::structs::array<int> >()) {
            if (const size_t len = available.length(); len > 0) {
                size_t index = focus > 0 ? static_cast<size_t>(focus) : 0;
                if (index >= len) index = len - 1;
                old_recipe = available.at(index);
            }
        }
        self.old_focus_recipe_ = old_recipe;
        self.scan_beyond_max_ = true;
        return false;
    }

    void recipe_manager::find_recipes_postfix(patch_handle_t, void **, void *,
                                              const patch_method_signature_t *) {
        auto &self = instance();
        if (!self.scan_beyond_max_) return;
        self.scan_beyond_max_ = false;

        int total = self.f_num_recipes_ ? self.f_num_recipes_.get_value<int>() : 0;
        if (total <= self.vanilla_max_recipes_) return;

        const auto player = self.m_get_local_player_.invoke<patchlib::handle>();
        if (!player) return;

        self.m_collect_items_.invoke<void>(player);

        int added = 0;
        if (const auto recipes = self.f_recipe_.get_value<patchlib::structs::array<patchlib::handle> >()) {
            for (int i = self.vanilla_max_recipes_; i < total; ++i) {
                const auto recipe = recipes.at(i);
                if (!recipe) break;

                bool ok = self.m_meets_tile_.invoke<bool>(player, recipe) &&
                          self.m_meets_env_.invoke<bool>(player, recipe);
                if (ok) ok = self.m_enough_items_.invoke<bool>(recipe);
                if (ok) {
                    self.m_add_available_.invoke<void>(i);
                    added++;
                }
            }
        }

        if (added != self.last_added_beyond_) {
            log::info("recipe_manager: >maxRecipes scan {}/{} available (max={}, total={})",
                      added, total - self.vanilla_max_recipes_, self.vanilla_max_recipes_, total);
            self.last_added_beyond_ = added;
        }

        const int old_recipe = self.old_focus_recipe_;
        self.m_refocus_.invoke<void>(old_recipe);
    }
#endif
} // namespace terraria::content
