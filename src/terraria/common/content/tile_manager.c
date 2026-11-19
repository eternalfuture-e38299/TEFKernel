/*******************************************************************************
 * tefkernel - tile_manager
 *
 * Owns the custom tile registry and all engine-side registration work.
 * A mod only describes a tile and calls terraria_tile_manager_register_tile().
 ******************************************************************************/

#include "internal/terraria/content/tile_manager.h"

#include "internal/log.h"
#include "internal/terraria/content/item_manager.h"
#include "internal/terraria/entity.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"
#if defined(__ANDROID__)
#include "patchlib/il2cpp_api.h"
#endif
#include "patchlib/struct/list.h"
#include "patchlib/type.h"
#include "terraria/asset.h"
#include "terraria/language_manager.h"
#include "terraria/texture2d.h"

#if defined(__ANDROID__)
#include "internal/terraria/android/content/tile_pe_patch.h"
#endif

#include <stdint.h>
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

/* 提前声明 */
static void resize_all_type_tile_arrays(patch_handle_t target_type,
                                        int new_size);
static void tile_resize_impl(int total);
static bool set_bool_array_element(patch_handle_t array, size_t index,
                                   bool value);

static int g_vanilla_tile_count;
/* 当前所有「按图格 ID 定长」的表已经扩到的长度。
 * 正常等于 原版 + 已注册自定义图格；读档时可以临时按世界文件里的
 * 图格数量扩大（Mod 缺失时靠它避免 LoadWorldTiles 越界）。 */
static int g_tile_capacity;
static bool g_ids_assigned;
static patch_handle_t g_tile_count_field;
static patch_handle_t g_tile_textures_field;

/* MapHelper 自定义图格查找表范围 */
static unsigned short g_map_mod_position;
static unsigned short g_map_mod_end;

/* 跨平台读取 Tile.type / active / frameX / frameY 缓存 */
static patch_handle_t g_tile_class = PATCH_NULL;
#if !defined(__ANDROID__)
static patch_handle_t f_pc_tile_type = PATCH_NULL;
static patch_handle_t f_pc_tile_sheader = PATCH_NULL;
static patch_handle_t f_pc_tile_framex = PATCH_NULL;
static patch_handle_t f_pc_tile_framey = PATCH_NULL;
#else
static patch_handle_t g_pe_tile_data_class = PATCH_NULL;
static uint32_t *const *g_pe_tile_lookup_slot = NULL;
static const uint16_t *const *g_pe_tile_type_slot = NULL;
static const int16_t *const *g_pe_tile_sheader_slot = NULL;
static const int16_t *const *g_pe_tile_framex_slot = NULL;
static const int16_t *const *g_pe_tile_framey_slot = NULL;
static patch_handle_t f_main_max_tiles_x = PATCH_NULL;
static patch_handle_t f_main_max_tiles_y = PATCH_NULL;
static const int *g_pe_max_tiles_x_slot = NULL;
static const int *g_pe_max_tiles_y_slot = NULL;
#endif

static void ensure_tile_access_initialized(void) {
#if defined(__ANDROID__)
    if (!patchlib_is_valid(g_pe_tile_data_class)) {
        g_pe_tile_data_class = patchlib_type_get_type("Terraria", "TileData");
        if (patchlib_is_valid(g_pe_tile_data_class)) {
            if (il2cpp_runtime_class_init) {
                il2cpp_runtime_class_init((void *)g_pe_tile_data_class);
            }
            patch_handle_t f_lookup = patchlib_type_get_field(g_pe_tile_data_class, "TileLookup");
            patch_handle_t f_type = patchlib_type_get_field(g_pe_tile_data_class, "TileType");
            patch_handle_t f_shdr = patchlib_type_get_field(g_pe_tile_data_class, "TileSHeader");
            patch_handle_t f_fx = patchlib_type_get_field(g_pe_tile_data_class, "TileFrameX");
            patch_handle_t f_fy = patchlib_type_get_field(g_pe_tile_data_class, "TileFrameY");
            if (patchlib_is_valid(f_lookup)) {
                g_pe_tile_lookup_slot = (uint32_t *const *)patchlib_field_get_pointer(f_lookup, PATCH_NULL);
            }
            if (patchlib_is_valid(f_type)) {
                g_pe_tile_type_slot = (const uint16_t *const *)patchlib_field_get_pointer(f_type, PATCH_NULL);
            }
            if (patchlib_is_valid(f_shdr)) {
                g_pe_tile_sheader_slot = (const int16_t *const *)patchlib_field_get_pointer(f_shdr, PATCH_NULL);
            }
            if (patchlib_is_valid(f_fx)) {
                g_pe_tile_framex_slot = (const int16_t *const *)patchlib_field_get_pointer(f_fx, PATCH_NULL);
            }
            if (patchlib_is_valid(f_fy)) {
                g_pe_tile_framey_slot = (const int16_t *const *)patchlib_field_get_pointer(f_fy, PATCH_NULL);
            }
        }
    }
    if (!patchlib_is_valid(f_main_max_tiles_x)) {
        patch_handle_t main_cls = patchlib_type_get_type("Terraria", "Main");
        if (patchlib_is_valid(main_cls)) {
            f_main_max_tiles_x = patchlib_type_get_field(main_cls, "maxTilesX");
            f_main_max_tiles_y = patchlib_type_get_field(main_cls, "maxTilesY");
            if (patchlib_is_valid(f_main_max_tiles_x)) {
                g_pe_max_tiles_x_slot = (const int *)patchlib_field_get_pointer(
                        f_main_max_tiles_x, PATCH_NULL);
            }
            if (patchlib_is_valid(f_main_max_tiles_y)) {
                g_pe_max_tiles_y_slot = (const int *)patchlib_field_get_pointer(
                        f_main_max_tiles_y, PATCH_NULL);
            }
            patchlib_free(main_cls);
        }
    }
#else
    if (!patchlib_is_valid(g_tile_class)) {
        g_tile_class = patchlib_type_get_type("Terraria", "Tile");
        if (patchlib_is_valid(g_tile_class)) {
            f_pc_tile_type = patchlib_type_get_field(g_tile_class, "type");
            f_pc_tile_sheader = patchlib_type_get_field(g_tile_class, "sTileHeader");
            f_pc_tile_framex = patchlib_type_get_field(g_tile_class, "frameX");
            f_pc_tile_framey = patchlib_type_get_field(g_tile_class, "frameY");
        }
    }
#endif
}

static bool read_tile_from_arg(void *arg_ptr, unsigned short *out_type,
                               bool *out_active, short *out_frame_x,
                               short *out_frame_y) {
    if (!arg_ptr) {
        return false;
    }
    ensure_tile_access_initialized();

#if defined(__ANDROID__)
    const int32_t tile_offset = *(const int32_t *)arg_ptr;
    if (tile_offset < 0 || !g_pe_tile_lookup_slot || !g_pe_tile_type_slot) {
        return false;
    }
    int max_x = 0, max_y = 0;
    if (patchlib_is_valid(f_main_max_tiles_x) && patchlib_is_valid(f_main_max_tiles_y)) {
        patchlib_field_get_value(f_main_max_tiles_x, PATCH_NULL, &max_x);
        patchlib_field_get_value(f_main_max_tiles_y, PATCH_NULL, &max_y);
    }
    if (max_x > 0 && max_y > 0 && (int64_t)tile_offset >= (int64_t)max_x * (int64_t)max_y) {
        return false;
    }
    const uint32_t *lookup = *g_pe_tile_lookup_slot;
    const uint16_t *types = *g_pe_tile_type_slot;
    if (!lookup || !types) {
        return false;
    }
    const uint32_t def = lookup[tile_offset];
    if (def >= 200000u) {
        return false;
    }
    if (out_type) {
        *out_type = types[def];
    }
    if (out_active && g_pe_tile_sheader_slot && *g_pe_tile_sheader_slot) {
        *out_active = (((*g_pe_tile_sheader_slot)[def]) & 0x20) != 0;
    }
    if (out_frame_x && g_pe_tile_framex_slot && *g_pe_tile_framex_slot) {
        *out_frame_x = (*g_pe_tile_framex_slot)[def];
    }
    if (out_frame_y && g_pe_tile_framey_slot && *g_pe_tile_framey_slot) {
        *out_frame_y = (*g_pe_tile_framey_slot)[def];
    }
    return true;
#else
    patch_handle_t tile = *(patch_handle_t *)arg_ptr;
    if (!patchlib_is_valid(tile) || !patchlib_is_valid(f_pc_tile_type)) {
        return false;
    }
    unsigned short type = 0;
    patchlib_field_get_value(f_pc_tile_type, tile, &type);
    if (out_type) {
        *out_type = type;
    }
    if (out_active && patchlib_is_valid(f_pc_tile_sheader)) {
        unsigned short shdr = 0;
        patchlib_field_get_value(f_pc_tile_sheader, tile, &shdr);
        *out_active = (shdr & 0x20) != 0;
    }
    if (out_frame_x && patchlib_is_valid(f_pc_tile_framex)) {
        patchlib_field_get_value(f_pc_tile_framex, tile, out_frame_x);
    }
    if (out_frame_y && patchlib_is_valid(f_pc_tile_framey)) {
        patchlib_field_get_value(f_pc_tile_framey, tile, out_frame_y);
    }
    return true;
#endif
}

static int get_list_count_typed(patch_handle_t list, patch_handle_t list_type) {
    if (!patchlib_is_valid(list) || !patchlib_is_valid(list_type)) {
        return 0;
    }
    patch_handle_t f_size = patchlib_type_get_field(list_type, "_size");
    int count = 0;
    if (patchlib_is_valid(f_size)) {
        patchlib_field_get_value(f_size, list, &count);
        patchlib_free(f_size);
        return count;
    }
    patch_handle_t m_get_count =
            patchlib_type_get_method_by_param_count(list_type, "get_Count", 0);
    if (patchlib_is_valid(m_get_count)) {
        patchlib_method_invoke_args(m_get_count, list, &count, NULL);
        patchlib_free(m_get_count);
    }
    return count;
}

static bool list_add_typed(patch_handle_t list, patch_handle_t list_type,
                           void *value) {
    if (!patchlib_is_valid(list) || !patchlib_is_valid(list_type) || !value) {
        return false;
    }
    patch_handle_t m_add =
            patchlib_type_get_method_by_param_count(list_type, "Add", 1);
    if (!patchlib_is_valid(m_add)) {
        return false;
    }
    void *args[1] = {value};
    const bool ok = patchlib_method_invoke_args(m_add, list, NULL, args);
    patchlib_free(m_add);
    return ok;
}

/* TileObjectData._data 是 List<TileObjectData>，GetTileData(type) 会先检查
 * type < _data.Count，超出就抛 ArgumentOutOfRangeException（PC/PE 都有这层判断），
 * 所以两端都要给自定义 ID 补占位项。 */
static void pad_tile_object_data(int total_tiles) {
    patch_handle_t type = patchlib_type_get_type(
            "Terraria.ObjectData", "TileObjectData");
    if (!patchlib_is_valid(type)) {
        TEKLOG_WARN("tile object data: TileObjectData type not found");
        return;
    }

#if defined(__ANDROID__)
    if (il2cpp_runtime_class_init) {
        il2cpp_runtime_class_init((void *)type);
    }
#endif

    patch_handle_t field = patchlib_type_get_field(type, "_data");
    patch_handle_t list = PATCH_NULL;
    patch_handle_t list_type = PATCH_NULL;
    if (patchlib_is_valid(field)) {
        patchlib_field_get_value(field, PATCH_NULL, &list);
        list_type = patchlib_field_get_type_handle(field);
    }
    patchlib_free(type);

    if (!patchlib_is_valid(list) || !patchlib_is_valid(list_type)) {
        if (patchlib_is_valid(list_type)) {
            patchlib_free(list_type);
        }
        if (patchlib_is_valid(field)) {
            patchlib_free(field);
        }
        return;
    }

    const int current_count = get_list_count_typed(list, list_type);
    if (current_count < total_tiles) {
        patch_handle_t empty_entry = PATCH_NULL;
        int added = 0;
        for (int i = current_count; i < total_tiles; ++i) {
            if (!list_add_typed(list, list_type, &empty_entry)) {
                TEKLOG_ERROR("tile object data: failed to add placeholder %d", i);
                break;
            }
            added++;
        }
        if (added > 0) {
            TEKLOG_INFO("tile object data: padded %d placeholder(s) (%d -> %d)",
                        added, current_count, current_count + added);
        }
    }

    patchlib_free(list_type);
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

static void modal_fill_bool_array(patch_handle_t old_array,
                                  patch_handle_t new_array,
                                  size_t old_size, size_t new_size) {
    if (!patchlib_is_valid(old_array) || !patchlib_is_valid(new_array) ||
        old_size == 0 || new_size <= old_size) {
        return;
    }
    const size_t sample = old_size < 256 ? old_size : 256;
    size_t trues = 0;
    for (size_t i = 0; i < sample; ++i) {
        unsigned char b = 0;
        if (patchlib_array_at(old_array, i, &b) && (b & 1) != 0) {
            trues++;
        }
    }
    if (trues > sample / 2) {
        for (size_t i = old_size; i < new_size; ++i) {
            set_bool_array_element(new_array, i, true);
        }
    }
}

static void modal_fill_primitive_array(patch_handle_t old_array,
                                       patch_handle_t new_array,
                                       size_t old_size, size_t new_size) {
    if (!patchlib_is_valid(old_array) || !patchlib_is_valid(new_array) ||
        old_size == 0 || new_size <= old_size) {
        return;
    }
    const size_t elem_size = patchlib_array_element_size(old_array);
    if (elem_size == 0 || elem_size > 16) {
        return;
    }

    enum { MAX_MODAL_SAMPLES = 32 };
    uint8_t unique_vals[MAX_MODAL_SAMPLES][16];
    size_t counts[MAX_MODAL_SAMPLES];
    size_t n_unique = 0;
    uint8_t buf[16];
    const size_t sample = old_size < 256 ? old_size : 256;

    for (size_t i = 0; i < sample; ++i) {
        memset(buf, 0, sizeof(buf));
        if (!patchlib_array_at(old_array, i, buf)) {
            return;
        }
        size_t found = n_unique;
        for (size_t j = 0; j < n_unique; ++j) {
            if (memcmp(unique_vals[j], buf, elem_size) == 0) {
                found = j;
                break;
            }
        }
        if (found < n_unique) {
            counts[found]++;
        } else if (n_unique < MAX_MODAL_SAMPLES) {
            memcpy(unique_vals[n_unique], buf, elem_size);
            counts[n_unique] = 1;
            n_unique++;
        }
    }

    if (n_unique == 0) {
        return;
    }
    size_t best = 0;
    for (size_t j = 1; j < n_unique; ++j) {
        if (counts[j] > counts[best]) {
            best = j;
        }
    }
    uint8_t zero[16] = {0};
    if (memcmp(unique_vals[best], zero, elem_size) == 0) {
        return;
    }
    for (size_t i = old_size; i < new_size; ++i) {
        if (!patchlib_array_set(new_array, i, unique_vals[best])) {
            break;
        }
    }
}

static bool resize_field_array_impl(patch_handle_t field, const char *name,
                                    const char *type_name, int new_size,
                                    bool modal_fill, short *short_fill) {
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

    if (short_fill) {
        for (size_t i = old_size; i < (size_t)new_size; ++i) {
            patchlib_array_set(new_array, i, short_fill);
        }
    } else if (modal_fill) {
        if (type_name && strstr(type_name, "Boolean[]")) {
            modal_fill_bool_array(old_array, new_array, old_size,
                                  (size_t)new_size);
        } else {
            modal_fill_primitive_array(old_array, new_array, old_size,
                                       (size_t)new_size);
        }
    }

    patchlib_field_set_value(field, PATCH_NULL, &new_array);
    TEKLOG_INFO("tile resize: %s %zu -> %d",
                name ? name : "?", old_size, new_size);
    patchlib_free(new_array);
    patchlib_free(old_array);
    return true;
}

static bool resize_field_array(patch_handle_t field, const char *name,
                               int new_size) {
    return resize_field_array_impl(field, name, NULL, new_size, false, NULL);
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
        if (patchlib_is_valid(field)) {
            patchlib_free(field);
        }
        return;
    }

    const size_t length = patchlib_array_length(array);
    if ((size_t)runtime_id < length) {
        set_bool_array_element(array, (size_t)runtime_id, value);
    } else {
        TEKLOG_WARN("tile flag: %s length %zu <= id %d",
                    name, length, runtime_id);
    }

    patchlib_free(array);
    if (patchlib_is_valid(field)) {
        patchlib_free(field);
    }
}

static void set_main_tile_int(patch_handle_t main_type, const char *name,
                              int runtime_id, int value) {
    if (runtime_id < 0) {
        return;
    }
    patch_handle_t field = patchlib_type_get_field(main_type, name);
    patch_handle_t array = PATCH_NULL;
    if (patchlib_is_valid(field)) {
        patchlib_field_get_value(field, PATCH_NULL, &array);
    }
    if (patchlib_is_valid(array)) {
        if ((size_t)runtime_id < patchlib_array_length(array)) {
            patchlib_array_set(array, (size_t)runtime_id, &value);
        }
        patchlib_free(array);
    }
    if (patchlib_is_valid(field)) {
        patchlib_free(field);
    }
}

static void set_tile_set_bool(patch_handle_t sets_type, const char *name,
                              int runtime_id, bool value) {
    if (!patchlib_is_valid(sets_type) || runtime_id < 0) {
        return;
    }
    patch_handle_t field = patchlib_type_get_field(sets_type, name);
    patch_handle_t array = PATCH_NULL;
    if (patchlib_is_valid(field)) {
        patchlib_field_get_value(field, PATCH_NULL, &array);
    }
    if (patchlib_is_valid(array)) {
        if ((size_t)runtime_id < patchlib_array_length(array)) {
            set_bool_array_element(array, (size_t)runtime_id, value);
        }
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
            TEKLOG_WARN("tile merge: outer resize %zu -> %d failed",
                        old_size, new_size);
            patchlib_free(old_outer);
            patchlib_free(merge_field);
            return;
        }
        TEKLOG_INFO("tile merge: outer %zu -> %d", old_size, new_size);
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

#if defined(__ANDROID__)
    if (il2cpp_runtime_class_init) {
        il2cpp_runtime_class_init((void *)sets_type);
    }
#endif

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

        patch_handle_t field_type = patchlib_field_get_type_handle(field);
        const char *type_name = patchlib_is_valid(field_type)
                ? patchlib_type_get_name(field_type)
                : NULL;
        if (type_name && strstr(type_name, "[]") && !strstr(type_name, "[][]")) {
            patch_handle_t current = PATCH_NULL;
            patchlib_field_get_value(field, PATCH_NULL, &current);
            if (!patchlib_is_valid(current) && strstr(type_name, "Boolean[]")) {
                patch_handle_t bool_type = patchlib_get_basic_type(PATCH_BOOL);
                if (patchlib_is_valid(bool_type)) {
                    patch_handle_t created = patchlib_array_create(
                            (size_t)new_size, bool_type);
                    if (patchlib_is_valid(created)) {
                        patchlib_field_set_value(field, PATCH_NULL, &created);
                        patchlib_free(created);
                    }
                    patchlib_free(bool_type);
                }
            } else if (patchlib_is_valid(current)) {
                const size_t cur_len = patchlib_array_length(current);
                /* 仅扩容长度等于原版 TileID.Count 的表；保留 CountsAsChairTypes 等短列表 */
                if (cur_len == (size_t)g_vanilla_tile_count) {
                    resize_field_array_impl(
                            field, patchlib_field_get_name(field),
                            type_name, new_size, true, NULL);
                }
                patchlib_free(current);
            }
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
            resize_field_array_impl(
                    pressure_plate, "TileID.Sets.PressurePlate",
                    type_name, new_size, true, NULL);
        }
        if (patchlib_is_valid(field_type)) {
            patchlib_free(field_type);
        }
        patchlib_free(pressure_plate);
    }

    tefstd_vector_destroy(&fields);
    resize_set_factory(sets_type, new_size);

    /* TileID.Sets 嵌套静态类（Conversion / TileCutIgnore 等）同样按 TileID 定长 */
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
    patch_handle_t tile_id_type = patchlib_type_get_type("Terraria.ID", "TileID");
    patch_handle_t sets_type = patchlib_is_valid(tile_id_type)
            ? patchlib_type_get_inner_type(tile_id_type, "Sets")
            : PATCH_NULL;
    if (patchlib_is_valid(tile_id_type)) {
        patchlib_free(tile_id_type);
    }

    for (size_t i = 0;
         i < tefstd_vector_size(&g_terraria_tile_registry);
         ++i) {
        terraria_tile_handle_t *tile =
                *(terraria_tile_handle_t **)tefstd_vector_at(
                        &g_terraria_tile_registry, i);
        if (!tile || tile->runtime_id < 0) {
            continue;
        }
        const int id = tile->runtime_id;
        const bool is_frame_important =
                tile->frame_important ||
                tile->object_style != TERRARIA_TILE_OBJECT_NONE;
        const bool is_lighted =
                tile->lighted ||
                tile->light_r > 0.0f || tile->light_g > 0.0f ||
                tile->light_b > 0.0f || tile->tile_ops.get_light != NULL;

        set_main_tile_flag(main_type, "tileSolid", id, tile->solid);
        set_main_tile_flag(main_type, "tileSolidTop", id, tile->solid_top);
        set_main_tile_flag(main_type, "tileMergeDirt", id, tile->merge_dirt);
        set_main_tile_flag(main_type, "tileBlockLight", id, tile->block_light);
        set_main_tile_flag(main_type, "tileFrameImportant", id,
                           is_frame_important);
        set_main_tile_flag(main_type, "tileLighted", id, is_lighted);
        set_main_tile_flag(main_type, "tileCut", id, tile->cut);
        set_main_tile_flag(main_type, "tileNoFail", id, tile->no_fail);
        set_main_tile_flag(main_type, "tileNoAttach", id, tile->no_attach);
        set_main_tile_flag(main_type, "tileTable", id, tile->table);
        set_main_tile_flag(main_type, "tileLavaDeath", id, tile->lava_death);
        set_main_tile_flag(main_type, "tileObsidianKill", id, tile->lava_death);
        set_main_tile_flag(main_type, "tileWaterDeath", id, tile->water_death);
        set_main_tile_flag(main_type, "tileSpelunker", id, tile->spelunker);
        set_main_tile_flag(main_type, "tileBouncy", id, tile->bouncy);
        set_main_tile_flag(main_type, "tileBlendAll", id, tile->blend_all);
        set_main_tile_flag(main_type, "tileShine2", id, tile->shine > 0);
        set_main_tile_int(main_type, "tileShine", id,
                          tile->shine > 0 ? tile->shine : 0);

        if (patchlib_is_valid(sets_type)) {
            const bool merge_with_all =
                    tile->blend_all ||
                    (tile->solid && !tile->solid_top && !is_frame_important);
            set_tile_set_bool(sets_type, "BlockMergesWithMergeAllBlock",
                              id, merge_with_all);
            if (tile->object_style == TERRARIA_TILE_OBJECT_PLATFORM) {
                set_tile_set_bool(sets_type, "Platforms", id, true);
            } else if (tile->object_style == TERRARIA_TILE_OBJECT_TORCH) {
                set_tile_set_bool(sets_type, "Torches", id, true);
            }
        }
    }

    if (patchlib_is_valid(sets_type)) {
        patchlib_free(sets_type);
    }
}

static void resize_all_type_tile_arrays(patch_handle_t target_type,
                                        int new_size) {
    if (!patchlib_is_valid(target_type)) {
        return;
    }
#if defined(__ANDROID__)
    if (il2cpp_runtime_class_init) {
        il2cpp_runtime_class_init((void *)target_type);
    }
#endif

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
        if (!type_name || !strstr(type_name, "[]") ||
            strstr(type_name, "[][]")) {
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
            resize_field_array_impl(
                    field, patchlib_field_get_name(field),
                    type_name, new_size, true, NULL);
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

/* 前向声明 */
static void apply_custom_tile_map_entries(void);

/* MapHelper.Initialize 在 Main.Initialize 体内才跑，之前扩容会被跳过，
 * 于是 tileOptionCounts/tileLookup 停在 754（地图保存/加载按
 * i < TileID.Count 遍历就会越界）。这里在它跑完之后补一次。 */
static patch_hook_id_t h_map_helper_init = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_map_is_background = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_lang_build_map_atlas = PATCH_HOOK_INVALID_ID;

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
    apply_custom_tile_map_entries();
}

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

terraria_tile_handle_t *terraria_tile_manager_find_tile(const char *loader,
                                                        const char *mod,
                                                        const char *name) {
    if (!loader || !mod || !name) {
        return NULL;
    }

    for (size_t i = 0;
         i < tefstd_vector_size(&g_terraria_tile_registry);
         ++i) {
        terraria_tile_handle_t **tile_ptr =
                tefstd_vector_at(&g_terraria_tile_registry, i);
        if (!tile_ptr || !*tile_ptr) {
            continue;
        }

        terraria_tile_handle_t *tile = *tile_ptr;
        if (tile->parent_modloader_id && tile->parent_id &&
            tile->internal_name &&
            strcmp(tile->parent_modloader_id, loader) == 0 &&
            strcmp(tile->parent_id, mod) == 0 &&
            strcmp(tile->internal_name, name) == 0) {
            return tile;
        }
    }
    return NULL;
}

int terraria_tile_id_get_total_count(void) {
    return g_vanilla_tile_count +
           (int)tefstd_vector_size(&g_terraria_tile_registry);
}

int terraria_tile_id_get_capacity(void) {
    return g_tile_capacity;
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
    const int total = g_vanilla_tile_count +
                      (int)tefstd_vector_size(&g_terraria_tile_registry);
    const int target = total > g_tile_capacity ? total : g_tile_capacity;
    if (target <= g_vanilla_tile_count) {
        return;
    }
    tile_resize_impl(target);
}

/* 读档前按世界文件里记录的图格数量把表扩到位。
 *
 * 场景：世界是在「装了自定义图格」的会话里保存的，现在 Mod 缺失。
 * 此时当前会话的 TileID.Count 只有 754，而世界里有 754 号图格，
 * LoadWorldTiles 里 WorldGen.tileCounts[754] / TileID.Sets.*[754]
 * 会直接 IndexOutOfRange（表现为「加载失败」），根本走不到后面的重映射。 */
void terraria_tile_manager_resize_to(int capacity) {
    if (capacity <= g_tile_capacity || g_vanilla_tile_count <= 0) {
        return;
    }
    TEKLOG_INFO("tile resize: capacity %d -> %d (world header)",
                g_tile_capacity, capacity);
    tile_resize_impl(capacity);
}

/* resize 的实际实现：把 total 作为「表长度」处理（可大于当前已注册图格数） */
static void tile_resize_impl(int total) {
    if (total <= 0 || g_vanilla_tile_count <= 0) {
        return;
    }
    if (total > g_tile_capacity) {
        g_tile_capacity = total;
    }

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

    short glow_default = -1;
    for (size_t i = 0;
         i < sizeof(g_main_tile_arrays) /
                     sizeof(g_main_tile_arrays[0]);
         ++i) {
        patch_handle_t field = patchlib_type_get_field(
                main_type, g_main_tile_arrays[i]);
        if (strcmp(g_main_tile_arrays[i], "tileGlowMask") == 0) {
            resize_field_array_impl(
                    field, g_main_tile_arrays[i], NULL, total,
                    false, &glow_default);
        } else {
            resize_field_array(
                    field, g_main_tile_arrays[i], total);
        }
        if (patchlib_is_valid(field)) {
            patchlib_free(field);
        }
    }

    resize_all_type_tile_arrays(main_type, total);
    resize_vanilla_type_tables(total);
    resize_instance_type_tables(total);

#if defined(__ANDROID__)
    /* PE 的图格数据全在 TileData 的裸指针池里；PC 没有这个类型 */
    patch_handle_t tile_data_type =
            patchlib_type_get_type("Terraria", "TileData");
    if (patchlib_is_valid(tile_data_type)) {
        resize_all_type_tile_arrays(tile_data_type, total);
        patchlib_free(tile_data_type);
    }
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

/* 未注册的槽位（典型场景：世界里有某个已卸载 Mod 的图格）不能是 null，
 * 否则渲染时会解引用空 Asset。这里统一借 1 号图格的资源填上。 */
static void fill_unregistered_tile_assets(patch_handle_t textures,
                                          int from, int to) {
    if (!patchlib_is_valid(textures) || to <= from) {
        return;
    }

    patch_handle_t fallback = PATCH_NULL;
    if (!patchlib_array_at(textures, 1, &fallback) ||
        !patchlib_is_valid(fallback)) {
        if (patchlib_is_valid(fallback)) {
            patchlib_free(fallback);
        }
        TEKLOG_WARN("tile texture: no fallback asset (tile 1)");
        return;
    }

    int filled = 0;
    for (int i = from; i < to; ++i) {
        patch_handle_t slot = PATCH_NULL;
        const bool has_slot =
                patchlib_array_at(textures, (size_t)i, &slot) &&
                patchlib_is_valid(slot);
        if (!has_slot) {
            patchlib_array_set(textures, (size_t)i, &fallback);
            ++filled;
        }
        if (patchlib_is_valid(slot)) {
            patchlib_free(slot);
        }
    }

    if (filled > 0) {
        TEKLOG_INFO("tile texture: filled %d unregistered slot(s) "
                    "[%d, %d) with fallback asset",
                    filled, from, to);
    }
    patchlib_free(fallback);
}

void terraria_tile_manager_init_textures(void) {
    const size_t custom_count =
            tefstd_vector_size(&g_terraria_tile_registry);
    const int total = g_tile_capacity > 0
                              ? g_tile_capacity
                              : g_vanilla_tile_count + (int)custom_count;
    if (total <= g_vanilla_tile_count ||
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

    /* 未注册的槽位（如已卸载 Mod 的图格）要有兜底贴图，避免渲染解引用 null */
    fill_unregistered_tile_assets(textures, g_vanilla_tile_count, total);

    patchlib_free(textures);

    /* LoadTextures 之后高亮描边图才有真实 Asset，这里再补一次模板填充 */
    fill_vanilla_reference_slots(total);
}

static void set_tile_property_bool(patch_handle_t type, patch_handle_t obj,
                                   const char *name, bool value) {
    if (!patchlib_is_valid(type) || !patchlib_is_valid(obj) || !name) {
        return;
    }
    char setter_name[64];
    snprintf(setter_name, sizeof(setter_name), "set_%s", name);
    patch_handle_t m =
            patchlib_type_get_method_by_param_count(type, setter_name, 1);
    if (patchlib_is_valid(m)) {
        void *args[1] = {&value};
        patchlib_method_invoke_args(m, obj, NULL, args);
        patchlib_free(m);
    }
}

static void apply_tile_object_data(terraria_tile_handle_t *tile) {
    if (!tile || tile->runtime_id < g_vanilla_tile_count ||
        tile->object_style == TERRARIA_TILE_OBJECT_NONE) {
        return;
    }

    patch_handle_t tod_cls =
            patchlib_type_get_type("Terraria.ObjectData", "TileObjectData");
    if (!patchlib_is_valid(tod_cls)) {
        return;
    }
#if defined(__ANDROID__)
    if (il2cpp_runtime_class_init) {
        il2cpp_runtime_class_init((void *)tod_cls);
    }
#endif

    patch_handle_t f_ro = patchlib_type_get_field(tod_cls, "readOnlyData");
    patch_handle_t f_new = patchlib_type_get_field(tod_cls, "newTile");
    patch_handle_t m_add =
            patchlib_type_get_method_by_param_count(tod_cls, "addTile", 1);

    if (!patchlib_is_valid(f_ro) || !patchlib_is_valid(f_new) ||
        !patchlib_is_valid(m_add)) {
        if (patchlib_is_valid(f_ro)) patchlib_free(f_ro);
        if (patchlib_is_valid(f_new)) patchlib_free(f_new);
        if (patchlib_is_valid(m_add)) patchlib_free(m_add);
        patchlib_free(tod_cls);
        return;
    }

    bool writable = false;
    patchlib_field_set_value(f_ro, PATCH_NULL, &writable);

    patch_handle_t new_tile = PATCH_NULL;
    patchlib_field_get_value(f_new, PATCH_NULL, &new_tile);
    if (patchlib_is_valid(new_tile)) {
        int vanilla_copy_id = -1;
        const char *style_field = NULL;
        switch (tile->object_style) {
            case TERRARIA_TILE_OBJECT_TORCH:       vanilla_copy_id = 4; break;
            case TERRARIA_TILE_OBJECT_PLATFORM:    vanilla_copy_id = 19; break;
            case TERRARIA_TILE_OBJECT_CHAIR:       vanilla_copy_id = 15; break;
            case TERRARIA_TILE_OBJECT_WORKBENCH:   vanilla_copy_id = 18; break;
            case TERRARIA_TILE_OBJECT_TABLE:       vanilla_copy_id = 14; break;
            case TERRARIA_TILE_OBJECT_SAPLING:     vanilla_copy_id = 20; break;
            case TERRARIA_TILE_OBJECT_BANNER:      vanilla_copy_id = 91; break;
            case TERRARIA_TILE_OBJECT_CLOSED_DOOR: vanilla_copy_id = 10; break;
            case TERRARIA_TILE_OBJECT_STYLE_1X1:   style_field = "Style1x1"; break;
            case TERRARIA_TILE_OBJECT_STYLE_1X2:   style_field = "Style1x2"; break;
            case TERRARIA_TILE_OBJECT_STYLE_2X1:   style_field = "Style2x1"; break;
            case TERRARIA_TILE_OBJECT_STYLE_2X2:   style_field = "Style2x2"; break;
            case TERRARIA_TILE_OBJECT_STYLE_3X2:   style_field = "Style3x2"; break;
            case TERRARIA_TILE_OBJECT_STYLE_3X3:   style_field = "Style3x3"; break;
            case TERRARIA_TILE_OBJECT_STYLE_3X3_WALL: style_field = "Style3x3Wall"; break;
            default: break;
        }

        bool copied = false;
        if (vanilla_copy_id >= 0) {
            patch_handle_t u16_t = patchlib_get_basic_type(PATCH_UINT16);
            patch_handle_t m_full = patchlib_type_get_method_by_param_types(
                    tod_cls, "FullCopyFrom", 1, &u16_t);
            if (patchlib_is_valid(m_full)) {
                unsigned short vid = (unsigned short)vanilla_copy_id;
                void *args[1] = {&vid};
                copied = patchlib_method_invoke_args(m_full, new_tile, NULL, args);
                patchlib_free(m_full);
            }
            if (patchlib_is_valid(u16_t)) patchlib_free(u16_t);
        } else if (style_field) {
            patch_handle_t f_tmpl = patchlib_type_get_field(tod_cls, style_field);
            patch_handle_t tmpl = PATCH_NULL;
            if (patchlib_is_valid(f_tmpl)) {
                patchlib_field_get_value(f_tmpl, PATCH_NULL, &tmpl);
                patchlib_free(f_tmpl);
            }
            patch_handle_t m_copy =
                    patchlib_type_get_method_by_param_count(tod_cls, "CopyFrom", 1);
            if (patchlib_is_valid(tmpl) && patchlib_is_valid(m_copy)) {
                void *args[1] = {&tmpl};
                copied = patchlib_method_invoke_args(m_copy, new_tile, NULL, args);
                set_tile_property_bool(tod_cls, new_tile, "StyleHorizontal", true);
            }
            if (patchlib_is_valid(m_copy)) patchlib_free(m_copy);
            if (patchlib_is_valid(tmpl)) patchlib_free(tmpl);
        }

        if (copied) {
            if (tile->lava_immune_object) {
                set_tile_property_bool(tod_cls, new_tile, "LavaDeath", false);
            } else if (tile->lava_death) {
                set_tile_property_bool(tod_cls, new_tile, "LavaDeath", true);
            }
            int rid = tile->runtime_id;
            void *args[1] = {&rid};
            patchlib_method_invoke_args(m_add, PATCH_NULL, NULL, args);
            TEKLOG_INFO("tile object data: registered style=%d for %s (id=%d)",
                        (int)tile->object_style,
                        tile->internal_name ? tile->internal_name : "?",
                        tile->runtime_id);
        }
        patchlib_free(new_tile);
    }

    bool readonly = true;
    patchlib_field_set_value(f_ro, PATCH_NULL, &readonly);

    patchlib_free(m_add);
    patchlib_free(f_new);
    patchlib_free(f_ro);
    patchlib_free(tod_cls);
}

void terraria_tile_manager_init_static(void) {
    /* 最后一轮兜底：Main.Initialize 里的默认值初始化可能把我们的旗标覆盖掉，
     * 这里再写一次（PC 上 tileSolid 决定能不能穿过）。 */
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    if (patchlib_is_valid(main_type)) {
        apply_tile_flags(main_type);
        patchlib_free(main_type);
    }

    const int total = g_tile_capacity > 0
            ? g_tile_capacity
            : g_vanilla_tile_count +
              (int)tefstd_vector_size(&g_terraria_tile_registry);
    pad_tile_object_data(total);

    for (size_t i = 0;
         i < tefstd_vector_size(&g_terraria_tile_registry);
         ++i) {
        terraria_tile_handle_t *tile =
                *(terraria_tile_handle_t **)tefstd_vector_at(
                        &g_terraria_tile_registry, i);
        if (tile) {
            apply_tile_object_data(tile);
            if (tile->tile_ops.init_static) {
                tile->tile_ops.init_static(tile);
            }
        }
    }
}

/* ---------------- 合并 API ---------------- */

void terraria_tile_manager_set_merge(int tile_a, int tile_b, bool merge) {
    const int cap = g_tile_capacity > 0 ? g_tile_capacity
                                        : terraria_tile_id_get_total_count();
    if (tile_a < 0 || tile_b < 0 || tile_a >= cap || tile_b >= cap) {
        return;
    }

    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    if (patchlib_is_valid(main_type)) {
        patch_handle_t merge_field =
                patchlib_type_get_field(main_type, "tileMerge");
        patch_handle_t outer = PATCH_NULL;
        if (patchlib_is_valid(merge_field)) {
            patchlib_field_get_value(merge_field, PATCH_NULL, &outer);
            patchlib_free(merge_field);
        }
        if (patchlib_is_valid(outer)) {
            patch_handle_t row_a = PATCH_NULL;
            patch_handle_t row_b = PATCH_NULL;
            if (patchlib_array_at(outer, (size_t)tile_a, &row_a) &&
                patchlib_is_valid(row_a)) {
                set_bool_array_element(row_a, (size_t)tile_b, merge);
                patchlib_free(row_a);
            }
            if (patchlib_array_at(outer, (size_t)tile_b, &row_b) &&
                patchlib_is_valid(row_b)) {
                set_bool_array_element(row_b, (size_t)tile_a, merge);
                patchlib_free(row_b);
            }
            patchlib_free(outer);
        }
        patchlib_free(main_type);
    }

    if (merge) {
        patch_handle_t tid_type = patchlib_type_get_type("Terraria.ID", "TileID");
        patch_handle_t sets_type = patchlib_is_valid(tid_type)
                ? patchlib_type_get_inner_type(tid_type, "Sets")
                : PATCH_NULL;
        if (patchlib_is_valid(tid_type)) patchlib_free(tid_type);
        if (patchlib_is_valid(sets_type)) {
            set_tile_set_bool(sets_type, "ChecksForMerge", tile_a, true);
            set_tile_set_bool(sets_type, "ChecksForMerge", tile_b, true);
            patchlib_free(sets_type);
        }
    }
}

/* ---------------- 小地图颜色 / 图例 ---------------- */

static void apply_custom_tile_map_entries(void) {
    const size_t custom_count = tefstd_vector_size(&g_terraria_tile_registry);
    if (custom_count == 0 || g_vanilla_tile_count <= 0) {
        return;
    }

    int custom_map_count = 0;
    for (size_t i = 0; i < custom_count; ++i) {
        terraria_tile_handle_t *t =
                *(terraria_tile_handle_t **)tefstd_vector_at(
                        &g_terraria_tile_registry, i);
        if (t && t->runtime_id >= g_vanilla_tile_count &&
            (t->has_map_color || t->has_map_name)) {
            custom_map_count++;
        }
    }
    if (custom_map_count == 0) {
        return;
    }

    patch_handle_t map_helper =
            patchlib_type_get_type("Terraria.Map", "MapHelper");
    patch_handle_t lang_cls = patchlib_type_get_type("Terraria", "Lang");
    if (!patchlib_is_valid(map_helper) || !patchlib_is_valid(lang_cls)) {
        if (patchlib_is_valid(map_helper)) patchlib_free(map_helper);
        if (patchlib_is_valid(lang_cls)) patchlib_free(lang_cls);
        return;
    }

    patch_handle_t f_color = patchlib_type_get_field(map_helper, "colorLookup");
    patch_handle_t f_tlookup = patchlib_type_get_field(map_helper, "tileLookup");
    patch_handle_t f_opts = patchlib_type_get_field(map_helper, "tileOptionCounts");
    patch_handle_t f_hell = patchlib_type_get_field(map_helper, "hellPosition");
    patch_handle_t f_legend = patchlib_type_get_field(lang_cls, "_mapLegendCache");

    patch_handle_t colors = PATCH_NULL;
    patch_handle_t tlookup = PATCH_NULL;
    patch_handle_t opts = PATCH_NULL;
    patch_handle_t legend = PATCH_NULL;
    unsigned short hell_pos = 0;

    if (patchlib_is_valid(f_color))
        patchlib_field_get_value(f_color, PATCH_NULL, &colors);
    if (patchlib_is_valid(f_tlookup))
        patchlib_field_get_value(f_tlookup, PATCH_NULL, &tlookup);
    if (patchlib_is_valid(f_opts))
        patchlib_field_get_value(f_opts, PATCH_NULL, &opts);
    if (patchlib_is_valid(f_hell))
        patchlib_field_get_value(f_hell, PATCH_NULL, &hell_pos);
    if (patchlib_is_valid(f_legend))
        patchlib_field_get_value(f_legend, PATCH_NULL, &legend);

    if (patchlib_is_valid(colors) && patchlib_is_valid(tlookup) &&
        patchlib_is_valid(opts)) {
        const size_t color_len = patchlib_array_length(colors);
        if (g_map_mod_position == 0) {
            g_map_mod_position = hell_pos > 0
                    ? (unsigned short)(hell_pos + 1)
                    : (unsigned short)color_len;
        }
        const int need_len = (int)g_map_mod_position + custom_map_count;
        g_map_mod_end = (unsigned short)need_len;

        if (color_len < (size_t)need_len) {
            patch_handle_t bigger =
                    patchlib_array_resize(colors, (size_t)need_len, PATCH_NULL);
            if (patchlib_is_valid(bigger)) {
                patchlib_field_set_value(f_color, PATCH_NULL, &bigger);
                patchlib_free(colors);
                colors = bigger;
            }
        }

        patch_handle_t empty_lt = PATCH_NULL;
        if (patchlib_is_valid(legend)) {
            const size_t legend_len = patchlib_array_length(legend);
            if (legend_len < (size_t)need_len) {
                patch_handle_t lt_cls = patchlib_type_get_type(
                        "Terraria.Localization", "LocalizedText");
                if (patchlib_is_valid(lt_cls)) {
#if defined(__ANDROID__)
                    if (il2cpp_runtime_class_init) {
                        il2cpp_runtime_class_init((void *)lt_cls);
                    }
#endif
                    patch_handle_t f_empty =
                            patchlib_type_get_field(lt_cls, "Empty");
                    if (patchlib_is_valid(f_empty)) {
                        patchlib_field_get_value(f_empty, PATCH_NULL, &empty_lt);
                        patchlib_free(f_empty);
                    }
                    patchlib_free(lt_cls);
                }

                patch_handle_t bigger_legend = patchlib_array_resize(
                        legend, (size_t)need_len, PATCH_NULL);
                if (patchlib_is_valid(bigger_legend)) {
                    if (patchlib_is_valid(empty_lt)) {
                        for (size_t li = legend_len; li < (size_t)need_len; ++li) {
                            patchlib_array_set(bigger_legend, li, &empty_lt);
                        }
                    }
                    patchlib_field_set_value(
                            f_legend, PATCH_NULL, &bigger_legend);
                    patchlib_free(legend);
                    legend = bigger_legend;
                }
            }
        }

        const size_t tlookup_len = patchlib_array_length(tlookup);
        const size_t opts_len = patchlib_array_length(opts);
        const size_t final_color_len = patchlib_array_length(colors);
        const size_t final_legend_len =
                patchlib_is_valid(legend) ? patchlib_array_length(legend) : 0;

        int slot_idx = 0;
        for (size_t i = 0; i < custom_count; ++i) {
            terraria_tile_handle_t *tile =
                    *(terraria_tile_handle_t **)tefstd_vector_at(
                            &g_terraria_tile_registry, i);
            if (!tile || tile->runtime_id < g_vanilla_tile_count ||
                (!tile->has_map_color && !tile->has_map_name)) {
                continue;
            }
            const unsigned short pos =
                    (unsigned short)(g_map_mod_position + slot_idx);
            slot_idx++;
            if ((size_t)pos >= final_color_len ||
                (size_t)tile->runtime_id >= tlookup_len ||
                (size_t)tile->runtime_id >= opts_len) {
                continue;
            }

            const unsigned char cr = tile->has_map_color ? tile->map_r : 128;
            const unsigned char cg = tile->has_map_color ? tile->map_g : 128;
            const unsigned char cb = tile->has_map_color ? tile->map_b : 128;
#if defined(__ANDROID__)
            unsigned char color_bytes[4] = {255u, cb, cg, cr};
#else
            unsigned char color_bytes[4] = {cr, cg, cb, 255u};
#endif
            patchlib_array_set(colors, (size_t)pos, color_bytes);

            int one_opt = 1;
            patchlib_array_set(opts, (size_t)tile->runtime_id, &one_opt);
            unsigned short u_pos = pos;
            patchlib_array_set(tlookup, (size_t)tile->runtime_id, &u_pos);

            if (tile->has_map_name && patchlib_is_valid(legend) &&
                (size_t)pos < final_legend_len) {
                char *key = terraria_entity_make_key(TEF_ENTITY(tile), "tile_name");
                if (key) {
                    patch_handle_t lt =
                            terraria_language_manager_get_localized_text(key);
                    if (patchlib_is_valid(lt)) {
                        patchlib_array_set(legend, (size_t)pos, &lt);
                        patchlib_free(lt);
                    } else if (patchlib_is_valid(empty_lt)) {
                        patchlib_array_set(legend, (size_t)pos, &empty_lt);
                    }
                    free(key);
                }
            }
        }
        if (patchlib_is_valid(empty_lt)) {
            patchlib_free(empty_lt);
        }
    }

    if (patchlib_is_valid(colors)) patchlib_free(colors);
    if (patchlib_is_valid(tlookup)) patchlib_free(tlookup);
    if (patchlib_is_valid(opts)) patchlib_free(opts);
    if (patchlib_is_valid(legend)) patchlib_free(legend);
    if (patchlib_is_valid(f_color)) patchlib_free(f_color);
    if (patchlib_is_valid(f_tlookup)) patchlib_free(f_tlookup);
    if (patchlib_is_valid(f_opts)) patchlib_free(f_opts);
    if (patchlib_is_valid(f_hell)) patchlib_free(f_hell);
    if (patchlib_is_valid(f_legend)) patchlib_free(f_legend);
    patchlib_free(map_helper);
    patchlib_free(lang_cls);
}

static bool map_is_background_prefix(patch_handle_t this, void **args,
                                     const patch_method_signature_t *sig,
                                     void *result) {
    (void)this;
    (void)sig;
    if (!args || !args[0] || !result || g_map_mod_position == 0) {
        return false;
    }
    const int map_type = *(const int *)args[0];
    if (map_type >= (int)g_map_mod_position && map_type < (int)g_map_mod_end) {
        *(bool *)result = false;
        return true;
    }
    return false;
}

static void lang_build_map_atlas_postfix(patch_handle_t this, void **args,
                                         void *result,
                                         const patch_method_signature_t *sig) {
    (void)this;
    (void)args;
    (void)result;
    (void)sig;
    apply_custom_tile_map_entries();
}

/* ---------------- 挖掘 / 掉落 / 粒子 / 音效 / 制作站 ---------------- */

static patch_hook_id_t h_get_pickaxe_damage = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_kill_tile_drops = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_kill_tile_dust = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_kill_tile_sounds = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_tile_frame_important = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_set_adj_tile = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_apply_tile_light = PATCH_HOOK_INVALID_ID;
static bool g_apply_tile_light_installed = false;

static patch_handle_t f_player_adj_tile = PATCH_NULL;
static patch_handle_t m_player_set_adj_tile = PATCH_NULL;
static bool g_in_adj_tile_recurse = false;

static void ensure_player_adj_tile(patch_handle_t player) {
    const size_t custom = tefstd_vector_size(&g_terraria_tile_registry);
    if (!patchlib_is_valid(player) || custom == 0 || g_vanilla_tile_count <= 0)
        return;
    if (!patchlib_is_valid(f_player_adj_tile))
        return;
    const int total = g_tile_capacity > 0 ? g_tile_capacity
                                          : g_vanilla_tile_count + (int)custom;
    resize_array_on_instance(f_player_adj_tile, player, NULL, total);
}

static bool player_set_adj_tile_prefix(patch_handle_t this, void **args,
                                       const patch_method_signature_t *sig,
                                       void *result) {
    (void)args;
    (void)sig;
    (void)result;
    ensure_player_adj_tile(this);
    return false;
}

static void player_set_adj_tile_postfix(patch_handle_t this, void **args,
                                        void *result,
                                        const patch_method_signature_t *sig) {
    (void)result;
    (void)sig;
    if (g_in_adj_tile_recurse || !patchlib_is_valid(this) ||
        !args || !args[0] || !patchlib_is_valid(m_player_set_adj_tile)) {
        return;
    }
    const int tile_type = *(const int *)args[0];
    terraria_tile_handle_t *h = terraria_tile_manager_get_tile(tile_type);
    if (!h || !h->adj_tile_ids || h->adj_tile_count <= 0) {
        return;
    }

    g_in_adj_tile_recurse = true;
    for (int i = 0; i < h->adj_tile_count; ++i) {
        int extra = h->adj_tile_ids[i];
        if (extra >= 0) {
            void *sub_args[1] = {&extra};
            patchlib_method_invoke_args(m_player_set_adj_tile, this, NULL, sub_args);
        }
    }
    g_in_adj_tile_recurse = false;
}

static bool get_pickaxe_damage_prefix(patch_handle_t this, void **args,
                                      const patch_method_signature_t *sig,
                                      void *result) {
    (void)this;
    (void)sig;
    if (!args || !result) {
        return false;
    }
    unsigned short type = 0;
    if (!read_tile_from_arg(args[4], &type, NULL, NULL, NULL)) {
        return false;
    }
    terraria_tile_handle_t *h = terraria_tile_manager_get_tile((int)type);
    if (!h) {
        return false;
    }
    if (h->no_fail) {
        *(int *)result = 100;
        return true;
    }
    const int pick_power = args[2] ? *(const int *)args[2] : 0;
    if (h->min_pick > 0 && pick_power < h->min_pick) {
        *(int *)result = 0;
        return true;
    }
    if (h->mine_resist > 0.0f && h->mine_resist != 1.0f) {
        int dmg = (int)((float)pick_power / h->mine_resist);
        if (dmg < 1) {
            dmg = 1;
        }
        *(int *)result = dmg;
        return true;
    }
    return false;
}

static bool kill_tile_get_item_drops_prefix(patch_handle_t this, void **args,
                                            const patch_method_signature_t *sig,
                                            void *result) {
    (void)this;
    (void)sig;
    (void)result;
    if (!args) {
        return false;
    }
    unsigned short type = 0;
    if (!read_tile_from_arg(args[2], &type, NULL, NULL, NULL)) {
        return false;
    }
    terraria_tile_handle_t *h = terraria_tile_manager_get_tile((int)type);
    if (!h) {
        return false;
    }

    const int x = args[0] ? *(const int *)args[0] : 0;
    const int y = args[1] ? *(const int *)args[1] : 0;
    int item_id = 0;
    int stack = 1;

    if (h->tile_ops.get_drop) {
        if (!h->tile_ops.get_drop(h, x, y, &item_id, &stack)) {
            item_id = 0;
            stack = 0;
        }
    } else if (h->drop_item_id > 0) {
        item_id = h->drop_item_id;
        stack = h->drop_stack > 0 ? h->drop_stack : 1;
    } else {
        item_id = 0;
        stack = 0;
    }

    if (args[3]) **(int **)args[3] = item_id;
    if (args[4]) **(int **)args[4] = item_id > 0 ? (stack > 0 ? stack : 1) : 0;
    if (args[5]) **(int **)args[5] = 0;
    if (args[6]) **(int **)args[6] = 0;
    if (args[7]) **(bool **)args[7] = true;
    return true;
}

static bool kill_tile_make_tile_dust_prefix(patch_handle_t this, void **args,
                                            const patch_method_signature_t *sig,
                                            void *result) {
    (void)this;
    (void)sig;
    if (!args || !result) {
        return false;
    }
    unsigned short type = 0;
    if (!read_tile_from_arg(args[2], &type, NULL, NULL, NULL)) {
        return false;
    }
    terraria_tile_handle_t *h = terraria_tile_manager_get_tile((int)type);
    if (!h || !h->has_dust_type) {
        return false;
    }
    *(int *)result = h->dust_type;
    return true;
}

static bool kill_tile_play_sounds_prefix(patch_handle_t this, void **args,
                                         const patch_method_signature_t *sig,
                                         void *result) {
    (void)this;
    (void)sig;
    (void)result;
    if (!args) {
        return false;
    }
    unsigned short type = 0;
    if (!read_tile_from_arg(args[3], &type, NULL, NULL, NULL)) {
        return false;
    }
    terraria_tile_handle_t *h = terraria_tile_manager_get_tile((int)type);
    if (!h || !h->has_hit_sound) {
        return false;
    }

    static patch_handle_t se_cls = PATCH_NULL;
    static patch_handle_t m_play_sound = PATCH_NULL;
    if (!patchlib_is_valid(m_play_sound)) {
        se_cls = patchlib_type_get_type("Terraria.Audio", "SoundEngine");
        if (patchlib_is_valid(se_cls)) {
            patch_handle_t int_t = patchlib_get_basic_type(PATCH_INT32);
            patch_handle_t flt_t = patchlib_get_basic_type(PATCH_FLOAT);
            patch_handle_t ptypes[6] = {int_t, int_t, int_t, int_t, flt_t, flt_t};
            m_play_sound = patchlib_type_get_method_by_param_types(
                    se_cls, "PlaySound", 6, ptypes);
            if (patchlib_is_valid(int_t)) patchlib_free(int_t);
            if (patchlib_is_valid(flt_t)) patchlib_free(flt_t);
        }
    }
    if (!patchlib_is_valid(m_play_sound)) {
        return false;
    }

    int i = args[0] ? *(const int *)args[0] : 0;
    int j = args[1] ? *(const int *)args[1] : 0;
    int s_type = h->hit_sound_type;
    int x_px = i * 16;
    int y_px = j * 16;
    int style = h->hit_sound_style;
    float vol = 1.0f;
    float pitch = 0.0f;
    void *sargs[6] = {&s_type, &x_px, &y_px, &style, &vol, &pitch};
    patchlib_method_invoke_args(m_play_sound, PATCH_NULL, NULL, sargs);
    return true;
}

/* ---------------- 多格物体断锚拆毁（WorldGen.TileFrameImportant） ---------------- */

static void check_top_banner_1x3(int x, int j, int expected_type) {
    static patch_handle_t framing_cls = PATCH_NULL;
    static patch_handle_t m_get_tile = PATCH_NULL;
    static patch_handle_t wg_cls = PATCH_NULL;
    static patch_handle_t f_destroy = PATCH_NULL;
    static patch_handle_t m_kill_tile = PATCH_NULL;
    static patch_handle_t main_cls = PATCH_NULL;
    static patch_handle_t f_tile_solid = PATCH_NULL;
    static patch_handle_t f_tile_solid_top = PATCH_NULL;
    static patch_handle_t tile_id_cls = PATCH_NULL;
    static patch_handle_t tile_sets_cls = PATCH_NULL;
    static patch_handle_t f_platforms = PATCH_NULL;

    if (!patchlib_is_valid(m_get_tile)) {
        framing_cls = patchlib_type_get_type("Terraria", "Framing");
        if (patchlib_is_valid(framing_cls)) {
            m_get_tile = patchlib_type_get_method_by_param_count(
                    framing_cls, "GetTileSafely", 2);
        }
        wg_cls = patchlib_type_get_type("Terraria", "WorldGen");
        if (patchlib_is_valid(wg_cls)) {
            f_destroy = patchlib_type_get_field(wg_cls, "destroyObject");
            m_kill_tile = patchlib_type_get_method_by_param_count(
                    wg_cls, "KillTile", 5);
        }
        main_cls = patchlib_type_get_type("Terraria", "Main");
        if (patchlib_is_valid(main_cls)) {
            f_tile_solid = patchlib_type_get_field(main_cls, "tileSolid");
            f_tile_solid_top = patchlib_type_get_field(main_cls, "tileSolidTop");
        }
        tile_id_cls = patchlib_type_get_type("Terraria.ID", "TileID");
        if (patchlib_is_valid(tile_id_cls)) {
            tile_sets_cls = patchlib_type_get_inner_type(tile_id_cls, "Sets");
            if (patchlib_is_valid(tile_sets_cls)) {
                f_platforms = patchlib_type_get_field(tile_sets_cls, "Platforms");
            }
        }
    }
    if (!patchlib_is_valid(m_get_tile) || !patchlib_is_valid(m_kill_tile) ||
        !patchlib_is_valid(f_destroy)) {
        return;
    }

    bool destroying = false;
    patchlib_field_get_value(f_destroy, PATCH_NULL, &destroying);
    if (destroying) {
        return;
    }

    /* 读取锚点瓷砖 (x, j) 的 frameY 以定位 3 格竖条的顶部 y */
    ensure_tile_access_initialized();
    int top_y = j;
    short frame_x = 0;
#if defined(__ANDROID__)
    int tile_off = -1;
    void *gargs[2] = {&x, &j};
    if (patchlib_method_invoke_args(m_get_tile, PATCH_NULL, &tile_off, gargs) &&
        tile_off >= 0) {
        short fy = 0;
        read_tile_from_arg(&tile_off, NULL, NULL, &frame_x, &fy);
        int cell = (fy / 18) % 3;
        if (cell < 0) cell += 3;
        top_y = j - cell;
    }
#else
    patch_handle_t cur_tile = PATCH_NULL;
    void *gargs[2] = {&x, &j};
    if (patchlib_method_invoke_args(m_get_tile, PATCH_NULL, &cur_tile, gargs) &&
        patchlib_is_valid(cur_tile)) {
        short fy = 0;
        read_tile_from_arg(&cur_tile, NULL, NULL, &frame_x, &fy);
        int cell = (fy / 18) % 3;
        if (cell < 0) cell += 3;
        top_y = j - cell;
        patchlib_free(cur_tile);
    }
#endif

    bool invalid = false;
    for (int k = 0; k < 3; ++k) {
        int ty = top_y + k;
        void *kargs[2] = {&x, &ty};
        unsigned short ttype = 0;
        bool tactive = false;
        short tfx = 0;
#if defined(__ANDROID__)
        int toff = -1;
        if (!patchlib_method_invoke_args(m_get_tile, PATCH_NULL, &toff, kargs) ||
            !read_tile_from_arg(&toff, &ttype, &tactive, &tfx, NULL)) {
            invalid = true;
            break;
        }
#else
        patch_handle_t kt = PATCH_NULL;
        if (!patchlib_method_invoke_args(m_get_tile, PATCH_NULL, &kt, kargs) ||
            !patchlib_is_valid(kt) ||
            !read_tile_from_arg(&kt, &ttype, &tactive, &tfx, NULL)) {
            invalid = true;
        }
        if (patchlib_is_valid(kt)) patchlib_free(kt);
        if (invalid) break;
#endif
        if (!tactive || (int)ttype != expected_type || tfx != frame_x) {
            invalid = true;
            break;
        }
    }

    if (!invalid) {
        int ay = top_y - 1;
        void *aargs[2] = {&x, &ay};
        unsigned short atype = 0;
        bool aactive = false;
#if defined(__ANDROID__)
        int aoff = -1;
        if (!patchlib_method_invoke_args(m_get_tile, PATCH_NULL, &aoff, aargs) ||
            !read_tile_from_arg(&aoff, &atype, &aactive, NULL, NULL) ||
            !aactive) {
            invalid = true;
        }
#else
        patch_handle_t at = PATCH_NULL;
        if (!patchlib_method_invoke_args(m_get_tile, PATCH_NULL, &at, aargs) ||
            !patchlib_is_valid(at) ||
            !read_tile_from_arg(&at, &atype, &aactive, NULL, NULL) ||
            !aactive) {
            invalid = true;
        }
        if (patchlib_is_valid(at)) patchlib_free(at);
#endif
        if (!invalid) {
            bool solid_ok = false;
            bool plat_ok = false;
            bool st_ok = false;
            patch_handle_t arr = PATCH_NULL;
            if (patchlib_is_valid(f_tile_solid)) {
                patchlib_field_get_value(f_tile_solid, PATCH_NULL, &arr);
                if (patchlib_is_valid(arr) &&
                    (size_t)atype < patchlib_array_length(arr)) {
                    unsigned char v = 0;
                    if (patchlib_array_at(arr, (size_t)atype, &v)) {
                        solid_ok = (v & 1) != 0;
                    }
                    patchlib_free(arr);
                    arr = PATCH_NULL;
                } else if (patchlib_is_valid(arr)) {
                    patchlib_free(arr);
                    arr = PATCH_NULL;
                }
            }
            if (patchlib_is_valid(f_tile_solid_top)) {
                patchlib_field_get_value(f_tile_solid_top, PATCH_NULL, &arr);
                if (patchlib_is_valid(arr) &&
                    (size_t)atype < patchlib_array_length(arr)) {
                    unsigned char v = 0;
                    if (patchlib_array_at(arr, (size_t)atype, &v)) {
                        st_ok = (v & 1) != 0;
                    }
                    patchlib_free(arr);
                    arr = PATCH_NULL;
                } else if (patchlib_is_valid(arr)) {
                    patchlib_free(arr);
                    arr = PATCH_NULL;
                }
            }
            if (patchlib_is_valid(f_platforms)) {
                patchlib_field_get_value(f_platforms, PATCH_NULL, &arr);
                if (patchlib_is_valid(arr) &&
                    (size_t)atype < patchlib_array_length(arr)) {
                    unsigned char v = 0;
                    if (patchlib_array_at(arr, (size_t)atype, &v)) {
                        plat_ok = (v & 1) != 0;
                    }
                    patchlib_free(arr);
                } else if (patchlib_is_valid(arr)) {
                    patchlib_free(arr);
                }
            }
            if (!(plat_ok || (solid_ok && !st_ok))) {
                invalid = true;
            }
        }
    }

    if (invalid) {
        bool d = true;
        patchlib_field_set_value(f_destroy, PATCH_NULL, &d);
        for (int k = 0; k < 3; ++k) {
            int ky = top_y + k;
            bool fail = false, eff = false, no_item = (k != 0);
            void *kargs[5] = {&x, &ky, &fail, &eff, &no_item};
            patchlib_method_invoke_args(m_kill_tile, PATCH_NULL, NULL, kargs);
        }
        d = false;
        patchlib_field_set_value(f_destroy, PATCH_NULL, &d);
    }
}

static void tile_frame_important_postfix(patch_handle_t this, void **args,
                                         void *result,
                                         const patch_method_signature_t *sig) {
    (void)this;
    (void)result;
    (void)sig;
    if (!args || !args[0] || !args[1] || !args[2]) {
        return;
    }
    const int i = *(const int *)args[0];
    const int j = *(const int *)args[1];
    const int type = *(const int *)args[2];
    if (type < g_vanilla_tile_count) {
        return;
    }
    terraria_tile_handle_t *h = terraria_tile_manager_get_tile(type);
    if (!h) {
        return;
    }

    static patch_handle_t wg_cls = PATCH_NULL;
    static patch_handle_t m_chk_1x1 = PATCH_NULL;
    static patch_handle_t m_chk_1x2 = PATCH_NULL;
    static patch_handle_t m_chk_2x1 = PATCH_NULL;
    static patch_handle_t m_chk_2x2 = PATCH_NULL;
    static patch_handle_t m_chk_3x2 = PATCH_NULL;
    static patch_handle_t m_chk_3x3 = PATCH_NULL;
    static patch_handle_t m_chk_3x3w = PATCH_NULL;
    static patch_handle_t m_chk_door = PATCH_NULL;
    if (!patchlib_is_valid(wg_cls)) {
        wg_cls = patchlib_type_get_type("Terraria", "WorldGen");
        if (patchlib_is_valid(wg_cls)) {
            m_chk_1x1 = patchlib_type_get_method_by_param_count(wg_cls, "Check1x1", 3);
            m_chk_1x2 = patchlib_type_get_method_by_param_count(wg_cls, "Check1x2", 3);
            m_chk_2x1 = patchlib_type_get_method_by_param_count(wg_cls, "Check2x1", 3);
            m_chk_2x2 = patchlib_type_get_method_by_param_count(wg_cls, "Check2x2", 3);
            m_chk_3x2 = patchlib_type_get_method_by_param_count(wg_cls, "Check3x2", 3);
            m_chk_3x3 = patchlib_type_get_method_by_param_count(wg_cls, "Check3x3", 3);
            m_chk_3x3w = patchlib_type_get_method_by_param_count(wg_cls, "Check3x3Wall", 2);
#if defined(__ANDROID__)
            m_chk_door = patchlib_type_get_method_by_param_count(wg_cls, "CheckDoorClosed", 5);
#else
            m_chk_door = patchlib_type_get_method_by_param_count(wg_cls, "CheckDoorClosed", 4);
#endif
        }
    }

    int ix = i, iy = j, itype = type;
    unsigned short utype = (unsigned short)type;
    switch (h->object_style) {
        case TERRARIA_TILE_OBJECT_CHAIR:
        case TERRARIA_TILE_OBJECT_SAPLING:
        case TERRARIA_TILE_OBJECT_STYLE_1X2:
            if (patchlib_is_valid(m_chk_1x2)) {
                void *cargs[3] = {&ix, &iy, &utype};
                patchlib_method_invoke_args(m_chk_1x2, PATCH_NULL, NULL, cargs);
            }
            break;
        case TERRARIA_TILE_OBJECT_WORKBENCH:
        case TERRARIA_TILE_OBJECT_STYLE_2X1:
            if (patchlib_is_valid(m_chk_2x1)) {
                void *cargs[3] = {&ix, &iy, &utype};
                patchlib_method_invoke_args(m_chk_2x1, PATCH_NULL, NULL, cargs);
            }
            break;
        case TERRARIA_TILE_OBJECT_TABLE:
        case TERRARIA_TILE_OBJECT_STYLE_3X2:
            if (patchlib_is_valid(m_chk_3x2)) {
                void *cargs[3] = {&ix, &iy, &itype};
                patchlib_method_invoke_args(m_chk_3x2, PATCH_NULL, NULL, cargs);
            }
            break;
        case TERRARIA_TILE_OBJECT_STYLE_1X1:
            if (patchlib_is_valid(m_chk_1x1)) {
                void *cargs[3] = {&ix, &iy, &itype};
                patchlib_method_invoke_args(m_chk_1x1, PATCH_NULL, NULL, cargs);
            }
            break;
        case TERRARIA_TILE_OBJECT_STYLE_2X2:
            if (patchlib_is_valid(m_chk_2x2)) {
                void *cargs[3] = {&ix, &iy, &itype};
                patchlib_method_invoke_args(m_chk_2x2, PATCH_NULL, NULL, cargs);
            }
            break;
        case TERRARIA_TILE_OBJECT_STYLE_3X3:
            if (patchlib_is_valid(m_chk_3x3)) {
                void *cargs[3] = {&ix, &iy, &itype};
                patchlib_method_invoke_args(m_chk_3x3, PATCH_NULL, NULL, cargs);
            }
            break;
        case TERRARIA_TILE_OBJECT_STYLE_3X3_WALL:
            if (patchlib_is_valid(m_chk_3x3w)) {
                void *cargs[2] = {&ix, &iy};
                patchlib_method_invoke_args(m_chk_3x3w, PATCH_NULL, NULL, cargs);
            }
            break;
        case TERRARIA_TILE_OBJECT_BANNER:
            check_top_banner_1x3(ix, iy, itype);
            break;
        case TERRARIA_TILE_OBJECT_CLOSED_DOOR:
            if (patchlib_is_valid(m_chk_door)) {
#if defined(__ANDROID__)
                short fx = 0, fy = 0;
                if (read_tile_from_arg(args[3], NULL, NULL, &fx, &fy)) {
                    int ifx = (int)fx, ify = (int)fy;
                    void *cargs[5] = {&ix, &iy, &ifx, &ify, &itype};
                    patchlib_method_invoke_args(m_chk_door, PATCH_NULL, NULL, cargs);
                }
#else
                if (args[3]) {
                    patch_handle_t tc = *(patch_handle_t *)args[3];
                    void *cargs[4] = {&ix, &iy, &tc, &itype};
                    patchlib_method_invoke_args(m_chk_door, PATCH_NULL, NULL, cargs);
                }
#endif
            }
            break;
        default:
            break;
    }
}

/* ---------------- 自发光（TileLightScanner.ApplyTileLight） ---------------- */

#if defined(__ANDROID__)
typedef struct {
    int32_t tile_offset;
} pe_tile_arg_t;

typedef void (*apply_tile_light_fn)(void *self, pe_tile_arg_t tile,
                                    int32_t x, int32_t y,
                                    void *local_random, float *light_color,
                                    const void *method);
static apply_tile_light_fn g_orig_apply_tile_light = NULL;

static void apply_tile_light_detour(void *self, pe_tile_arg_t tile,
                                    int32_t x, int32_t y,
                                    void *local_random, float *light_color,
                                    const void *method) {
    if (!g_pe_tile_lookup_slot || !g_pe_tile_type_slot ||
        tile.tile_offset < 0 || !light_color) {
        if (g_orig_apply_tile_light) {
            g_orig_apply_tile_light(self, tile, x, y, local_random,
                                    light_color, method);
        }
        return;
    }
    const int max_x = g_pe_max_tiles_x_slot ? *g_pe_max_tiles_x_slot : 0;
    const int max_y = g_pe_max_tiles_y_slot ? *g_pe_max_tiles_y_slot : 0;
    if (max_x > 0 && max_y > 0 &&
        (int64_t)tile.tile_offset >= (int64_t)max_x * (int64_t)max_y) {
        if (g_orig_apply_tile_light) {
            g_orig_apply_tile_light(self, tile, x, y, local_random,
                                    light_color, method);
        }
        return;
    }
    const uint32_t *lookup = *g_pe_tile_lookup_slot;
    const uint16_t *types = *g_pe_tile_type_slot;
    if (!lookup || !types) {
        if (g_orig_apply_tile_light) {
            g_orig_apply_tile_light(self, tile, x, y, local_random,
                                    light_color, method);
        }
        return;
    }
    const uint32_t def = lookup[tile.tile_offset];
    if (def >= 200000u) {
        if (g_orig_apply_tile_light) {
            g_orig_apply_tile_light(self, tile, x, y, local_random,
                                    light_color, method);
        }
        return;
    }
    const uint16_t type = types[def];
    if ((int)type < g_vanilla_tile_count) {
        if (g_orig_apply_tile_light) {
            g_orig_apply_tile_light(self, tile, x, y, local_random,
                                    light_color, method);
        }
        return;
    }

    terraria_tile_handle_t *h = terraria_tile_manager_get_tile((int)type);
    if (!h) {
        if (g_orig_apply_tile_light) {
            g_orig_apply_tile_light(self, tile, x, y, local_random,
                                    light_color, method);
        }
        return;
    }

    float r = h->light_r;
    float g = h->light_g;
    float b = h->light_b;
    bool active_light = h->lighted || r > 0.0f || g > 0.0f || b > 0.0f;
    if (h->tile_ops.get_light) {
        active_light = h->tile_ops.get_light(h, (int)x, (int)y, &r, &g, &b);
    }
    if (!active_light) {
        return;
    }
    if (r > light_color[0]) light_color[0] = r;
    if (g > light_color[1]) light_color[1] = g;
    if (b > light_color[2]) light_color[2] = b;
}
#else
static bool apply_tile_light_prefix(patch_handle_t this, void **args,
                                    const patch_method_signature_t *sig,
                                    void *result) {
    (void)this;
    (void)sig;
    (void)result;
    if (!args || !args[0] || !args[4]) {
        return false;
    }
    unsigned short type = 0;
    if (!read_tile_from_arg(args[0], &type, NULL, NULL, NULL) ||
        (int)type < g_vanilla_tile_count) {
        return false;
    }
    terraria_tile_handle_t *h = terraria_tile_manager_get_tile((int)type);
    if (!h) {
        return false;
    }

    const int x = args[1] ? *(const int *)args[1] : 0;
    const int y = args[2] ? *(const int *)args[2] : 0;
    float r = h->light_r;
    float g = h->light_g;
    float b = h->light_b;
    bool active_light = h->lighted || r > 0.0f || g > 0.0f || b > 0.0f;
    if (h->tile_ops.get_light) {
        active_light = h->tile_ops.get_light(h, x, y, &r, &g, &b);
    }
    if (!active_light) {
        return false;
    }

    float *light_color = *(float **)args[4];
    if (light_color) {
        if (r > light_color[0]) light_color[0] = r;
        if (g > light_color[1]) light_color[1] = g;
        if (b > light_color[2]) light_color[2] = b;
    }
    return true;
}
#endif

static void ensure_apply_tile_light_hook(void) {
    if (g_apply_tile_light_installed) {
        return;
    }
    bool any_lighted = false;
    const size_t count = tefstd_vector_size(&g_terraria_tile_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_tile_handle_t *t =
                *(terraria_tile_handle_t **)tefstd_vector_at(
                        &g_terraria_tile_registry, i);
        if (t && (t->lighted || t->light_r > 0.0f || t->light_g > 0.0f ||
                  t->light_b > 0.0f || t->tile_ops.get_light != NULL)) {
            any_lighted = true;
            break;
        }
    }
    if (!any_lighted) {
        return;
    }

    ensure_tile_access_initialized();
    patch_handle_t tls_cls = patchlib_type_get_type(
            "Terraria.Graphics.Light", "TileLightScanner");
    if (!patchlib_is_valid(tls_cls)) {
        return;
    }
    patch_handle_t m_apply =
            patchlib_type_get_method_by_param_count(tls_cls, "ApplyTileLight", 5);
    if (patchlib_is_valid(m_apply)) {
#if defined(__ANDROID__)
        if (patchlib_install_typed_hook(
                    m_apply, (void *)apply_tile_light_detour,
                    (void **)&g_orig_apply_tile_light)) {
            g_apply_tile_light_installed = true;
            TEKLOG_INFO("tile light: installed Android typed detour");
        }
#else
        h_apply_tile_light = patchlib_install_prepost_hook(
                m_apply, apply_tile_light_prefix, NULL);
        if (h_apply_tile_light != PATCH_HOOK_INVALID_ID) {
            g_apply_tile_light_installed = true;
            TEKLOG_INFO("tile light: installed Desktop prefix hook");
        }
#endif
        patchlib_free(m_apply);
    }
    patchlib_free(tls_cls);
}

/* ---------------- PostSetupContent 联动（掉落 / 放置细节 / 合并 / 光照 / 地图） ---------------- */

typedef struct {
    int32_t tile_type;
    int16_t tile_style;
    int16_t padding;
} derived_placement_details_t;

void terraria_tile_manager_post_setup_content(void) {
    const size_t tile_count = tefstd_vector_size(&g_terraria_tile_registry);
    if (tile_count == 0) {
        return;
    }

    /* 1. 显式 drop_item_name 解析 */
    for (size_t i = 0; i < tile_count; ++i) {
        terraria_tile_handle_t *tile =
                *(terraria_tile_handle_t **)tefstd_vector_at(
                        &g_terraria_tile_registry, i);
        if (!tile) continue;
        if (tile->drop_item_name && tile->drop_item_id <= 0) {
            terraria_item_handle_t *it = terraria_item_manager_find_item(
                    tile->parent_modloader_id, tile->parent_id,
                    tile->drop_item_name);
            if (it && it->runtime_id > 0) {
                tile->drop_item_id = it->runtime_id;
            }
        }
    }

    /* 2. 扫描自定义物品：同步 DerivedPlacementDetails + 自动关联 drop_item_id */
    patch_handle_t cs_cls = patchlib_type_get_type("Terraria.ID", "ContentSamples");
    patch_handle_t item_cls = patchlib_type_get_type("Terraria", "Item");
    patch_handle_t item_id_cls = patchlib_type_get_type("Terraria.ID", "ItemID");
    patch_handle_t sets_cls = patchlib_is_valid(item_id_cls)
            ? patchlib_type_get_inner_type(item_id_cls, "Sets")
            : PATCH_NULL;
    if (patchlib_is_valid(item_id_cls)) patchlib_free(item_id_cls);

    patch_handle_t f_items_by_type = patchlib_is_valid(cs_cls)
            ? patchlib_type_get_field(cs_cls, "ItemsByType") : PATCH_NULL;
    patch_handle_t f_derived = patchlib_is_valid(sets_cls)
            ? patchlib_type_get_field(sets_cls, "DerivedPlacementDetails") : PATCH_NULL;
    patch_handle_t f_create_tile = patchlib_is_valid(item_cls)
            ? patchlib_type_get_field(item_cls, "createTile") : PATCH_NULL;
    patch_handle_t f_place_style = patchlib_is_valid(item_cls)
            ? patchlib_type_get_field(item_cls, "placeStyle") : PATCH_NULL;

    patch_handle_t dict_items = PATCH_NULL;
    patch_handle_t t_dict_items = PATCH_NULL;
    patch_handle_t derived_arr = PATCH_NULL;
    if (patchlib_is_valid(f_items_by_type)) {
        patchlib_field_get_value(f_items_by_type, PATCH_NULL, &dict_items);
        t_dict_items = patchlib_field_get_type_handle(f_items_by_type);
    }
    if (patchlib_is_valid(f_derived)) {
        patchlib_field_get_value(f_derived, PATCH_NULL, &derived_arr);
    }

    patch_handle_t m_get_sample = patchlib_is_valid(t_dict_items)
            ? patchlib_type_get_method_by_param_count(t_dict_items, "get_Item", 1)
            : PATCH_NULL;
    patch_handle_t m_try_get = patchlib_is_valid(t_dict_items)
            ? patchlib_type_get_method_by_param_count(t_dict_items, "TryGetValue", 2)
            : PATCH_NULL;
    (void)m_try_get;
    patch_handle_t m_contains = patchlib_is_valid(t_dict_items)
            ? patchlib_type_get_method_by_param_count(t_dict_items, "ContainsKey", 1)
            : PATCH_NULL;

    const size_t item_count = tefstd_vector_size(&g_terraria_item_registry);
    const size_t derived_len = patchlib_is_valid(derived_arr)
            ? patchlib_array_length(derived_arr) : 0;
    const bool derived_ok = patchlib_is_valid(derived_arr) &&
            patchlib_array_element_size(derived_arr) ==
                    sizeof(derived_placement_details_t);

    if (patchlib_is_valid(dict_items) && patchlib_is_valid(m_get_sample) &&
        patchlib_is_valid(f_create_tile)) {
        for (size_t i = 0; i < item_count; ++i) {
            terraria_item_handle_t **pp =
                    tefstd_vector_at(&g_terraria_item_registry, i);
            if (!pp || !*pp || (*pp)->runtime_id <= 0) continue;
            int id = (*pp)->runtime_id;

            bool has_key = true;
            if (patchlib_is_valid(m_contains)) {
                void *cargs[1] = {&id};
                has_key = false;
                patchlib_method_invoke_args(m_contains, dict_items, &has_key, cargs);
            }
            if (!has_key) continue;

            patch_handle_t sample = PATCH_NULL;
            void *gargs[1] = {&id};
            if (!patchlib_method_invoke_args(m_get_sample, dict_items, &sample, gargs) ||
                !patchlib_is_valid(sample)) {
                continue;
            }

            int create_tile = -1;
            int place_style = 0;
            patchlib_field_get_value(f_create_tile, sample, &create_tile);
            if (patchlib_is_valid(f_place_style)) {
                patchlib_field_get_value(f_place_style, sample, &place_style);
            }

            if (create_tile >= 0) {
                if (derived_ok && (size_t)id < derived_len) {
                    derived_placement_details_t dpd = {
                        (int32_t)create_tile, (int16_t)place_style, 0
                    };
                    patchlib_array_set(derived_arr, (size_t)id, &dpd);
                }

                terraria_tile_handle_t *target_tile =
                        terraria_tile_manager_get_tile(create_tile);
                if (target_tile && target_tile->drop_item_id == 0 &&
                    !target_tile->drop_item_name &&
                    !target_tile->tile_ops.get_drop) {
                    target_tile->drop_item_id = id;
                }
            }
            patchlib_free(sample);
        }
    }

    if (patchlib_is_valid(m_contains)) patchlib_free(m_contains);
    if (patchlib_is_valid(m_try_get)) patchlib_free(m_try_get);
    if (patchlib_is_valid(m_get_sample)) patchlib_free(m_get_sample);
    if (patchlib_is_valid(derived_arr)) patchlib_free(derived_arr);
    if (patchlib_is_valid(t_dict_items)) patchlib_free(t_dict_items);
    if (patchlib_is_valid(dict_items)) patchlib_free(dict_items);
    if (patchlib_is_valid(f_place_style)) patchlib_free(f_place_style);
    if (patchlib_is_valid(f_create_tile)) patchlib_free(f_create_tile);
    if (patchlib_is_valid(f_derived)) patchlib_free(f_derived);
    if (patchlib_is_valid(f_items_by_type)) patchlib_free(f_items_by_type);
    if (patchlib_is_valid(sets_cls)) patchlib_free(sets_cls);
    if (patchlib_is_valid(item_cls)) patchlib_free(item_cls);
    if (patchlib_is_valid(cs_cls)) patchlib_free(cs_cls);

    /* 3. 应用图格双向合并（id 与同 Mod 内部名） */
    for (size_t i = 0; i < tile_count; ++i) {
        terraria_tile_handle_t *tile =
                *(terraria_tile_handle_t **)tefstd_vector_at(
                        &g_terraria_tile_registry, i);
        if (!tile || tile->runtime_id < g_vanilla_tile_count) continue;
        for (int j = 0; tile->merge_with_ids && j < tile->merge_with_count; ++j) {
            terraria_tile_manager_set_merge(
                    tile->runtime_id, tile->merge_with_ids[j], true);
        }
        for (int j = 0; tile->merge_with_names && j < tile->merge_with_names_count; ++j) {
            if (!tile->merge_with_names[j]) continue;
            terraria_tile_handle_t *other = terraria_tile_manager_find_tile(
                    tile->parent_modloader_id, tile->parent_id,
                    tile->merge_with_names[j]);
            if (other && other->runtime_id >= 0) {
                terraria_tile_manager_set_merge(
                        tile->runtime_id, other->runtime_id, true);
            }
        }
    }

    /* 4. 懒挂载自发光 / 补写地图查找表 */
    ensure_apply_tile_light_hook();
    apply_custom_tile_map_entries();
}

/* ---------------- 生命周期 ---------------- */

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
    g_tile_capacity = g_vanilla_tile_count;

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

    /* Player.adjTile 懒扩容与制作站传播（桌面不释放缓存句柄所在类型） */
    patch_handle_t player_type = patchlib_type_get_type("Terraria", "Player");
    if (patchlib_is_valid(player_type)) {
        f_player_adj_tile = patchlib_type_get_field(player_type, "adjTile");
        m_player_set_adj_tile = patchlib_type_get_method_by_param_count(
                player_type, "SetAdjTile", 1);
        if (patchlib_is_valid(m_player_set_adj_tile)) {
            h_set_adj_tile = patchlib_install_prepost_hook(
                    m_player_set_adj_tile, player_set_adj_tile_prefix,
                    player_set_adj_tile_postfix);
        }

        patch_handle_t m_pick = patchlib_type_get_method_by_param_count(
                player_type, "GetPickaxeDamage", 5);
        if (patchlib_is_valid(m_pick)) {
            h_get_pickaxe_damage = patchlib_install_prepost_hook(
                    m_pick, get_pickaxe_damage_prefix, NULL);
            patchlib_free(m_pick);
        }
#if defined(__ANDROID__)
        patchlib_free(player_type);
#endif
    }

    /* WorldGen：掉落 / 粒子 / 音效 / 多格物体的 framing 检查 */
    patch_handle_t wg = patchlib_type_get_type("Terraria", "WorldGen");
    if (patchlib_is_valid(wg)) {
        patch_handle_t m_drops = patchlib_type_get_method_by_param_count(
                wg, "KillTile_GetItemDrops", 9);
        if (patchlib_is_valid(m_drops)) {
            h_kill_tile_drops = patchlib_install_prepost_hook(
                    m_drops, kill_tile_get_item_drops_prefix, NULL);
            patchlib_free(m_drops);
        }
        patch_handle_t m_dust = patchlib_type_get_method_by_param_count(
                wg, "KillTile_MakeTileDust", 3);
        if (patchlib_is_valid(m_dust)) {
            h_kill_tile_dust = patchlib_install_prepost_hook(
                    m_dust, kill_tile_make_tile_dust_prefix, NULL);
            patchlib_free(m_dust);
        }
        patch_handle_t m_snd = patchlib_type_get_method_by_param_count(
                wg, "KillTile_PlaySounds", 4);
        if (patchlib_is_valid(m_snd)) {
            h_kill_tile_sounds = patchlib_install_prepost_hook(
                    m_snd, kill_tile_play_sounds_prefix, NULL);
            patchlib_free(m_snd);
        }
        patch_handle_t m_frame = patchlib_type_get_method_by_param_count(
                wg, "TileFrameImportant", 5);
        if (patchlib_is_valid(m_frame)) {
            h_tile_frame_important = patchlib_install_prepost_hook(
                    m_frame, NULL, tile_frame_important_postfix);
            patchlib_free(m_frame);
        }
        patchlib_free(wg);
    }

    /* MapHelper：初始化补扩 + 自定义图例背景过滤 */
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
        patch_handle_t map_is_bg =
                patchlib_type_get_method_by_param_count(
                        map_helper, "IsBackground", 1);
        if (patchlib_is_valid(map_is_bg)) {
            h_map_is_background = patchlib_install_prepost_hook(
                    map_is_bg, map_is_background_prefix, NULL);
            patchlib_free(map_is_bg);
        }
        patchlib_free(map_helper);
    }

    /* Lang.BuildMapAtlas 后重填自定义图格地图图例文本 */
    patch_handle_t lang_cls = patchlib_type_get_type("Terraria", "Lang");
    if (patchlib_is_valid(lang_cls)) {
        patch_handle_t m_atlas = patchlib_type_get_method_by_param_count(
                lang_cls, "BuildMapAtlas", 0);
        if (patchlib_is_valid(m_atlas)) {
            h_lang_build_map_atlas = patchlib_install_prepost_hook(
                    m_atlas, NULL, lang_build_map_atlas_postfix);
            patchlib_free(m_atlas);
        }
        patchlib_free(lang_cls);
    }

    TEKLOG_INFO("tile manager initialized: vanilla count = %d",
                g_vanilla_tile_count);
}

void terraria_tile_manager_destroy(void) {
    patch_hook_id_t *const hooks[] = {
        &h_map_helper_init,
        &h_map_is_background,
        &h_lang_build_map_atlas,
        &h_set_adj_tile,
        &h_get_pickaxe_damage,
        &h_kill_tile_drops,
        &h_kill_tile_dust,
        &h_kill_tile_sounds,
        &h_tile_frame_important,
#if !defined(__ANDROID__)
        &h_apply_tile_light,
#endif
    };
    for (size_t i = 0; i < sizeof(hooks) / sizeof(hooks[0]); ++i) {
        if (*hooks[i] != PATCH_HOOK_INVALID_ID) {
            patchlib_uninstall_hook(*hooks[i]);
            *hooks[i] = PATCH_HOOK_INVALID_ID;
        }
    }
#if defined(__ANDROID__)
    if (g_apply_tile_light_installed) {
        patch_handle_t tls_cls = patchlib_type_get_type(
                "Terraria.Graphics.Light", "TileLightScanner");
        if (patchlib_is_valid(tls_cls)) {
            patch_handle_t m_apply = patchlib_type_get_method_by_param_count(
                    tls_cls, "ApplyTileLight", 5);
            if (patchlib_is_valid(m_apply)) {
                patchlib_uninstall_typed_hook(m_apply);
                patchlib_free(m_apply);
            }
            patchlib_free(tls_cls);
        }
        g_orig_apply_tile_light = NULL;
    }
#endif
    g_apply_tile_light_installed = false;
    tefstd_vector_destroy(&g_terraria_tile_registry);
}
