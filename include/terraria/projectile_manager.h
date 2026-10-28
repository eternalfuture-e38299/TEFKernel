/*******************************************************************************
 * tefkernel - projectile_manager
 *
 * 自定义弹幕（Projectile）管理：注册 / ID 分配 / 贴图 / SetDefaults / AI 分发。
 * 结构与 item_manager 同构，便于维护。
 *******************************************************************************/
#ifndef TEFFKERNEL_TERRARIA_PROJECTILE_MANAGER_H
#define TEFFKERNEL_TERRARIA_PROJECTILE_MANAGER_H

#include "../tef_api.h"
#include "../patchlib/type.h"
#include "../tefstd/vector.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct terraria_projectile_handle_t terraria_projectile_handle_t;

typedef struct terraria_projectile_ops_t {
    /**
     * @brief 最早初始化（注册回调等前置操作），仅执行一次
     */
    void (*early_init)(terraria_projectile_handle_t *current);

    /**
     * @brief 静态初始化（ID 分配完成后调用）
     */
    void (*init_static)(terraria_projectile_handle_t *current);

    /**
     * @brief 属性设置：每次创建该弹幕实例时调用，给 instance 写字段
     * @warning instance 由 hook 管理器释放，不要持有
     */
    void (*set_defaults)(terraria_projectile_handle_t *current, patch_handle_t instance);

    /**
     * @brief 每帧 AI。
     *
     * 默认（ai_after_vanilla = false）：调用后内核会**跳过原版 AI**，
     * 位移/重力/旋转都要在回调里自己做。
     *
     * ai_after_vanilla = true 时：先让原版 AI（按 aiStyle）完整执行，
     * 内核再调用本回调，用于在 vanilla 行为之上补逻辑。
     *
     * 该机制只依赖内核本身（与具体扩展/加载器无关）：
     * 任何扩展注册的弹幕都可以选择「替换」或「补在 vanilla 之后」。
     * @warning instance 由 hook 管理器释放，不要持有
     */
    void (*ai)(terraria_projectile_handle_t *current, patch_handle_t instance);

    /**
     * @brief 获取纹理实例（Asset<Texture2D>），为 NULL 时用占位贴图
     */
    patch_handle_t (*get_texture)(terraria_projectile_handle_t *current);

    /**
     * @brief 弹幕被杀掉时回调一次（命中消失、超时、手动 Kill 都会触发）
     * @warning instance 由 hook 管理器释放，不要持有
     */
    void (*on_kill)(terraria_projectile_handle_t *current, patch_handle_t instance);

    /**
     * @brief 弹幕对 NPC 造成伤害后回调（穿透 / 多段命中会按 NPC 逐个调用）
     * @param npc           被命中的 NPC
     * @param damage        结算的伤害
     * @param hit_direction 击退方向（-1 / 0 / 1）
     * @param crit          是否暴击
     */
    void (*on_hit_npc)(terraria_projectile_handle_t *current, patch_handle_t instance,
                       patch_handle_t npc, int damage, int hit_direction, bool crit);

    /**
     * @brief 弹幕对玩家造成伤害后回调（PvP 与敌弹打玩家都会触发）
     * @param player        被命中的玩家
     * @param damage        结算的伤害
     * @param hit_direction 击退方向
     * @param pvp           是否 PvP 伤害
     * @param crit          是否暴击
     */
    void (*on_hit_player)(terraria_projectile_handle_t *current, patch_handle_t instance,
                          patch_handle_t player, int damage, int hit_direction,
                          bool pvp, bool crit);

    /**
     * @brief 弹幕撞到实心块时回调（每个发生碰撞的帧一次）
     * @param old_vx 碰撞前的水平速度
     * @param old_vy 碰撞前的垂直速度
     * @note 想要反弹就自己把 velocity 写回去；直接 Kill() 则本帧不会再结算伤害。
     */
    void (*on_tile_collide)(terraria_projectile_handle_t *current, patch_handle_t instance,
                            float old_vx, float old_vy);
} terraria_projectile_ops_t;

typedef struct terraria_projectile_handle_t {
    const char *parent_modloader_id;   //<< 所属 modloader id
    const char *parent_id;            //<< 所属 mod id
    const char *internal_name;        //<< 内部名称
    int runtime_id;                   //<< 由内核分配
    bool no_ai_hook;                  //<< true = 不接管 AI（沿用原版 aiStyle）
    bool ai_after_vanilla;            //<< true = 原版 AI 先执行，再调用扩展 ai 回调
    int frames;                       //<< 帧图帧数（Main.projFrames[type]）；0 = 单帧整图
    terraria_projectile_ops_t projectile_ops;
} terraria_projectile_handle_t;

/**
 * @brief 注册弹幕
 * @warning 需在 Main.Initialize 之前调用（否则分配不到有效 runtime_id）
 */
DEFINE_FUNCTION(bool, terraria_projectile_manager_register_projectile,
                terraria_projectile_handle_t* projectile_handle)

/**
 * @brief 通过运行时 ID 注销
 */
DEFINE_FUNCTION(bool, terraria_projectile_manager_unregister_projectile_by_id, int runtime_id)

/**
 * @brief 取自定义弹幕句柄（越界/未注册返回 NULL）
 */
DEFINE_FUNCTION(terraria_projectile_handle_t*, terraria_projectile_manager_get_projectile, int runtime_id)

/**
 * @brief 取全部自定义弹幕（tefstd_vector_t<terraria_projectile_handle_t*>）
 */
DEFINE_FUNCTION(tefstd_vector_t*, terraria_projectile_manager_get_projectiles)

/**
 * @brief 按 loader/mod/name 查找
 */
DEFINE_FUNCTION(terraria_projectile_handle_t*, terraria_projectile_manager_find_projectile,
                const char* loader, const char* mod, const char* name)

/**
 * @brief 原版 ProjectileID.Count
 */
DEFINE_FUNCTION(int, terraria_projectile_id_get_count)

#ifdef __cplusplus
}
#endif

#endif /* TEFFKERNEL_TERRARIA_PROJECTILE_MANAGER_H */
