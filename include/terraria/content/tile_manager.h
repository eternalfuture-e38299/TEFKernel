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

typedef struct terraria_tile_ops_t {
    /** Called after all custom tile IDs have been assigned. */
    void (*init_static)(terraria_tile_handle_t *tile);

    /** Return Asset<Texture2D> for TextureAssets.Tile. */
    patch_handle_t (*get_texture)(terraria_tile_handle_t *tile);
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

/** Return the captured vanilla TileID.Count. */
DEFINE_FUNCTION(int, terraria_tile_id_get_count, void)

#ifdef __cplusplus
}
#endif

#endif
