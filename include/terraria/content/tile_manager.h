/*******************************************************************************
 * tefkernel - custom tile API
 *
 * A mod supplies an identity, basic properties, and optional callbacks.
 * TEFKernel owns ID assignment, engine array growth, textures, and platform
 * initialization details.
 ******************************************************************************/
#ifndef TEFKERNEL_TILE_MANAGER_H
#define TEFKERNEL_TILE_MANAGER_H

#include "../../patchlib/type.h"
#include "../../tef_api.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct terraria_tile_handle_t terraria_tile_handle_t;

/** Built-in TileObjectData placement templates for framed/multi-tile blocks. */
typedef enum terraria_tile_object_style_t {
    TERRARIA_TILE_OBJECT_NONE = 0,
    TERRARIA_TILE_OBJECT_TORCH = 1,
    TERRARIA_TILE_OBJECT_PLATFORM = 2,
    TERRARIA_TILE_OBJECT_CHAIR = 3,
    TERRARIA_TILE_OBJECT_WORKBENCH = 4,
    TERRARIA_TILE_OBJECT_TABLE = 5,
    TERRARIA_TILE_OBJECT_SAPLING = 6,
    TERRARIA_TILE_OBJECT_BANNER = 7,
    TERRARIA_TILE_OBJECT_CLOSED_DOOR = 8,
    TERRARIA_TILE_OBJECT_STYLE_1X1 = 9,
    TERRARIA_TILE_OBJECT_STYLE_1X2 = 10,
    TERRARIA_TILE_OBJECT_STYLE_2X1 = 11,
    TERRARIA_TILE_OBJECT_STYLE_2X2 = 12,
    TERRARIA_TILE_OBJECT_STYLE_3X2 = 13,
    TERRARIA_TILE_OBJECT_STYLE_3X3 = 14,
    TERRARIA_TILE_OBJECT_STYLE_3X3_WALL = 15
} terraria_tile_object_style_t;

typedef struct terraria_tile_ops_t {
    /** Called after all custom tile IDs have been assigned. */
    void (*init_static)(terraria_tile_handle_t *tile);

    /** Return Asset<Texture2D> for TextureAssets.Tile. */
    patch_handle_t (*get_texture)(terraria_tile_handle_t *tile);

    /**
     * Optional dynamic drop override on tile destruction.
     * Return true to override drop_item_id and drop_stack with *out_item_id and *out_stack.
     */
    bool (*get_drop)(terraria_tile_handle_t *tile, int x, int y,
                     int *out_item_id, int *out_stack);

    /**
     * Optional dynamic self-illumination callback (RGB typically in 0..1).
     * Note: TileLightScanner may execute on background render threads.
     */
    bool (*get_light)(terraria_tile_handle_t *tile, int x, int y,
                      float *out_r, float *out_g, float *out_b);

    /**
     * 右键（使用键）交互回调，仅在 interactable == true 时调用。
     * x/y 是玩家点击的那一格（多格图格时不一定是最左上角）。
     * 返回 true 表示已处理，TEFKernel 会吞掉这次使用键事件；
     * 返回 false 则继续交给原版逻辑。
     */
    bool (*on_interact)(terraria_tile_handle_t *tile, int x, int y,
                        int player_index);
} terraria_tile_ops_t;

typedef struct terraria_tile_handle_t {
    /** Loader and mod identity. All strings must remain valid. */
    const char *parent_modloader_id;
    const char *parent_id;
    const char *internal_name;

    /** Assigned by TEFKernel after registration. Use -1 initially. */
    int runtime_id;

    /** Basic one-block tile properties. */
    bool solid;
    bool solid_top;
    bool merge_dirt;
    bool block_light;
    bool frame_important;

    terraria_tile_ops_t tile_ops;

    /** Mining & behavior flags (0 / false preserves default behavior). */
    int min_pick;
    float mine_resist;
    bool cut;
    bool no_fail;
    bool no_attach;
    bool table;
    bool lava_death;
    bool water_death;
    int shine;
    bool spelunker;
    bool bouncy;
    bool blend_all;

    /** Dust & hit sound overrides when has_* is true. */
    bool has_dust_type;
    int dust_type;
    bool has_hit_sound;
    int hit_sound_type;
    int hit_sound_style;

    /**
     * Item drop configuration:
     *   drop_item_id == 0: auto-resolve custom item with createTile == runtime_id
     *   drop_item_id <  0: drop nothing
     *   drop_item_id >  0: drop this ItemID
     * drop_item_name (same modloader/mod internal name) takes precedence when non-NULL.
     */
    int drop_item_id;
    const char *drop_item_name;
    int drop_stack;

    /** Bidirectional merge targets by runtime TileID or same-mod internal name. */
    const int *merge_with_ids;
    int merge_with_count;
    const char *const *merge_with_names;
    int merge_with_names_count;

    /** Vanilla/custom crafting station TileIDs counted as adjacent (e.g. WorkBenches=18). */
    const int *adj_tile_ids;
    int adj_tile_count;

    /** Multi-tile / furniture TileObjectData preset. */
    terraria_tile_object_style_t object_style;
    bool lava_immune_object;

    /** Self-illumination (Main.tileLighted + TileLightScanner.ApplyTileLight). */
    bool lighted;
    float light_r;
    float light_g;
    float light_b;

    /** Minimap lookup entry (MapHelper + Lang._mapLegendCache). */
    bool has_map_color;
    unsigned char map_r;
    unsigned char map_g;
    unsigned char map_b;
    bool has_map_name;

    /* ================= 多格图格（TileObjectData 几何） =================
     * 0 表示沿用 object_style 预设；tile_width/tile_height > 0 时覆盖预设尺寸。
     * 例如 4x2 的床：tile_width=4, tile_height=2。
     * 未设置坐标参数时使用 Terraria 家具的通用默认值：
     *   coordinate_width=16, coordinate_padding=2, 每行高度=16。 */
    int tile_width;
    int tile_height;
    /** 纹理原点（相对左上角，单位：格），一般 0,0。 */
    int origin_x;
    int origin_y;
    /** 每格的像素宽/高与间隔（0 = 16 / 16 / 2）。 */
    int coordinate_width;
    int coordinate_padding;
    const int *coordinate_heights;   /**< 可选：每行像素高度数组 */
    int coordinate_height_count;
    /** 横向排布样式（多套纹理时使用）；0 表示不修改。 */
    int style_wrap_limit;
    int style_multiplier;
    int style_line_skip;
    int draw_y_offset;
    bool style_horizontal;
    /** 不需要下方支撑（墙挂类图格）。 */
    bool anchor_wall;

    /* ================= 交互图格 ================= */
    /** 允许右键交互；需要在 tile_ops.on_interact 里处理。 */
    bool interactable;

    /* ================= 物品槽位（容器） =================
     * container=true 时，放下后会像箱子一样创建 Chest 记录，
     * 右键打开、破坏时校验/清理，槽位通过下面三个接口读写。
     * container_slots: 1..40，0 表示 40。 */
    bool container;
    int container_slots;
} terraria_tile_handle_t;

/**
 * Register a custom tile.
 *
 * Re-registering the same loader/mod/name updates the stored pointer while
 * preserving its assigned runtime ID.
 */
DEFINE_FUNCTION(bool, terraria_tile_manager_register_tile,
                terraria_tile_handle_t *tile)

/** Remove a previously registered tile. */
DEFINE_FUNCTION(bool, terraria_tile_manager_unregister_tile,
                terraria_tile_handle_t *tile)

/** Resolve a registered tile by its runtime TileID. */
DEFINE_FUNCTION(terraria_tile_handle_t *, terraria_tile_manager_get_tile,
                int runtime_id)

/** Resolve a registered tile by loader/mod/internal_name. */
DEFINE_FUNCTION(terraria_tile_handle_t *, terraria_tile_manager_find_tile,
                const char *loader, const char *mod, const char *name)

/** Return the captured vanilla TileID.Count. */
DEFINE_FUNCTION(int, terraria_tile_id_get_count, void)

/** Set symmetric Main.tileMerge[a][b] and ChecksForMerge flags at runtime. */
DEFINE_FUNCTION(void, terraria_tile_manager_set_merge,
                int tile_a, int tile_b, bool merge)

/* ==================== 容器（物品槽位） ==================== */

/** 计算 (x,y) 所在多格图格的左上角坐标（非多格图格返回自身）。 */
DEFINE_FUNCTION(void, terraria_tile_manager_get_origin,
                terraria_tile_handle_t *tile, int x, int y,
                int *out_x, int *out_y)

/** 返回该容器实际可用的槽位数（container_slots，0 视为 40）。 */
DEFINE_FUNCTION(int, terraria_tile_container_slot_count,
                terraria_tile_handle_t *tile)

/** 查找 (x,y) 所在容器对应的 Chest 索引，没有则返回 -1。 */
DEFINE_FUNCTION(int, terraria_tile_container_index,
                terraria_tile_handle_t *tile, int x, int y)

/** 读取槽位；成功返回 true。 */
DEFINE_FUNCTION(bool, terraria_tile_container_get_slot,
                terraria_tile_handle_t *tile, int x, int y, int slot,
                int *out_item_id, int *out_stack, int *out_prefix)

/** 写入槽位；item_id <= 0 表示清空该槽。成功返回 true。 */
DEFINE_FUNCTION(bool, terraria_tile_container_set_slot,
                terraria_tile_handle_t *tile, int x, int y, int slot,
                int item_id, int stack, int prefix)

/** 清空整个容器。 */
DEFINE_FUNCTION(bool, terraria_tile_container_clear,
                terraria_tile_handle_t *tile, int x, int y)

#ifdef __cplusplus
}
#endif

#endif
