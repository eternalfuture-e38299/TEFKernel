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
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"

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

static bool
create_reverse_wall_recipes_prefix(patch_handle_t this, void **args,
                                   const patch_method_signature_t *sig_info,
                                   void *result) {
    for (int i = 0; i < tefstd_vector_size(&g_recipe_callback); ++i) {
        terraria_recipe_manager_addrecipes_t callback =
                *(terraria_recipe_manager_addrecipes_t *) tefstd_vector_at(
                    &g_recipe_callback, i);
        if (callback)
            callback();
    }

    return false;
}

void terraria_recipe_manager_init() {
    tefstd_vector_init(&g_recipe_callback,
                       sizeof(terraria_recipe_manager_addrecipes_t));

    patch_handle_t recipe_cls = patchlib_type_get_type("Terraria", "Recipe");
    patch_handle_t create_reverse_wall_recipes =
            patchlib_type_get_method_by_param_count(recipe_cls,
                                                    "CreateReverseWallRecipes", 0);

    set_ingredients =
            patchlib_type_get_method_by_param_count(recipe_cls, "SetIngredients", 1);
    set_crafting_station = patchlib_type_get_method_by_param_count(
        recipe_cls, "CreateReverseWallRecipes", 0);

    add_recipe =
            patchlib_type_get_method_by_param_count(recipe_cls, "AddRecipe", 0);

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

    patchlib_install_prepost_hook(create_reverse_wall_recipes,
                                  create_reverse_wall_recipes_prefix, NULL);

    patch_handle_t main_cls = patchlib_type_get_type("Terraria", "Main");
    recipe_f = patchlib_type_get_field(main_cls, "recipe");
#if __ANDROID__
    available_recipe_f = patchlib_type_get_field(main_cls, "availableRecipeY");
#else
    available_recipe_f = patchlib_type_get_field(main_cls, "availableRecipe");
#endif

    patchlib_free(main_cls);
    patchlib_free(create_reverse_wall_recipes);
    patchlib_free(item_class);
    patchlib_free(recipe_cls);
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

void terraria_recipe_set_material(int *materials, int count) {
    patch_handle_t current_recipe = terraria_recipe_manager_get_current_recipe();
    patch_handle_t int_type = patchlib_get_basic_type(PATCH_INT32);

    int array_size = count * 2;
    patch_handle_t ingredients =
            patchlib_array_create(array_size, int_type);

    patchlib_array_copy_from_c(ingredients, materials, array_size);

    patch_handle_t is_a_material = PATCH_NULL;
    patchlib_field_get_value(is_a_material_f, NULL, &is_a_material);


    /* 遍历材料，设置 isAMaterial 属性 */
    for (int i = 0; i < count; i++) {
        int item_id = materials[i * 2];

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

void terraria_recipe_add() {

    int max_recipes = 0;
    int num_recipes = 0;

    patchlib_field_get_value(max_recipes_f, PATCH_NULL, &max_recipes);
    patchlib_field_get_value(num_recipes_f, PATCH_NULL, &num_recipes);

    if (max_recipes != 0 && num_recipes >= max_recipes) {
        int resize_recipes = 500 + num_recipes;

        patch_handle_t recipe_array = PATCH_NULL;
        patch_handle_t available_recipe_array = PATCH_NULL;
        patchlib_field_get_value(recipe_f, PATCH_NULL, &recipe_array);
        patchlib_field_get_value(available_recipe_f, PATCH_NULL, &available_recipe_array);

        patch_handle_t new_recipe_array = patchlib_array_resize(recipe_array, resize_recipes, PATCH_NULL);
        patch_handle_t new_available_recipe_array = patchlib_array_resize(available_recipe_array, resize_recipes, PATCH_NULL);

        patchlib_field_set_value(recipe_f, PATCH_NULL, &new_recipe_array);
        patchlib_field_set_value(available_recipe_f, PATCH_NULL, &new_available_recipe_array);
#if !defined(__ANDROID__)
        patchlib_field_set_value(max_recipes_f, PATCH_NULL, &resize_recipes);

        patchlib_free(recipe_array);
        patchlib_free(available_recipe_array);
        patchlib_free(new_recipe_array);
        patchlib_free(new_available_recipe_array);
#endif
    }



#if __ANDROID__
    ((void (*)()) patchlib_method_get_pointer(add_recipe))();
#else
    patchlib_method_invoke_args(add_recipe, NULL, NULL, NULL);
#endif
}

void terraria_recipe_manager_register_callback(
    terraria_recipe_manager_addrecipes_t callback) {
    tefstd_vector_push_back(&g_recipe_callback, &callback);
}
