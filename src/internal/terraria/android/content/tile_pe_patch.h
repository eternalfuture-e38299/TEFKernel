/*******************************************************************************
 * tefkernel - PE tile instruction patches (internal)
 ******************************************************************************/
#ifndef TEFKERNEL_INTERNAL_TILE_PE_PATCH_H
#define TEFKERNEL_INTERNAL_TILE_PE_PATCH_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Detect the running libil2cpp.so build and cache its patch profile.
 *
 * @return true when the build is recognized and instruction patching is
 *         available.
 */
bool terraria_tile_pe_patch_init(void);

/**
 * Extend embedded TileID.Count checks to include all registered tile IDs.
 *
 * The function is idempotent. It only applies patches when the requested
 * upper bound grows.
 */
void terraria_tile_pe_patch_apply(int vanilla_count, int total_tiles);

#ifdef __cplusplus
}
#endif

#endif
