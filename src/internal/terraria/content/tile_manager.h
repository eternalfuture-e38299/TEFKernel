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
void terraria_tile_manager_destroy(void);
void terraria_tile_manager_assign_ids(void);
void terraria_tile_manager_resize(void);
void terraria_tile_manager_init_textures(void);
void terraria_tile_manager_init_static(void);
void terraria_tile_manager_post_setup_content(void);

/**
 * @brief 原版图格数 + 已注册自定义图格数（当前运行时的上限）
 */
int terraria_tile_id_get_total_count(void);

/**
 * @brief 当前所有按图格 ID 定长的表已经扩到的长度
 *
 * 正常等于 terraria_tile_id_get_total_count()；读档时可以按世界文件里
 * 记录的图格数量临时扩大（Mod 缺失时避免 LoadWorldTiles 越界）。
 */
int terraria_tile_id_get_capacity(void);

/**
 * @brief 把图格相关表扩容到至少 capacity（只增不减，读档前调用）
 */
void terraria_tile_manager_resize_to(int capacity);

#ifdef __cplusplus
}
#endif

#endif
