/*******************************************************************************
 * tefkernel - buff_manager
 *
 * 自定义 Buff（不占用原版 BuffID）
 *
 * 为什么不用原版 buffType：
 *   手机端原生 Buff 栏（GUIBuffs）会把 buffType 里 >= BuffID.Count 的项过滤掉，
 *   而且原版 AddBuff/UpdateBuffs 对未知 type 有一堆按 type 索引的表与清理逻辑
 *   （IsFedState、MountType、buffImmune...），既容易越界也会误删。
 *
 * 现在改成：
 *   · runtime_id 只是本注册表里的稳定句柄，不写进 buffType；
 *   · 内核自己维护「玩家 -> 活跃 buff -> 剩余帧数」，每帧推进并回调 update；
 *   · 图标与倒计时由内核 GUI 覆盖层绘制；
 *   · 不扩容任何原版数组，不挂钩 AddBuff / FindBuffIndex / DelBuff。
 *     原版按 buff id 索引、长度 BuffID.Count 的数组（Main.buffNoSave /
 *     buffNoTimeDisplay / pvpBuff / lightPet / vanityPet / debuff、
 *     Player/NPC.buffImmune、TextureAssets.Buff、Lang._buffNameCache /
 *     _buffDescriptionCache、BuffID.Sets.*）只会收到原版 0..Count-1 的 id；
 *     自定义 buff 从不进入这些索引，因此无需扩容。
 *******************************************************************************/

#include "internal/terraria/content/buff_manager.h"

#include <stdbool.h>
#include <stdint.h>
#include "patchlib/il2cpp_api.h"
#include "patchlib/struct/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/property.h"
#include "patchlib/struct/array.h"
#include "patchlib/type.h"
#include "terraria/gui.h"

#define BUFF_ACTIVE_MAX 32

tefstd_vector_t g_terraria_buff_registry;

static patch_handle_t f_player_buff_type = PATCH_NULL;
static patch_handle_t f_main_player = PATCH_NULL;
static patch_handle_t m_get_my_player = PATCH_NULL;
static patch_hook_id_t h_player_update = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_buff_bar = PATCH_HOOK_INVALID_ID;
static patch_handle_t f_sprite_batch = PATCH_NULL;
static patch_handle_t f_main_my_player = PATCH_NULL;  /* PC 是 Main.myPlayer 字段 */
static patch_handle_t f_main_mouse_x = PATCH_NULL;
static patch_handle_t f_main_mouse_y = PATCH_NULL;
static patch_handle_t f_player_who_am_i = PATCH_NULL; /* Player.whoAmI */
/* 原版 Buff 栏布局：Buffs_Layout.ItemScale / NormalColour */
static patch_handle_t m_layout_instance = PATCH_NULL;
static patch_handle_t f_layout_item_scale = PATCH_NULL;
static patch_handle_t f_layout_selected_scale = PATCH_NULL;
static patch_handle_t f_layout_normal_colour = PATCH_NULL;
static patch_handle_t f_layout_text_scale = PATCH_NULL;
static patch_handle_t f_layout_text_offset = PATCH_NULL;
static patch_handle_t g_font = PATCH_NULL;   /* FontAssets.ItemStack.Value */
/* 鼠标/触摸：PC 是字段，手机是属性（getter） */
static patch_handle_t m_get_mouse_x = PATCH_NULL;
static patch_handle_t m_get_mouse_y = PATCH_NULL;
static patch_handle_t m_get_mouse_left = PATCH_NULL;
static patch_handle_t m_get_mouse_right = PATCH_NULL;
static patch_handle_t f_main_mouse_left = PATCH_NULL;
static patch_handle_t f_main_mouse_right = PATCH_NULL;
static int g_mx = -1, g_my = -1;
static bool g_left_down, g_right_down;
static bool g_left_tap, g_right_tap;
/* 每帧每个自定义 Buff 的悬停强度（0.4 ~ 1.0），复刻 Main.buffAlpha */
static float g_buff_alpha[32];
static int g_last_tap_buff = -1;      /* 双击取消：上一次点到的 Buff */
static unsigned g_last_tap_frame;     /* 用绘制帧号近似 */
static unsigned g_frame_no;
static bool g_ids_assigned = false;
static int g_base_count = -1;

typedef struct {
    patch_handle_t player;   // 玩家对象（不持有；桌面版每次取值是新的 GCHandle，不能用于比较）
    int who;                 // whoAmI：跨平台稳定的玩家身份
    int buff_id;             // runtime_id
    int time;                // 剩余帧数
} active_buff_t;

/* 桌面版每次从这里拿到的对象句柄都不同，所以一律用 whoAmI 判断“同一个玩家”。 */
static int player_who(patch_handle_t player) {
    if (player == PATCH_NULL || f_player_who_am_i == PATCH_NULL) return -1;
    unsigned char buf[64] = {0};
    patchlib_field_get_value(f_player_who_am_i, player, buf);
    int who = -1;
    memcpy(&who, buf, sizeof(who));
    return who;
}

static active_buff_t g_active[BUFF_ACTIVE_MAX];
static int g_active_count;

/* ------------------------------------------------------------------ */
/* 注册表                                                              */
/* ------------------------------------------------------------------ */

static int compare_buff(const void *a, const void *b) {
    const terraria_buff_handle_t *pa = *(const terraria_buff_handle_t **) a;
    const terraria_buff_handle_t *pb = *(const terraria_buff_handle_t **) b;

    int cmp = strcmp(pa->parent_modloader_id, pb->parent_modloader_id);
    if (cmp != 0) return cmp;
    cmp = strcmp(pa->parent_id, pb->parent_id);
    if (cmp != 0) return cmp;
    return strcmp(pa->internal_name, pb->internal_name);
}

static char *dup_cstr(const char *s) {
    if (!s || !*s) return NULL;
    const size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* 注册时把展示文本拷进内核（此时 Lua 的 entry 一定还活着） */
static void capture_display_text(terraria_buff_handle_t *h) {
    if (!h) return;
    const char *name = h->buff_ops.get_display_name ? h->buff_ops.get_display_name(h) : NULL;
    const char *desc = h->buff_ops.get_display_description
                               ? h->buff_ops.get_display_description(h) : NULL;
    /* 注册时句柄是全新的（LuaLoader 刚构造），不能 free 未初始化指针 */
    h->display_name = dup_cstr(name ? name : h->internal_name);
    h->display_description = dup_cstr(desc);
}

bool terraria_buff_manager_register_buff(terraria_buff_handle_t *buff_handle) {
    if (!buff_handle || !buff_handle->parent_modloader_id ||
        !buff_handle->parent_id || !buff_handle->internal_name)
        return false;

    capture_display_text(buff_handle);

    const size_t count = tefstd_vector_size(&g_terraria_buff_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_buff_handle_t **existing =
                tefstd_vector_at(&g_terraria_buff_registry, i);
        if (!existing || !*existing)
            continue;
        if (strcmp((*existing)->parent_modloader_id,
                   buff_handle->parent_modloader_id) == 0 &&
            strcmp((*existing)->parent_id, buff_handle->parent_id) == 0 &&
            strcmp((*existing)->internal_name,
                   buff_handle->internal_name) == 0) {
            buff_handle->runtime_id = (*existing)->runtime_id;
            buff_handle->texture = (*existing)->texture;
            *existing = buff_handle;
            TEKLOG_INFO("buff re-registered (hot reload): %s.%s id=%d",
                        buff_handle->parent_id, buff_handle->internal_name,
                        buff_handle->runtime_id);
            return true;
        }
    }

    if (g_ids_assigned)
        TEKLOG_WARN("buff registered after IDs assigned: %s.%s", buff_handle->parent_id,
                    buff_handle->internal_name);

    return tefstd_vector_push_back(&g_terraria_buff_registry, &buff_handle);
}

static void clear_active_of(const int runtime_id) {
    for (int i = 0; i < g_active_count;) {
        if (g_active[i].buff_id == runtime_id) {
            g_active[i] = g_active[g_active_count - 1];
            g_active_count--;
        } else {
            i++;
        }
    }
}

void terraria_buff_manager_refresh_display_text(terraria_buff_handle_t *buff_handle) {
    capture_display_text(buff_handle);
}

bool terraria_buff_manager_unregister_buff_by_id(const int runtime_id) {
    if (runtime_id < 0)
        return false;
    const size_t count = tefstd_vector_size(&g_terraria_buff_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_buff_handle_t **pp =
                tefstd_vector_at(&g_terraria_buff_registry, i);
        if (!pp || !*pp)
            continue;
        if ((*pp)->runtime_id == runtime_id) {
            clear_active_of(runtime_id);
            const bool removed =
                    tefstd_vector_erase(&g_terraria_buff_registry, i, NULL);
            if (removed)
                terraria_buff_manager_assign_ids(terraria_buff_id_get_count());
            return removed;
        }
    }
    return false;
}

bool terraria_buff_manager_assign_ids(const int base_id) {
    const size_t count = tefstd_vector_size(&g_terraria_buff_registry);
    if (count == 0) {
        g_ids_assigned = true;
        return true;
    }
    terraria_buff_handle_t **items = g_terraria_buff_registry.data;
    qsort(items, count, sizeof(terraria_buff_handle_t *), compare_buff);
    for (size_t i = 0; i < count; ++i)
        items[i]->runtime_id = base_id + (int) i;
    g_ids_assigned = true;
    return true;
}

int terraria_buff_id_get_count(void) {
    return 401;
}

terraria_buff_handle_t *terraria_buff_manager_get_buff(const int runtime_id) {
    const int base = terraria_buff_id_get_count();
    const int nid = runtime_id - base;
    const size_t count = tefstd_vector_size(&g_terraria_buff_registry);
    if (nid < 0 || nid >= (int) count)
        return NULL;
    terraria_buff_handle_t **pp =
            tefstd_vector_at(&g_terraria_buff_registry, (size_t) nid);
    return pp ? *pp : NULL;
}

tefstd_vector_t *terraria_buff_manager_get_buffs(void) {
    return &g_terraria_buff_registry;
}

terraria_buff_handle_t *terraria_buff_manager_find_buff(const char *loader, const char *mod,
                                                        const char *name) {
    if (!loader || !mod || !name)
        return NULL;
    const size_t count = tefstd_vector_size(&g_terraria_buff_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_buff_handle_t **pp = tefstd_vector_at(&g_terraria_buff_registry, i);
        if (!pp || !*pp)
            continue;
        if (strcmp((*pp)->parent_modloader_id, loader) == 0 &&
            strcmp((*pp)->parent_id, mod) == 0 &&
            strcmp((*pp)->internal_name, name) == 0)
            return *pp;
    }
    return NULL;
}

static terraria_buff_handle_t *find_by_runtime_id(const int runtime_id) {
    const size_t count = tefstd_vector_size(&g_terraria_buff_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_buff_handle_t **pp = tefstd_vector_at(&g_terraria_buff_registry, i);
        if (pp && *pp && (*pp)->runtime_id == runtime_id)
            return *pp;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* 活跃 Buff                                                           */
/* ------------------------------------------------------------------ */

static active_buff_t *find_active(patch_handle_t player, const int runtime_id) {
    const int who = player_who(player);
    for (int i = 0; i < g_active_count; ++i)
        if (g_active[i].who == who && g_active[i].buff_id == runtime_id)
            return &g_active[i];
    return NULL;
}

int terraria_buff_manager_add(terraria_buff_handle_t *buff_handle, patch_handle_t player, const int time) {
    if (!buff_handle || buff_handle->runtime_id < 0 || player == PATCH_NULL || time <= 0)
        return -1;

    active_buff_t *a = find_active(player, buff_handle->runtime_id);
    if (!a) {
        if (g_active_count >= BUFF_ACTIVE_MAX) {
            TEKLOG_WARN("buff: active list is full (%d)", BUFF_ACTIVE_MAX);
            return -1;
        }
        a = &g_active[g_active_count++];
        a->player = PATCH_NULL;   /* 桌面版句柄是临时的，身份只用 who */
        a->who = player_who(player);
        a->buff_id = buff_handle->runtime_id;
        a->time = 0;
    }
    if (time > a->time) a->time = time;
    if (buff_handle->buff_ops.apply) buff_handle->buff_ops.apply(buff_handle, player, a->time);
    return a->time;
}

static patch_handle_t local_player_obj(void);

/* 护士治疗成功：PC 走 Main.NPCChatText_DoNurseHeal，手机走 GUINPCDialogue.Option1Clicked，
 * 但两者都会调用 AchievementsHelper.HandleNurseService —— 这是唯一的“已付费治疗”信号。 */
void terraria_buff_manager_clear_debuffs(patch_handle_t player) {
    if (player == PATCH_NULL) return;
    const int who = player_who(player);
    TEKLOG_INFO("buff: clear_debuffs who=%d active=%d", who, g_active_count);
    for (int i = 0; i < g_active_count;) {
        const int h_id = g_active[i].buff_id;
        terraria_buff_handle_t *h = find_by_runtime_id(h_id);
        if (g_active[i].who == who && h && h->debuff) {
            TEKLOG_DEBUG("buff: nurse heal removed debuff %s", h->internal_name);
            g_active[i] = g_active[g_active_count - 1];
            g_active_count--;
        } else {
            i++;
        }
    }
}

/* 护士治疗成功 → 清自定义减益（原版只清 buffType 里的） */
static void nurse_service_postfix(patch_handle_t this, void **args, void *result,
                                  const patch_method_signature_t *sig) {
    (void) this; (void) args; (void) result; (void) sig;
    patch_handle_t local = local_player_obj();
    if (local == PATCH_NULL) return;
    terraria_buff_manager_clear_debuffs(local);
    patchlib_free(local);
}

void terraria_buff_manager_visit_active(patch_handle_t player,
                                        const terraria_buff_active_visitor_t visitor,
                                        void *user) {
    if (!visitor || player == PATCH_NULL) return;
    const int who = player_who(player);
    for (int i = 0; i < g_active_count; ++i) {
        if (g_active[i].who != who || g_active[i].time <= 0) continue;
        terraria_buff_handle_t *h = find_by_runtime_id(g_active[i].buff_id);
        if (h) visitor(user, h, g_active[i].time);
    }
}

int terraria_buff_manager_time(terraria_buff_handle_t *buff_handle, patch_handle_t player) {
    if (!buff_handle || player == PATCH_NULL) return 0;
    active_buff_t *a = find_active(player, buff_handle->runtime_id);
    return a ? a->time : 0;
}

void terraria_buff_manager_remove(terraria_buff_handle_t *buff_handle, patch_handle_t player) {
    if (!buff_handle || player == PATCH_NULL) return;
    for (int i = 0; i < g_active_count;) {
        if (g_active[i].who == player_who(player) && g_active[i].buff_id == buff_handle->runtime_id) {
            g_active[i] = g_active[g_active_count - 1];
            g_active_count--;
        } else {
            i++;
        }
    }
}

/* 每帧：推进这个玩家身上的自定义 Buff */
static void player_tick(patch_handle_t this) {
    if (!this || g_active_count == 0)
        return;
    for (int i = 0; i < g_active_count;) {
        active_buff_t *a = &g_active[i];
        if (a->who != player_who(this)) {
            i++;
            continue;
        }
        terraria_buff_handle_t *h = find_by_runtime_id(a->buff_id);
        if (!h) {
            g_active[i] = g_active[g_active_count - 1];
            g_active_count--;
            continue;
        }
        if (a->time > 0) a->time--;
        if (a->time <= 0) {
            TEKLOG_DEBUG("buff: %s 到期", h->internal_name);
            g_active[i] = g_active[g_active_count - 1];
            g_active_count--;
            continue;
        }
        /* i 是内核 g_active[] 的下标，不是 player.buffType[] 的槽位。
         * 模组不得拿它（或 runtime_id）去索引原版按 buff 槽位/id 定长的数组。 */
        if (h->buff_ops.update)
            h->buff_ops.update(h, this, i);
        i++;
    }
}

static void update_buffs_postfix(patch_handle_t this, void **args, void *result,
                                 const patch_method_signature_t *sig) {
    (void) args; (void) result; (void) sig;
    player_tick(this);
}

/* ------------------------------------------------------------------ */
/* 图标                                                                */
/* ------------------------------------------------------------------ */

void terraria_buff_manager_init_texture2d(void) {
    const size_t count = tefstd_vector_size(&g_terraria_buff_registry);
    int done = 0;
    for (size_t i = 0; i < count; ++i) {
        terraria_buff_handle_t **pp = tefstd_vector_at(&g_terraria_buff_registry, i);
        if (!pp || !*pp)
            continue;
        terraria_buff_handle_t *h = *pp;
        if (h->texture == PATCH_NULL && h->buff_ops.get_texture)
            h->texture = h->buff_ops.get_texture(h);
#if defined(__ANDROID__)
        /* C 结构体里的引用不算 GC root，必须用 GCHandle 钉住，否则贴图会被回收 */
        if (h->texture != PATCH_NULL && h->texture_pin == NULL && il2cpp_gchandle_new)
            h->texture_pin = il2cpp_gchandle_new(h->texture, false);
        TEKLOG_INFO("buff: pin id=%d gchandle_new=%p texture=%p pin=%p",
                    h->runtime_id, (void *) il2cpp_gchandle_new,
                    (void *) h->texture, (void *) h->texture_pin);
#endif
        if (h->texture != PATCH_NULL) done++;
    }
    TEKLOG_INFO("buff textures: %d/%zu set", done, count);
}

/* ------------------------------------------------------------------ */
/* 覆盖层：图标 + 倒计时                                                */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* 原生 Buff 栏接管                                                     */
/*                                                                     */
/* GUIBuffs.Draw 每帧:                                                  */
/*   numActiveBuffs = 0;                                               */
/*   扫 buffType: 1..400 -> activeBuffs[n++]；>=401 -> buffType[i] = 0 */
/*   GUIItemGrid.Draw(grid, itemScale, itemOver, itemDraw, n, tex, 0)  */
/*   -> 逐项回调 GUIBuffs.ItemDraw(index, position, scale)              */
/*                                                                     */
/* 我们在 GUIItemGrid.Draw 前把计数 +N，再在 ItemDraw 里接管多出来的     */
/* index：位置/缩放/批次/颜色都由原版网格算好，天然与原生 Buff 栏一致。   */
/* ------------------------------------------------------------------ */

static int g_in_bar;          /* 正在 GUIBuffs.Draw 内 */
static int g_bar_base = -1;   /* 原版数量（多出来的 index 从这里开始） */
static unsigned g_bar_epoch = 0;       /* GUIBuffs.Draw 每帧自增 */
static unsigned g_bar_mask_epoch = 0;  /* 已画掩码所属帧 */
static unsigned long long g_bar_mask = 0;

static int main_my_player(void) {
    unsigned char buf[64] = {0};
    if (m_get_my_player != PATCH_NULL) {
        patchlib_method_invoke_args(m_get_my_player, NULL, buf, NULL);
        int my = 0;
        memcpy(&my, buf, sizeof(my));
        return my;
    }
    if (f_main_my_player != PATCH_NULL) {
        patchlib_field_get_value(f_main_my_player, NULL, buf);
        int my = 0;
        memcpy(&my, buf, sizeof(my));
        return my;
    }
    return -1;
}

static bool read_int_handle(patch_handle_t field, patch_handle_t getter, int *out) {
    if (!out) return false;
    unsigned char buf[64] = {0};
    if (getter != PATCH_NULL) {
        patchlib_method_invoke_args(getter, NULL, buf, NULL);
    } else if (field != PATCH_NULL) {
        patchlib_field_get_value(field, NULL, buf);
    } else {
        return false;
    }
    memcpy(out, buf, sizeof(int));
    return true;
}

static bool read_mouse_x(int *x) { return read_int_handle(f_main_mouse_x, m_get_mouse_x, x); }
static bool read_mouse_y(int *y) { return read_int_handle(f_main_mouse_y, m_get_mouse_y, y); }

static bool read_bool_handle(patch_handle_t field, patch_handle_t getter, bool *out) {
    if (!out) return false;
    unsigned char buf[64] = {0};
    if (getter != PATCH_NULL) {
        patchlib_method_invoke_args(getter, NULL, buf, NULL);
    } else if (field != PATCH_NULL) {
        patchlib_field_get_value(field, NULL, buf);
    } else {
        return false;
    }
    *out = buf[0] != 0;
    return true;
}

static void update_mouse_state(void) {
    int x = -1, y = -1;
    if (read_mouse_x(&x) && read_mouse_y(&y)) {
        g_mx = x;
        g_my = y;
    }
    bool left = false, right = false;
    read_bool_handle(f_main_mouse_left, m_get_mouse_left, &left);
    read_bool_handle(f_main_mouse_right, m_get_mouse_right, &right);
    g_left_tap = left && !g_left_down;
    g_right_tap = right && !g_right_down;
    g_left_down = left;
    g_right_down = right;
}

static patch_handle_t local_player_obj(void) {
    if (f_main_player == PATCH_NULL) return PATCH_NULL;
    const int my = main_my_player();
    if (my < 0) return PATCH_NULL;
    patch_handle_t players = PATCH_NULL;
    patchlib_field_get_value(f_main_player, NULL, &players);
    if (players == PATCH_NULL) return PATCH_NULL;
    patch_handle_t local = PATCH_NULL;
    patchlib_array_at(players, (size_t) my, &local);
    patchlib_free(players);
    return local;
}

static void *main_sprite_batch(void) {
    if (f_sprite_batch == PATCH_NULL) return NULL;
    unsigned char sbbuf[64] = {0};
    patchlib_field_get_value(f_sprite_batch, NULL, sbbuf);
    void *sb = NULL;
    memcpy(&sb, sbbuf, sizeof(sb));
    return sb;
}

/* 内核自绘提示：不碰游戏的 MouseText（桌面端会崩），样式照原版：
 * 深色底 + 细边框，第一行名字，第二行说明，跟随指针。 */
#if defined(__ANDROID__)
/* PE：直接调游戏 Main.MouseText —— 面板、字体、材质包全部原生。
 * 文本句柄常驻（内核自有文本 + gchandle 钉住），不会再悬空。 */
static patch_handle_t f_main_inst = PATCH_NULL;
static patch_handle_t m_pe_mouse_text = PATCH_NULL;
static patch_handle_t g_pe_sn = PATCH_NULL, g_pe_sd = PATCH_NULL;
static void *g_pe_pn = NULL, *g_pe_pd = NULL;
static int g_pe_id = -1;

static void show_tooltip_native(terraria_buff_handle_t *h) {
    if (!h || m_pe_mouse_text == PATCH_NULL) return;
    const char *name = h->display_name ? h->display_name : h->internal_name;
    const char *desc = h->display_description;
    if (!name || !*name) return;
    if (g_pe_id != h->runtime_id) {
        if (g_pe_pn && il2cpp_gchandle_free) il2cpp_gchandle_free(g_pe_pn);
        if (g_pe_pd && il2cpp_gchandle_free) il2cpp_gchandle_free(g_pe_pd);
        g_pe_pn = g_pe_pd = NULL;
        g_pe_sn = patchlib_string_create(name);
        g_pe_sd = (desc && *desc) ? patchlib_string_create(desc) : PATCH_NULL;
        if (il2cpp_gchandle_new) {
            if (g_pe_sn) g_pe_pn = il2cpp_gchandle_new(g_pe_sn, false);
            if (g_pe_sd) g_pe_pd = il2cpp_gchandle_new(g_pe_sd, false);
        }
        g_pe_id = h->runtime_id;
    }
    if (!g_pe_sn) return;
    patch_handle_t inst = PATCH_NULL;
    if (f_main_inst != PATCH_NULL) {
        unsigned char ib[64];
        memset(ib, 0, sizeof(ib));
        patchlib_field_get_value(f_main_inst, NULL, ib);
        memcpy(&inst, ib, sizeof(inst));
    }
    if (inst == PATCH_NULL) return;
    int rare = 0, diff = 0, mx = -1, my = -1, sw = -1, sh = -1, push = 0;
    bool no_override = false;
    patch_handle_t sn = g_pe_sn, sd = g_pe_sd;
    void *args[10] = {&sn, &sd, &rare, &diff, &mx, &my, &sw, &sh, &push, &no_override};
    patchlib_method_invoke_args(m_pe_mouse_text, inst, NULL, args);
}
#endif

static void draw_buff_tooltip(terraria_buff_handle_t *h) {
    if (!h) return;
    const char *name = h->display_name ? h->display_name : h->internal_name;
    const char *desc = h->display_description;
    if (!name || !*name) return;

#if defined(__ANDROID__)
    show_tooltip_native(h);
#else
    /* PC：原版（Main.MouseText_DrawBuffTooltip）没有底板，只有两行文字，
     * MouseText 字体、scale 1.0。 */
    const float scale = 1.0f;
    const float line_h = 24.0f;
    const float x = (float) g_mx + 12.0f;
    const float y = (float) g_my + 12.0f;

    terraria_gui_text(name, x, y, scale, 0xFFFFFFFFu);
    if (desc && *desc)
        terraria_gui_text(desc, x, y + line_h, scale, 0xFFD8D8D8u);
#endif
}

static int local_custom_count(void) {
    if (f_main_player == PATCH_NULL) return 0;
    const int my = main_my_player();
    if (my < 0) return 0;
    patch_handle_t players = PATCH_NULL;
    patchlib_field_get_value(f_main_player, NULL, &players);
    if (players == PATCH_NULL) return 0;
    patch_handle_t local = PATCH_NULL;
    patchlib_array_at(players, (size_t) my, &local);
    patchlib_free(players);
    if (local == PATCH_NULL) return 0;
    int n = 0;
    const int my_who = player_who(local);
    patchlib_free(local);   /* 桌面版 array_at 每次分配一个 GCHandle */
    for (int i = 0; i < g_active_count; ++i)
        if (g_active[i].who == my_who && g_active[i].time > 0) n++;
    return n;
}

#if defined(__ANDROID__)
/* 取 FontAssets.<name>.Value（XNA SpriteFont）。
 * 注意：不能走属性 getter —— 泛型实例 Asset`1 上取属性会失败/拿到错对象，
 * 手机端曾因此把 Asset 对象当 SpriteFont 传给 DrawString 直接崩。 */
static patch_handle_t resolve_font_asset(const char *name) {
    patch_handle_t t = patchlib_type_get_type("Terraria.GameContent", "FontAssets");
    if (t == PATCH_NULL) return PATCH_NULL;
    patch_handle_t f = patchlib_type_get_field(t, name);
    patch_handle_t result = PATCH_NULL;
    if (f != PATCH_NULL) {
        unsigned char buf[64];
        memset(buf, 0, sizeof(buf));
        patchlib_field_get_value(f, NULL, buf);
        patch_handle_t holder = PATCH_NULL;
        memcpy(&holder, buf, sizeof(holder));
        patch_handle_t ft = patchlib_field_get_type_handle(f);
        const char *tn = (ft != PATCH_NULL) ? patchlib_type_get_name(ft) : NULL;
        if (holder != PATCH_NULL && tn != NULL && strstr(tn, "Asset") != NULL) {
            patch_handle_t vf = patchlib_type_get_field(ft, "Value");
            if (vf != PATCH_NULL) {
                unsigned char vb[64];
                memset(vb, 0, sizeof(vb));
                patchlib_field_get_value(vf, holder, vb);
                memcpy(&result, vb, sizeof(result));
            }
        } else if (tn == NULL || strstr(tn, "Asset") == NULL) {
            result = holder;
        }
        if (ft != PATCH_NULL) patchlib_free(ft);
        if (f != PATCH_NULL) patchlib_free(f);
    }
    patchlib_free(t);
    return result;
}

static patch_handle_t get_font(void) {
    if (g_font != PATCH_NULL) return g_font;
    static int tries;
    if (tries >= 3) return PATCH_NULL;
    tries++;
    g_font = resolve_font_asset("ItemStack");
    if (g_font == PATCH_NULL) g_font = resolve_font_asset("MouseText");
    return g_font;
}
#endif

static patch_handle_t resolve_getter_like(patch_handle_t type, const char *name) {
    patch_handle_t prop = patchlib_type_get_property(type, name);
    if (!prop || !patchlib_is_valid(prop)) return PATCH_NULL;
    patch_handle_t getter = patchlib_property_get_get_method(prop);
    patchlib_free(prop);
    return getter;
}

static active_buff_t *local_nth_custom(int n) {
    if (f_main_player == PATCH_NULL) return NULL;
    const int my = main_my_player();
    if (my < 0) return NULL;
    patch_handle_t players = PATCH_NULL;
    patchlib_field_get_value(f_main_player, NULL, &players);
    if (players == PATCH_NULL) return NULL;
    patch_handle_t local = PATCH_NULL;
    patchlib_array_at(players, (size_t) my, &local);
    patchlib_free(players);
    if (local == PATCH_NULL) return NULL;
    const int who = player_who(local);
    patchlib_free(local);
    for (int i = 0; i < g_active_count; ++i) {
        if (g_active[i].who != who || g_active[i].time <= 0) continue;
        if (n-- == 0) return &g_active[i];
    }
    return NULL;
}

static bool buffs_draw_prefix(patch_handle_t this, void **args,
                              const patch_method_signature_t *sig, void *result) {
    (void) this; (void) args; (void) sig; (void) result;
    g_in_bar = 1;
    g_bar_base = -1;
    g_bar_epoch++;
    g_frame_no++;
    update_mouse_state();
    return false;
}

static void buffs_draw_postfix(patch_handle_t this, void **args, void *result,
                               const patch_method_signature_t *sig) {
    (void) this; (void) args; (void) result; (void) sig;
    g_in_bar = 0;
    g_bar_base = -1;
}

static void buff_item_draw_postfix(patch_handle_t this, void **args, void *result,
                                   const patch_method_signature_t *sig) {
    (void) this; (void) sig; (void) result;
    if (!g_in_bar || g_bar_base < 0 || !args || !args[1]) return;
    const int index = *(int *) args[1];
    if (index >= g_bar_base) {
        static int dbg;
        if (dbg < 3) {
            dbg++;
            TEKLOG_DEBUG("buff bar: postfix ran for index=%d", index);
        }
    }
}

/* GUIItemGrid.Draw(grid, itemScale, itemOver, itemDraw, count, texture, flag) */
static bool item_grid_draw_prefix(patch_handle_t this, void **args,
                                  const patch_method_signature_t *sig, void *result) {
    (void) this; (void) sig; (void) result;
    if (!g_in_bar || !args || !args[4]) return false;
    /* 同一帧里这个网格可能被画多次（重排/重绘），只扩展一次；
     * 否则 g_bar_base 会一帧内不断上移，导致我们的图标被重复画、
     * 而原版又拿到越界的 activeBuffs[index] 画出别的 Buff 和随机时长。 */
    if (g_bar_base >= 0) {
        static int repeat_logged;
        if (repeat_logged < 3) {
            repeat_logged++;
            TEKLOG_WARN("buff bar: GUIItemGrid.Draw 同一帧被调用多次（count=%d, base=%d）",
                        *(int *) args[4], g_bar_base);
        }
        return false;
    }
    const int extra = local_custom_count();
    if (extra <= 0) return false;
    int *count = args[4];
    g_bar_base = *count;
    *count += extra;
    TEKLOG_DEBUG("buff bar: extend %d -> %d", g_bar_base, *count);
    return false;
}

/* GUIBuffs.ItemDraw(gridLayout, index, position, scale) */
static bool buff_item_draw_prefix(patch_handle_t this, void **args,
                                  const patch_method_signature_t *sig, void *result) {
    (void) this; (void) sig; (void) result;
    if (!g_in_bar || g_bar_base < 0 || !args || !args[1] || !args[2] || !args[3])
        return false;
    const int index = *(int *) args[1];
    if (index < g_bar_base) return false;   /* 原版项交给原版画 */

    const int slot = index - g_bar_base;
    if (slot >= 0 && slot < 64) {
        /* 同一帧同一格只画一次，避免重复绘制叠加亮度 */
        if (g_bar_mask_epoch != g_bar_epoch) {
            g_bar_mask_epoch = g_bar_epoch;
            g_bar_mask = 0;
        }
        const unsigned long long bit = 1ull << slot;
        if (g_bar_mask & bit) return true;
        g_bar_mask |= bit;
    }

    active_buff_t *abuff = local_nth_custom(slot);
    if (!abuff) return true;
    terraria_buff_handle_t *h = find_by_runtime_id(abuff->buff_id);
    if (!h) return true;

    const float *pos = args[2];
    /* 原版：scale = Buffs_Layout.ItemScale（未选中时），贴图 32x32 按它缩放 */
    float item_scale = 1.0f;
    /* 原版: Lerp(NormalColour, SelectedColour, buffAlpha)；激活态 buffAlpha 高，
     * 等价于 SelectedColour(白)。NormalColour 是 40% 透明灰，直接用会压暗图标。 */
    /* 图标略微透明：原版图标是 Lerp(NormalColour, SelectedColour, buffAlpha)，
     * 不是纯白不透明；纯白会明显比旁边的原版图标亮。 */
    /* 原版未悬停：color = Lerp(NormalColour, SelectedColour, buffAlpha)，
     * buffAlpha 静止值 0.4，NormalColour 的 RGB 与 A 相同 → 等价于
     * “白色 RGB + NormalColour.A 的 alpha”。这里照抄它的 alpha。 */
    unsigned tint = 0x66FFFFFFu;   /* 0.4，原版 buffAlpha 静止值 */
    {
        patch_handle_t inst_t = PATCH_NULL;
        if (m_layout_instance != PATCH_NULL) {
            unsigned char ibuf_t[64] = {0};
            patchlib_method_invoke_args(m_layout_instance, NULL, ibuf_t, NULL);
            memcpy(&inst_t, ibuf_t, sizeof(inst_t));
        }
        if (inst_t != PATCH_NULL) {
            if (f_layout_normal_colour != PATCH_NULL) {
                unsigned char cbuf_t[64] = {0};
                patchlib_field_get_value(f_layout_normal_colour, inst_t, cbuf_t);
                unsigned char a = cbuf_t[3];
                if (a > 0) tint = ((unsigned) a << 24) | 0x00FFFFFFu;
            }
            patchlib_free(inst_t);
        }
    }
    if (m_layout_instance != PATCH_NULL) {
        patch_handle_t inst = PATCH_NULL;
        unsigned char ibuf[64] = {0};
        patchlib_method_invoke_args(m_layout_instance, NULL, ibuf, NULL);
        memcpy(&inst, ibuf, sizeof(inst));
        if (inst != PATCH_NULL) {
            if (f_layout_item_scale != PATCH_NULL) {
                unsigned char fbuf[64] = {0};
                patchlib_field_get_value(f_layout_item_scale, inst, fbuf);
                memcpy(&item_scale, fbuf, sizeof(item_scale));
            }
            patchlib_free(inst);
        }
    }
    if (!(item_scale > 0.0f)) item_scale = 1.0f;
    float selected_scale = item_scale;
    if (m_layout_instance != PATCH_NULL && f_layout_selected_scale != PATCH_NULL) {
        patch_handle_t inst_s = PATCH_NULL;
        unsigned char sb[64] = {0};
        patchlib_method_invoke_args(m_layout_instance, NULL, sb, NULL);
        memcpy(&inst_s, sb, sizeof(inst_s));
        if (inst_s != PATCH_NULL) {
            unsigned char vb[64] = {0};
            patchlib_field_get_value(f_layout_selected_scale, inst_s, vb);
            float v = 0.0f;
            memcpy(&v, vb, sizeof(v));
            if (v > 0.0f) selected_scale = v;
            patchlib_free(inst_s);
        }
    }

    /* 悬停高亮：复刻 Main.buffAlpha（0.4 起步，悬停 +0.1/帧，离开 -0.05/帧） */
    const int alpha_slot = (abuff->buff_id & 31);
    float hover_t = g_buff_alpha[alpha_slot];
    if (hover_t <= 0.0f) hover_t = 0.4f;
    const bool hovered = g_mx >= (int) pos[0] && g_mx < (int) pos[0] + 34 &&
                         g_my >= (int) pos[1] && g_my < (int) pos[1] + 34;
    hover_t += hovered ? 0.1f : -0.05f;
    if (hover_t > 1.0f) hover_t = 1.0f;
    if (hover_t < 0.4f) hover_t = 0.4f;
    g_buff_alpha[alpha_slot] = hover_t;
    {
        const unsigned char a8 = (unsigned char) (hover_t * 255.0f + 0.5f);
        tint = ((unsigned) a8 << 24) | 0x00FFFFFFu;
    }
    const float size = 32.0f * (item_scale + (selected_scale - item_scale) * hover_t);

    /* 双击/右键取消 */
    if (hovered && g_left_tap && !h->debuff) {
        if (g_last_tap_buff == abuff->buff_id && g_frame_no - g_last_tap_frame <= 20) {
            patch_handle_t pl = local_player_obj();
            if (pl != PATCH_NULL) {
                terraria_buff_manager_remove(h, pl);
                patchlib_free(pl);
            }
            g_last_tap_buff = -1;
            terraria_gui_end_external();
            return true;
        }
        g_last_tap_buff = abuff->buff_id;
        g_last_tap_frame = g_frame_no;
    }

    void *sb = NULL;
    if (f_sprite_batch != PATCH_NULL) {
        unsigned char sbbuf[64] = {0};
        patchlib_field_get_value(f_sprite_batch, NULL, sbbuf);
        memcpy(&sb, sbbuf, sizeof(sb));
    }
    if (!sb) return true;
    /* 游戏 UI 批次已经开着：借用它，坐标/混合/字体都与原版一致 */
    terraria_gui_begin_external(sb);

    if (hovered) draw_buff_tooltip(h);
    if (h->texture != PATCH_NULL)
        terraria_gui_image_raw(h->texture, pos[0], pos[1], size, size, tint);
    else
        terraria_gui_rect(pos[0], pos[1], size, size, 0xFF00FF00u);


    if (!h->no_time_display) {
        /* 与原版 GUIBuffs.ItemDraw 一致：<60s 用秒，<60min 用分，再往上用小时，
         * 数字四舍五入；带单位后缀。 */
        char label[24];
        if (abuff->time < 3600) {
            int v = (abuff->time + 30) / 60;
            if (v < 1) v = 1;
            snprintf(label, sizeof(label), "%d s", v);
        } else if (abuff->time < 216000) {
            snprintf(label, sizeof(label), "%d m", (abuff->time + 1800) / 3600);
        } else {
            snprintf(label, sizeof(label), "%d h", (abuff->time + 108000) / 216000);
        }
        /* 原版：位置 = 图标位置 + TextOffset * ItemScale，
         *       字号 = ItemScale * TextScale，
         *       用 SpriteBatch.DrawString（无描边），颜色同样吃 50% 透明 */
        float text_scale = 0.8f;
        float off_x = size * 0.15f, off_y = size * 0.95f;
        patch_handle_t inst2 = PATCH_NULL;
        if (m_layout_instance != PATCH_NULL) {
            unsigned char ibuf2[64] = {0};
            patchlib_method_invoke_args(m_layout_instance, NULL, ibuf2, NULL);
            memcpy(&inst2, ibuf2, sizeof(inst2));
        }
        if (inst2 != PATCH_NULL) {
            if (f_layout_text_scale != PATCH_NULL) {
                unsigned char tbuf[64] = {0};
                patchlib_field_get_value(f_layout_text_scale, inst2, tbuf);
                float v = 0.0f;
                memcpy(&v, tbuf, sizeof(v));
                if (v > 0.0f) text_scale = v;
            }
            if (f_layout_text_offset != PATCH_NULL) {
                unsigned char obuf[64] = {0};
                patchlib_field_get_value(f_layout_text_offset, inst2, obuf);
                float ox = 0.0f, oy = 0.0f;
                memcpy(&ox, obuf, sizeof(ox));
                memcpy(&oy, obuf + 4, sizeof(oy));
                off_x = ox * item_scale;
                off_y = oy * item_scale;
            }
            patchlib_free(inst2);
        }
        const float glyph_scale = item_scale * text_scale;
#if defined(__ANDROID__)
        patch_handle_t font = get_font();
        if (font != PATCH_NULL) {
            terraria_gui_draw_string(font, label, pos[0] + off_x, pos[1] + off_y,
                                     glyph_scale, tint);
        } else
#endif
        {
            terraria_gui_text(label, pos[0] + off_x, pos[1] + off_y, glyph_scale, tint);
        }
    }
    terraria_gui_end_external();
    return true;   /* 跳过原版（它会把 activeBuffs 越界） */
}

/* ------------------------------------------------------------------ */
/* PC（桌面）原生 Buff 栏                                               */
/* ------------------------------------------------------------------ */
#if !defined(__ANDROID__)
/* 记录原版这一帧画 Buff 图标用的坐标，直接以它为基准续画，不猜布局 */
static int g_pc_icon_count;
static int g_pc_icon_x, g_pc_icon_y;                 /* 最后一个图标 */
static int g_pc_icon_prev_x, g_pc_icon_prev_y;       /* 倒数第二个 */
static int g_pc_first_x = -1, g_pc_first_y = -1;      /* 第一个图标 */

static bool pc_draw_buff_icon_prefix(patch_handle_t this, void **args,
                                     const patch_method_signature_t *sig, void *result) {
    (void) this; (void) sig; (void) result;
    if (!args || !args[2] || !args[3]) return false;
    const int x = *(int *) args[2];
    const int y = *(int *) args[3];
    if (g_pc_icon_count == 0) {
        g_pc_first_x = x;
        g_pc_first_y = y;
    } else {
        g_pc_icon_prev_x = g_pc_icon_x;
        g_pc_icon_prev_y = g_pc_icon_y;
    }
    g_pc_icon_x = x;
    g_pc_icon_y = y;
    g_pc_icon_count++;
    return false;
}

static bool pc_buffs_draw_prefix(patch_handle_t this, void **args,
                                 const patch_method_signature_t *sig, void *result) {
    (void) this; (void) args; (void) sig; (void) result;
    g_pc_icon_count = 0;
    g_pc_icon_x = g_pc_icon_y = -1;
    g_pc_icon_prev_x = g_pc_icon_prev_y = -1;
    g_pc_first_x = g_pc_first_y = -1;
    update_mouse_state();
    g_frame_no++;
    return false;
}

/* 原版布局（Terraria 1.4.5.8 Main.DrawInterface_Resources_Buffs）：
 *   x = 32 + n*38, y = 76 + (n/11)*50，图标 32x32、scale 1
 *   计时文字 FontAssets.ItemStack、scale 0.8、位置 (x, y+32) */
static void pc_buffs_draw_postfix(patch_handle_t this, void **args, void *result,
                                  const patch_method_signature_t *sig) {
    (void) this; (void) args; (void) result; (void) sig;
    if (g_active_count <= 0 || f_player_buff_type == PATCH_NULL) return;
    patch_handle_t local = local_player_obj();
    if (local == PATCH_NULL) return;
    patch_handle_t types = PATCH_NULL;
    patchlib_field_get_value(f_player_buff_type, local, &types);
    if (types == PATCH_NULL) return;
    const size_t len = patchlib_array_length(types);
    int n = 0;
    for (size_t i = 0; i < len; ++i) {
        int t = 0;
        if (patchlib_array_at(types, i, &t) && t > 0) n++;
    }
    patchlib_free(types);
    const int local_who = local ? player_who(local) : -1;

    /* 基准：优先用原版这一帧第一个图标的真实坐标（比抄常量可靠） */
    int base_x = 32, base_y = 76, step = 38;
    if (g_pc_icon_count > 0 && g_pc_first_x >= 0) {
        base_x = g_pc_first_x;
        base_y = g_pc_first_y;
        if (g_pc_icon_count > 1) {
            const int dx = g_pc_icon_x - g_pc_icon_prev_x;
            const int dy = g_pc_icon_y - g_pc_icon_prev_y;
            if (dx > 0 && dy == 0) step = dx;
        }
    }

    void *sb = main_sprite_batch();
    if (!sb) return;
    terraria_gui_begin_external(sb);
    bool removed = false;
    for (int i = 0; i < g_active_count; ++i) {
        active_buff_t *a = &g_active[i];
        if (a->who != local_who || a->time <= 0) continue;
        terraria_buff_handle_t *h = find_by_runtime_id(a->buff_id);
        if (!h) continue;
        const float x = (float) (base_x + n % 11 * step);
        const float y = (float) (base_y + n / 11 * 50); // NOLINT(*-integer-division)
        const int aslot = a->buff_id & 31;
        float hover_t = g_buff_alpha[aslot];
        if (hover_t <= 0.0f) hover_t = 0.4f;
        const bool hovered = g_mx >= (int) x && g_mx < (int) x + 34 &&
                             g_my >= (int) y && g_my < (int) y + 34;
        hover_t += hovered ? 0.1f : -0.05f;
        if (hover_t > 1.0f) hover_t = 1.0f;
        if (hover_t < 0.4f) hover_t = 0.4f;
        g_buff_alpha[aslot] = hover_t;
        const unsigned char a8 = (unsigned char) (hover_t * 255.0f + 0.5f);
        const unsigned pc_tint = ((unsigned) a8 << 24) | 0x00FFFFFFu;
        /* 原版 PC 悬停只变亮、不放大（放大是 PE 的行为） */
        const float sz = 32.0f;
        if (hovered) draw_buff_tooltip(h);
        if (h->texture != PATCH_NULL)
            terraria_gui_image_raw(h->texture, x, y, sz, sz, pc_tint);
        else
            terraria_gui_rect(x, y, sz, sz, 0xFF00FF00u);
        /* 原版行为：右键点图标取消 Buff */
        if (!h->debuff && g_right_tap && g_mx >= (int) x && g_mx < (int) x + 32 &&
            g_my >= (int) y && g_my < (int) y + 32) {
            terraria_buff_manager_remove(h, local);
            removed = true;
        }
        if (!h->no_time_display) {
            const unsigned tint = 0x66FFFFFFu;
            char label[24];
            if (a->time < 3600) {
                int v = (a->time + 30) / 60;
                if (v < 1) v = 1;
                snprintf(label, sizeof(label), "%d s", v);
            } else if (a->time < 216000) {
                snprintf(label, sizeof(label), "%d m", (a->time + 1800) / 3600);
            } else {
                snprintf(label, sizeof(label), "%d h", (a->time + 108000) / 216000);
            }
            terraria_gui_text(label, x, y + 32.0f, 0.8f, tint);
        }
        n++;
        if (removed) break;
    }
    terraria_gui_end_external();
    if (local) patchlib_free(local);
}
#endif

/* ------------------------------------------------------------------ */
/* 生命周期                                                            */
/* ------------------------------------------------------------------ */

void terraria_buff_manager_init(void) {
    tefstd_vector_init(&g_terraria_buff_registry, sizeof(terraria_buff_handle_t *));

    patch_handle_t main_cls = patchlib_type_get_type("Terraria", "Main");
    if (patchlib_is_valid(main_cls)) {
        f_main_player = patchlib_type_get_field(main_cls, "player");
        f_sprite_batch = patchlib_type_get_field(main_cls, "spriteBatch");
        f_main_my_player = patchlib_type_get_field(main_cls, "myPlayer");
#if !defined(__ANDROID__)
        f_main_mouse_x = patchlib_type_get_field(main_cls, "mouseX");
        f_main_mouse_y = patchlib_type_get_field(main_cls, "mouseY");
        f_main_mouse_left = patchlib_type_get_field(main_cls, "mouseLeft");
        f_main_mouse_right = patchlib_type_get_field(main_cls, "mouseRight");
#endif
#if defined(__ANDROID__)
        f_main_inst = patchlib_type_get_field(main_cls, "instance");
        m_pe_mouse_text = patchlib_type_get_method_by_param_count(main_cls, "MouseText", 10);
        m_get_mouse_x = resolve_getter_like(main_cls, "mouseX");
        m_get_mouse_y = resolve_getter_like(main_cls, "mouseY");
        m_get_mouse_left = resolve_getter_like(main_cls, "mouseLeft");
        m_get_mouse_right = resolve_getter_like(main_cls, "mouseRight");
#endif
        m_get_my_player =
                patchlib_type_get_method_by_param_count(main_cls, "get_myPlayer", 0);
        if (!patchlib_is_valid(m_get_my_player))
            m_get_my_player = patchlib_type_get_method_by_param_count(main_cls, "get_myPlayer", 0);
#if !defined(__ANDROID__)
        /* PC 没有 GUIBuffs/GUIItemGrid，Buff 栏由 DrawInterface_Resources_Buffs 画 */
        {
            patch_handle_t d = patchlib_type_get_method_by_param_count(
        main_cls, "DrawInterface_Resources_Buffs", 0);
            if (patchlib_is_valid(d)) {
                h_buff_bar = patchlib_install_prepost_hook(d, pc_buffs_draw_prefix,
                                                           pc_buffs_draw_postfix);
                TEKLOG_INFO("buff: hooked Main.DrawInterface_Resources_Buffs (%d)",
                            (int) h_buff_bar);
            } else {
                TEKLOG_WARN("buff: Main.DrawInterface_Resources_Buffs not found (PC icons will not be displayed)");
            }

            /* 记录原版图标坐标（静态方法 4 参：drawBuffText, slot, x, y） */
            patch_handle_t icon = patchlib_type_get_method_by_param_count(
                    main_cls, "DrawBuffIcon", 4);
            if (patchlib_is_valid(icon)) {
                patchlib_install_prepost_hook(icon, pc_draw_buff_icon_prefix, NULL);
                TEKLOG_INFO("buff: hooked Main.DrawBuffIcon (reference coordinates)");
            } else {
                TEKLOG_WARN("buff: Main.DrawBuffIcon not found (icon position will fall back to constants)");
            }
        }
#endif
        patchlib_free(main_cls);
    }

    patch_handle_t player_cls = patchlib_type_get_type("Terraria", "Player");
    if (patchlib_is_valid(player_cls)) {
        f_player_buff_type = patchlib_type_get_field(player_cls, "buffType");
        f_player_who_am_i = patchlib_type_get_field(player_cls, "whoAmI");
        patch_handle_t upd = patchlib_type_get_method_by_param_count(player_cls, "UpdateBuffs", 1);
        if (patchlib_is_valid(upd)) {
            h_player_update = patchlib_install_prepost_hook(upd, NULL, update_buffs_postfix);
            TEKLOG_INFO("buff: hooked Player.UpdateBuffs (%d)", (int) h_player_update);
        }
        patchlib_free(player_cls);
    }

    /* 原版 Buff 栏布局（缩放与配色） */
    {
        patch_handle_t lay = patchlib_type_get_type("", "Buffs_Layout");
        if (patchlib_is_valid(lay)) {
            m_layout_instance = resolve_getter_like(lay, "Instance");
            f_layout_item_scale = patchlib_type_get_field(lay, "ItemScale");
            f_layout_selected_scale = patchlib_type_get_field(lay, "SelectedScale");
            f_layout_normal_colour = patchlib_type_get_field(lay, "NormalColour");
            f_layout_text_scale = patchlib_type_get_field(lay, "TextScale");
            if (!patchlib_is_valid(f_layout_text_scale))
                f_layout_text_scale = patchlib_type_get_field(lay, "ItemTextScale");
            f_layout_text_offset = patchlib_type_get_field(lay, "TextOffset");
            patchlib_free(lay);
        }
    }

    /* 护士治疗成功（PC/手机共同入口）：AchievementsHelper.HandleNurseService(int) */
    {
        patch_handle_t t_helper = patchlib_type_get_type(
                "Terraria.GameContent.Achievements", "AchievementsHelper");
        if (patchlib_is_valid(t_helper)) {
            patch_handle_t m_service = patchlib_type_get_method_by_param_count(
                    t_helper, "HandleNurseService", 1);
            if (patchlib_is_valid(m_service)) {
                const patch_hook_id_t hid =
                        patchlib_install_prepost_hook(m_service, NULL, nurse_service_postfix);
                TEKLOG_INFO("buff: hooked AchievementsHelper.HandleNurseService id=%d", (int) hid);
            } else {
                TEKLOG_WARN("buff: AchievementsHelper.HandleNurseService not found");
            }
            patchlib_free(t_helper);
        }
    }

    /* 原生 Buff 栏接管 */
    {
        patch_handle_t gui = patchlib_type_get_type("", "GUIBuffs");
        if (patchlib_is_valid(gui)) {
            patch_handle_t d = patchlib_type_get_method_by_param_count(gui, "Draw", 0);
            if (patchlib_is_valid(d)) {
                h_buff_bar = patchlib_install_prepost_hook(d, buffs_draw_prefix,
                                                           buffs_draw_postfix);
                TEKLOG_INFO("buff: hooked GUIBuffs.Draw (%d)", (int) h_buff_bar);
            }
            patch_handle_t idraw = patchlib_type_get_method_by_param_count(gui, "ItemDraw", 4);
            if (patchlib_is_valid(idraw)) {
                patch_hook_id_t hid = patchlib_install_prepost_hook(
                        idraw, buff_item_draw_prefix, buff_item_draw_postfix);
                TEKLOG_INFO("buff: hooked GUIBuffs.ItemDraw (%d)", (int) hid);
            }
            patchlib_free(gui);
        }
        patch_handle_t grid = patchlib_type_get_type("", "GUIItemGrid");
        if (patchlib_is_valid(grid)) {
            patch_handle_t gd = patchlib_type_get_method_by_param_count(grid, "Draw", 6);
            if (patchlib_is_valid(gd)) {
                patch_hook_id_t hid =
                        patchlib_install_prepost_hook(gd, item_grid_draw_prefix, NULL);
                TEKLOG_INFO("buff: hooked GUIItemGrid.Draw (%d)", (int) hid);
            }
            patchlib_free(grid);
        }
    }
TEKLOG_INFO("buff: init done (custom=%zu, does not occupy vanilla BuffID)",
    tefstd_vector_size(&g_terraria_buff_registry));
}

void terraria_buff_manager_destroy(void) {
#if defined(__ANDROID__)
    const size_t n = tefstd_vector_size(&g_terraria_buff_registry);
    for (size_t i = 0; i < n; ++i) {
        terraria_buff_handle_t **pp = tefstd_vector_at(&g_terraria_buff_registry, i);
        if (pp && *pp && (*pp)->texture_pin && il2cpp_gchandle_free) {
            il2cpp_gchandle_free((*pp)->texture_pin);
            (*pp)->texture_pin = NULL;
        }
    }
#endif
    if (h_player_update != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(h_player_update);
        h_player_update = PATCH_HOOK_INVALID_ID;
    }
    tefstd_vector_destroy(&g_terraria_buff_registry);
    g_active_count = 0;
}