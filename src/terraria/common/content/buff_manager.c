/*******************************************************************************
 * tefkernel - buff_manager
 *
 * 自定义 Buff（占用 BuffID.Count 之后的 ID）
 *
 * 现在自定义 ID 会真正写进 Player.buffType / buffTime，由原版
 * AddBuff/UpdateBuffs/存档/联机逻辑和原生 Buff 栏接管显示：
 *   · 注册时按 loader/mod/name 排序分配 runtime_id = BuffID.Count + i；
 *   · 启动时扩容 BuffID.Sets.* / TextureAssets.Buff / Lang 缓存 /
 *     Main.* bool 表 / Player.buffImmune 等按 buff id 索引的原版数组；
 *   · PC 直接抬 BuffID.Count（static readonly）；
 *   · PE 的 GUIBuffs.Draw 把 buffType >= BuffID.Count 的项过滤/清零，
 *     这是编译进指令里的常量，用 buff_pe_patch 内存补丁扩到自定义总数；
 *   · 图标/倒计时/悬停提示全部交给原生 Buff 栏，内核不再手绘；
 *   · 内核保留 apply/update 回调与按名字的 plr.tef 存档，
 *     并设置 Main.buffNoSave[id] = true 防止原版按 runtime_id 存档。
 *******************************************************************************/

#include "internal/terraria/content/buff_manager.h"

#include <stdbool.h>
#include "patchlib/il2cpp_api.h"
#include "patchlib/struct/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/log.h"
#include "internal/terraria/entity.h"
#include "patchlib/field.h"
#include "patchlib/il2cpp_api.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"
#include "patchlib/type.h"
#include "terraria/asset.h"
#include "terraria/language_manager.h"
#include "terraria/texture2d.h"

#if defined(__ANDROID__)
#include "internal/terraria/android/content/buff_pe_patch.h"
#endif

#define BUFF_ACTIVE_MAX 32

tefstd_vector_t g_terraria_buff_registry;

static patch_handle_t f_player_buff_type = PATCH_NULL;
static patch_handle_t f_player_buff_time = PATCH_NULL;
static patch_handle_t f_player_buff_immune = PATCH_NULL;
static patch_hook_id_t h_player_update = PATCH_HOOK_INVALID_ID;
static patch_handle_t f_player_who_am_i = PATCH_NULL; /* Player.whoAmI */
static bool g_ids_assigned = false;

/* Lang._buffNameCache / _buffDescriptionCache（定长 BuffID.Count） */
static patch_handle_t f_texture_buff = PATCH_NULL;   /* TextureAssets.Buff */
static patch_handle_t f_buff_name_cache = PATCH_NULL;
static patch_handle_t f_buff_description_cache = PATCH_NULL;
/* LocalizedText.FromLanguageKey：桌面 (string)，手机 (short, string) */
static patch_handle_t m_from_language_key = PATCH_NULL;
/* 是否已扩容，避免重复扩容 */
static bool g_buff_cache_expanded = false;

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

bool terraria_buff_manager_register_buff(terraria_buff_handle_t *buff_handle) {
    if (!buff_handle || !buff_handle->parent_modloader_id ||
        !buff_handle->parent_id || !buff_handle->internal_name)
        return false;

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

bool terraria_buff_manager_unregister_buff(terraria_buff_handle_t *buff_handle) {
    if (!buff_handle)
        return false;
    const size_t count = tefstd_vector_size(&g_terraria_buff_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_buff_handle_t **pp =
                tefstd_vector_at(&g_terraria_buff_registry, i);
        if (!pp || *pp != buff_handle)
            continue;
        if ((*pp)->runtime_id >= 0)
            clear_active_of((*pp)->runtime_id);
        const bool removed =
                tefstd_vector_erase(&g_terraria_buff_registry, i, NULL);
        if (removed && g_ids_assigned)
            terraria_buff_manager_assign_ids(terraria_buff_id_get_count());
        return removed;
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
/* 原版 Buff 表扩容 / PE 常量补丁                                      */
/* ------------------------------------------------------------------ */

static bool buff_field_is_array(patch_handle_t field) {
    patch_handle_t ft = patchlib_field_get_type_handle(field);
    if (!patchlib_is_valid(ft)) return false;
    const char *tn = patchlib_type_get_name(ft);
    const bool result = tn && strstr(tn, "[]") != NULL;
    patchlib_free(ft);
    return result;
}

static void buff_resize_array_field(patch_handle_t field, const char *name,
                                    const int base, const int new_size) {
    if (!patchlib_is_valid(field) || !buff_field_is_array(field)) return;
    patch_handle_t arr = PATCH_NULL;
    patchlib_field_get_value(field, NULL, &arr);
    if (!patchlib_is_valid(arr)) return;
    const size_t len = patchlib_array_length(arr);
    if (len < (size_t) new_size && len + 16 >= (size_t) base) {
        patch_handle_t bigger =
                patchlib_array_resize(arr, (size_t) new_size, NULL);
        if (patchlib_is_valid(bigger)) {
            patchlib_field_set_value(field, NULL, &bigger);
            TEKLOG_INFO("buff resize: %s %zu -> %d", name ? name : "?", len,
                        new_size);
            patchlib_free(bigger);
        } else {
            TEKLOG_WARN("buff resize: %s %zu -> %d failed",
                        name ? name : "?", len, new_size);
        }
    }
    patchlib_free(arr);
}

static void buff_resize_sets(const int base, const int new_size) {
    patch_handle_t id_cls = patchlib_type_get_type("Terraria.ID", "BuffID");
    if (!patchlib_is_valid(id_cls)) return;
    patch_handle_t sets = patchlib_type_get_inner_type(id_cls, "Sets");
    patchlib_free(id_cls);
    if (!patchlib_is_valid(sets)) return;
#if defined(__ANDROID__)
    /* 和 tile_manager 一样：PE 的静态构造是惰性的，没初始化时字段全是 null，
     * 扩容会被静默跳过，等 cctor 跑完就还是旧长度。这里先强制初始化。 */
    if (il2cpp_runtime_class_init) {
        il2cpp_runtime_class_init((void *) sets);
    }
#endif

    tefstd_vector_t fields;
    tefstd_vector_init(&fields, sizeof(patch_handle_t));
    if (patchlib_type_get_fields(sets, false, &fields)) {
        for (size_t i = 0; i < fields.size; ++i) {
            patch_handle_t f = *(patch_handle_t *) tefstd_vector_at(&fields, i);
            if (!patchlib_is_valid(f)) continue;
            const char *name = patchlib_field_get_name(f);
            if (name && (strcmp(name, "Factory") == 0 ||
                         strcmp(name, "Count") == 0)) {
                patchlib_free(f);
                continue;
            }
            if (!patchlib_field_is_static(f) || patchlib_field_is_const(f) ||
                !buff_field_is_array(f)) {
                patchlib_free(f);
                continue;
            }
            buff_resize_array_field(f, name, base, new_size);
            patchlib_free(f);
        }
    }
    tefstd_vector_destroy(&fields);

    /* BuffID.Sets.Factory._size */
    patch_handle_t f_factory = patchlib_type_get_field(sets, "Factory");
    if (patchlib_is_valid(f_factory)) {
        patch_handle_t factory = PATCH_NULL;
        patchlib_field_get_value(f_factory, NULL, &factory);
        if (patchlib_is_valid(factory)) {
            patch_handle_t sf = patchlib_type_get_type(
                    "Terraria.ID", "SetFactory");
            patch_handle_t f_size = patchlib_is_valid(sf)
                    ? patchlib_type_get_field(sf, "_size") : PATCH_NULL;
            if (patchlib_is_valid(f_size)) {
                int n = new_size;
                patchlib_field_set_value(f_size, factory, &n);
            }
            patchlib_free(f_size);
            patchlib_free(sf);
            patchlib_free(factory);
        }
        patchlib_free(f_factory);
    }
    patchlib_free(sets);
}

/* 某些 BuffID.Sets 的 int 数组默认值不是 0（Factory.CreateIntSet 的 default）。
 * array_resize 新槽一律补 0，会让自定义 buff 误命中：
 *   MountType = 0  -> 每帧 SetMount(0)，冒出别的坐骑效果（1.4.5.8 实测）
 * 这里把已知的非 0 默认值补回去。 */
static void buff_fill_set_defaults(const int base, const int new_size) {
    patch_handle_t id_cls = patchlib_type_get_type("Terraria.ID", "BuffID");
    if (!patchlib_is_valid(id_cls)) return;
    patch_handle_t sets = patchlib_type_get_inner_type(id_cls, "Sets");
    patchlib_free(id_cls);
    if (!patchlib_is_valid(sets)) return;

    static const struct {
        const char *name;
        int value;
    } defaults[] = {
            {"MountType", -1},
            {"SortingPriorityFoodBuffs", -1},
    };
    for (size_t i = 0; i < sizeof(defaults) / sizeof(defaults[0]); ++i) {
        patch_handle_t f = patchlib_type_get_field(sets, defaults[i].name);
        if (!patchlib_is_valid(f)) continue;
        patch_handle_t arr = PATCH_NULL;
        patchlib_field_get_value(f, NULL, &arr);
        if (patchlib_is_valid(arr)) {
            int value = defaults[i].value;
            const size_t len = patchlib_array_length(arr);
            for (int j = base; j < new_size && (size_t) j < len; ++j)
                patchlib_array_set(arr, (size_t) j, &value);
            TEKLOG_INFO("buff resize: %s[%d..%d] = %d",
                        defaults[i].name, base, new_size - 1, value);
        }
        patchlib_free(arr);
        patchlib_free(f);
    }
    patchlib_free(sets);
}

static void buff_resize_main_arrays(const int base, const int new_size) {
    patch_handle_t main_cls = patchlib_type_get_type("Terraria", "Main");
    if (!patchlib_is_valid(main_cls)) return;
    static const char *const names[] = {
            "pvpBuff", "persistentBuff", "vanityPet", "lightPet",
            "meleeBuff", "debuff", "buffNoSave", "buffNoTimeDisplay",
            "buffDoubleApply", "buffAlpha"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        patch_handle_t f = patchlib_type_get_field(main_cls, names[i]);
        if (patchlib_is_valid(f)) {
            buff_resize_array_field(f, names[i], base, new_size);
            patchlib_free(f);
        }
    }
    patchlib_free(main_cls);
}

static void buff_resize_instance_array(patch_handle_t inst,
                                       patch_handle_t field,
                                       const int new_size) {
    if (!patchlib_is_valid(inst) || !patchlib_is_valid(field)) return;
    patch_handle_t arr = PATCH_NULL;
    patchlib_field_get_value(field, inst, &arr);
    if (!patchlib_is_valid(arr)) return;
    if (patchlib_array_length(arr) < (size_t) new_size) {
        patch_handle_t bigger =
                patchlib_array_resize(arr, (size_t) new_size, NULL);
        if (patchlib_is_valid(bigger)) {
            patchlib_field_set_value(field, inst, &bigger);
            patchlib_free(bigger);
        }
    }
    patchlib_free(arr);
}

static void buff_resize_immune_arrays(const int new_size) {
    patch_handle_t main_cls = patchlib_type_get_type("Terraria", "Main");
    if (!patchlib_is_valid(main_cls)) return;
    patch_handle_t f_players = patchlib_type_get_field(main_cls, "player");
    patch_handle_t f_npcs = patchlib_type_get_field(main_cls, "npc");
    patch_handle_t player_cls = patchlib_type_get_type("Terraria", "Player");
    patch_handle_t npc_cls = patchlib_type_get_type("Terraria", "NPC");
    patch_handle_t f_player_immune = patchlib_is_valid(player_cls)
            ? patchlib_type_get_field(player_cls, "buffImmune") : PATCH_NULL;
    patch_handle_t f_npc_immune = patchlib_is_valid(npc_cls)
            ? patchlib_type_get_field(npc_cls, "buffImmune") : PATCH_NULL;

    patch_handle_t arrays[2] = {f_players, f_npcs};
    patch_handle_t fields[2] = {f_player_immune, f_npc_immune};
    for (int a = 0; a < 2; ++a) {
        if (!patchlib_is_valid(arrays[a])) continue;
        patch_handle_t arr = PATCH_NULL;
        patchlib_field_get_value(arrays[a], NULL, &arr);
        if (!patchlib_is_valid(arr)) continue;
        const size_t len = patchlib_array_length(arr);
        for (size_t i = 0; i < len; ++i) {
            patch_handle_t inst = PATCH_NULL;
            if (patchlib_array_at(arr, i, &inst) &&
                patchlib_is_valid(inst)) {
                buff_resize_instance_array(inst, fields[a], new_size);
                patchlib_free(inst);
            }
        }
        patchlib_free(arr);
    }

    patchlib_free(f_player_immune);
    patchlib_free(f_npc_immune);
    patchlib_free(f_players);
    patchlib_free(f_npcs);
    patchlib_free(player_cls);
    patchlib_free(npc_cls);
    patchlib_free(main_cls);
}

/* 自定义 buff 由内核 plr.tef 按名字存档；告诉原版不要把它写进 .plr，
 * 避免卸载/增删 Mod 后 runtime_id 漂移读成别的 buff。 */
static void buff_apply_flags(void) {
    patch_handle_t main_cls = patchlib_type_get_type("Terraria", "Main");
    if (!patchlib_is_valid(main_cls)) return;
    patch_handle_t f_no_save = patchlib_type_get_field(main_cls, "buffNoSave");
    patch_handle_t f_debuff = patchlib_type_get_field(main_cls, "debuff");
    patch_handle_t no_save = PATCH_NULL;
    patch_handle_t debuff = PATCH_NULL;
    if (patchlib_is_valid(f_no_save))
        patchlib_field_get_value(f_no_save, NULL, &no_save);
    if (patchlib_is_valid(f_debuff))
        patchlib_field_get_value(f_debuff, NULL, &debuff);

    const size_t count = tefstd_vector_size(&g_terraria_buff_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_buff_handle_t **pp =
                tefstd_vector_at(&g_terraria_buff_registry, i);
        if (!pp || !*pp || (*pp)->runtime_id < 0) continue;
        const size_t id = (size_t) (*pp)->runtime_id;
        if (patchlib_is_valid(no_save)) {
            bool value = true;
            patchlib_array_set(no_save, id, &value);
        }
        if (patchlib_is_valid(debuff)) {
            bool value = (*pp)->debuff;
            patchlib_array_set(debuff, id, &value);
        }
    }
    patchlib_free(no_save);
    patchlib_free(debuff);
    patchlib_free(f_no_save);
    patchlib_free(f_debuff);
    patchlib_free(main_cls);
}

static void buff_raise_count(const int new_size) {
#if !defined(__ANDROID__)
    patch_handle_t id_cls = patchlib_type_get_type("Terraria.ID", "BuffID");
    if (!patchlib_is_valid(id_cls)) return;
    patch_handle_t f_count = patchlib_type_get_field(id_cls, "Count");
    if (patchlib_is_valid(f_count)) {
        int current = 0;
        patchlib_field_get_value(f_count, NULL, &current);
        if (current < new_size) {
            int value = new_size;
            patchlib_field_set_value(f_count, NULL, &value);
            TEKLOG_INFO("buff resize: BuffID.Count %d -> %d", current, new_size);
        }
        patchlib_free(f_count);
    }
    patchlib_free(id_cls);
#else
    (void) new_size;
#endif
}

void terraria_buff_manager_resize(void) {
    const int base = terraria_buff_id_get_count();
    const int custom =
            (int) tefstd_vector_size(&g_terraria_buff_registry);
    if (custom <= 0 || base <= 0) return;
    const int total = base + custom;

    /* 注意：先只扩数组，不抬 BuffID.Count。
     * Main.Initialize 里的 ContentSamples.Initialize 会按 BuffID.Count
     * 遍历 NPCDebuffImmunityData.ApplyToNPC 等逻辑；此时如果 Count 已经
     * 变大，而某些实例数组还是旧长度，就会 IndexOutOfRange。
     * Count 等 Main.Initialize 完全结束后再抬（raise_count）。 */
    TEKLOG_INFO("buff resize: start base=%d total=%d", base, total);
    TEKLOG_INFO("buff resize: [1/6] BuffID.Sets");
    buff_resize_sets(base, total);
    TEKLOG_INFO("buff resize: [2/6] Sets defaults");
    buff_fill_set_defaults(base, total);
    TEKLOG_INFO("buff resize: [3/6] Main arrays");
    buff_resize_main_arrays(base, total);
    TEKLOG_INFO("buff resize: [4/6] buffImmune");
    buff_resize_immune_arrays(total);
    TEKLOG_INFO("buff resize: [5/6] flags");
    buff_apply_flags();

#if defined(__ANDROID__)
    TEKLOG_INFO("buff resize: [6/6] GUIBuffs patch");
    terraria_buff_pe_patch_apply(base, total);
#endif
    TEKLOG_INFO("buff resize: done");
}

void terraria_buff_manager_raise_count(void) {
#if !defined(__ANDROID__)
    const int base = terraria_buff_id_get_count();
    const int custom =
            (int) tefstd_vector_size(&g_terraria_buff_registry);
    if (custom <= 0 || base <= 0) return;
    buff_raise_count(base + custom);
#endif
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
    if (!patchlib_is_valid(f_player_buff_type) ||
        !patchlib_is_valid(f_player_buff_time))
        return -1;

    patch_handle_t types = PATCH_NULL;
    patch_handle_t times = PATCH_NULL;
    patchlib_field_get_value(f_player_buff_type, player, &types);
    patchlib_field_get_value(f_player_buff_time, player, &times);
    if (!patchlib_is_valid(types) || !patchlib_is_valid(times)) {
        patchlib_free(types);
        patchlib_free(times);
        return -1;
    }

    const int id = buff_handle->runtime_id;
    const size_t slots = patchlib_array_length(types);
    int existing = -1;
    int free_slot = -1;
    for (size_t i = 0; i < slots; ++i) {
        int type = 0;
        if (!patchlib_array_at(types, i, &type)) continue;
        if (type == id) { existing = (int) i; break; }
        if (type == 0 && free_slot < 0) free_slot = (int) i;
    }

    const int slot = existing >= 0 ? existing : free_slot;
    if (slot < 0) {
        TEKLOG_WARN("buff: no free buffType slot for %s", buff_handle->internal_name);
        patchlib_free(types);
        patchlib_free(times);
        return -1;
    }

    int current = 0;
    if (existing >= 0) patchlib_array_at(times, (size_t) slot, &current);
    const int next = time > current ? time : current;
    patchlib_array_set(types, (size_t) slot, (void *) &id);
    patchlib_array_set(times, (size_t) slot, (void *) &next);

    patchlib_free(types);
    patchlib_free(times);

    if (buff_handle->buff_ops.apply)
        buff_handle->buff_ops.apply(buff_handle, player, next);
    return next;
}

/* 供加载器/Mod 手动清除玩家身上的自定义减益。
 * 护士治疗不需要它：自定义 buff 现在已经真正写进 Player.buffType，
 * Main.debuff 已扩容，buff_pe_patch 也把 BuffID.Count 比较改成了总数，
 * 原版 NPCChatText_DoNurseHeal 的循环会自己清理。 */
void terraria_buff_manager_clear_debuffs(patch_handle_t player) {
    if (player == PATCH_NULL) return;
    if (!patchlib_is_valid(f_player_buff_type) ||
        !patchlib_is_valid(f_player_buff_time))
        return;

    patch_handle_t types = PATCH_NULL;
    patch_handle_t times = PATCH_NULL;
    patchlib_field_get_value(f_player_buff_type, player, &types);
    patchlib_field_get_value(f_player_buff_time, player, &times);
    if (!patchlib_is_valid(types)) {
        patchlib_free(times);
        return;
    }

    const int base = terraria_buff_id_get_count();
    const int custom =
            (int) tefstd_vector_size(&g_terraria_buff_registry);
    const size_t slots = patchlib_array_length(types);
    int removed = 0;
    for (size_t i = 0; i < slots; ++i) {
        int type = 0;
        if (!patchlib_array_at(types, i, &type)) continue;
        const int nid = type - base;
        if (nid < 0 || nid >= custom) continue;
        terraria_buff_handle_t *h = find_by_runtime_id(type);
        if (!h || !h->debuff) continue;
        int zero = 0;
        patchlib_array_set(types, i, &zero);
        if (patchlib_is_valid(times)) patchlib_array_set(times, i, &zero);
        removed++;
    }
    TEKLOG_INFO("buff: clear_debuffs removed=%d", removed);
    patchlib_free(types);
    patchlib_free(times);
}

void terraria_buff_manager_visit_active(patch_handle_t player,
                                        const terraria_buff_active_visitor_t visitor,
                                        void *user) {
    if (!visitor || player == PATCH_NULL ||
        !patchlib_is_valid(f_player_buff_type) ||
        !patchlib_is_valid(f_player_buff_time))
        return;

    patch_handle_t types = PATCH_NULL;
    patch_handle_t times = PATCH_NULL;
    patchlib_field_get_value(f_player_buff_type, player, &types);
    patchlib_field_get_value(f_player_buff_time, player, &times);
    if (!patchlib_is_valid(types)) {
        patchlib_free(times);
        return;
    }

    const int base = terraria_buff_id_get_count();
    const int custom =
            (int) tefstd_vector_size(&g_terraria_buff_registry);
    const size_t slots = patchlib_array_length(types);
    for (size_t i = 0; i < slots; ++i) {
        int type = 0, time = 0;
        if (!patchlib_array_at(types, i, &type)) continue;
        const int nid = type - base;
        if (nid < 0 || nid >= custom) continue;
        if (patchlib_is_valid(times))
            patchlib_array_at(times, i, &time);
        if (time <= 0) continue;
        terraria_buff_handle_t *h = find_by_runtime_id(type);
        if (h) visitor(user, h, time);
    }
    patchlib_free(types);
    patchlib_free(times);
}

int terraria_buff_manager_time(terraria_buff_handle_t *buff_handle, patch_handle_t player) {
    if (!buff_handle || player == PATCH_NULL ||
        !patchlib_is_valid(f_player_buff_type) ||
        !patchlib_is_valid(f_player_buff_time))
        return 0;

    patch_handle_t types = PATCH_NULL;
    patch_handle_t times = PATCH_NULL;
    patchlib_field_get_value(f_player_buff_type, player, &types);
    patchlib_field_get_value(f_player_buff_time, player, &times);
    int result = 0;
    if (patchlib_is_valid(types) && patchlib_is_valid(times)) {
        const size_t slots = patchlib_array_length(types);
        for (size_t i = 0; i < slots; ++i) {
            int type = 0;
            if (!patchlib_array_at(types, i, &type)) continue;
            if (type == buff_handle->runtime_id) {
                patchlib_array_at(times, i, &result);
                break;
            }
        }
    }
    patchlib_free(types);
    patchlib_free(times);
    return result;
}

void terraria_buff_manager_remove(terraria_buff_handle_t *buff_handle, patch_handle_t player) {
    if (!buff_handle || player == PATCH_NULL ||
        !patchlib_is_valid(f_player_buff_type) ||
        !patchlib_is_valid(f_player_buff_time))
        return;

    patch_handle_t types = PATCH_NULL;
    patch_handle_t times = PATCH_NULL;
    patchlib_field_get_value(f_player_buff_type, player, &types);
    patchlib_field_get_value(f_player_buff_time, player, &times);
    if (patchlib_is_valid(types)) {
        const size_t slots = patchlib_array_length(types);
        int zero = 0;
        for (size_t i = 0; i < slots; ++i) {
            int type = 0;
            if (!patchlib_array_at(types, i, &type)) continue;
            if (type != buff_handle->runtime_id) continue;
            patchlib_array_set(types, i, &zero);
            if (patchlib_is_valid(times)) patchlib_array_set(times, i, &zero);
            break;
        }
    }
    patchlib_free(types);
    patchlib_free(times);
}

/* 每帧：推进这个玩家身上的自定义 Buff */
static void player_tick(patch_handle_t this) {
    if (!this || !patchlib_is_valid(f_player_buff_type) ||
        !patchlib_is_valid(f_player_buff_time))
        return;

    patch_handle_t types = PATCH_NULL;
    patch_handle_t times = PATCH_NULL;
    patchlib_field_get_value(f_player_buff_type, this, &types);
    patchlib_field_get_value(f_player_buff_time, this, &times);
    if (!patchlib_is_valid(types) || !patchlib_is_valid(times)) {
        patchlib_free(types);
        patchlib_free(times);
        return;
    }

    const int base = terraria_buff_id_get_count();
    const int custom =
            (int) tefstd_vector_size(&g_terraria_buff_registry);
    const size_t slots = patchlib_array_length(types);
    int zero = 0;
    for (size_t i = 0; i < slots; ++i) {
        int type = 0, time = 0;
        if (!patchlib_array_at(types, i, &type)) continue;
        const int nid = type - base;
        if (nid < 0 || nid >= custom) continue;
        patchlib_array_at(times, i, &time);
        if (time <= 0) continue;
        terraria_buff_handle_t *h = find_by_runtime_id(type);
        if (!h) {
            /* Mod 卸载后残留的自定义 ID：清掉，避免原版继续绘制空图标 */
            patchlib_array_set(types, i, &zero);
            patchlib_array_set(times, i, &zero);
            continue;
        }
        if (h->buff_ops.update)
            h->buff_ops.update(h, this, (int) i);
    }
    patchlib_free(types);
    patchlib_free(times);
}

static void buff_ensure_player_immune(patch_handle_t player) {
    if (player == PATCH_NULL || !patchlib_is_valid(f_player_buff_immune))
        return;
    const int base = terraria_buff_id_get_count();
    const int custom =
            (int) tefstd_vector_size(&g_terraria_buff_registry);
    buff_resize_instance_array(player, f_player_buff_immune, base + custom);
}

/* AddBuff/FindBuffIndex 会直接读 buffImmune[type]；PE 的构造函数按
 * 常量 401 分配，新玩家实例在自定义 ID 上会越界。进原版 AddBuff 前补齐。 */
static bool add_buff_prefix(patch_handle_t this, void **args,
                            const patch_method_signature_t *sig, void *result) {
    (void) args; (void) result; (void) sig;
    buff_ensure_player_immune(this);
    /* patchlib 约定：prefix 返回 true = 跳过原函数，返回 false = 继续执行。
     * 这里只做 buffImmune 扩容，必须让原版 Player.AddBuff 继续跑，
     * 否则旗帜(MonsterBanner)/篝火/心形灯笼等所有原版 buff 都加不上。 */
    return false;
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
    TEKLOG_INFO("buff textures: enter");
    const size_t count = tefstd_vector_size(&g_terraria_buff_registry);
    if (count == 0) return;

    /* 让原版 Buff 栏能直接按 buffType 取图标：扩容 TextureAssets.Buff，
     * 再把自定义 ID 对应的 Asset 写进去。 */
    patch_handle_t textures = PATCH_NULL;
    patch_handle_t fallback = PATCH_NULL;
    const int base = terraria_buff_id_get_count();
    const int total = base + (int) count;
    if (patchlib_is_valid(f_texture_buff)) {
        patchlib_field_get_value(f_texture_buff, PATCH_NULL, &textures);
        if (patchlib_is_valid(textures)) {
            const size_t len = patchlib_array_length(textures);
            if (len < (size_t) total) {
                patch_handle_t bigger = patchlib_array_resize(
                        textures, (size_t) total, NULL);
                if (patchlib_is_valid(bigger)) {
                    patchlib_field_set_value(f_texture_buff, PATCH_NULL,
                                             &bigger);
                    patchlib_free(textures);
                    textures = bigger;
                    TEKLOG_INFO("buff resize: TextureAssets.Buff %zu -> %d",
                                len, total);
                }
            }
            patchlib_array_at(textures, 0, &fallback);
        }
    }

    patch_handle_t tex2d_cls = terraria_texture2d_get_class();
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
#endif
        if (patchlib_is_valid(textures) && h->runtime_id >= 0) {
            /* TextureAssets.Buff 的元素类型是 Asset<Texture2D>，加载器返回的是
             * 裸 Texture2D。直接塞进去原生 Buff 栏读不到 .Value，图标会是空白。
             * 这里先包一层 Asset；fallback 本来就是 Asset。 */
            patch_handle_t owned_asset = PATCH_NULL;
            patch_handle_t use = PATCH_NULL;
            if (patchlib_is_valid(h->texture) && patchlib_is_valid(tex2d_cls)) {
                owned_asset = terraria_asset_create(tex2d_cls, h->texture);
                if (patchlib_is_valid(owned_asset)) use = owned_asset;
            }
            if (!patchlib_is_valid(use)) use = fallback;
            if (patchlib_is_valid(use))
                patchlib_array_set(textures, (size_t) h->runtime_id, &use);
            if (patchlib_is_valid(owned_asset)) patchlib_free(owned_asset);
        }
        if (h->texture != PATCH_NULL) done++;
    }
    patchlib_free(textures);
    patchlib_free(fallback);
    TEKLOG_INFO("buff textures: %d/%zu set, TextureAssets.Buff -> %d",
                done, count, total);
}

/**
 * @brief 扩容 Lang._buffNameCache / _buffDescriptionCache 到 base + extra
 * @param field          字段句柄（f_buff_name_cache / f_buff_description_cache）
 * @param element_type   LocalizedText 的类型句柄
 * @param base           原版 BuffID.Count（原数组长度）
 * @param extra          自定义 buff 数量
 * @return 扩容后的数组句柄；失败返回 PATCH_NULL
 * @note 成功后把 field 指向新数组；调用方负责 patchlib_free 旧数组句柄
 */
static patch_handle_t expand_lang_cache(patch_handle_t field,
                                        patch_handle_t element_type,
                                        int base, int extra) {
    (void) element_type;
    if (field == PATCH_NULL || extra <= 0)
        return PATCH_NULL;

    patch_handle_t old_arr = PATCH_NULL;
    patchlib_field_get_value(field, PATCH_NULL, &old_arr);
    if (old_arr == PATCH_NULL)
        return PATCH_NULL;

    const size_t old_len = patchlib_array_length(old_arr);
    const size_t new_len = (size_t) base + (size_t) extra;
    if (new_len <= old_len) {
        /* 已经够大，不重复扩容；句柄仍由字段持有，调用方别 free */
        return old_arr;
    }

    /* 用 resize 而不是 array_create + 逐元素拷贝：
     * PE 上新建引用数组再回填会失败，resize 保类型且自动拷贝。 */
    patch_handle_t new_arr = patchlib_array_resize(old_arr, new_len, NULL);
    if (new_arr == PATCH_NULL) {
        patchlib_free(old_arr);
        return PATCH_NULL;
    }

    patchlib_field_set_value(field, PATCH_NULL, &new_arr);
    patchlib_free(old_arr);
    return new_arr;
}

void terraria_buff_manager_init_localized_text(void) {
    TEKLOG_INFO("buff l10n: enter");
    const int base = terraria_buff_id_get_count();
    const int custom_count = (int) tefstd_vector_size(&g_terraria_buff_registry);
    if (custom_count <= 0)
        return;

    /* 1. 拿 LocalizedText 类型句柄 */
    patch_handle_t localized_text_cls =
            patchlib_type_get_type("Terraria.Localization", "LocalizedText");
    if (localized_text_cls == PATCH_NULL) {
        TEKLOG_WARN("buff l10n: LocalizedText type not found");
        return;
    }

    /* 诊断：字段是否解析到、当前数组长度 */
    {
        patch_handle_t cur = PATCH_NULL;
        size_t name_len = 0, desc_len = 0;
        if (patchlib_is_valid(f_buff_name_cache)) {
            patchlib_field_get_value(f_buff_name_cache, PATCH_NULL, &cur);
            if (patchlib_is_valid(cur)) name_len = patchlib_array_length(cur);
            patchlib_free(cur);
            cur = PATCH_NULL;
        }
        if (patchlib_is_valid(f_buff_description_cache)) {
            patchlib_field_get_value(f_buff_description_cache, PATCH_NULL, &cur);
            if (patchlib_is_valid(cur)) desc_len = patchlib_array_length(cur);
            patchlib_free(cur);
        }
        TEKLOG_INFO("buff l10n: name_field=%d desc_field=%d "
                    "name_len=%zu desc_len=%zu target=%d",
                    patchlib_is_valid(f_buff_name_cache) ? 1 : 0,
                    patchlib_is_valid(f_buff_description_cache) ? 1 : 0,
                    name_len, desc_len, base + custom_count);
    }

    /* 2. 扩容两个缓存数组 */
    patch_handle_t name_cache = expand_lang_cache(
            f_buff_name_cache, localized_text_cls, base, custom_count);
    TEKLOG_INFO("buff l10n: name_cache=%d", patchlib_is_valid(name_cache) ? 1 : 0);
    patch_handle_t desc_cache = expand_lang_cache(
            f_buff_description_cache, localized_text_cls, base, custom_count);
    TEKLOG_INFO("buff l10n: desc_cache=%d", patchlib_is_valid(desc_cache) ? 1 : 0);

    if (name_cache == PATCH_NULL && desc_cache == PATCH_NULL) {
        patchlib_free(localized_text_cls);
        TEKLOG_WARN("buff l10n: expand failed");
        return;
    }
    g_buff_cache_expanded = true;

    /* 3. 逐个写 name / description */
    for (int i = 0; i < custom_count; ++i) {
        terraria_buff_handle_t **pp =
                tefstd_vector_at(&g_terraria_buff_registry, (size_t) i);
        if (!pp || !*pp)
            continue;
        terraria_buff_handle_t *current = *pp;
        const int target_index = base + i;

        /* ---- name ---- */
        if (name_cache != PATCH_NULL) {
            char *name_key_buf = terraria_entity_make_key(TEF_ENTITY(current), "buff_name");
            if (name_key_buf) {
                TEKLOG_INFO("buff l10n: get name text %s", name_key_buf);
                patch_handle_t name_lt =
                        terraria_language_manager_get_localized_text(name_key_buf);
                TEKLOG_INFO("buff l10n: name text=%d", patchlib_is_valid(name_lt) ? 1 : 0);
                if (patchlib_is_valid(name_lt))
                    patchlib_array_set(name_cache, target_index, &name_lt);
                patchlib_free(name_lt);
                free(name_key_buf);
            }
        }

        /* ---- description ---- */
        if (desc_cache != PATCH_NULL && current->has_desc &&
            patchlib_is_valid(m_from_language_key)) {
            char *desc_key_buf = terraria_entity_make_key(TEF_ENTITY(current), "buff_tooltip");
            if (desc_key_buf) {
                TEKLOG_INFO("buff l10n: get desc text %s", desc_key_buf);
                patch_handle_t desc_key_str = patchlib_string_create(desc_key_buf);
                patch_handle_t desc_lt = terraria_language_manager_get_localized_text(desc_key_str);
                TEKLOG_INFO("buff l10n: desc text=%d", patchlib_is_valid(desc_lt) ? 1 : 0);
                if (patchlib_is_valid(desc_lt))
                    patchlib_array_set(desc_cache, target_index, &desc_lt);
                patchlib_free(desc_lt);
                patchlib_free(desc_key_str);
                free(desc_key_buf);
            }
        }
    }

    /* 4. 释放数组句柄（字段已指向新数组，句柄只是本地引用） */
    if (name_cache != PATCH_NULL) patchlib_free(name_cache);
    if (desc_cache != PATCH_NULL) patchlib_free(desc_cache);
    patchlib_free(localized_text_cls);

    TEKLOG_INFO("buff l10n: %d custom buffs localized (base=%d)", custom_count, base);
}

/* 语言切换后由加载器调用：重新把 name / description 写进 Lang 缓存。
 * 直接复用 init_localized_text（幂等），避免每个 buff 各写一套逻辑。 */
void terraria_buff_manager_refresh_display_text(terraria_buff_handle_t *buff_handle) {
    (void) buff_handle;
    terraria_buff_manager_init_localized_text();
}

/* ------------------------------------------------------------------ */
/* 原生 Buff 栏                                                        */
/*                                                                     */
/* 自定义 Buff 直接写进 Player.buffType，图标/倒计时/悬停提示全部由     */
/* 原版 GUIBuffs 与原版 MouseText 处理；内核不再安装任何手绘钩子。       */
/* ------------------------------------------------------------------ */

void terraria_buff_manager_init(void) {
    tefstd_vector_init(&g_terraria_buff_registry, sizeof(terraria_buff_handle_t *));

    patch_handle_t player_cls = patchlib_type_get_type("Terraria", "Player");
    if (patchlib_is_valid(player_cls)) {
        f_player_buff_type = patchlib_type_get_field(player_cls, "buffType");
        f_player_buff_time = patchlib_type_get_field(player_cls, "buffTime");
        f_player_buff_immune = patchlib_type_get_field(player_cls, "buffImmune");
        f_player_who_am_i = patchlib_type_get_field(player_cls, "whoAmI");
        patch_handle_t upd = patchlib_type_get_method_by_param_count(player_cls, "UpdateBuffs", 1);
        if (patchlib_is_valid(upd)) {
            h_player_update = patchlib_install_prepost_hook(upd, NULL, update_buffs_postfix);
            TEKLOG_INFO("buff: hooked Player.UpdateBuffs (%d)", (int) h_player_update);
        }
        patch_handle_t add = patchlib_type_get_method_by_param_count(player_cls, "AddBuff", 3);
        if (patchlib_is_valid(add)) {
            patchlib_install_prepost_hook(add, add_buff_prefix, NULL);
            TEKLOG_INFO("buff: hooked Player.AddBuff (buffImmune resize)");
        }
        patchlib_free(player_cls);
    }

    /* 原生 Buff 栏接管：自定义 buff 已写进 buffType，由 GUIBuffs 直接绘制。
     * 原先的 GUIBuffs.Draw / ItemDraw / GUIItemGrid.Draw 手绘钩子不再安装。 */

    /* TextureAssets.Buff（原版 Buff 栏按 buffType 取图标） */
    {
        patch_handle_t tex_cls = patchlib_type_get_type(
                "Terraria.GameContent", "TextureAssets");
        if (patchlib_is_valid(tex_cls)) {
            f_texture_buff = patchlib_type_get_field(tex_cls, "Buff");
            patchlib_free(tex_cls);
        }
    }

    // 初始化本地化相关
    {
        /* Lang 静态字段：_buffNameCache / _buffDescriptionCache */
        patch_handle_t lang_cls = patchlib_type_get_type("Terraria", "Lang");
        if (!patchlib_is_valid(lang_cls))
            lang_cls = patchlib_type_get_type("Terraria.Localization", "Lang");
        if (patchlib_is_valid(lang_cls)) {
            f_buff_name_cache = patchlib_type_get_field(lang_cls, "_buffNameCache");
            f_buff_description_cache = patchlib_type_get_field(lang_cls, "_buffDescriptionCache");

            /* LocalizedText.FromLanguageKey：桌面/手机签名不同，按参数个数取 */
#if defined(__ANDROID__)
            patch_handle_t lt_cls = patchlib_type_get_type("Terraria.Localization", "LocalizedText");
            if (patchlib_is_valid(lt_cls)) {
                m_from_language_key = patchlib_type_get_method_by_param_count(lt_cls, "FromLanguageKey", 2);
                if (!patchlib_is_valid(m_from_language_key))
                    m_from_language_key = patchlib_type_get_method_by_param_count(lt_cls, "FromLanguageKey", 1);
                patchlib_free(lt_cls);
            }
#else
            patch_handle_t lt_cls = patchlib_type_get_type("Terraria.Localization", "LocalizedText");
            if (patchlib_is_valid(lt_cls)) {
                m_from_language_key = patchlib_type_get_method_by_param_count(lt_cls, "FromLanguageKey", 1);
                patchlib_free(lt_cls);
            }
#endif
            patchlib_free(lang_cls);
        }
    }

#if defined(__ANDROID__)
    terraria_buff_pe_patch_init();
#endif

    TEKLOG_INFO("buff: init done (custom=%zu, native buff bar)",
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