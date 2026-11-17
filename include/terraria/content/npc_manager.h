/*******************************************************************************
 * tefkernel - npc_manager
 *
 * 自定义 NPC 管理：注册 / ID 分配 / 贴图 / SetDefaults / AI 分发。
 * 结构与 projectile_manager 同构。
 *******************************************************************************/
#ifndef TEFFKERNEL_TERRARIA_NPC_MANAGER_H
#define TEFFKERNEL_TERRARIA_NPC_MANAGER_H

#include "../../tef_api.h"
#include "../../patchlib/type.h"
#include "../../tefstd/vector.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct terraria_npc_handle_t terraria_npc_handle_t;

typedef struct terraria_npc_ops_t {
    /**
     * @brief 最早初始化（注册回调等前置操作），仅执行一次
     */
    void (*early_init)(terraria_npc_handle_t *current);

    /**
     * @brief 静态初始化（ID 分配完成后调用）
     */
    void (*init_static)(terraria_npc_handle_t *current);

    /**
     * @brief 属性设置：每次创建该 NPC 实例时调用，给 instance 写字段
     * @warning instance 由 hook 管理器释放，不要持有
     */
    void (*set_defaults)(terraria_npc_handle_t *current, patch_handle_t instance);

    /**
     * @brief 每帧 AI。
     *
     * 默认（ai_after_vanilla = false）：调用后内核会跳过原版 NPC.AI()，
     * 位移/重力/帧动画都要在回调里自己做。
     *
     * ai_after_vanilla = true 时：先让原版 AI（按 aiStyle）执行，再回调。
     * @warning instance 由 hook 管理器释放，不要持有
     */
    void (*ai)(terraria_npc_handle_t *current, patch_handle_t instance);

    /**
     * @brief 获取纹理实例（Asset<Texture2D>），为 NULL 时用占位图
     */
    patch_handle_t (*get_texture)(terraria_npc_handle_t *current);

    /**
     * @brief 获取 Boss 头像 Asset<Texture2D>（仅 boss=true 用），为 NULL 时用默认头像
     */
    patch_handle_t (*get_boss_head)(terraria_npc_handle_t *current);

    /**
     * @brief NPC 死亡掉落时回调（NPC.NPCLoot 后置）
     * @warning instance 由 hook 管理器释放，不要持有
     */
    void (*on_death)(terraria_npc_handle_t *current, patch_handle_t instance);
} terraria_npc_ops_t;

typedef struct terraria_npc_handle_t {
    const char *parent_modloader_id;   /*<< 所属 modloader id */
    const char *parent_id;             /*<< 所属 mod id */
    const char *internal_name;         /*<< 内部名称 */
    int runtime_id;                    /*<< 由内核分配 */
    int frames;                        /*<< 竖向帧数（Main.npcFrameCount[type]），0 = 1 */
    bool ai_after_vanilla;             /*<< true = 原版 AI 先跑，再调用 ai */
    terraria_npc_ops_t npc_ops;
} terraria_npc_handle_t;

/** 图鉴条目分类（对应 BestiaryEntry.Enemy / Critter / TownNPC） */
typedef enum terraria_npc_bestiary_kind_t {
    TERRARIA_NPC_BESTIARY_ENEMY = 0,
    TERRARIA_NPC_BESTIARY_CRITTER = 1,
    TERRARIA_NPC_BESTIARY_TOWN_NPC = 2
} terraria_npc_bestiary_kind_t;

/** 图鉴掉落展示规则 */
typedef struct terraria_npc_loot_entry_t {
    int item_id;               /* > 0 时直接使用该物品 id */
    const char *item_name;     /* item_id <= 0 时按同 Mod 物品内部名称解析 */
    int chance_numerator;      /* <= 0 默认 1 */
    int chance_denominator;    /* <= 0 默认 1 */
    int min_stack;             /* <= 0 默认 1 */
    int max_stack;             /* < min_stack 时取 min_stack */
} terraria_npc_loot_entry_t;

/** 自定义 NPC 的图鉴与旗帜配置 */
typedef struct terraria_npc_bestiary_t {
    bool exclude;                           /* true = 不注入图鉴数据库 */
    terraria_npc_bestiary_kind_t kind;
    int stars;                              /* 0 = 默认 1 星，范围 1..5 */
    bool quick_unlock;                      /* 击杀 1 次即可解锁全部图鉴信息 */
    bool has_flavor_text;                   /* 绑定 npc_bestiary_<loader>_<mod>.<name> */
    const char *const *tags;                /* SpawnConditions 静态字段名列表 */
    int tag_count;
    const terraria_npc_loot_entry_t *drops;
    int drop_count;
    int banner_item_id;                     /* > 0 时按击杀间隔掉落旗帜 */
    const char *banner_item_name;           /* banner_item_id <= 0 时按同 Mod 物品名解析 */
    int kills_to_banner;                    /* <= 0 默认 50 */
} terraria_npc_bestiary_t;

/**
 * @brief 注册自定义 NPC
 * @warning 需在 Main.Initialize 之前调用
 */
DEFINE_FUNCTION(bool, terraria_npc_manager_register_npc,
                terraria_npc_handle_t* npc_handle)

DEFINE_FUNCTION(bool, terraria_npc_manager_unregister_npc_by_id, int runtime_id)

DEFINE_FUNCTION(bool, terraria_npc_manager_unregister_npc, terraria_npc_handle_t* npc_handle)

DEFINE_FUNCTION(terraria_npc_handle_t*, terraria_npc_manager_get_npc, int runtime_id)

DEFINE_FUNCTION(tefstd_vector_t*, terraria_npc_manager_get_npcs)

DEFINE_FUNCTION(terraria_npc_handle_t*, terraria_npc_manager_find_npc,
                const char* loader, const char* mod, const char* name)

/** 原版 NPCID.Count（自定义 id 从它开始） */
DEFINE_FUNCTION(int, terraria_npc_id_get_count)

/**
 * @brief 配置自定义 NPC 的图鉴条目与旗帜掉落（内部深拷贝，可在 init_static 调用）
 */
DEFINE_FUNCTION(bool, terraria_npc_manager_set_bestiary, int runtime_id,
                const terraria_npc_bestiary_t *bestiary)

/**
 * @brief 在指定世界坐标生成一个自定义 NPC
 * @return 成功返回 Main.npc 下标，失败返回 -1
 */
DEFINE_FUNCTION(int, terraria_npc_manager_spawn, int runtime_id, float x, float y)

/** 在指定位置掉落一个物品（内核内部走 Item.NewItem） */
DEFINE_FUNCTION(int, terraria_npc_manager_drop_item, int item_id, float x, float y,
                int count)

#ifdef __cplusplus
}
#endif
#endif /* TEFFKERNEL_TERRARIA_NPC_MANAGER_H */
