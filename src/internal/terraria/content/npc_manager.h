/*******************************************************************************
 * tefkernel - npc_manager (internal)
 *******************************************************************************/
#ifndef TEFKERNEL_INTERNAL_NPC_MANAGER_H
#define TEFKERNEL_INTERNAL_NPC_MANAGER_H

#include "terraria/content/npc_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

extern tefstd_vector_t g_terraria_npc_registry; /* terraria_npc_handle_t* */

void terraria_npc_manager_init(void);
void terraria_npc_manager_destroy(void);

/** 分配 runtime_id（base 起紧凑分配，按 loader/mod/name 排序） */
bool terraria_npc_manager_assign_ids(int base_id);

/** 扩容 NPCID.Sets / TextureAssets.Npc / Main.npcFrameCount 等（Main.Initialize 之后调用） */
void terraria_npc_manager_resize(void);

/** 为自定义 NPC 写入纹理 */
void terraria_npc_manager_init_texture2d(void);

#ifdef __cplusplus
}
#endif

#endif /* TEFKERNEL_INTERNAL_NPC_MANAGER_H */
