/*******************************************************************************
 * tefkernel - tile_manager
 *
 * Owns the custom tile registry and all engine-side registration work.
 * A mod only describes a tile and calls terraria_tile_manager_register_tile().
 ******************************************************************************/

#include "internal/terraria/content/tile_manager.h"

#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"
#if defined(__ANDROID__)
#include "patchlib/il2cpp_api.h"
#endif
#include "patchlib/struct/list.h"
#include "patchlib/type.h"
#include "terraria/asset.h"
#include "terraria/texture2d.h"

#if defined(__ANDROID__)
#include "internal/terraria/android/content/tile_pe_patch.h"
#endif

#include <stdio.h>
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

/* 提前声明：resize_tile_sets() 里要递归处理 TileID.Sets 的嵌套类 */
static void resize_all_type_tile_arrays(patch_handle_t target_type,
                                        int new_size);

static int g_vanilla_tile_count;
static bool g_ids_assigned;
static patch_handle_t g_tile_count_field;
static patch_handle_t g_tile_textures_field;

static int g_tile_object_data_padding;

#if defined(__ANDROID__)
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

#endif /* __ANDROID__ */

/* TileObjectData._data 是 List<TileObjectData>，GetTileData(type) 会先检查
 * type < _data.Count，超出就抛 ArgumentOutOfRangeException（PC/PE 都有这层判断），
 * 所以两端都要给自定义 ID 补占位项。 */
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

/* 写 bool 数组元素。
 * Android 的 patchlib_array_set 是 memcpy(element_size)，传 1 字节即可；
 * 桌面 loader 对 bool 按 4 字节 BOOL 封送（Marshal.SizeOf(bool)==4），逐元素
 * set 不可靠（实测 write=0 read=1），所以改为整表暂存读写。 */
static bool set_bool_array_element(patch_handle_t array, size_t index,
                                   bool value) {
    if (!patchlib_is_valid(array)) {
        return false;
    }
    const size_t length = patchlib_array_length(array);
    if (index >= length) {
        return false;
    }
#if defined(__ANDROID__)
    unsigned char byte = value ? 1 : 0;
    return patchlib_array_set(array, index, &byte);
#else
    if (length > 8192) {
        /* 防御：异常长度不走整表路径 */
        int word = value ? 1 : 0;
        return patchlib_array_set(array, index, &word);
    }
    int *stage = (int *)malloc(length * sizeof(int));
    if (!stage) {
        return false;
    }
    bool ok = false;
    if (patchlib_array_copy_to_c(stage, array, length)) {
        stage[index] = value ? 1 : 0;
        ok = patchlib_array_copy_from_c(array, stage, length);
    }
    free(stage);
    return ok;
#endif
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
    if (!patchlib_is_valid(array)) {
        TEKLOG_WARN("tile flag: %s array not found", name);
        if (patchlib_is_valid(field)) {
            patchlib_free(field);
        }
        return;
    }

    const size_t length = patchlib_array_length(array);
    if ((size_t)runtime_id >= length) {
        TEKLOG_WARN("tile flag: %s length %zu <= id %d (resize missing?)",
                    name, length, runtime_id);
        patchlib_free(array);
        patchlib_free(field);
        return;
    }

    const bool ok = set_bool_array_element(array, (size_t)runtime_id, value);

    int readback = -1;
    const bool read_ok =
            patchlib_array_at(array, (size_t)runtime_id, &readback);
    if (!ok || !read_ok || (readback != 0) != value) {
        TEKLOG_WARN(
                "tile flag: %s[%d] write=%d read=%d (len=%zu, ok=%d, read_ok=%d)",
                name, runtime_id, value ? 1 : 0, readback, length, ok ? 1 : 0,
                read_ok ? 1 : 0);
    } else {
        TEKLOG_INFO("tile flag: %s[%d] = %d (len=%zu)",
                    name, runtime_id, value ? 1 : 0, length);
    }

    patchlib_free(array);
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

    /* TileID.Sets 下面还有嵌套静态类，它们的数组同样是按图格 ID 定长的：
     *   Conversion / TileCutIgnore / ForAdvancedCollision / RoomNeeds / Wiring
     * 例如 WorldGen.WouldTileReplacementWork 里的
     *   TileID.Sets.Conversion.Grass[tileType]
     * 只扩 Sets 本层字段会漏掉它们（放置/替换方块时直接越界）。 */
    tefstd_vector_t inner_types;
    tefstd_vector_init(&inner_types, sizeof(patch_handle_t));
    patchlib_type_get_inner_types(sets_type, false, &inner_types);
    for (size_t i = 0; i < tefstd_vector_size(&inner_types); ++i) {
        patch_handle_t inner =
                *(patch_handle_t *)tefstd_vector_at(&inner_types, i);
        if (patchlib_is_valid(inner)) {
            resize_all_type_tile_arrays(inner, new_size);
        }
    }
    tefstd_vector_destroy(&inner_types);

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

/* 下面这些类型里也有一批「按图格 ID 定长、初始化时只分配一次」的表。
 * 原版很多写法是 for (i = 0; i < TileID.Count; i++) table[i] 或者直接 [type]，
 * 只扩 Main / TileData 会漏掉它们：
 *   - WorldGen.Skyblock.hasTile  → 读档最后 ScanTiles() 越界（表现为「加载失败」）
 *   - WorldGen.houseTile / tileCounts → 房屋检测 / 图格统计
 *   - Recipe.TileUsedInRecipes / TileCountsAs → Player.SetAdjTile
 *   - MapHelper.tileOptionCounts / tileLookup → 地图保存/加载
 *   - TileMaterials.MaterialsByTileId → 高尔夫碰撞材质
 *   - TextureAssets.HighlightMask → 智能光标高亮描边 */
/* 引用类型数组扩容后新槽位是 null，有两处不能是 null：
 *   - TextureAssets.HighlightMask[type].Value（智能光标高亮描边）
 *   - TileMaterials.MaterialsByTileId[type]（高尔夫碰撞材质）
 * 用数组里第一个非 null 元素做模板填进新槽位。 */
static void fill_new_slots(patch_handle_t field, const char *name,
                           int from_index, int to_index) {
    if (!patchlib_is_valid(field) || to_index <= from_index)
        return;
    patch_handle_t array = PATCH_NULL;
    patchlib_field_get_value(field, PATCH_NULL, &array);
    if (!patchlib_is_valid(array))
        return;

    patch_handle_t template_value = PATCH_NULL;
    const size_t length = patchlib_array_length(array);
    for (size_t i = 0; i < length && i < (size_t)from_index; ++i) {
        patch_handle_t candidate = PATCH_NULL;
        if (patchlib_array_at(array, i, &candidate) &&
            patchlib_is_valid(candidate)) {
            template_value = candidate;
            break;
        }
        if (patchlib_is_valid(candidate))
            patchlib_free(candidate);
    }

    if (patchlib_is_valid(template_value)) {
        int filled = 0;
        for (int i = from_index; i < to_index && (size_t)i < length; ++i) {
            patch_handle_t current = PATCH_NULL;
            patchlib_array_at(array, (size_t)i, &current);
            if (!patchlib_is_valid(current)) {
                patchlib_array_set(array, (size_t)i, &template_value);
                filled++;
            }
            if (patchlib_is_valid(current))
                patchlib_free(current);
        }
        patchlib_free(template_value);
        if (filled > 0)
            TEKLOG_INFO("tile resize: %s filled %d new slot(s)",
                        name ? name : "?", filled);
    }
    patchlib_free(array);
}

static void fill_vanilla_reference_slots(int total) {
    patch_handle_t texture_assets = patchlib_type_get_type(
            "Terraria.GameContent", "TextureAssets");
    if (patchlib_is_valid(texture_assets)) {
        patch_handle_t field =
                patchlib_type_get_field(texture_assets, "HighlightMask");
        fill_new_slots(field, "TextureAssets.HighlightMask",
                       g_vanilla_tile_count, total);
        if (patchlib_is_valid(field))
            patchlib_free(field);
        patchlib_free(texture_assets);
    }

    patch_handle_t materials = patchlib_type_get_type(
            "Terraria.GameContent.Metadata", "TileMaterials");
    if (patchlib_is_valid(materials)) {
        patch_handle_t field =
                patchlib_type_get_field(materials, "MaterialsByTileId");
        fill_new_slots(field, "TileMaterials.MaterialsByTileId",
                       g_vanilla_tile_count, total);
        if (patchlib_is_valid(field))
            patchlib_free(field);
        patchlib_free(materials);
    }
}

/* TileMaterials 的静态构造里才 new 出 MaterialsByTileId，如果我们在它初始化
 * 之前去扩数组只能拿到 null。这里先调一次 GetByTileId(0) 强制跑静态构造。 */
static void ensure_tile_materials_initialized(void) {
    patch_handle_t type = patchlib_type_get_type(
            "Terraria.GameContent.Metadata", "TileMaterials");
    if (!patchlib_is_valid(type))
        return;
    patch_handle_t getter = patchlib_type_get_method_by_param_count(
            type, "GetByTileId", 1);
    if (patchlib_is_valid(getter)) {
        unsigned short zero = 0;
        void *args[1] = {&zero};
        patch_handle_t material = PATCH_NULL;
        patchlib_method_invoke_args(getter, PATCH_NULL, &material, args);
        if (patchlib_is_valid(material))
            patchlib_free(material);
        patchlib_free(getter);
    }
    patchlib_free(type);
}

static void resize_vanilla_type_tables(int new_size) {
    ensure_tile_materials_initialized();
    static const struct {
        const char *namespace_name;
        const char *type_name;
    } types[] = {
        {"Terraria", "WorldGen"},
        {"Terraria", "Recipe"},
        {"Terraria.Map", "MapHelper"},
        {"Terraria.GameContent.Metadata", "TileMaterials"},
        {"Terraria.GameContent", "TextureAssets"},
    };

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
        patch_handle_t type = patchlib_type_get_type(
                types[i].namespace_name, types[i].type_name);
        if (!patchlib_is_valid(type)) {
            TEKLOG_WARN("tile resize: type %s not found", types[i].type_name);
            continue;
        }
        resize_all_type_tile_arrays(type, new_size);
        patchlib_free(type);
    }

    /* 嵌套类型：WorldGen.Skyblock.hasTile */
    patch_handle_t worldgen = patchlib_type_get_type("Terraria", "WorldGen");
    if (patchlib_is_valid(worldgen)) {
        patch_handle_t skyblock =
                patchlib_type_get_inner_type(worldgen, "Skyblock");
        if (patchlib_is_valid(skyblock)) {
            resize_all_type_tile_arrays(skyblock, new_size);
            patchlib_free(skyblock);
        }
        patchlib_free(worldgen);
    }

    fill_vanilla_reference_slots(new_size);
}

/* 实例上的定长表：SceneMetrics._tileCounts（ScanTiles 里 [type]++）、
 * Player.adjTile（SetAdjTile 里 [type] = true）。
 * 这些对象在 Main.Initialize / 静态初始化阶段就建好了，长度停在旧值。 */
static bool resize_array_on_instance(patch_handle_t field,
                                     patch_handle_t instance,
                                     const char *name, int new_size) {
    if (!patchlib_is_valid(field) || !patchlib_is_valid(instance))
        return false;

    patch_handle_t old_array = PATCH_NULL;
    patchlib_field_get_value(field, instance, &old_array);
    if (!patchlib_is_valid(old_array))
        return false;

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

    patchlib_field_set_value(field, instance, &new_array);
    if (name) {
        TEKLOG_INFO("tile resize: %s %zu -> %d",
                    name, old_size, new_size);
    }
    patchlib_free(new_array);
    patchlib_free(old_array);
    return true;
}

static void resize_instance_type_tables(int new_size) {
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    if (!patchlib_is_valid(main_type))
        return;

    patch_handle_t metrics_type =
            patchlib_type_get_type("Terraria", "SceneMetrics");
    if (patchlib_is_valid(metrics_type)) {
        patch_handle_t counts =
                patchlib_type_get_field(metrics_type, "_tileCounts");
        static const char *const holders[] = {
            "_playerSceneMetrics", "_cameraSceneMetrics"};
        for (size_t i = 0; i < sizeof(holders) / sizeof(holders[0]); ++i) {
            patch_handle_t holder =
                    patchlib_type_get_field(main_type, holders[i]);
            patch_handle_t instance = PATCH_NULL;
            if (patchlib_is_valid(holder))
                patchlib_field_get_value(holder, PATCH_NULL, &instance);
            if (patchlib_is_valid(instance)) {
                resize_array_on_instance(counts, instance,
                                         "SceneMetrics._tileCounts", new_size);
                patchlib_free(instance);
            }
            if (patchlib_is_valid(holder))
                patchlib_free(holder);
        }
        if (patchlib_is_valid(counts))
            patchlib_free(counts);
        patchlib_free(metrics_type);
    }

    patch_handle_t player_type = patchlib_type_get_type("Terraria", "Player");
    patch_handle_t players_field =
            patchlib_type_get_field(main_type, "player");
    if (patchlib_is_valid(player_type) &&
        patchlib_is_valid(players_field)) {
        patch_handle_t adj = patchlib_type_get_field(player_type, "adjTile");
        patch_handle_t players = PATCH_NULL;
        patchlib_field_get_value(players_field, PATCH_NULL, &players);
        if (patchlib_is_valid(players) && patchlib_is_valid(adj)) {
            const size_t player_count = patchlib_array_length(players);
            int resized = 0;
            for (size_t i = 0; i < player_count; ++i) {
                patch_handle_t player = PATCH_NULL;
                if (!patchlib_array_at(players, i, &player) ||
                    !patchlib_is_valid(player))
                    continue;
                if (resize_array_on_instance(
                            adj, player, NULL, new_size)) {
                    resized++;
                }
                patchlib_free(player);
            }
            if (resized > 0) {
                TEKLOG_INFO("tile resize: Player.adjTile 754/… -> %d (%d players)",
                            new_size, resized);
            }
        }
        if (patchlib_is_valid(players))
            patchlib_free(players);
        if (patchlib_is_valid(adj))
            patchlib_free(adj);
    }
    if (patchlib_is_valid(players_field))
        patchlib_free(players_field);
    if (patchlib_is_valid(player_type))
        patchlib_free(player_type);
    patchlib_free(main_type);
}

/* MapHelper.Initialize 在 Main.Initialize 体内才跑，之前扩容会被跳过，
 * 于是 tileOptionCounts/tileLookup 停在 754（地图保存/加载按
 * i < TileID.Count 遍历就会越界）。这里在它跑完之后补一次。 */
static patch_hook_id_t h_map_helper_init = PATCH_HOOK_INVALID_ID;

static void map_helper_init_postfix(patch_handle_t this, void **args,
                                    void *result,
                                    const patch_method_signature_t *sig) {
    (void) this;
    (void) args;
    (void) result;
    (void) sig;

    const size_t custom = tefstd_vector_size(&g_terraria_tile_registry);
    if (custom == 0 || g_vanilla_tile_count <= 0)
        return;
    const int total = g_vanilla_tile_count + (int)custom;

    patch_handle_t map_helper =
            patchlib_type_get_type("Terraria.Map", "MapHelper");
    if (patchlib_is_valid(map_helper)) {
        resize_all_type_tile_arrays(map_helper, total);
        patchlib_free(map_helper);
    }

    /* 到这里 TileMaterials 的静态构造一定已经跑过，顺便补上它的扩容 */
    ensure_tile_materials_initialized();
    patch_handle_t materials = patchlib_type_get_type(
            "Terraria.GameContent.Metadata", "TileMaterials");
    if (patchlib_is_valid(materials)) {
        resize_all_type_tile_arrays(materials, total);
        patchlib_free(materials);
    }
    fill_vanilla_reference_slots(total);
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

    pad_tile_object_data(total);

#if defined(__ANDROID__)
    terraria_tile_pe_patch_apply(g_vanilla_tile_count, total);
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
    resize_vanilla_type_tables(total);
    resize_instance_type_tables(total);

    patch_handle_t tile_data_type = PATCH_NULL;
#if defined(__ANDROID__)
    /* PE 的图格数据全在 TileData 的裸指针池里；PC 没有这个类型 */
    tile_data_type = patchlib_type_get_type("Terraria", "TileData");
    if (patchlib_is_valid(tile_data_type)) {
        resize_all_type_tile_arrays(tile_data_type, total);
        patchlib_free(tile_data_type);
    }
    log_type_array_fields("Terraria", "TileData");
#else
    (void) tile_data_type;
#endif

    resize_tile_merge(main_type, total);
    apply_tile_flags(main_type);
    patchlib_free(main_type);
}

/* Main.LoadTiles(i) 会做：
 *     if (TextureAssets.Tile[i].State == 0)
 *         Assets.Request<Texture2D>(TextureAssets.Tile[i].Name, Immediate);
 * 我们注入的 Asset 由 terraria_asset_create 用空字符串当 Name，
 * 只要 State 不是 Loaded（或中途被重置），就会抛
 *     AssetLoadException: Asset could not be found: ""
 * 所以这里给自定义图格的 Asset 复制一个原版图格的资源名，并强制 State=Loaded。 */
static void prepare_tile_asset(patch_handle_t textures, patch_handle_t asset,
                               size_t index) {
    patch_handle_t asset_class =
            terraria_asset_get_generic_class(terraria_texture2d_get_class());
    if (!patchlib_is_valid(asset_class)) {
        return;
    }

    patch_handle_t name_field =
            patchlib_type_get_field(asset_class, "<Name>k__BackingField");
    if (!patchlib_is_valid(name_field)) {
        name_field = patchlib_type_get_field(asset_class, "Name");
    }
    patch_handle_t state_field =
            patchlib_type_get_field(asset_class, "<State>k__BackingField");
    if (!patchlib_is_valid(state_field)) {
        state_field = patchlib_type_get_field(asset_class, "State");
    }

    /* 借用一个原版图格的 Name；没有就保持原样（下面会打 WARN） */
    if (patchlib_is_valid(name_field)) {
        patch_handle_t template_asset = PATCH_NULL;
        if (patchlib_array_at(textures, 1, &template_asset) &&
            patchlib_is_valid(template_asset)) {
            patch_handle_t template_name = PATCH_NULL;
            patchlib_field_get_value(
                    name_field, template_asset, &template_name);
            if (patchlib_is_valid(template_name)) {
                patchlib_field_set_value(name_field, asset, &template_name);
                patchlib_free(template_name);
                TEKLOG_INFO("tile asset[%zu]: name copied from tile 1",
                            index);
            }
            patchlib_free(template_asset);
        }
        patchlib_free(name_field);
    } else {
        TEKLOG_WARN("tile asset[%zu]: Name field not found", index);
    }

    if (patchlib_is_valid(state_field)) {
        int loaded = 2; /* ReLogic.Content.AssetState.Loaded */
        patchlib_field_set_value(state_field, asset, &loaded);
        patchlib_free(state_field);
    }
    patchlib_free(asset_class);
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
            prepare_tile_asset(
                    textures, asset, (size_t)tile->runtime_id);
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

    /* LoadTextures 之后高亮描边图才有真实 Asset，这里再补一次模板填充 */
    fill_vanilla_reference_slots(total);
}

void terraria_tile_manager_init_static(void) {
    /* 最后一轮兜底：Main.Initialize 里的默认值初始化可能把我们的旗标覆盖掉，
     * 这里再写一次（PC 上 tileSolid 决定能不能穿过）。 */
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    if (patchlib_is_valid(main_type)) {
        apply_tile_flags(main_type);
        patchlib_free(main_type);
    }

    /* TileObjectData._data 是 Main.Initialize 里才创建的，这里再补一次占位 */
    pad_tile_object_data(g_vanilla_tile_count +
                         (int)tefstd_vector_size(&g_terraria_tile_registry));

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

/* Player.LoadPlayer 会 new Player() 替换 Main.player[i]，新实例的 adjTile
 * 又是 754；而 Player.AdjTiles()/SetAdjTile() 会直接 adjTile[tileType] = true。
 * 这里在 SetAdjTile 上做懒扩容：长度不足就先扩，再让原函数写。 */
static patch_handle_t f_player_adj_tile = PATCH_NULL;
static patch_handle_t m_player_set_adj_tile = PATCH_NULL;

static void ensure_player_adj_tile(patch_handle_t player) {
    const size_t custom = tefstd_vector_size(&g_terraria_tile_registry);
    if (!patchlib_is_valid(player) || custom == 0 || g_vanilla_tile_count <= 0)
        return;
    if (!patchlib_is_valid(f_player_adj_tile))
        return;
    const int total = g_vanilla_tile_count + (int)custom;
    resize_array_on_instance(f_player_adj_tile, player, NULL, total);
}

static bool player_set_adj_tile_prefix(patch_handle_t this, void **args,
                                       const patch_method_signature_t *sig,
                                       void *result) {
    (void) args;
    (void) sig;
    (void) result;
    ensure_player_adj_tile(this);
    return false; /* 继续执行原函数（此时 adjTile 已是扩容后的数组） */
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

    /* Player.adjTile 懒扩容（LoadPlayer 会换新实例，初始化时的那次扩容会失效） */
    patch_handle_t player_type = patchlib_type_get_type("Terraria", "Player");
    if (patchlib_is_valid(player_type)) {
        f_player_adj_tile = patchlib_type_get_field(player_type, "adjTile");
        m_player_set_adj_tile = patchlib_type_get_method_by_param_count(
                player_type, "SetAdjTile", 1);
        if (patchlib_is_valid(m_player_set_adj_tile)) {
            patchlib_install_prepost_hook(
                    m_player_set_adj_tile, player_set_adj_tile_prefix, NULL);
        }
        patchlib_free(player_type);
    }

    /* 地图表要在 MapHelper.Initialize 之后补一次扩容 */
    patch_handle_t map_helper =
            patchlib_type_get_type("Terraria.Map", "MapHelper");
    if (patchlib_is_valid(map_helper)) {
        patch_handle_t map_init =
                patchlib_type_get_method_by_param_count(
                        map_helper, "Initialize", 0);
        if (patchlib_is_valid(map_init)) {
            h_map_helper_init = patchlib_install_prepost_hook(
                    map_init, NULL, map_helper_init_postfix);
            patchlib_free(map_init);
        }
        patchlib_free(map_helper);
    }

    TEKLOG_INFO("tile manager initialized: vanilla count = %d",
                g_vanilla_tile_count);
}
