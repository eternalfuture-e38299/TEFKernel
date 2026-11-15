/*******************************************************************************
 * tefkernel - tile_manager (internal)
 ******************************************************************************/
#ifndef TEFKERNEL_INTERNAL_TILE_MANAGER_H
#define TEFKERNEL_INTERNAL_TILE_MANAGER_H

#include "tefstd/vector.h"
#include "terraria/content/tile_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

extern tefstd_vector_t g_terraria_tile_registry;

void terraria_tile_manager_init(void);
void terraria_tile_manager_assign_ids(void);
void terraria_tile_manager_resize(void);
void terraria_tile_manager_init_textures(void);
void terraria_tile_manager_init_static(void);

#ifdef __cplusplus
}
#endif

#endif
