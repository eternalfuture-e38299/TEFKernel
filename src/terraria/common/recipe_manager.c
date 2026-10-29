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
 * Created: 2026/9/15
 *******************************************************************************/

#include "internal/terraria/recipe_manager.h"
#include "internal/log.h"

#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/property.h"
#include "patchlib/struct/array.h"
#include "terraria/content/item_manager.h"

static tefstd_vector_t g_recipe_callback;

static patch_handle_t set_ingredients =
        PATCH_NULL; // void SetIngredients(params int[] ingredients)

static patch_handle_t set_crafting_station =
        PATCH_NULL; // void SetCraftingStation(int tileType)

static patch_handle_t add_recipe = PATCH_NULL; // static void AddRecipe()

static patch_handle_t max_recipes_f = PATCH_NULL;
static patch_handle_t num_recipes_f = PATCH_NULL;

static patch_handle_t current_recipe_f = PATCH_NULL;
static patch_handle_t create_item_f = PATCH_NULL;

static patch_handle_t stack_f = PATCH_NULL;
static patch_handle_t set_defaults = PATCH_NULL;

static patch_handle_t is_a_material_f = PATCH_NULL;

static patch_handle_t recipe_f = PATCH_NULL;
static patch_handle_t available_recipe_f = PATCH_NULL;

static patch_handle_t recipe_cls = PATCH_NULL;
static patch_handle_t m_recipe_ctor = PATCH_NULL;

#if defined(__ANDROID__)
/* 手机端 Recipe.maxRecipes 是 const 3700，无法像桌面端那样抬高；FindRecipes
 * 的循环上限被编译成立即数，超过 3700 的配方原版永远扫不到。这里在原版扫描
 * 结束后补扫 [3700, numRecipes)，把满足条件的配方追加进 availableRecipe。 */
static patch_handle_t available_recipe_y_f = PATCH_NULL;
static patch_handle_t m_get_available_recipe = PATCH_NULL;
static patch_handle_t m_set_available_recipe = PATCH_NULL;
static patch_handle_t m_get_focus_recipe = PATCH_NULL;
static patch_handle_t m_get_local_player = PATCH_NULL;
static patch_handle_t m_get_in_guide_menu = PATCH_NULL;
static patch_handle_t m_collect_items = PATCH_NULL;
static patch_handle_t m_meets_tile = PATCH_NULL;
static patch_handle_t m_meets_env = PATCH_NULL;
static patch_handle_t m_enough_items = PATCH_NULL;
static patch_handle_t m_add_available = PATCH_NULL;
static patch_handle_t m_refocus = PATCH_NULL;

static int g_vanilla_max_recipes = 3700;
static int g_old_focus_recipe = 0;
static int g_last_added_beyond = -1;
static bool g_scan_beyond_max = false;

static bool find_recipes_prefix(patch_handle_t this, void **args,
                                const patch_method_signature_t *sig_info,
                                void *result);
static void find_recipes_postfix(patch_handle_t this, void **args, void *result,
                                 const patch_method_signature_t *sig_info);
#endif

static void ensure_recipe_capacity(void);

#if !defined(__ANDROID__)
static void resize_crafting_ui_recipe_y(int new_size);
static void resize_new_crafting_ui_lookup(int new_size);
#endif

static bool recipe_add_prefix(patch_handle_t this, void **args,
                              const patch_method_signature_t *sig_info,
                              void *result);

static bool
setup_recipes_prefix(patch_handle_t this, void **args,
                                   const patch_method_signature_t *sig_info,
                                   void *result) {
    for (int i = 0; i < tefstd_vector_size(&g_recipe_callback); ++i) {
        const terraria_recipe_manager_addrecipes_t callback =
                *(terraria_recipe_manager_addrecipes_t *) tefstd_vector_at(
                    &g_recipe_callback, i);
        if (callback)
            callback();
    }

    return false;
}

#if defined(__ANDROID__)
static bool invoke_bool_static(patch_handle_t method, void **args) {
    if (!patchlib_is_valid(method)) return false;
    bool out = false;
    patchlib_method_invoke_args(method, PATCH_NULL, &out, args);
    return out;
}

static patch_handle_t invoke_object_static(patch_handle_t method) {
    if (!patchlib_is_valid(method)) return PATCH_NULL;
    patch_handle_t out = PATCH_NULL;
    patchlib_method_invoke_args(method, PATCH_NULL, &out, NULL);
    return out;
}

/* 手机端 availableRecipe / availableRecipeY 长度都是 maxRecipes(3700)，
 * 超过 3700 个可选配方时要在原版扫描前先撑大，否则 AddToAvailableRecipes
 * 和后面的 UI 重排会越界。 */
static void grow_available_recipe_arrays(const int total) {
    patch_handle_t available = invoke_object_static(m_get_available_recipe);
    if (patchlib_is_valid(available) &&
        patchlib_array_length(available) < (size_t) total) {
        int zero = 0;
        patch_handle_t bigger =
                patchlib_array_resize(available, (size_t) total, &zero);
        if (patchlib_is_valid(bigger) &&
            patchlib_is_valid(m_set_available_recipe)) {
            void *set_args[1] = {&bigger};
            patchlib_method_invoke_args(m_set_available_recipe, PATCH_NULL,
                                        NULL, set_args);
        }
        if (patchlib_is_valid(available)) patchlib_free(available);
        available = bigger;
    }
    if (patchlib_is_valid(available)) patchlib_free(available);

    if (!patchlib_is_valid(available_recipe_y_f)) return;
    patch_handle_t y = PATCH_NULL;
    patchlib_field_get_value(available_recipe_y_f, PATCH_NULL, &y);
    if (patchlib_is_valid(y) && patchlib_array_length(y) < (size_t) total) {
        float zero = 0.0f;
        patch_handle_t bigger =
                patchlib_array_resize(y, (size_t) total, &zero);
        if (patchlib_is_valid(bigger))
            patchlib_field_set_value(available_recipe_y_f, PATCH_NULL, &bigger);
    }
    if (patchlib_is_valid(y)) patchlib_free(y);
}

static bool find_recipes_prefix(patch_handle_t this, void **args,
                                const patch_method_signature_t *sig_info,
                                void *result) {
    (void) this;
    (void) sig_info;
    (void) result;
    g_scan_beyond_max = false;

    const bool can_delay = (args && args[0]) ? *(bool *) args[0] : true;
    if (can_delay) return false;   /* 原版只是标记延迟，等它真正扫描时再补 */

    /* 向导「材料→可合成」模式走的是 CollectGuideRecipes，原版只扫前 3700 条；
     * 这里不能再把普通可合成配方追加进去，否则向导列表会混入无关配方。 */
    if (patchlib_is_valid(m_get_in_guide_menu)) {
        bool in_guide = false;
        patchlib_method_invoke_args(m_get_in_guide_menu, PATCH_NULL, &in_guide, NULL);
        if (in_guide) return false;
    }

    int total = 0;
    if (patchlib_is_valid(num_recipes_f))
        patchlib_field_get_value(num_recipes_f, PATCH_NULL, &total);
    if (total <= g_vanilla_max_recipes) {
        g_last_added_beyond = -1;
        return false;
    }

    grow_available_recipe_arrays(total);

    int focus = 0;
    if (patchlib_is_valid(m_get_focus_recipe))
        patchlib_method_invoke_args(m_get_focus_recipe, PATCH_NULL, &focus, NULL);

    int old_recipe = 0;
    patch_handle_t available = invoke_object_static(m_get_available_recipe);
    if (patchlib_is_valid(available)) {
        const size_t len = patchlib_array_length(available);
        if (len > 0) {
            size_t index = focus > 0 ? (size_t) focus : 0;
            if (index >= len) index = len - 1;
            patchlib_array_at(available, index, &old_recipe);
        }
        patchlib_free(available);
    }
    g_old_focus_recipe = old_recipe;
    g_scan_beyond_max = true;
    return false;
}

static void find_recipes_postfix(patch_handle_t this, void **args, void *result,
                                 const patch_method_signature_t *sig_info) {
    (void) this;
    (void) args;
    (void) result;
    (void) sig_info;
    if (!g_scan_beyond_max) return;
    g_scan_beyond_max = false;

    int total = 0;
    if (patchlib_is_valid(num_recipes_f))
        patchlib_field_get_value(num_recipes_f, PATCH_NULL, &total);
    if (total <= g_vanilla_max_recipes) return;

    patch_handle_t player = invoke_object_static(m_get_local_player);
    if (!patchlib_is_valid(player)) return;

    void *player_args[1] = {&player};
    patchlib_method_invoke_args(m_collect_items, PATCH_NULL, NULL, player_args);

    patch_handle_t recipes = PATCH_NULL;
    patchlib_field_get_value(recipe_f, PATCH_NULL, &recipes);
    int added = 0;
    if (patchlib_is_valid(recipes)) {
        for (int i = g_vanilla_max_recipes; i < total; ++i) {
            patch_handle_t recipe = PATCH_NULL;
            if (!patchlib_array_at(recipes, (size_t) i, &recipe) ||
                !patchlib_is_valid(recipe))
                break;

            void *recipe_args[2] = {&player, &recipe};
            bool ok = invoke_bool_static(m_meets_tile, recipe_args) &&
                      invoke_bool_static(m_meets_env, recipe_args);
            if (ok) {
                void *recipe_only[1] = {&recipe};
                ok = invoke_bool_static(m_enough_items, recipe_only);
            }
            if (ok) {
                int index = i;
                void *index_args[1] = {&index};
                patchlib_method_invoke_args(m_add_available, PATCH_NULL, NULL,
                                            index_args);
                added++;
            }
            patchlib_free(recipe);
        }
        patchlib_free(recipes);
    }

    if (added != g_last_added_beyond) {
        TEKLOG_INFO("recipe: >maxRecipes scan %d/%d available (max=%d, total=%d)",
                    added, total - g_vanilla_max_recipes, g_vanilla_max_recipes,
                    total);
        g_last_added_beyond = added;
    }

    int old_recipe = g_old_focus_recipe;
    void *refocus_args[1] = {&old_recipe};
    patchlib_method_invoke_args(m_refocus, PATCH_NULL, NULL, refocus_args);

    patchlib_free(player);
}
#endif

void terraria_recipe_manager_init() {
    tefstd_vector_init(&g_recipe_callback,
                       sizeof(terraria_recipe_manager_addrecipes_t));

    recipe_cls = patchlib_type_get_type("Terraria", "Recipe");
    m_recipe_ctor =
            patchlib_type_get_method_by_param_count(recipe_cls, ".ctor", 0);
    patch_handle_t setup_recipes =
            patchlib_type_get_method_by_param_count(recipe_cls,
                                                    "SetupRecipes", 0);

    set_ingredients =
            patchlib_type_get_method_by_param_count(recipe_cls, "SetIngredients", 1);
    set_crafting_station = patchlib_type_get_method_by_param_count(
        recipe_cls, "SetCraftingStation", 1);

    add_recipe =
            patchlib_type_get_method_by_param_count(recipe_cls, "AddRecipe", 0);
    if (patchlib_is_valid(add_recipe)) {
        patchlib_install_prepost_hook(add_recipe, recipe_add_prefix, NULL);
        TEKLOG_INFO("recipe: hooked Recipe.AddRecipe");
    }

    current_recipe_f = patchlib_type_get_field(recipe_cls, "currentRecipe");
    create_item_f = patchlib_type_get_field(recipe_cls, "createItem");

    max_recipes_f = patchlib_type_get_field(recipe_cls, "maxRecipes");
    num_recipes_f = patchlib_type_get_field(recipe_cls, "numRecipes");

    patch_handle_t item_class = patchlib_type_get_type("Terraria", "Item");
    patch_handle_t item_id_cls = patchlib_type_get_type("Terraria.ID", "ItemID");
    patch_handle_t item_id_sets_cls = patchlib_type_get_inner_type(item_id_cls, "Sets");
    is_a_material_f = patchlib_type_get_field(item_id_sets_cls, "IsAMaterial");

    stack_f = patchlib_type_get_field(item_class, "stack");
    set_defaults =
            patchlib_type_get_method_by_param_count(item_class, "SetDefaults", 2);

    patchlib_install_prepost_hook(setup_recipes,
                                  setup_recipes_prefix, NULL);

    patch_handle_t main_cls = patchlib_type_get_type("Terraria", "Main");
    recipe_f = patchlib_type_get_field(main_cls, "recipe");
#if __ANDROID__
    available_recipe_f = patchlib_type_get_property(main_cls, "availableRecipe");
#else
    available_recipe_f = patchlib_type_get_field(main_cls, "availableRecipe");
#endif

#if defined(__ANDROID__)
    available_recipe_y_f = patchlib_type_get_field(main_cls, "availableRecipeY");
    if (patchlib_is_valid(available_recipe_f)) {
        m_get_available_recipe =
                patchlib_property_get_get_method(available_recipe_f);
        m_set_available_recipe =
                patchlib_property_get_set_method(available_recipe_f);
    }
    patch_handle_t focus_prop =
            patchlib_type_get_property(main_cls, "focusRecipe");
    if (patchlib_is_valid(focus_prop)) {
        m_get_focus_recipe = patchlib_property_get_get_method(focus_prop);
        patchlib_free(focus_prop);
    }
    patch_handle_t local_player_prop =
            patchlib_type_get_property(main_cls, "LocalPlayer");
    if (patchlib_is_valid(local_player_prop)) {
        m_get_local_player =
                patchlib_property_get_get_method(local_player_prop);
        patchlib_free(local_player_prop);
    }
    patch_handle_t guide_prop =
            patchlib_type_get_property(main_cls, "InGuideCraftMenu");
    if (patchlib_is_valid(guide_prop)) {
        m_get_in_guide_menu = patchlib_property_get_get_method(guide_prop);
        patchlib_free(guide_prop);
    }

    m_collect_items = patchlib_type_get_method(recipe_cls,
                                               "CollectItemsToCraftWithFrom");
    m_meets_tile = patchlib_type_get_method(recipe_cls,
                                            "PlayerMeetsTileRequirements");
    m_meets_env = patchlib_type_get_method(recipe_cls,
                                           "PlayerMeetsEnvironmentConditions");
    {
        const char *names[1] = {"recipe"};
        m_enough_items = patchlib_type_get_method_by_param_names(
                recipe_cls, "CollectedEnoughItemsToCraft", 1, names);
    }
    if (!patchlib_is_valid(m_enough_items))
        m_enough_items = patchlib_type_get_method_by_param_count(
                recipe_cls, "CollectedEnoughItemsToCraft", 1);
    m_add_available = patchlib_type_get_method(recipe_cls,
                                               "AddToAvailableRecipes");
    m_refocus = patchlib_type_get_method(recipe_cls, "TryRefocusingRecipe");

    int vanilla_max = 0;
    patch_handle_t max_field = patchlib_type_get_field(recipe_cls, "maxRecipes");
    if (patchlib_is_valid(max_field)) {
        patchlib_field_get_value(max_field, PATCH_NULL, &vanilla_max);
        patchlib_free(max_field);
    }
    g_vanilla_max_recipes = vanilla_max > 0 ? vanilla_max : 3700;

    patch_handle_t find_recipes = patchlib_type_get_method_by_param_count(
            recipe_cls, "FindRecipes", 1);
    if (patchlib_is_valid(find_recipes) && patchlib_is_valid(m_get_available_recipe) &&
        patchlib_is_valid(m_get_local_player) && patchlib_is_valid(m_collect_items) &&
        patchlib_is_valid(m_meets_tile) && patchlib_is_valid(m_meets_env) &&
        patchlib_is_valid(m_enough_items) && patchlib_is_valid(m_add_available) &&
        patchlib_is_valid(m_refocus)) {
        patchlib_install_prepost_hook(find_recipes, find_recipes_prefix,
                                      find_recipes_postfix);
        TEKLOG_INFO("recipe: hooked Recipe.FindRecipes (mobile maxRecipes=%d)",
                    g_vanilla_max_recipes);
    } else {
        TEKLOG_WARN("recipe: mobile >maxRecipes fix unavailable (max=%d)",
                    g_vanilla_max_recipes);
    }
#endif

    patchlib_free(main_cls);
    patchlib_free(item_class);
    patchlib_free(item_id_cls);
    patchlib_free(item_id_sets_cls);
}

patch_handle_t terraria_recipe_manager_get_current_recipe() {
    patch_handle_t r = PATCH_NULL;
    patchlib_field_get_value(current_recipe_f, NULL, &r);
    return r;
}

void terraria_recipe_set_result(int item_id, int stack) {
    patch_handle_t current_recipe = terraria_recipe_manager_get_current_recipe();
    patch_handle_t create_item = PATCH_NULL;
    patchlib_field_get_value(create_item_f, current_recipe, &create_item);

#if __ANDROID__
    ((void (*)(void *, int, void *)) patchlib_method_get_pointer(set_defaults))(
        create_item, item_id, NULL);
#else
    void *args[2] = {&item_id, NULL};
    patchlib_method_invoke_args(set_defaults, create_item, NULL, args);
#endif

    patchlib_field_set_value(stack_f, create_item, &stack);

    patchlib_free(current_recipe);
    patchlib_free(create_item);
}

void terraria_recipe_set_material(const int *materials, const int count) {
    patch_handle_t current_recipe = terraria_recipe_manager_get_current_recipe();
    patch_handle_t int_type = patchlib_get_basic_type(PATCH_INT32);

    const int array_size = count * 2;
    patch_handle_t ingredients =
            patchlib_array_create(array_size, int_type);

    patchlib_array_copy_from_c(ingredients, materials, array_size);

    patch_handle_t is_a_material = PATCH_NULL;
    patchlib_field_get_value(is_a_material_f, NULL, &is_a_material);


    /* 遍历材料，设置 isAMaterial 属性 */
    for (int i = 0; i < count; i++) {
        const int item_id = materials[i * 2];

        static bool material = true;
        patchlib_array_set(is_a_material, item_id, &material);
    }

#if __ANDROID__
    ((void (*)(void *, void *)) patchlib_method_get_pointer(set_ingredients))(
        current_recipe, ingredients);
#else
    void *args[1] = {&ingredients};
    patchlib_method_invoke_args(set_ingredients, current_recipe, NULL, args);
#endif

    patchlib_free(int_type);
    patchlib_free(ingredients);
    patchlib_free(current_recipe);
    patchlib_free(is_a_material);
}


void terraria_recipe_set_station(int tile_id) {
    patch_handle_t current_recipe = terraria_recipe_manager_get_current_recipe();

#if __ANDROID__
    ((void (*)(void *, int)) patchlib_method_get_pointer(set_crafting_station))(
        current_recipe, tile_id);
#else
    void *args[1] = {&tile_id};
    patchlib_method_invoke_args(set_crafting_station, current_recipe, NULL, args);
#endif

    patchlib_free(current_recipe);
}

/* 确保 Main.recipe / Main.availableRecipe 还有空位。
 * 所有 AddRecipe 调用（原版、内核、其他扩展）都会经过这个检查。
 * 桌面端同时抬高 Recipe.maxRecipes；手机端 maxRecipes 是 const 3700，
 * 改不了，数组先撑大避免越界，超出部分由 FindRecipes 的补扫逻辑进菜单。 */
static void ensure_recipe_capacity(void) {
    int num_recipes = 0;
    patchlib_field_get_value(num_recipes_f, PATCH_NULL, &num_recipes);

    patch_handle_t recipe_array = PATCH_NULL;
    patchlib_field_get_value(recipe_f, PATCH_NULL, &recipe_array);
    if (!patchlib_is_valid(recipe_array)) return;

    const int max_recipes = (int) patchlib_array_length(recipe_array);
    if (max_recipes <= 0) return;

#if !defined(__ANDROID__)
    /* 别的扩展可能把 Main.recipe 加长了却没动 Recipe.maxRecipes；
     * 这里同步一次，避免原版/UI 仍按旧上限遍历。 */
    if (patchlib_is_valid(max_recipes_f)) {
        int configured = 0;
        patchlib_field_get_value(max_recipes_f, PATCH_NULL, &configured);
        if (configured < max_recipes) {
            int synced = max_recipes;
            patchlib_field_set_value(max_recipes_f, PATCH_NULL, &synced);
            resize_crafting_ui_recipe_y(synced);
            resize_new_crafting_ui_lookup(synced);
            TEKLOG_INFO("recipe: maxRecipes %d -> %d（同步数组长度）", configured,
                        synced);
        }
    }
#endif

    if (num_recipes < max_recipes) return;

    int resize_recipes = max_recipes + 4096;

    /* 手机端 availableRecipe / availableRecipeY 交给 FindRecipes 前缀按需扩容，
     * 这里只扩 Main.recipe；桌面端没有 FindRecipes，必须在这里一起扩。 */
    patch_handle_t available_recipe_array = PATCH_NULL;
#if !defined(__ANDROID__)
    patchlib_field_get_value(available_recipe_f, PATCH_NULL, &available_recipe_array);
#endif

    patch_handle_t new_recipe_obj = PATCH_NULL;
    if (patchlib_is_valid(m_recipe_ctor))
        patchlib_constructor_invoke(m_recipe_ctor, &new_recipe_obj, NULL);
    if (!patchlib_is_valid(new_recipe_obj))
        new_recipe_obj = patchlib_type_new_instance(recipe_cls);
    static int zero = 0;

    if (!patchlib_is_valid(new_recipe_obj)) {
        TEKLOG_ERROR("recipe: 无法创建 Recipe 模板，放弃扩容（保持 %d）",
                     max_recipes);
        if (patchlib_is_valid(available_recipe_array))
            patchlib_free(available_recipe_array);
        return;
    }

    /* ⚠️ 两个平台的 dvalue 语义不一样：
     *   Android: dvalue 指向「值内存」（引用类型就是 &handle）
     *   桌面端 : 引用类型要直接传 GCHandle 本身（&handle 会被当成对象地址，
     *            分配出野对象，随后 GC/SIGSEGV）
     * 之前桌面端传 &new_recipe_obj，所以配方一超过 3700 就崩。 */
#if defined(__ANDROID__)
    void *recipe_fill = &new_recipe_obj;
#else
    void *recipe_fill = (void *) new_recipe_obj;
#endif

    patch_handle_t new_recipe_array =
            patchlib_array_resize(recipe_array, resize_recipes, recipe_fill);
    patch_handle_t new_available_recipe_array = patchlib_is_valid(available_recipe_array)
            ? patchlib_array_resize(available_recipe_array, resize_recipes, &zero)
            : PATCH_NULL;

    patchlib_free(new_recipe_obj);

    patchlib_field_set_value(recipe_f, PATCH_NULL, &new_recipe_array);

#if defined(__ANDROID__)
    if (patchlib_is_valid(new_available_recipe_array))
        ((void (*)(void *)) patchlib_method_get_pointer(
                patchlib_property_get_set_method(available_recipe_f)))(
                new_available_recipe_array);
#else
    patchlib_field_set_value(max_recipes_f, PATCH_NULL, &resize_recipes);
    resize_crafting_ui_recipe_y(resize_recipes);
    resize_new_crafting_ui_lookup(resize_recipes);
    if (patchlib_is_valid(new_available_recipe_array))
        patchlib_field_set_value(available_recipe_f, PATCH_NULL,
                                 &new_available_recipe_array);

    if (patchlib_is_valid(available_recipe_array))
        patchlib_free(available_recipe_array);
    if (patchlib_is_valid(new_recipe_array))
        patchlib_free(new_recipe_array);
    if (patchlib_is_valid(new_available_recipe_array))
        patchlib_free(new_available_recipe_array);
#endif

    TEKLOG_INFO("recipe: 扩容到 %d（原 %d，已有 %d）", resize_recipes,
                max_recipes, num_recipes);
}

#if !defined(__ANDROID__)
/* 桌面端 CraftingUI.availableRecipeY 是 new float[Recipe.maxRecipes]。
 * maxRecipes 抬高后必须同步扩容，否则 DrawRecipesList 循环到新上限时
 * 在 i=3700 越界；DrawInventory 每帧抛异常，被 Main.ignoreErrors 吞掉，
 * 表现为背包 / 箱子 / 合成界面全部不显示。 */
static void resize_crafting_ui_recipe_y(const int new_size) {
    static patch_handle_t field = PATCH_NULL;
    if (field == PATCH_NULL) {
        patch_handle_t cls = patchlib_type_get_type("Terraria.UI", "CraftingUI");
        if (patchlib_is_valid(cls)) {
            field = patchlib_type_get_field(cls, "availableRecipeY");
            patchlib_free(cls);
        }
    }
    if (!patchlib_is_valid(field)) return;

    patch_handle_t arr = PATCH_NULL;
    patchlib_field_get_value(field, PATCH_NULL, &arr);
    if (!patchlib_is_valid(arr)) return;

    const size_t len = patchlib_array_length(arr);
    if (len < (size_t) new_size) {
        patch_handle_t bigger =
                patchlib_array_resize(arr, (size_t) new_size, NULL);
        if (patchlib_is_valid(bigger)) {
            for (size_t i = len; i < (size_t) new_size; ++i) {
                float y = 65.0f * (float) i;
                patchlib_array_set(bigger, i, &y);
            }
            patchlib_field_set_value(field, PATCH_NULL, &bigger);
            TEKLOG_INFO("recipe: CraftingUI.availableRecipeY %zu -> %d", len,
                        new_size);
            patchlib_free(bigger);
        } else {
            TEKLOG_ERROR("recipe: CraftingUI.availableRecipeY resize failed");
        }
    }
    patchlib_free(arr);
}

/* 桌面端「高级合成网格」NewCraftingUI 里还有一份按配方下标索引的表：
 *     private RecipeEntry[] _recipeListLookup;   // Array.Resize(.., Recipe.maxRecipes)
 * 它只在 ResetRecipes()（打开面板 / 切过滤器 / RefreshGrid）时按当时的
 * maxRecipes 重建，而 UpdateContents() 每帧用
 *     int num = Main.availableRecipe[i];  ...  _recipeListLookup[num]
 * 去索引（num 是配方下标，不是可选项下标）。运行期新增配方把上限抬高后，
 * 若网格已经打开过，这张表还停在旧长度，索引 [旧上限, numRecipes) 会
 * IndexOutOfRange，异常被 Main.ignoreErrors 吞掉，表现为合成网格空白/缺项。
 * 新槽保持 null，与 ResetRecipes() 里的 Array.Clear 等价。 */
static void resize_new_crafting_ui_lookup(const int new_size) {
    static patch_handle_t instance_f = PATCH_NULL;
    static patch_handle_t lookup_f = PATCH_NULL;
    static bool resolved = false;

    if (!resolved) {
        resolved = true;
        patch_handle_t cls =
                patchlib_type_get_type("Terraria.GameContent.UI", "NewCraftingUI");
        if (patchlib_is_valid(cls)) {
            instance_f = patchlib_type_get_field(cls, "_instance");
            lookup_f = patchlib_type_get_field(cls, "_recipeListLookup");
            patchlib_free(cls);
        }
    }
    if (!patchlib_is_valid(instance_f) || !patchlib_is_valid(lookup_f)) return;

    /* 面板从未打开过时 _instance 还是 null；等它第一次 Open 时
     * ResetRecipes() 会按新的 maxRecipes 建表，这里不用管。 */
    patch_handle_t instance = PATCH_NULL;
    patchlib_field_get_value(instance_f, PATCH_NULL, &instance);
    if (!patchlib_is_valid(instance)) return;

    patch_handle_t arr = PATCH_NULL;
    patchlib_field_get_value(lookup_f, instance, &arr);
    if (patchlib_is_valid(arr)) {
        const size_t len = patchlib_array_length(arr);
        if (len < (size_t) new_size) {
            patch_handle_t bigger =
                    patchlib_array_resize(arr, (size_t) new_size, NULL);
            if (patchlib_is_valid(bigger)) {
                patchlib_field_set_value(lookup_f, instance, &bigger);
                TEKLOG_DEBUG("recipe: NewCraftingUI._recipeListLookup %zu -> %d",
                             len, new_size);
                patchlib_free(bigger);
            } else {
                TEKLOG_ERROR(
                    "recipe: NewCraftingUI._recipeListLookup resize failed");
            }
        }
        patchlib_free(arr);
    }
    patchlib_free(instance);
}
#endif

/* Recipe.AddRecipe() 前置：原版追加时同样保证容量 */
static bool recipe_add_prefix(patch_handle_t this, void **args,
                              const patch_method_signature_t *sig_info,
                              void *result) {
    (void) this;
    (void) args;
    (void) sig_info;
    (void) result;
    ensure_recipe_capacity();
    return false;
}

void terraria_recipe_add() {
    /* 兜底：直接走 invoke 时也保证容量（如果 hook 没被触发） */
    ensure_recipe_capacity();
#if __ANDROID__
    ((void (*)()) patchlib_method_get_pointer(add_recipe))();
#else
    patchlib_method_invoke_args(add_recipe, NULL, NULL, NULL);
#endif
}

void terraria_recipe_manager_register_callback(
    const terraria_recipe_manager_addrecipes_t callback) {
    tefstd_vector_push_back(&g_recipe_callback, &callback);
}
