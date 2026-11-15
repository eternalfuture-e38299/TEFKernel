/*******************************************************************************
 * tefkernel - tile_manager
 *
 * Owns the custom tile registry and all engine-side registration work.
 * A mod only describes a tile and calls terraria_tile_manager_register_tile().
 ******************************************************************************/

#include "internal/terraria/content/tile_manager.h"

#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/struct/array.h"
#if defined(__ANDROID__)
#include "patchlib/il2cpp_api.h"
#endif
#if defined(__ANDROID__)
#include "patchlib/struct/list.h"
#endif
#include "patchlib/type.h"
#include "terraria/asset.h"
#include "terraria/texture2d.h"

#if defined(__ANDROID__)
#include "internal/terraria/android/content/tile_pe_patch.h"
#endif

#include <stdlib.h>
#include <string.h>

static const char *const g_main_tile_arrays[] = {
    "tileLighted",
    "tileMergeDirt",
    "tileCut",
    "tileAlch",
    "tileShine",
    "tileShine2",
    "tileStone",
    "tileAxe",
    "tileHammer",
    "tileWaterDeath",
    "tileLavaDeath",
    "tileTable",
    "tileBlockLight",
    "tileNoSunLight",
    "tileDungeon",
    "tileSpelunker",
    "tileSolidTop",
    "tileSolid",
    "tileBouncy",
    "tileOreFinderPriority",
    "tileLargeFrames",
    "tileRope",
    "tileBrick",
    "tileMoss",
    "tileNoAttach",
    "tileNoFail",
    "tileCracked",
    "tileObsidianKill",
    "tileFrameImportant",
    "tilePile",
    "tileBlendAll",
    "tileGlowMask",
    "tileContainer",
    "tileSign",
    "tileSand",
    "tileFlame",
    "tileFrame",
    "tileFrameCounter"
};

tefstd_vector_t g_terraria_tile_registry;

static int g_vanilla_tile_count;
static bool g_ids_assigned;
static patch_handle_t g_tile_count_field;
static patch_handle_t g_tile_textures_field;

#if defined(__ANDROID__)
static int g_tile_object_data_padding;
static bool g_logged_collision_layout;

static void log_collision_array_layout(patch_handle_t main_type) {
    if (g_logged_collision_layout) {
        return;
    }
    g_logged_collision_layout = true;

    patch_handle_t instance_field =
            patchlib_type_get_field(main_type, "instance");
    patch_handle_t main_instance = PATCH_NULL;
    if (patchlib_is_valid(instance_field)) {
        patchlib_field_get_value(
                instance_field, PATCH_NULL, &main_instance);
        patchlib_free(instance_field);
    }
    if (!patchlib_is_valid(main_instance)) {
        return;
    }

    static const size_t offsets[] = {
        0xb8, 0x68, 0xa8, 0xb0, 0xc0, 0xd8, 0xe0, 0xe8
    };
    tefstd_vector_t fields;
    tefstd_vector_init(&fields, sizeof(patch_handle_t));
    patchlib_type_get_fields(main_type, false, &fields);

    for (size_t i = 0; i < tefstd_vector_size(&fields); ++i) {
        patch_handle_t field =
                *(patch_handle_t *)tefstd_vector_at(&fields, i);
        if (!patchlib_is_valid(field) ||
            !patchlib_field_is_instance(field)) {
            if (patchlib_is_valid(field)) {
                patchlib_free(field);
            }
            continue;
        }

        void *pointer =
                patchlib_field_get_pointer(field, main_instance);
        const size_t offset =
                (size_t)((uintptr_t)pointer -
                         (uintptr_t)main_instance);
        bool interesting = false;
        for (size_t j = 0;
             j < sizeof(offsets) / sizeof(offsets[0]);
             ++j) {
            if (offset == offsets[j]) {
                interesting = true;
                break;
            }
        }
        if (!interesting) {
            patchlib_free(field);
            continue;
        }

        patch_handle_t array = PATCH_NULL;
        patchlib_field_get_value(field, main_instance, &array);
        const size_t length =
                patchlib_is_valid(array)
                ? patchlib_array_length(array)
                : 0;
        TEKLOG_INFO("tile Main layout +0x%zx: %s length=%zu",
                    offset, patchlib_field_get_name(field), length);
        if (patchlib_is_valid(array)) {
            patchlib_free(array);
        }
        patchlib_free(field);
    }

    tefstd_vector_destroy(&fields);

    patch_handle_t tile_field =
            patchlib_type_get_field(main_type, "tile");
    patch_handle_t tile_collection = PATCH_NULL;
    if (patchlib_is_valid(tile_field)) {
        patchlib_field_get_value(
                tile_field, main_instance, &tile_collection);
        patchlib_free(tile_field);
    }
    if (!patchlib_is_valid(tile_collection)) {
        return;
    }

    patch_handle_t tile_class =
            il2cpp_object_get_class(tile_collection);
    tefstd_vector_init(&fields, sizeof(patch_handle_t));
    patchlib_type_get_fields(tile_class, false, &fields);
    for (size_t i = 0; i < tefstd_vector_size(&fields); ++i) {
        patch_handle_t field =
                *(patch_handle_t *)tefstd_vector_at(&fields, i);
        if (!patchlib_is_valid(field) ||
            !patchlib_field_is_instance(field)) {
            if (patchlib_is_valid(field)) {
                patchlib_free(field);
            }
            continue;
        }

        void *pointer =
                patchlib_field_get_pointer(field, tile_collection);
        const size_t offset =
                (size_t)((uintptr_t)pointer -
                         (uintptr_t)tile_collection);
        bool interesting = false;
        for (size_t j = 1;
             j < sizeof(offsets) / sizeof(offsets[0]);
             ++j) {
            if (offset == offsets[j]) {
                interesting = true;
                break;
            }
        }
        if (!interesting) {
            patchlib_free(field);
            continue;
        }

        patch_handle_t array = PATCH_NULL;
        patchlib_field_get_value(field, tile_collection, &array);
        const size_t length =
                patchlib_is_valid(array)
                ? patchlib_array_length(array)
                : 0;
        TEKLOG_INFO(
                "tile collection +0x%zx: %s length=%zu",
                offset, patchlib_field_get_name(field), length);
        if (patchlib_is_valid(array)) {
            patchlib_free(array);
        }
        patchlib_free(field);
    }
    tefstd_vector_destroy(&fields);
}

static void pad_tile_object_data(int total_tiles) {
    const int required_padding =
            total_tiles - g_vanilla_tile_count;
    if (required_padding <= g_tile_object_data_padding) {
        return;
    }

    patch_handle_t type = patchlib_type_get_type(
            "Terraria.ObjectData", "TileObjectData");
    if (!patchlib_is_valid(type)) {
        TEKLOG_WARN("tile object data: TileObjectData type not found");
        return;
    }

    patch_handle_t field =
            patchlib_type_get_field(type, "_data");
    patch_handle_t list = PATCH_NULL;
    if (patchlib_is_valid(field)) {
        patchlib_field_get_value(field, PATCH_NULL, &list);
    }
    patchlib_free(type);

    if (!patchlib_is_valid(list)) {
        TEKLOG_WARN("tile object data: _data list not found");
        if (patchlib_is_valid(field)) {
            patchlib_free(field);
        }
        return;
    }

    patch_handle_t empty_entry = PATCH_NULL;
    for (int i = g_tile_object_data_padding;
         i < required_padding; ++i) {
        if (!patchlib_list_add(list, &empty_entry)) {
            TEKLOG_ERROR(
                    "tile object data: failed to add placeholder %d",
                    i);
            break;
        }
        g_tile_object_data_padding++;
    }

    TEKLOG_INFO("tile object data: padded %d placeholder(s)",
                g_tile_object_data_padding);
    patchlib_free(list);
    patchlib_free(field);
}
#endif

static bool tiles_match(const terraria_tile_handle_t *left,
                        const terraria_tile_handle_t *right) {
    return strcmp(left->parent_modloader_id, right->parent_modloader_id) == 0 &&
           strcmp(left->parent_id, right->parent_id) == 0 &&
           strcmp(left->internal_name, right->internal_name) == 0;
}

static int compare_tiles(const void *left, const void *right) {
    const terraria_tile_handle_t *a =
            *(terraria_tile_handle_t *const *)left;
    const terraria_tile_handle_t *b =
            *(terraria_tile_handle_t *const *)right;

    int result = strcmp(a->parent_modloader_id, b->parent_modloader_id);
    if (result != 0) {
        return result;
    }

    result = strcmp(a->parent_id, b->parent_id);
    if (result != 0) {
        return result;
    }
    return strcmp(a->internal_name, b->internal_name);
}

static bool resize_field_array(patch_handle_t field, const char *name,
                               int new_size) {
    if (!patchlib_is_valid(field)) {
        return false;
    }

    patch_handle_t old_array = PATCH_NULL;
    patchlib_field_get_value(field, PATCH_NULL, &old_array);
    if (!patchlib_is_valid(old_array)) {
        return false;
    }

    const size_t old_size = patchlib_array_length(old_array);
    if (old_size >= (size_t)new_size) {
        patchlib_free(old_array);
        return true;
    }

    patch_handle_t new_array =
            patchlib_array_resize(old_array, (size_t)new_size, PATCH_NULL);
    if (!patchlib_is_valid(new_array)) {
        TEKLOG_ERROR("tile resize: %s %zu -> %d failed",
                     name ? name : "?", old_size, new_size);
        patchlib_free(old_array);
        return false;
    }

    patchlib_field_set_value(field, PATCH_NULL, &new_array);
    TEKLOG_INFO("tile resize: %s %zu -> %d",
                name ? name : "?", old_size, new_size);
    patchlib_free(new_array);
    patchlib_free(old_array);
    return true;
}

static void set_main_tile_flag(patch_handle_t main_type, const char *name,
                               int runtime_id, bool value) {
    if (runtime_id < 0) {
        return;
    }

    patch_handle_t field = patchlib_type_get_field(main_type, name);
    patch_handle_t array = PATCH_NULL;
    if (patchlib_is_valid(field)) {
        patchlib_field_get_value(field, PATCH_NULL, &array);
    }

    if (patchlib_is_valid(array) &&
        (size_t)runtime_id < patchlib_array_length(array)) {
        patchlib_array_set(array, (size_t)runtime_id, &value);
    }

    if (patchlib_is_valid(array)) {
        patchlib_free(array);
    }
    if (patchlib_is_valid(field)) {
        patchlib_free(field);
    }
}

static void resize_tile_merge(patch_handle_t main_type, int new_size) {
    patch_handle_t merge_field =
            patchlib_type_get_field(main_type, "tileMerge");
    if (!patchlib_is_valid(merge_field)) {
        return;
    }

    patch_handle_t old_outer = PATCH_NULL;
    patchlib_field_get_value(merge_field, PATCH_NULL, &old_outer);
    if (!patchlib_is_valid(old_outer)) {
        patchlib_free(merge_field);
        return;
    }

    const size_t old_size = patchlib_array_length(old_outer);
    patch_handle_t outer = old_outer;
    if (old_size < (size_t)new_size) {
        outer = patchlib_array_resize(
                old_outer, (size_t)new_size, PATCH_NULL);
        if (!patchlib_is_valid(outer)) {
            patchlib_free(old_outer);
            patchlib_free(merge_field);
            return;
        }
    }

    patch_handle_t bool_type = patchlib_get_basic_type(PATCH_BOOL);
    if (patchlib_is_valid(outer) && patchlib_is_valid(bool_type)) {
        for (int i = 0; i < new_size; ++i) {
            patch_handle_t row = PATCH_NULL;
            if ((size_t)i < old_size) {
                patchlib_array_at(outer, (size_t)i, &row);
            }

            patch_handle_t new_row = PATCH_NULL;
            if (patchlib_is_valid(row)) {
                if (patchlib_array_length(row) < (size_t)new_size) {
                    new_row = patchlib_array_resize(
                            row, (size_t)new_size, PATCH_NULL);
                }
            } else {
                new_row = patchlib_array_create(
                        (size_t)new_size, bool_type);
            }

            if (patchlib_is_valid(new_row)) {
                patchlib_array_set(outer, (size_t)i, &new_row);
                patchlib_free(new_row);
            }
            if (patchlib_is_valid(row)) {
                patchlib_free(row);
            }
        }

        if (outer != old_outer) {
            patchlib_field_set_value(merge_field, PATCH_NULL, &outer);
        }
    }

    if (patchlib_is_valid(bool_type)) {
        patchlib_free(bool_type);
    }
    if (outer != old_outer && patchlib_is_valid(outer)) {
        patchlib_free(outer);
    }
    patchlib_free(old_outer);
    patchlib_free(merge_field);
}

static void resize_set_factory(patch_handle_t sets_type, int new_size) {
    patch_handle_t factory_field =
            patchlib_type_get_field(sets_type, "Factory");
    patch_handle_t factory = PATCH_NULL;
    if (patchlib_is_valid(factory_field)) {
        patchlib_field_get_value(factory_field, PATCH_NULL, &factory);
    }

    if (patchlib_is_valid(factory)) {
        patch_handle_t factory_type =
                patchlib_type_get_type("Terraria.ID", "SetFactory");
        patch_handle_t size_field = patchlib_is_valid(factory_type)
                ? patchlib_type_get_field(factory_type, "_size")
                : PATCH_NULL;

        if (patchlib_is_valid(size_field)) {
            int size = new_size;
            patchlib_field_set_value(size_field, factory, &size);
        }
        if (patchlib_is_valid(size_field)) {
            patchlib_free(size_field);
        }
        if (patchlib_is_valid(factory_type)) {
            patchlib_free(factory_type);
        }
        patchlib_free(factory);
    }
    if (patchlib_is_valid(factory_field)) {
        patchlib_free(factory_field);
    }
}

static void resize_tile_sets(int new_size) {
    patch_handle_t tile_id_type =
            patchlib_type_get_type("Terraria.ID", "TileID");
    patch_handle_t sets_type = patchlib_is_valid(tile_id_type)
            ? patchlib_type_get_inner_type(tile_id_type, "Sets")
            : PATCH_NULL;
    if (patchlib_is_valid(tile_id_type)) {
        patchlib_free(tile_id_type);
    }
    if (!patchlib_is_valid(sets_type)) {
        return;
    }

    tefstd_vector_t fields;
    tefstd_vector_init(&fields, sizeof(patch_handle_t));
    patchlib_type_get_fields(sets_type, false, &fields);

    for (size_t i = 0; i < tefstd_vector_size(&fields); ++i) {
        patch_handle_t field =
                *(patch_handle_t *)tefstd_vector_at(&fields, i);
        if (!patchlib_is_valid(field) ||
            !patchlib_field_is_static(field) ||
            patchlib_field_is_const(field)) {
            if (patchlib_is_valid(field)) {
                patchlib_free(field);
            }
            continue;
        }

        patch_handle_t field_type =
                patchlib_field_get_type_handle(field);
        const char *type_name = patchlib_is_valid(field_type)
                ? patchlib_type_get_name(field_type)
                : NULL;
        if (type_name && strstr(type_name, "[]")) {
            resize_field_array(
                    field, patchlib_field_get_name(field), new_size);
        }

        if (patchlib_is_valid(field_type)) {
            patchlib_free(field_type);
        }
        patchlib_free(field);
    }

    patch_handle_t pressure_plate =
            patchlib_type_get_field(sets_type, "PressurePlate");
    if (patchlib_is_valid(pressure_plate)) {
        patch_handle_t field_type =
                patchlib_field_get_type_handle(pressure_plate);
        const char *type_name = patchlib_is_valid(field_type)
                ? patchlib_type_get_name(field_type)
                : NULL;
        if (type_name && strstr(type_name, "[]")) {
            resize_field_array(
                    pressure_plate, "TileID.Sets.PressurePlate",
                    new_size);
        } else {
            TEKLOG_WARN(
                    "tile resize: PressurePlate has unexpected type %s",
                    type_name ? type_name : "?");
        }
        if (patchlib_is_valid(field_type)) {
            patchlib_free(field_type);
        }
        patchlib_free(pressure_plate);
    }

    tefstd_vector_destroy(&fields);
    resize_set_factory(sets_type, new_size);
    patchlib_free(sets_type);
}

static void apply_tile_flags(patch_handle_t main_type) {
    for (size_t i = 0;
         i < tefstd_vector_size(&g_terraria_tile_registry);
         ++i) {
        terraria_tile_handle_t *tile =
                *(terraria_tile_handle_t **)tefstd_vector_at(
                        &g_terraria_tile_registry, i);
        set_main_tile_flag(
                main_type, "tileSolid", tile->runtime_id, tile->solid);
        set_main_tile_flag(
                main_type, "tileSolidTop", tile->runtime_id,
                tile->solid_top);
        set_main_tile_flag(
                main_type, "tileMergeDirt", tile->runtime_id,
                tile->merge_dirt);
        set_main_tile_flag(
                main_type, "tileBlockLight", tile->runtime_id,
                tile->block_light);
        set_main_tile_flag(
                main_type, "tileFrameImportant", tile->runtime_id,
                tile->frame_important);
    }
}

static void resize_all_type_tile_arrays(patch_handle_t target_type,
                                        int new_size) {
    tefstd_vector_t fields;
    tefstd_vector_init(&fields, sizeof(patch_handle_t));
    patchlib_type_get_fields(target_type, false, &fields);

    for (size_t i = 0; i < tefstd_vector_size(&fields); ++i) {
        patch_handle_t field =
                *(patch_handle_t *)tefstd_vector_at(&fields, i);
        if (!patchlib_is_valid(field) ||
            !patchlib_field_is_static(field) ||
            patchlib_field_is_const(field)) {
            if (patchlib_is_valid(field)) {
                patchlib_free(field);
            }
            continue;
        }

        patch_handle_t field_type =
                patchlib_field_get_type_handle(field);
        const char *type_name = patchlib_is_valid(field_type)
                ? patchlib_type_get_name(field_type)
                : NULL;
        if (!type_name || !strstr(type_name, "[]")) {
            if (patchlib_is_valid(field_type)) {
                patchlib_free(field_type);
            }
            patchlib_free(field);
            continue;
        }

        patch_handle_t array = PATCH_NULL;
        patchlib_field_get_value(field, PATCH_NULL, &array);
        if (patchlib_is_valid(array) &&
            patchlib_array_length(array) ==
                    (size_t)g_vanilla_tile_count) {
            resize_field_array(
                    field, patchlib_field_get_name(field), new_size);
        }
        if (patchlib_is_valid(array)) {
            patchlib_free(array);
        }
        if (patchlib_is_valid(field_type)) {
            patchlib_free(field_type);
        }
        patchlib_free(field);
    }

    tefstd_vector_destroy(&fields);
}

#if defined(__ANDROID__)
static void log_type_array_fields(const char *namespace_name,
                                  const char *type_name) {
    patch_handle_t type =
            patchlib_type_get_type(namespace_name, type_name);
    if (!patchlib_is_valid(type)) {
        TEKLOG_WARN("tile arrays: %s.%s type not found",
                    namespace_name, type_name);
        return;
    }

    tefstd_vector_t fields;
    tefstd_vector_init(&fields, sizeof(patch_handle_t));
    patchlib_type_get_fields(type, false, &fields);
    for (size_t i = 0; i < tefstd_vector_size(&fields); ++i) {
        patch_handle_t field =
                *(patch_handle_t *)tefstd_vector_at(&fields, i);
        if (!patchlib_is_valid(field) ||
            !patchlib_field_is_static(field)) {
            if (patchlib_is_valid(field)) {
                patchlib_free(field);
            }
            continue;
        }

        patch_handle_t field_type =
                patchlib_field_get_type_handle(field);
        const char *field_type_name = patchlib_is_valid(field_type)
                ? patchlib_type_get_name(field_type)
                : NULL;
        if (field_type_name &&
            strstr(field_type_name, "[]")) {
            patch_handle_t array = PATCH_NULL;
            patchlib_field_get_value(field, PATCH_NULL, &array);
            const size_t length =
                    patchlib_is_valid(array)
                    ? patchlib_array_length(array)
                    : 0;
            TEKLOG_INFO(
                    "tile arrays: %s.%s.%s type=%s length=%zu",
                    namespace_name, type_name,
                    patchlib_field_get_name(field),
                    field_type_name, length);
            if (patchlib_is_valid(array)) {
                patchlib_free(array);
            }
        }

        if (patchlib_is_valid(field_type)) {
            patchlib_free(field_type);
        }
        patchlib_free(field);
    }
    tefstd_vector_destroy(&fields);
    patchlib_free(type);
}
#endif

bool terraria_tile_manager_register_tile(terraria_tile_handle_t *tile) {
    if (!tile || !tile->parent_modloader_id ||
        !tile->parent_id || !tile->internal_name) {
        return false;
    }

    for (size_t i = 0;
         i < tefstd_vector_size(&g_terraria_tile_registry);
         ++i) {
        terraria_tile_handle_t **existing_ptr =
                tefstd_vector_at(&g_terraria_tile_registry, i);
        if (!existing_ptr || !*existing_ptr) {
            continue;
        }

        if (tiles_match(*existing_ptr, tile)) {
            tile->runtime_id = (*existing_ptr)->runtime_id;
            *existing_ptr = tile;
            return true;
        }
    }

    tile->runtime_id = -1;
    return tefstd_vector_push_back(
            &g_terraria_tile_registry, &tile);
}

bool terraria_tile_manager_unregister_tile(terraria_tile_handle_t *tile) {
    if (!tile) {
        return false;
    }

    for (size_t i = 0;
         i < tefstd_vector_size(&g_terraria_tile_registry);
         ++i) {
        terraria_tile_handle_t **current_ptr =
                tefstd_vector_at(&g_terraria_tile_registry, i);
        if (!current_ptr || *current_ptr != tile) {
            continue;
        }

        const bool removed = tefstd_vector_erase(
                &g_terraria_tile_registry, i, PATCH_NULL);
        if (removed && g_ids_assigned) {
            terraria_tile_manager_assign_ids();
            terraria_tile_manager_resize();
        }
        return removed;
    }
    return false;
}

int terraria_tile_id_get_count(void) {
    return g_vanilla_tile_count;
}

terraria_tile_handle_t *terraria_tile_manager_get_tile(int runtime_id) {
    const int index = runtime_id - g_vanilla_tile_count;
    if (index < 0 ||
        (size_t)index >=
                tefstd_vector_size(&g_terraria_tile_registry)) {
        return NULL;
    }

    terraria_tile_handle_t **tile_ptr =
            tefstd_vector_at(
                    &g_terraria_tile_registry, (size_t)index);
    return tile_ptr ? *tile_ptr : NULL;
}

void terraria_tile_manager_assign_ids(void) {
    const size_t count =
            tefstd_vector_size(&g_terraria_tile_registry);
    qsort(g_terraria_tile_registry.data, count,
          sizeof(terraria_tile_handle_t *), compare_tiles);

    for (size_t i = 0; i < count; ++i) {
        terraria_tile_handle_t **tile_ptr =
                tefstd_vector_at(&g_terraria_tile_registry, i);
        if (tile_ptr && *tile_ptr) {
            (*tile_ptr)->runtime_id =
                    g_vanilla_tile_count + (int)i;
        }
    }
    g_ids_assigned = true;
}

void terraria_tile_manager_resize(void) {
    const size_t custom_count =
            tefstd_vector_size(&g_terraria_tile_registry);
    if (custom_count == 0 || g_vanilla_tile_count <= 0) {
        return;
    }

    const int total = g_vanilla_tile_count + (int)custom_count;
#if defined(__ANDROID__)
    terraria_tile_pe_patch_apply(g_vanilla_tile_count, total);
    pad_tile_object_data(total);
#endif

    if (patchlib_is_valid(g_tile_count_field) &&
        !patchlib_field_is_const(g_tile_count_field)) {
        unsigned short count = (unsigned short)total;
        patchlib_field_set_value(
                g_tile_count_field, PATCH_NULL, &count);
    }

    resize_tile_sets(total);

    patch_handle_t main_type =
            patchlib_type_get_type("Terraria", "Main");
    if (!patchlib_is_valid(main_type)) {
        return;
    }

#if defined(__ANDROID__)
    log_collision_array_layout(main_type);
#endif

    for (size_t i = 0;
         i < sizeof(g_main_tile_arrays) /
                     sizeof(g_main_tile_arrays[0]);
         ++i) {
        patch_handle_t field = patchlib_type_get_field(
                main_type, g_main_tile_arrays[i]);
        resize_field_array(
                field, g_main_tile_arrays[i], total);
        if (patchlib_is_valid(field)) {
            patchlib_free(field);
        }
    }

    resize_all_type_tile_arrays(main_type, total);

    patch_handle_t tile_data_type =
            patchlib_type_get_type("Terraria", "TileData");
    if (patchlib_is_valid(tile_data_type)) {
        resize_all_type_tile_arrays(tile_data_type, total);
        patchlib_free(tile_data_type);
    }
#if defined(__ANDROID__)
    log_type_array_fields("Terraria", "TileData");
#endif

    resize_tile_merge(main_type, total);
    apply_tile_flags(main_type);
    patchlib_free(main_type);
}

void terraria_tile_manager_init_textures(void) {
    const size_t custom_count =
            tefstd_vector_size(&g_terraria_tile_registry);
    const int total = g_vanilla_tile_count + (int)custom_count;
    if (custom_count == 0 ||
        !resize_field_array(
                g_tile_textures_field,
                "TextureAssets.Tile", total)) {
        return;
    }

    patch_handle_t textures = PATCH_NULL;
    patchlib_field_get_value(
            g_tile_textures_field, PATCH_NULL, &textures);
    if (!patchlib_is_valid(textures)) {
        return;
    }

    for (size_t i = 0; i < custom_count; ++i) {
        terraria_tile_handle_t *tile =
                *(terraria_tile_handle_t **)tefstd_vector_at(
                        &g_terraria_tile_registry, i);
        if (!tile || !tile->tile_ops.get_texture) {
            continue;
        }

        patch_handle_t asset =
                tile->tile_ops.get_texture(tile);
        if (patchlib_is_valid(asset)) {
            patchlib_array_set(
                    textures, (size_t)tile->runtime_id, &asset);
            patch_handle_t stored = PATCH_NULL;
            const bool stored_ok =
                    patchlib_array_at(
                            textures, (size_t)tile->runtime_id,
                            &stored) &&
                    patchlib_is_valid(stored);
            TEKLOG_INFO(
                    "tile texture set: %s.%s index=%d asset=%p stored=%s",
                    tile->parent_id, tile->internal_name,
                    tile->runtime_id, asset,
                    stored_ok ? "true" : "false");
            if (patchlib_is_valid(stored)) {
                patchlib_free(stored);
            }
            patchlib_free(asset);
        } else {
            TEKLOG_ERROR(
                    "tile texture missing: %s.%s index=%d",
                    tile->parent_id, tile->internal_name,
                    tile->runtime_id);
        }
    }
    patchlib_free(textures);
}

void terraria_tile_manager_init_static(void) {
    for (size_t i = 0;
         i < tefstd_vector_size(&g_terraria_tile_registry);
         ++i) {
        terraria_tile_handle_t *tile =
                *(terraria_tile_handle_t **)tefstd_vector_at(
                        &g_terraria_tile_registry, i);
        if (tile && tile->tile_ops.init_static) {
            tile->tile_ops.init_static(tile);
        }
    }
}

void terraria_tile_manager_init(void) {
    tefstd_vector_init(
            &g_terraria_tile_registry,
            sizeof(terraria_tile_handle_t *));

    patch_handle_t tile_id_type =
            patchlib_type_get_type("Terraria.ID", "TileID");
    if (patchlib_is_valid(tile_id_type)) {
        g_tile_count_field =
                patchlib_type_get_field(tile_id_type, "Count");
        patchlib_free(tile_id_type);
    }

    unsigned short vanilla_count = 0;
    if (patchlib_is_valid(g_tile_count_field)) {
        patchlib_field_get_value(
                g_tile_count_field, PATCH_NULL, &vanilla_count);
    }
    g_vanilla_tile_count = (int)vanilla_count;

    patch_handle_t texture_assets =
            patchlib_type_get_type(
                    "Terraria.GameContent", "TextureAssets");
    if (patchlib_is_valid(texture_assets)) {
        g_tile_textures_field =
                patchlib_type_get_field(texture_assets, "Tile");
        patchlib_free(texture_assets);
    }

#if defined(__ANDROID__)
    terraria_tile_pe_patch_init();
#endif

    TEKLOG_INFO("tile manager initialized: vanilla count = %d",
                g_vanilla_tile_count);
}
