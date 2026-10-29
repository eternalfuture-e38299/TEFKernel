/*******************************************************************************
 * tefkernel - projectile_manager (internal)
 *******************************************************************************/
#ifndef TEFKERNEL_INTERNAL_PROJECTILE_MANAGER_H
#define TEFKERNEL_INTERNAL_PROJECTILE_MANAGER_H

#include "terraria/content/projectile_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

extern tefstd_vector_t g_terraria_projectile_registry; // terraria_projectile_handle_t*

void terraria_projectile_manager_init();
void terraria_projectile_manager_destroy();

/** 分配 runtime_id（base_id 起紧凑分配，按 loader/mod/name 排序，保证跨端一致） */
bool terraria_projectile_manager_assign_ids(int base_id);

/** 扩容 TextureAssets.Projectile[] 与 ProjectileID.Sets.*（在 Main.Initialize 阶段调用） */
void terraria_projectile_manager_resize();

/** 为自定义弹幕创建/写入纹理（无纹理用占位图） */
void terraria_projectile_manager_init_texture2d();

#ifdef __cplusplus
}
#endif

#endif /* TEFKERNEL_INTERNAL_PROJECTILE_MANAGER_H */
