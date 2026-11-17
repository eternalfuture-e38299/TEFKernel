/*******************************************************************************
 * tefkernel - buff_manager (internal)
 *******************************************************************************/
#ifndef TEFKERNEL_INTERNAL_BUFF_MANAGER_H
#define TEFKERNEL_INTERNAL_BUFF_MANAGER_H

#include "terraria/content/buff_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

extern tefstd_vector_t g_terraria_buff_registry; // terraria_buff_handle_t*

void terraria_buff_manager_init(void);
void terraria_buff_manager_destroy(void);

/** 分配 runtime_id（base_id 起紧凑分配，按 loader/mod/name 排序，保证跨端一致） */
bool terraria_buff_manager_assign_ids(int base_id);

/** 扩容原版 Buff 表 / 打 PE Count 补丁（不抬 PC 的 BuffID.Count） */
void terraria_buff_manager_resize(void);

/** Main.Initialize 完全结束后再抬 PC 的 BuffID.Count */
void terraria_buff_manager_raise_count(void);

/** 为自定义 Buff 创建/写入图标（无图标用占位图） */
void terraria_buff_manager_init_texture2d(void);

// 初始化本地化文本
void terraria_buff_manager_init_localized_text(void);

#ifdef __cplusplus
}
#endif

#endif /* TEFKERNEL_INTERNAL_BUFF_MANAGER_H */
