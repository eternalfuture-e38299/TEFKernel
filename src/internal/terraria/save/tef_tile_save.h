/*******************************************************************************
 * tefkernel - tef_tile_save（世界图格身份表）
 *
 * 世界文件（.wld）里图格只能存数值 id，而运行时 id 由「本次加载的 Mod 集合」
 * 决定：少装一个 Mod、换加载顺序，旧存档里的自定义图格就会变成另一个 Mod 的
 * 图格（串档），甚至因为超过数组长度直接「加载失败」。
 *
 * 这里的做法与 item 存档一致：
 *   1. 保存时把 (loader, mod, name) → 本次使用的数值 id 写进 <world>.tef；
 *   2. 读档时只按 (loader, mod, name) 重新解析 id，绝不直接信任数值 id；
 *   3. 解析结果与世界里的数值不同就重映射世界图格；Mod 缺失则降级成占位图格。
 *******************************************************************************/
#ifndef TEFKERNEL_TEF_TILE_SAVE_H
#define TEFKERNEL_TEF_TILE_SAVE_H

#include <stdbool.h>

#include "internal/terraria/save/tef_save_format.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 安装 LoadFileFormatHeader / LoadChests 相关钩子（幂等） */
void tef_tile_save_init(void);

/**
 * 读档前需要保证的下限：当前运行时图格总数，或存档里出现过的最大自定义
 * id + 1（取较大者）。
 *
 * PE 的 TileID.Count 是编译期常量，WorldFile.SaveFileFormatHeader 会写死
 * 754，LoadFileFormatHeader 又按这个值分配 importance[]，而 LoadWorldTiles
 * 直接 importance[tileType] → 自定义 id 越界抛异常（表现为「加载失败」）。
 */
int tef_tile_save_required_tile_count(void);

/** 把当前世界的自定义图格身份表写进 TILES 段（没有任何自定义图格时写空表） */
void tef_tile_save_write_seg(tef_save_wbuf_t *seg);

/** 读档后按身份重映射世界里的图格类型（无表/无需重映射时直接返回） */
void tef_tile_save_apply_to_world(void);

#ifdef __cplusplus
}
#endif

#endif /* TEFKERNEL_TEF_TILE_SAVE_H */
