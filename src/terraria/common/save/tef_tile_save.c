/*******************************************************************************
 * tefkernel - tef_tile_save
 *
 * 与 item 存档一致的做法：只认 (loader, mod, name) 稳定身份，数值 id 只在
 * 同一个世界内作为「存档内索引」使用，读档时重新解析并重映射。
 *******************************************************************************/

#include "internal/terraria/save/tef_tile_save.h"

#include <stdlib.h>
#include <string.h>

#include "internal/log.h"
#include "internal/terraria/content/tile_manager.h"
#include "internal/terraria/save/tef_world_save.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"
#include "patchlib/struct/string.h"

/* PE TileData 的定义池上限（TileBufferSize = 200000） */
#define TEF_TILE_PE_DEFINITION_LIMIT 200000u

/* 缺失 Mod 时降级成的原版图格：0 = 泥土。
 * 选它是因为一定有贴图、一定可挖、不会引入新的越界风险。 */
#define TEF_TILE_FALLBACK_TYPE 0

typedef struct {
    int saved_id;
    int current_id;
} tef_tile_remap_t;

typedef struct {
    tef_tile_remap_t *items;
    size_t count;
    size_t capacity;
    int max_saved_id;
} tef_tile_table_t;

static void table_free(tef_tile_table_t *table) {
    if (!table) {
        return;
    }
    free(table->items);
    memset(table, 0, sizeof *table);
}

static bool table_push(tef_tile_table_t *table, int saved_id, int current_id) {
    if (table->count == table->capacity) {
        const size_t next = table->capacity ? table->capacity * 2 : 8;
        tef_tile_remap_t *items =
                realloc(table->items, next * sizeof(tef_tile_remap_t));
        if (!items) {
            return false;
        }
        table->items = items;
        table->capacity = next;
    }

    table->items[table->count].saved_id = saved_id;
    table->items[table->count].current_id = current_id;
    ++table->count;
    return true;
}

static bool identity_present(const char *loader, const char *mod,
                             const char *name) {
    return loader && loader[0] && mod && mod[0] && name && name[0];
}


/*
 * 解析一条存档条目（与 resolve_item_type 同语义）：
 *   1) 有 (loader, mod, name) → 只按身份查找，Mod 未加载则降级占位；
 *   2) 没有身份串（旧格式）→ 仅当该 id 当前仍指向某个注册项时才保留；
 *   3) 其它情况保持原 id。
 */
static int resolve_tile_type(int saved_id, const char *loader, const char *mod,
                             const char *name) {
    if (identity_present(loader, mod, name)) {
        terraria_tile_handle_t *tile =
                terraria_tile_manager_find_tile(loader, mod, name);
        if (tile && tile->runtime_id >= 0) {
            return tile->runtime_id;
        }

        TEKLOG_WARN("tef_tile_save: tile %s.%s.%s not loaded, saved id %d -> "
                    "fallback %d",
                    loader, mod, name, saved_id, TEF_TILE_FALLBACK_TYPE);
        return TEF_TILE_FALLBACK_TYPE;
    }

    /* 旧格式没有身份串：只有该 id 仍然属于某个已注册图格时才安全保留。 */
    if (saved_id < terraria_tile_id_get_count()) {
        return saved_id;
    }
    if (terraria_tile_manager_get_tile(saved_id)) {
        return saved_id;
    }

    TEKLOG_WARN("tef_tile_save: unowned custom tile id %d, fallback %d",
                saved_id, TEF_TILE_FALLBACK_TYPE);
    return TEF_TILE_FALLBACK_TYPE;
}

static void table_add_entry(tef_tile_table_t *table, int saved_id,
                            const char *loader, const char *mod,
                            const char *name) {
    /* 图格类型是 ushort：超出范围的值一定是坏档，直接忽略 */
    if (saved_id <= 0 || saved_id > 0xFFFF) {
        return;
    }
    if (saved_id > table->max_saved_id) {
        table->max_saved_id = saved_id;
    }

    const int current_id = resolve_tile_type(saved_id, loader, mod, name);
    if (current_id < 0 || current_id == saved_id) {
        return;
    }
    table_push(table, saved_id, current_id);
}

static void table_add_parsed_object(tef_tile_table_t *table,
                                    const tef_save_tlv_t *object) {
    tef_save_rd_t fields;
    if (!tef_save_rd_enter(object, &fields)) {
        return;
    }

    int saved_id = -1;
    const char *loader = NULL;
    const char *mod = NULL;
    const char *name = NULL;

    tef_save_tlv_t field;
    while (tef_save_rd_next(&fields, &field)) {
        switch (field.tag) {
            case TEF_SAVE_TILE_SAVED_ID:
                saved_id = (int)tef_save_rd_u32(&field);
                break;
            case TEF_SAVE_TILE_ORIGINAL_ID:
                if (saved_id < 0) {
                    saved_id = (int)tef_save_rd_u32(&field);
                }
                break;
            case TEF_SAVE_TILE_LOADER:
                loader = tef_save_rd_str(&field);
                break;
            case TEF_SAVE_TILE_MOD:
                mod = tef_save_rd_str(&field);
                break;
            case TEF_SAVE_TILE_NAME:
                name = tef_save_rd_str(&field);
                break;
            default:
                break;
        }
    }

    table_add_entry(table, saved_id, loader, mod, name);
}

/* 读取 <world>.tef 的 TILES 段；没有表/文件损坏时得到空表。 */
static void load_tile_table(tef_tile_table_t *table) {
    memset(table, 0, sizeof *table);

    char *wld = tef_world_save_base_path();
    if (!wld || !wld[0]) {
        free(wld);
        return;
    }

    char path[1400];
    if (!tef_save_make_path(wld, path, sizeof path)) {
        free(wld);
        return;
    }
    free(wld);

    tef_save_file_t file;
    if (!tef_save_file_open(path, &file)) {
        TEKLOG_DEBUG("tef_tile_save: no sidecar table (%s)", path);
        return;
    }
    TEKLOG_DEBUG("tef_tile_save: reading sidecar %s", path);
    if (file.kind != TEF_SAVE_KIND_WORLD) {
        tef_save_file_close(&file);
        return;
    }

    for (int i = 0; i < file.seg_count; ++i) {
        if (file.segs[i].id != TEF_SAVE_SEG_TILES || file.segs[i].len == 0) {
            continue;
        }

        tef_save_rd_t seg = tef_save_file_seg(&file, i);
        tef_save_tlv_t tlv;
        while (tef_save_rd_next(&seg, &tlv)) {
            if (tlv.tag != TEF_SAVE_TILES_ARRAY) {
                continue;
            }

            tef_save_rd_t elems;
            uint32_t count = 0;
            if (!tef_save_rd_array_begin(&tlv, &elems, &count)) {
                continue;
            }

            for (uint32_t e = 0; e < count; ++e) {
                tef_save_tlv_t object;
                if (!tef_save_rd_next(&elems, &object)) {
                    break;
                }
                if (object.type != TEF_SAVE_T_OBJECT) {
                    continue;
                }
                table_add_parsed_object(table, &object);
            }
        }
    }

    tef_save_file_close(&file);
    TEKLOG_DEBUG("tef_tile_save: %zu remap(s), max saved id %d",
                 table->count, table->max_saved_id);
}

/* ================================================================
 * 写入
 * ================================================================ */
void tef_tile_save_write_seg(tef_save_wbuf_t *seg) {
    if (!seg) {
        return;
    }

    const size_t arr = tef_save_wb_begin_array(seg, TEF_SAVE_TILES_ARRAY);

    const size_t count = tefstd_vector_size(&g_terraria_tile_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_tile_handle_t **pp =
                tefstd_vector_at(&g_terraria_tile_registry, i);
        if (!pp || !*pp) {
            continue;
        }

        const terraria_tile_handle_t *tile = *pp;
        if (tile->runtime_id < 0) {
            continue;
        }

        const size_t elem = tef_save_wb_begin_elem(seg);
        tef_save_wb_tlv_str(
                seg, TEF_SAVE_TILE_LOADER,
                tile->parent_modloader_id ? tile->parent_modloader_id : "");
        tef_save_wb_tlv_str(
                seg, TEF_SAVE_TILE_MOD,
                tile->parent_id ? tile->parent_id : "");
        tef_save_wb_tlv_str(
                seg, TEF_SAVE_TILE_NAME,
                tile->internal_name ? tile->internal_name : "");
        tef_save_wb_tlv_u32(
                seg, TEF_SAVE_TILE_SAVED_ID, (uint32_t)tile->runtime_id);
        tef_save_wb_tlv_u32(
                seg, TEF_SAVE_TILE_ORIGINAL_ID, (uint32_t)tile->runtime_id);
        tef_save_wb_end(seg, elem);
    }

    tef_save_wb_end_array(seg, arr);
    TEKLOG_DEBUG("tef_tile_save: wrote %zu custom tile identity entry(s)",
                 count);
}

/* ================================================================
 * 读档重映射
 * ================================================================ */
static int16_t *build_dense_remap(const tef_tile_table_t *table,
                                  size_t *out_length) {
    size_t length = (size_t)terraria_tile_id_get_total_count();
    if (table && (size_t)table->max_saved_id + 1 > length) {
        length = (size_t)table->max_saved_id + 1;
    }
    if (length < 16) {
        length = 16;
    }

    int16_t *map = malloc(length * sizeof(int16_t));
    if (!map) {
        *out_length = 0;
        return NULL;
    }
    memset(map, 0xff, length * sizeof(int16_t));

    for (size_t i = 0; table && i < table->count; ++i) {
        const int saved_id = table->items[i].saved_id;
        const int current_id = table->items[i].current_id;
        if (saved_id < 0 || current_id < 0 || (size_t)saved_id >= length) {
            continue;
        }
        /* int16 紧凑表：超过 32000 的自定义 id 不可能出现，防御性跳过 */
        if (current_id > 32000) {
            continue;
        }
        map[saved_id] = (int16_t)current_id;
    }

    *out_length = length;
    return map;
}

static int remap_lookup(const int16_t *map, const size_t length,
                        const int type) {
    if (!map || type < 0 || (size_t)type >= length) {
        return -1;
    }
    return map[type];
}

/* PC：Main.tile 是 Tile[,]，元素是 Tile 对象，直接改 type 字段。 */
#if !defined(__ANDROID__)
static int remap_world_pc(const int16_t *map, const size_t length) {
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    if (!patchlib_is_valid(main_type)) {
        return 0;
    }

    patch_handle_t tile_field = patchlib_type_get_field(main_type, "tile");
    patch_handle_t tiles = PATCH_NULL;
    if (patchlib_is_valid(tile_field)) {
        patchlib_field_get_value(tile_field, PATCH_NULL, &tiles);
    }

    patch_handle_t tile_type = patchlib_type_get_type("Terraria", "Tile");
    patch_handle_t type_field = PATCH_NULL;
    if (patchlib_is_valid(tile_type)) {
        type_field = patchlib_type_get_field(tile_type, "type");
    }

    int changed = 0;
    if (patchlib_is_valid(tiles) && patchlib_is_valid(type_field)) {
        const size_t total = patchlib_array_length(tiles);
        for (size_t i = 0; i < total; ++i) {
            void *object = NULL;
            if (!patchlib_array_at(tiles, i, &object) || !object) {
                continue;
            }

            unsigned short type = 0;
            patchlib_field_get_value(type_field, object, &type);
            const int target = remap_lookup(map, length, (int)type);
            if (target >= 0) {
                unsigned short next = (unsigned short)target;
                patchlib_field_set_value(type_field, object, &next);
                ++changed;
            }
            patchlib_free(object);
        }

        TEKLOG_DEBUG("tef_tile_save: pc scan total=%zu changed=%d",
                     total, changed);
    }

    if (patchlib_is_valid(type_field)) {
        patchlib_free(type_field);
    }
    if (patchlib_is_valid(tiles)) {
        patchlib_free(tiles);
    }
    if (patchlib_is_valid(tile_field)) {
        patchlib_free(tile_field);
    }
    if (patchlib_is_valid(tile_type)) {
        patchlib_free(tile_type);
    }
    patchlib_free(main_type);
    return changed;
}
#endif /* !__ANDROID__ */

/* PE：图格数据在 TileData 的裸指针池里。
 *   TileLookup[地图索引] → 定义索引
 *   TileType[定义索引]   → 图格类型
 * 定义池被同内容图格共享，所以这里按「地图索引」逐个用 SetType 重挂定义，
 * 让引擎自己维护引用计数与去重链表。 */
#if defined(__ANDROID__)
static int remap_world_pe(const int16_t *map, const size_t length) {
    patch_handle_t tile_data = patchlib_type_get_type("Terraria", "TileData");
    if (!patchlib_is_valid(tile_data)) {
        return 0;
    }

    patch_handle_t lookup_field =
            patchlib_type_get_field(tile_data, "TileLookup");
    patch_handle_t type_field =
            patchlib_type_get_field(tile_data, "TileType");
    void *lookup = NULL;
    void *types = NULL;
    if (patchlib_is_valid(lookup_field)) {
        patchlib_field_get_value(lookup_field, PATCH_NULL, &lookup);
    }
    if (patchlib_is_valid(type_field)) {
        patchlib_field_get_value(type_field, PATCH_NULL, &types);
    }

    int width = 0;
    int height = 0;
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    if (patchlib_is_valid(main_type)) {
        patch_handle_t f_width =
                patchlib_type_get_field(main_type, "maxTilesX");
        patch_handle_t f_height =
                patchlib_type_get_field(main_type, "maxTilesY");
        if (patchlib_is_valid(f_width)) {
            patchlib_field_get_value(f_width, PATCH_NULL, &width);
            patchlib_free(f_width);
        }
        if (patchlib_is_valid(f_height)) {
            patchlib_field_get_value(f_height, PATCH_NULL, &height);
            patchlib_free(f_height);
        }
        patchlib_free(main_type);
    }

    patch_handle_t set_type =
            patchlib_type_get_method_by_param_count(tile_data, "SetType", 2);

    int changed = 0;
    if (lookup && types && width > 0 && height > 0 &&
        patchlib_is_valid(set_type)) {
        const uint32_t *lookup_table = (const uint32_t *)lookup;
        const unsigned short *type_table = (const unsigned short *)types;
        const size_t total = (size_t)width * (size_t)height;

        for (size_t i = 0; i < total; ++i) {
            const uint32_t definition = lookup_table[i];
            if (definition >= TEF_TILE_PE_DEFINITION_LIMIT) {
                continue;
            }

            const int target =
                    remap_lookup(map, length, (int)type_table[definition]);
            if (target < 0) {
                continue;
            }

            int index = (int)i;
            unsigned short next = (unsigned short)target;
            void *args[2] = { &index, &next };
            patchlib_method_invoke_args(set_type, PATCH_NULL, NULL, args);
            ++changed;
        }
    }

    if (patchlib_is_valid(set_type)) {
        patchlib_free(set_type);
    }
    if (patchlib_is_valid(type_field)) {
        patchlib_free(type_field);
    }
    if (patchlib_is_valid(lookup_field)) {
        patchlib_free(lookup_field);
    }
    patchlib_free(tile_data);
    return changed;
}
#endif /* __ANDROID__ */

int tef_tile_save_required_tile_count(void) {
    int required = terraria_tile_id_get_total_count();
    if (required <= 0) {
        return 0;
    }

    tef_tile_table_t table;
    load_tile_table(&table);
    if (table.max_saved_id + 1 > required) {
        required = table.max_saved_id + 1;
    }
    table_free(&table);
    return required;
}

void tef_tile_save_apply_to_world(void) {
    tef_tile_table_t table;
    load_tile_table(&table);

    TEKLOG_DEBUG("tef_tile_save: LoadChests postfix, %zu remap(s), "
                 "max saved id %d",
                 table.count, table.max_saved_id);

    if (table.count == 0) {
        /* 没有 TILES 段（旧存档 / 身份与 id 都没变）—— 保持原样即可 */
        table_free(&table);
        return;
    }

    size_t length = 0;
    int16_t *map = build_dense_remap(&table, &length);
    if (!map) {
        table_free(&table);
        return;
    }

    int changed = 0;
#if defined(__ANDROID__)
    changed = remap_world_pe(map, length);
#else
    changed = remap_world_pc(map, length);
#endif

    TEKLOG_INFO("tef_tile_save: remapped %d world tile(s) (%zu mapping(s))",
                changed, table.count);

    free(map);
    table_free(&table);
}

/* ================================================================
 * 钩子
 * ================================================================ */
#if defined(__ANDROID__)
static int find_signature_arg(const patch_method_signature_t *sig,
                              const char *name) {
    if (!sig || !name) {
        return -1;
    }
    const size_t count = tefstd_vector_size(&sig->arg_names);
    for (size_t i = 0; i < count; ++i) {
        const char *const *slot =
                (const char *const *)tefstd_vector_at(&sig->arg_names, i);
        if (slot && *slot && strcmp(*slot, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* WorldFile.LoadFileFormatHeader(reader, out importance, ...) 之后：
 * importance 按文件头里的图格数量分配长度，而 PE 的头部把 Count 写死成
 * 754，世界里却可能有自定义 id → LoadWorldTiles 的 importance[type] 越界
 * 抛异常（表现为「加载失败」）。这里把 importance 增长到
 * 「存档里出现过的最大 id + 1」。 */
static void load_file_format_header_postfix(patch_handle_t this, void **args,
                                            void *result,
                                            const patch_method_signature_t *sig) {
    (void) this;
    (void) result;

    const int index = find_signature_arg(sig, "importance");
    /* 参数名拿不到时按 ABI 位置兜底：PC 与 PE 的 importance 都是第 2 个参数 */
    const int arg_index = index >= 0 ? index : 1;
    const size_t arg_count =
            sig ? tefstd_vector_size(&sig->arg_types) : 0;
    if (!args || arg_index < 0 || (size_t)arg_index >= arg_count) {
        return;
    }
    if (!args[arg_index]) {
        return;
    }

    /* out bool[] importance 的 ABI 类型是 System_Boolean_array**：
     * args[i] → 保存参数值的槽；槽里存的是「调用方 importance 变量的地址」。
     * 必须再解一层，否则会把一个栈地址当成 bool[] 用（PC 上直接 SIGSEGV）。 */
    patch_handle_t *target = *(patch_handle_t **)args[arg_index];
    if (!target) {
        return;
    }

    patch_handle_t importance = *target;
    if (!patchlib_is_valid(importance)) {
        return;
    }

    const size_t length = patchlib_array_length(importance);
    const size_t element = patchlib_array_element_size(importance);
    /* 防御：正常一定是 bool[]（元素 1 字节、长度 = 图格数量）。
     * element==0 表示宿主不提供元素大小，此时只校验长度。 */
    if (length == 0 || length > (1u << 20) ||
        (element != 0 && element != 1)) {
        TEKLOG_WARN("tef_tile_save: unexpected importance array "
                    "(len=%zu, element=%zu), skip",
                    length, element);
        return;
    }

    const int current_total = terraria_tile_id_get_capacity() > 0
                                      ? terraria_tile_id_get_capacity()
                                      : terraria_tile_id_get_total_count();
    int required = current_total;

#if defined(__ANDROID__)
    /* PE 的 TileID.Count 是编译期常量，存档头部被写死成 754；如果这个世界
     * 是在「更多自定义图格」的会话里保存的，就需要按 <world>.tef 里记下的
     * 最大 id 兜底扩容。
     *
     * 只在 Android 上做：桌面端的头部数量本来就和当前会话一致，而且
     * LoadFileFormatHeader 在扫描世界列表时也会被调用（那时没有可靠的活动
     * 世界），不应该在里面做任何托管调用/文件读取。 */
    const int from_table = tef_tile_save_required_tile_count();
    if (from_table > required) {
        required = from_table;
    }
#endif

    TEKLOG_DEBUG("tef_tile_save: importance hook len=%zu element=%zu "
                 "required=%d",
                 length, element, required);
    if (required <= 0 || length >= (size_t)required) {
        return;
    }

    patch_handle_t bigger =
            patchlib_array_resize(importance, (size_t)required, PATCH_NULL);
    if (!patchlib_is_valid(bigger)) {
        TEKLOG_WARN("tef_tile_save: importance %zu -> %d failed",
                    length, required);
        return;
    }

    *target = bigger;
    TEKLOG_INFO("tef_tile_save: importance %zu -> %d", length, required);
    patchlib_free(bigger);
}
#endif /* __ANDROID__ */

static void log_managed_exception(const char *tag, patch_handle_t exception) {
    if (!patchlib_is_valid(exception)) {
        TEKLOG_ERROR("%s: <exception is null>", tag);
        return;
    }

    patch_handle_t exception_type =
            patchlib_type_get_type("System", "Exception");
    patch_handle_t to_string = PATCH_NULL;
    if (patchlib_is_valid(exception_type)) {
        to_string = patchlib_type_get_method_by_param_count(
                exception_type, "ToString", 0);
        patchlib_free(exception_type);
    }

    if (!patchlib_is_valid(to_string)) {
        TEKLOG_ERROR("%s: <Exception.ToString not found>", tag);
        return;
    }

    patch_handle_t text_handle = PATCH_NULL;
    patchlib_method_invoke_args(to_string, exception, &text_handle, NULL);
    if (patchlib_is_valid(text_handle)) {
        char *text = patchlib_string_cstr(text_handle);
        if (text) {
            TEKLOG_ERROR("%s: %s", tag, text);
            free(text);
        }
        patchlib_free(text_handle);
    }
    patchlib_free(to_string);
}

/* WorldFile.LoadWorld 之后：失败时把托管异常打出来（否则只会看到一句
 * 「加载失败」，无法定位是哪个表/哪个类型越界）。 */
static void world_load_postfix(patch_handle_t this, void **args, void *result,
                               const patch_method_signature_t *sig) {
    (void) this;
    (void) args;
    (void) result;
    (void) sig;

    patch_handle_t worldgen = patchlib_type_get_type("Terraria", "WorldGen");
    if (!patchlib_is_valid(worldgen)) {
        return;
    }

    patch_handle_t failed_field =
            patchlib_type_get_field(worldgen, "loadFailed");
    bool failed = false;
    if (patchlib_is_valid(failed_field)) {
        patchlib_field_get_value(failed_field, PATCH_NULL, &failed);
        patchlib_free(failed_field);
    }
    patchlib_free(worldgen);

    if (!failed) {
        return;
    }

    TEKLOG_ERROR("tef_tile_save: WorldGen.loadFailed = true");

    patch_handle_t world_file =
            patchlib_type_get_type("Terraria.IO", "WorldFile");
    if (!patchlib_is_valid(world_file)) {
        return;
    }

    patch_handle_t exception_field =
            patchlib_type_get_field(world_file, "LastThrownLoadException");
    patch_handle_t exception = PATCH_NULL;
    if (patchlib_is_valid(exception_field)) {
        patchlib_field_get_value(exception_field, PATCH_NULL, &exception);
    }

    log_managed_exception("tef_tile_save: load exception", exception);

    if (patchlib_is_valid(exception)) {
        patchlib_free(exception);
    }
    if (patchlib_is_valid(exception_field)) {
        patchlib_free(exception_field);
    }
    patchlib_free(world_file);
}

/* 读取 <world>.wld 头部里记录的图格数量（= 保存时那一方的 TileID.Count）。
 * 纯文件解析，不碰托管对象：LoadWorld 会在读档最前面被调用，这里必须
 * 绝对安全。
 * 布局：int32 version | FileMetadata(20) | int16 sectionCount | int32 * n | uint16 tileCount */
static int read_world_header_tile_count(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return 0;
    }

    int result = 0;
    int32_t version = 0;
    int16_t sections = 0;
    uint16_t count = 0;

    do {
        if (fread(&version, sizeof version, 1, file) != 1) {
            break;
        }
        /* 只处理 1.4 的世界格式（>=135 才有 FileMetadata） */
        if (version < 135 || version > 1000) {
            break;
        }
        if (fseek(file, 20L, SEEK_CUR) != 0) {
            break;
        }
        if (fread(&sections, sizeof sections, 1, file) != 1) {
            break;
        }
        if (sections <= 0 || sections > 64) {
            break;
        }
        if (fseek(file, (long)sections * 4L, SEEK_CUR) != 0) {
            break;
        }
        if (fread(&count, sizeof count, 1, file) != 1) {
            break;
        }
        result = (int)count;
    } while (0);

    fclose(file);
    return result;
}

/* LoadWorld 之前：按世界文件里的图格数量把表扩到位。
 *
 * Mod 缺失时当前 TileID.Count 只有原版数量，而世界里有已卸载 Mod 的图格 id，
 * LoadWorldTiles 里的 WorldGen.tileCounts[type] / TileID.Sets.*[type] 会
 * 直接 IndexOutOfRange（表现为「加载失败」），根本走不到 LoadChests 的重映射。 */
static bool world_load_prefix(patch_handle_t this, void **args,
                              const patch_method_signature_t *sig,
                              void *result) {
    (void) this;
    (void) args;
    (void) sig;
    (void) result;

    char *wld = tef_world_save_base_path();
    if (!wld || !wld[0]) {
        free(wld);
        return false;
    }

    const int count = read_world_header_tile_count(wld);
    free(wld);

    if (count > terraria_tile_id_get_capacity()) {
        TEKLOG_INFO("tef_tile_save: world header tile count = %d "
                    "(capacity %d -> %d)",
                    count, terraria_tile_id_get_capacity(), count);
        terraria_tile_manager_resize_to(count);
        terraria_tile_manager_init_textures();
    }
    return false; /* 继续执行原函数 */
}

static void load_chests_postfix(patch_handle_t this, void **args, void *result,
                                const patch_method_signature_t *sig) {
    (void) this;
    (void) args;
    (void) result;
    (void) sig;
    tef_tile_save_apply_to_world();
}

void tef_tile_save_init(void) {
    /* 应急开关：排查时可用 TEF_TILE_SAVE_DISABLE=1 完全停掉图格存档钩子 */
    if (getenv("TEF_TILE_SAVE_DISABLE")) {
        TEKLOG_WARN("tef_tile_save: disabled by TEF_TILE_SAVE_DISABLE");
        return;
    }

    patch_handle_t world_file =
            patchlib_type_get_type("Terraria.IO", "WorldFile");
    if (!patchlib_is_valid(world_file)) {
        TEKLOG_WARN("tef_tile_save: Terraria.IO.WorldFile not found");
        return;
    }

    patch_handle_t load_header =
            patchlib_type_get_method(world_file, "LoadFileFormatHeader");
    if (patchlib_is_valid(load_header)) {
#if defined(__ANDROID__)
        /* 只给 PE 装：PE 的 TileID.Count 是编译期常量，存档头部写死 754，
         * 读档时 importance[] 会短一截 → LoadWorldTiles 越界。
         *
         * 桌面端绝对不能装：桌面加载器是 Harmony，postfix 收到的 __args 里
         * out 参数是 null，写回会把 out int[] positions 置成 null，调用方
         * LoadWorld_Version2 立刻 NullReferenceException（+0x2a）。
         * 而 PC 的 TileID.Count 可写，头部数量本来就等于当前总数，不需要扩容。 */
        const patch_hook_id_t hook = patchlib_install_prepost_hook(
                load_header, NULL, load_file_format_header_postfix);
        if (hook == PATCH_HOOK_INVALID_ID) {
            TEKLOG_WARN("tef_tile_save: hook WorldFile.LoadFileFormatHeader "
                        "failed (importance 扩容不会生效)");
        } else {
            TEKLOG_INFO(
                    "tef_tile_save: hooked WorldFile.LoadFileFormatHeader");
        }
#else
        TEKLOG_DEBUG("tef_tile_save: skip hooking LoadFileFormatHeader on "
                     "desktop (Harmony out-parameter write-back)");
#endif
        patchlib_free(load_header);
    } else {
        TEKLOG_WARN("tef_tile_save: LoadFileFormatHeader not found");
    }

    patch_handle_t load_chests =
            patchlib_type_get_method_by_param_count(world_file, "LoadChests", 1);
    if (patchlib_is_valid(load_chests)) {
        const patch_hook_id_t hook = patchlib_install_prepost_hook(
                load_chests, NULL, load_chests_postfix);
        if (hook == PATCH_HOOK_INVALID_ID) {
            TEKLOG_WARN("tef_tile_save: hook WorldFile.LoadChests failed "
                        "(读档图格重映射不会生效)");
        } else {
            TEKLOG_INFO("tef_tile_save: hooked WorldFile.LoadChests");
        }
        patchlib_free(load_chests);
    }

    /* LoadWorld：prefix 按世界头部扩容，postfix 在读档失败时打印托管异常 */
    patch_handle_t load_world =
            patchlib_type_get_method(world_file, "LoadWorld");
    if (patchlib_is_valid(load_world)) {
        const patch_hook_id_t hook = patchlib_install_prepost_hook(
                load_world, world_load_prefix, world_load_postfix);
        if (hook != PATCH_HOOK_INVALID_ID) {
            TEKLOG_INFO("tef_tile_save: hooked WorldFile.LoadWorld");
        }
        patchlib_free(load_world);
    }

    patchlib_free(world_file);
}

