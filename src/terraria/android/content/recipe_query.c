#include "terraria/recipe_query.h"
#include "patchlib/field.h"
#include "patchlib/struct/array.h"
#include <stdlib.h>
#include <string.h>
#define RECIPE_LIMIT 65536u
static terraria_recipe_view_t *snapshot;
static size_t length;
static uint64_t generation = 1;
static bool valid;
static patch_handle_t f_recipes, f_count, f_result, f_materials, f_stations, f_type, f_stack;
static patch_handle_t condition_fields[5];
static const char *condition_names[] = {"needWater", "needHoney", "needLava", "needSnowBiome",
                                        "needGraveyardBiome"};
static patch_handle_t typed_field(patch_handle_t type, const char *name, patch_type_t expected) {
    patch_handle_t f = patchlib_type_get_field(type, name);
    return f && patchlib_field_get_type(f) == expected ? f : PATCH_NULL;
}
static bool resolve(void) {
    if (f_recipes && f_count && f_result && f_materials && f_stations && f_type && f_stack)
        return true;
    patch_handle_t main = patchlib_type_get_type("Terraria", "Main");
    patch_handle_t recipe = patchlib_type_get_type("Terraria", "Recipe");
    patch_handle_t item = patchlib_type_get_type("Terraria", "Item");
    if (!main || !recipe || !item)
        return false;
    f_recipes = typed_field(main, "recipe", PATCH_OBJECT);
    f_count = typed_field(recipe, "numRecipes", PATCH_INT32);
    f_result = typed_field(recipe, "createItem", PATCH_OBJECT);
    f_materials = typed_field(recipe, "requiredItem", PATCH_OBJECT);
    f_stations = typed_field(recipe, "requiredTile", PATCH_OBJECT);
    f_type = typed_field(item, "type", PATCH_INT32);
    f_stack = typed_field(item, "stack", PATCH_INT32);
    for (size_t i = 0; i < 5; ++i)
        condition_fields[i] = typed_field(recipe, condition_names[i], PATCH_BOOL);
    return f_recipes && f_count && f_result && f_materials && f_stations && f_type && f_stack;
}
static bool item_value(patch_handle_t item, terraria_recipe_item_t *out) {
    if (!item)
        return false;
    patchlib_field_get_value(f_type, item, &out->type);
    patchlib_field_get_value(f_stack, item, &out->stack);
    return true;
}
static bool read_recipe(patch_handle_t recipe, int index, terraria_recipe_view_t *out) {
    memset(out, 0, sizeof(*out));
    out->index = index;
    patch_handle_t item = PATCH_NULL, materials = PATCH_NULL, stations = PATCH_NULL;
    patchlib_field_get_value(f_result, recipe, &item);
    if (!item_value(item, &out->result))
        return false;
    patchlib_field_get_value(f_materials, recipe, &materials);
    patchlib_field_get_value(f_stations, recipe, &stations);
    if (!materials || !stations || !patchlib_array_element_is_reference(materials) ||
        patchlib_array_element_is_reference(stations) ||
        patchlib_array_element_size(stations) != sizeof(int))
        return false;
    size_t n = patchlib_array_length(materials);
    if (n > TERRARIA_RECIPE_REQUIREMENTS_MAX)
        return false;
    for (size_t i = 0; i < n; ++i) {
        terraria_recipe_item_t value = {0};
        item = PATCH_NULL;
        if (!patchlib_array_at(materials, i, &item))
            return false;
        if (item && item_value(item, &value) && value.type > 0 && value.stack > 0)
            out->materials[out->material_count++] = value;
    }
    n = patchlib_array_length(stations);
    if (n > TERRARIA_RECIPE_STATIONS_MAX)
        return false;
    for (size_t i = 0; i < n; ++i) {
        int station = -1;
        if (!patchlib_array_at(stations, i, &station))
            return false;
        if (station >= 0)
            out->stations[out->station_count++] = station;
    }
    for (size_t i = 0; i < 5; ++i)
        if (condition_fields[i]) {
            bool value = false;
            patchlib_field_get_value(condition_fields[i], recipe, &value);
            out->conditions_known |= 1u << i;
            if (value)
                out->conditions |= 1u << i;
        }
    return true;
}
bool terraria_recipe_query_refresh(void) {
    valid = false;
    if (!resolve())
        return false;
    int n = 0;
    patch_handle_t recipes = PATCH_NULL;
    patchlib_field_get_value(f_count, PATCH_NULL, &n);
    patchlib_field_get_value(f_recipes, PATCH_NULL, &recipes);
    if (n < 0 || (unsigned)n > RECIPE_LIMIT || !recipes ||
        !patchlib_array_element_is_reference(recipes) || (size_t)n > patchlib_array_length(recipes))
        return false;
    terraria_recipe_view_t *next = n ? calloc((size_t)n, sizeof(*next)) : NULL;
    if (n && !next)
        return false;
    for (int i = 0; i < n; ++i) {
        patch_handle_t recipe = PATCH_NULL;
        if (!patchlib_array_at(recipes, (size_t)i, &recipe) || !recipe ||
            !read_recipe(recipe, i, &next[i])) {
            free(next);
            return false;
        }
    }
    free(snapshot);
    snapshot = next;
    length = (size_t)n;
    valid = true;
    ++generation;
    return true;
}
void terraria_recipe_query_invalidate(void) {
    valid = false;
    ++generation;
}
uint64_t terraria_recipe_query_generation(void) { return generation; }
size_t terraria_recipe_query_count(void) {
    return (valid || terraria_recipe_query_refresh()) ? length : 0;
}
bool terraria_recipe_query_get(size_t index, terraria_recipe_view_t *out) {
    if (!out || (!valid && !terraria_recipe_query_refresh()) || index >= length)
        return false;
    *out = snapshot[index];
    return true;
}
size_t terraria_recipe_query_find(int item_type, bool as_material, size_t offset, int *indices,
                                  size_t capacity) {
    if (item_type <= 0 || (!indices && capacity) || (!valid && !terraria_recipe_query_refresh()))
        return 0;
    size_t total = 0, written = 0;
    for (size_t i = 0; i < length; ++i) {
        bool match = snapshot[i].result.type == item_type;
        if (as_material) {
            match = false;
            for (size_t m = 0; m < snapshot[i].material_count; ++m)
                if (snapshot[i].materials[m].type == item_type) {
                    match = true;
                    break;
                }
        }
        if (match) {
            if (total >= offset && written < capacity)
                indices[written++] = (int)i;
            ++total;
        }
    }
    return total;
}
