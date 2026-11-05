#ifndef TEFKERNEL_RECIPE_QUERY_H
#define TEFKERNEL_RECIPE_QUERY_H
#include "../patchlib/type.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#if defined(__ANDROID__)
#define TERRARIA_RECIPE_REQUIREMENTS_MAX 64
#define TERRARIA_RECIPE_STATIONS_MAX 64
typedef struct terraria_recipe_item_t {
    int type, stack;
} terraria_recipe_item_t;
typedef struct terraria_recipe_view_t {
    int index;
    terraria_recipe_item_t result;
    size_t material_count, station_count;
    terraria_recipe_item_t materials[TERRARIA_RECIPE_REQUIREMENTS_MAX];
    int stations[TERRARIA_RECIPE_STATIONS_MAX];
    /* Bitmask of readable vanilla condition fields; values only meaningful when known. */
    unsigned conditions_known, conditions;
} terraria_recipe_view_t;
/* Cached value copies. No game handles escape. Call on the game thread.
 * Exact ingredient IDs only: recipe groups/substitutions need a separate adapter.
 * Index is the current runtime Recipe index, not a persistent identity. */
DEFINE_FUNCTION(bool, terraria_recipe_query_refresh, void)
DEFINE_FUNCTION(void, terraria_recipe_query_invalidate, void)
DEFINE_FUNCTION(uint64_t, terraria_recipe_query_generation, void)
DEFINE_FUNCTION(size_t, terraria_recipe_query_count, void)
DEFINE_FUNCTION(bool, terraria_recipe_query_get, size_t index, terraria_recipe_view_t *out)
/* Returns total matches; writes at most capacity runtime recipe indices.
 * offset is a match offset for pagination, not a game recipe index. */
DEFINE_FUNCTION(size_t, terraria_recipe_query_find, int item_type, bool as_material, size_t offset,
                int *indices, size_t capacity)
#endif
#ifdef __cplusplus
}
#endif
#endif
