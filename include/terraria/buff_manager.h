/*******************************************************************************
 * tefkernel - buff_manager
 *
 * 自定义 Buff：注册 / ID 分配 / 贴图 / 名称与说明 / 每帧回调。
 * 结构与 item_manager、projectile_manager 同构，便于维护。
 *
 * 设计要点：
 *   · ID：base = BuffID.Count（原版数量），runtime_id = base + 注册表下标；
 *     注册表按 (loader, mod, name) 排序后分配，保证客户端/服务端一致。
 *   · 自管理：自定义 Buff 不写入 Player.buffType，也不经过原版 AddBuff/UpdateBuffs。
 *     原版按 buff id 定长的表（TextureAssets.Buff、Main.*、BuffID.Sets.*、Lang 缓存、
 *     Player/NPC.buffImmune、Main.buffNoSave 等）只会收到原版 0..Count-1，
 *     不需要为自定义 ID 扩容，内核也不会把 runtime_id 写进这些表。
 *   · 显示：内核维护激活列表，接管原生 Buff 栏的绘制（桌面/手机）。
 *   · 回调：apply 在挂上时调用；update 每帧调用，第三个参数是内核 g_active[]
 *     的下标，不是 player.buffType[] 槽位。禁止拿它或 runtime_id 去索引原版定长表。
 *   · 存档：自定义 Buff 不进原版存档，由内核的 plr.tef 伴生存档按
 *     (loader/mod/name) 存读，卸载 Mod 也不会残留无效 ID。
 *******************************************************************************/
#ifndef TEFFKERNEL_TERRARIA_BUFF_MANAGER_H
#define TEFFKERNEL_TERRARIA_BUFF_MANAGER_H

#include "../tef_api.h"
#include "../patchlib/type.h"
#include "../tefstd/vector.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct terraria_buff_handle_t terraria_buff_handle_t;

typedef struct terraria_buff_ops_t {
    /**
     * @brief 最早初始化（注册回调等前置操作），仅执行一次
     */
    void (*early_init)(terraria_buff_handle_t *current);

    /**
     * @brief 静态初始化（ID 分配完成后调用）
     */
    void (*init_static)(terraria_buff_handle_t *current);

    /**
     * @brief Buff 被成功挂到玩家身上时调用（Aura/一次性生效用）
     * @warning player / 句柄由内核管理，不要持有
     */
    void (*apply)(terraria_buff_handle_t *current, patch_handle_t player, int time);

    /**
     * @brief 每帧回调：玩家身上该 Buff 仍激活时调用。
     *
     * 原版 Buff 的效果是写死在 Player.UpdateBuffs 里的，自定义 Buff 想要
     * 实际效果（加属性、扣血、召唤弹幕等）必须在这里实现。
     *
     * @param current     当前buff句柄
     * @param player      玩家实例
     * @param buff_index  内核激活列表 g_active[] 的下标（不是 player.buffType[] 槽位）
     * @warning instance 由 hook 管理器释放，不要持有
     */
    void (*update)(terraria_buff_handle_t *current, patch_handle_t player, int buff_index);

    /**
     * @brief 获取图标纹理实例（Asset<Texture2D>），为 NULL 时用占位图
     */
    patch_handle_t (*get_texture)(terraria_buff_handle_t *current);

    /**
     * @brief 当前语言的显示名称（UTF-8，生命周期由 loader 保证）
     */
    const char *(*get_display_name)(terraria_buff_handle_t *current);

    /**
     * @brief 当前语言的显示说明（UTF-8）
     */
    const char *(*get_display_description)(terraria_buff_handle_t *current);
} terraria_buff_ops_t;

typedef struct terraria_buff_handle_t {
    const char *parent_modloader_id;   //<< 所属 modloader id
    const char *parent_id;             //<< 所属 mod id
    const char *internal_name;         //<< 内部名称
    int runtime_id;                    //<< 由内核分配

    /* 显示/行为标志（对应 Main 上的同名静态表） */
    bool debuff;                       //<< 减益：图标红框、优先被顶掉
    bool no_time_display;              //<< 不显示剩余时间
    bool pvp;                          //<< PvP 专属：非 PvP 目标不生效
    bool light_pet;                    //<< 光源宠物（同类互斥）
    bool vanity_pet;                   //<< 观赏宠物（同类互斥）

    patch_handle_t texture;            //<< 图标 Texture2D（内核持有，不写进 TextureAssets.Buff）
    void *texture_pin;                 //<< Android: il2cpp GCHandle，防止图标被 GC 回收

    terraria_buff_ops_t buff_ops;

    /* 展示文本：注册时从 get_display_* 回调**复制**成内核自有内存，
     * 悬停时不再跨库取指针（Lua 侧重建/卸载后指针会悬空）。 */
    char *display_name;
    char *display_description;
} terraria_buff_handle_t;

/**
 * @brief 注册 Buff
 * @warning 需在 Main.Initialize 之前调用（否则分配不到有效 runtime_id）
 */
DEFINE_FUNCTION(bool, terraria_buff_manager_register_buff,
                terraria_buff_handle_t* buff_handle)

/**
 * @brief 重新捕获显示名/说明（语言切换后调用）
 */
DEFINE_FUNCTION(void, terraria_buff_manager_refresh_display_text,
                terraria_buff_handle_t* buff_handle)

/**
 * @brief 通过运行时 ID 注销
 */
DEFINE_FUNCTION(bool, terraria_buff_manager_unregister_buff_by_id, int runtime_id)

/**
 * @brief 取自定义 Buff 句柄（越界/未注册返回 NULL）
 */
DEFINE_FUNCTION(terraria_buff_handle_t*, terraria_buff_manager_get_buff, int runtime_id)

/**
 * @brief 取全部自定义 Buff（tefstd_vector_t<terraria_buff_handle_t*>）
 */
DEFINE_FUNCTION(tefstd_vector_t*, terraria_buff_manager_get_buffs)

/**
 * @brief 按 loader/mod/name 查找
 */
DEFINE_FUNCTION(terraria_buff_handle_t*, terraria_buff_manager_find_buff,
                const char* loader, const char* mod, const char* name)

/**
 * @brief 原版 BuffID.Count（仅用于 runtime_id 起始值，不写进游戏 buffType）
 */
DEFINE_FUNCTION(int, terraria_buff_id_get_count)

/**
 * @brief 清除某玩家身上的全部自定义减益（护士治疗等场景）
 */
DEFINE_FUNCTION(void, terraria_buff_manager_clear_debuffs, patch_handle_t player)


typedef void (*terraria_buff_active_visitor_t)(void *user, terraria_buff_handle_t *buff,
                                               int time);

/**
 * @brief 遍历某玩家身上激活的自定义 Buff（用于伴生存档）
 * @param player  玩家实例
 * @param visitor 回调：user / buff 句柄 / 剩余帧数
 */
DEFINE_FUNCTION(void, terraria_buff_manager_visit_active, patch_handle_t player,
                terraria_buff_active_visitor_t visitor, void *user)

/**
 * @brief 给玩家挂上自定义 Buff（内核自己计时，不写 buffType）
 * @param time 帧数（60 = 1 秒）；已存在时取较长者
 * @return 剩余帧数，失败返回 -1
 */
DEFINE_FUNCTION(int, terraria_buff_manager_add, terraria_buff_handle_t* buff_handle,
                patch_handle_t player, int time)

/**
 * @brief 玩家身上该 Buff 的剩余帧数（未挂上返回 0）
 */
DEFINE_FUNCTION(int, terraria_buff_manager_time, terraria_buff_handle_t* buff_handle,
                patch_handle_t player)

/**
 * @brief 立刻移除
 */
DEFINE_FUNCTION(void, terraria_buff_manager_remove, terraria_buff_handle_t* buff_handle,
                patch_handle_t player)

#ifdef __cplusplus
}
#endif

#endif /* TEFFKERNEL_TERRARIA_BUFF_MANAGER_H */
