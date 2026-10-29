/*******************************************************************************
 * tefkernel - npc_manager
 * Copyright (C) 2026 eternalfuture-e38299
 *
 * 自定义 NPC：注册 / ID 分配 / 贴图 / SetDefaults / AI 分发 / 生成。
 * 结构与 projectile_manager 同构。
 *******************************************************************************/

#include "internal/terraria/content/npc_manager.h"
#include "internal/log.h"

#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/property.h"
#include "patchlib/struct/array.h"
#include "terraria/asset.h"
#include "terraria/language_manager.h"
#include "terraria/texture2d.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

tefstd_vector_t g_terraria_npc_registry;

static patch_handle_t npc_class = PATCH_NULL;
static patch_handle_t f_type = PATCH_NULL;
static patch_handle_t f_active = PATCH_NULL;
static patch_handle_t npc_id_count = PATCH_NULL;       /* NPCID.Count (short) */
static patch_handle_t f_texture_npc = PATCH_NULL;      /* TextureAssets.Npc */
static patch_handle_t f_npc_frame_count = PATCH_NULL;  /* Main.npcFrameCount */
static patch_hook_id_t h_set_defaults = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_town_room_ctor = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_npc_loot = PATCH_HOOK_INVALID_ID;
static patch_handle_t f_has_room = PATCH_NULL;  /* TownRoomManager._hasRoom */
static patch_hook_id_t h_ai = PATCH_HOOK_INVALID_ID;
static bool g_ids_assigned = false;

/* 原版 NPCID.Count（697）。桌面端之后会把 NPCID.Count 抬高用于绘制，
 * 所以内核内部一律用这个缓存值做 base，避免 nid 计算被改掉。 */
static int g_npc_base = 697;

/* NPC.boss / NPCID.Sets.BossHeadTextures：boss=true 的自定义 NPC 在
 * 小地图/大地图上会画一个 Boss 头像（BossHeadTextures 默认 -1 = 不画）。 */
static patch_handle_t f_npc_boss = PATCH_NULL;
static patch_handle_t f_boss_head_textures = PATCH_NULL;

#if defined(__ANDROID__)
/* Android: NPCID.Count 是 const 697，Main.DrawNPCs 的 type < NPCID.Count
 * 会把自定义 NPC 跳过。这里在 DrawNPCs 后置里手动补画。 */
static patch_handle_t m_draw_direct = PATCH_NULL;
static patch_handle_t f_main_sprite_batch = PATCH_NULL;
static patch_handle_t f_main_screen_pos = PATCH_NULL;
static patch_handle_t m_screen_pos_get = PATCH_NULL;
static patch_handle_t f_main_npc_arr = PATCH_NULL;
static patch_handle_t f_npc_hide = PATCH_NULL;
static patch_handle_t f_npc_behind_tiles = PATCH_NULL;
static patch_hook_id_t h_draw_npcs = PATCH_HOOK_INVALID_ID;

static void draw_npcs_postfix(patch_handle_t this, void **args, void *result,
                              const patch_method_signature_t *sig_info) {
    (void) result;
    (void) sig_info;
    if (!this || !args || !args[0] || !patchlib_is_valid(m_draw_direct))
        return;

    if (!patchlib_is_valid(f_main_sprite_batch) ||
        !patchlib_is_valid(m_screen_pos_get) ||
        !patchlib_is_valid(f_main_npc_arr) ||
        !patchlib_is_valid(f_npc_hide) ||
        !patchlib_is_valid(f_npc_behind_tiles) ||
        !patchlib_is_valid(f_active) ||
        !patchlib_is_valid(f_type))
        return;

    const bool behind = *(bool *) args[0];
    patch_handle_t sb = PATCH_NULL;
    /* Main.spriteBatch / screenPosition 是静态字段，instance 必须传 NULL */
    patchlib_field_get_value(f_main_sprite_batch, NULL, &sb);
    if (!patchlib_is_valid(sb)) return;

    float screen_pos[2] = {0.0f, 0.0f};
    patchlib_method_invoke_args(m_screen_pos_get, NULL, screen_pos, NULL);

    patch_handle_t arr = PATCH_NULL;
    patchlib_field_get_value(f_main_npc_arr, NULL, &arr);
    if (!patchlib_is_valid(arr)) return;

    const size_t n = patchlib_array_length(arr);
    const size_t custom = tefstd_vector_size(&g_terraria_npc_registry);
    for (size_t i = 0; i < n; ++i) {
        patch_handle_t npc = PATCH_NULL;
        if (!patchlib_array_at(arr, i, &npc) || !npc) continue;
        bool active = false, hide = false, npc_behind = false;
        int type = 0;
        patchlib_field_get_value(f_active, npc, &active);
        patchlib_field_get_value(f_npc_hide, npc, &hide);
        patchlib_field_get_value(f_npc_behind_tiles, npc, &npc_behind);
        patchlib_field_get_value(f_type, npc, &type);
        const int nid = type - g_npc_base;
        if (active && !hide && type > 0 && nid >= 0 && nid < (int) custom &&
            npc_behind == behind) {
            void *dargs[4] = {&sb, &npc, (void *) &behind, screen_pos};
            patchlib_method_invoke_args(m_draw_direct, this, NULL, dargs);
        }
        patchlib_free(npc);
    }
    patchlib_free(arr);
}
#endif

/* ------------------------------------------------------------------ */
/* 注册表                                                              */
/* ------------------------------------------------------------------ */

static int compare_npc(const void *a, const void *b) {
    const terraria_npc_handle_t *pa = *(const terraria_npc_handle_t **) a;
    const terraria_npc_handle_t *pb = *(const terraria_npc_handle_t **) b;

    int cmp = strcmp(pa->parent_modloader_id, pb->parent_modloader_id);
    if (cmp != 0) return cmp;
    cmp = strcmp(pa->parent_id, pb->parent_id);
    if (cmp != 0) return cmp;
    return strcmp(pa->internal_name, pb->internal_name);
}

bool terraria_npc_manager_register_npc(terraria_npc_handle_t *npc_handle) {
    if (!npc_handle || !npc_handle->parent_modloader_id ||
        !npc_handle->parent_id || !npc_handle->internal_name)
        return false;

    const size_t count = tefstd_vector_size(&g_terraria_npc_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_npc_handle_t **existing =
                tefstd_vector_at(&g_terraria_npc_registry, i);
        if (!existing || !*existing)
            continue;
        if (strcmp((*existing)->parent_modloader_id,
                   npc_handle->parent_modloader_id) == 0 &&
            strcmp((*existing)->parent_id, npc_handle->parent_id) == 0 &&
            strcmp((*existing)->internal_name, npc_handle->internal_name) == 0) {
            npc_handle->runtime_id = (*existing)->runtime_id;
            *existing = npc_handle;
            TEKLOG_INFO("npc re-registered: %s.%s id=%d",
                        npc_handle->parent_id, npc_handle->internal_name,
                        npc_handle->runtime_id);
            return true;
        }
    }

    if (g_ids_assigned)
        TEKLOG_WARN("npc registered after IDs assigned: %s.%s",
                    npc_handle->parent_id, npc_handle->internal_name);

    return tefstd_vector_push_back(&g_terraria_npc_registry, &npc_handle);
}

bool terraria_npc_manager_unregister_npc_by_id(const int runtime_id) {
    if (runtime_id < 0)
        return false;
    const size_t count = tefstd_vector_size(&g_terraria_npc_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_npc_handle_t **pp =
                tefstd_vector_at(&g_terraria_npc_registry, i);
        if (!pp || !*pp)
            continue;
        if ((*pp)->runtime_id == runtime_id) {
            const bool removed =
                    tefstd_vector_erase(&g_terraria_npc_registry, i, NULL);
            if (removed)
                terraria_npc_manager_assign_ids(g_npc_base);
            return removed;
        }
    }
    return false;
}

bool terraria_npc_manager_assign_ids(const int base_id) {
    const size_t count = tefstd_vector_size(&g_terraria_npc_registry);
    if (count == 0) {
        g_ids_assigned = true;
        return true;
    }
    terraria_npc_handle_t **items = g_terraria_npc_registry.data;
    qsort(items, count, sizeof(terraria_npc_handle_t *), compare_npc);
    for (size_t i = 0; i < count; ++i)
        items[i]->runtime_id = base_id + (int) i;
    g_ids_assigned = true;
    return true;
}

int terraria_npc_id_get_count(void) {
    short count = 0;
    if (patchlib_is_valid(npc_id_count))
        patchlib_field_get_value(npc_id_count, NULL, &count);
    return count;
}

terraria_npc_handle_t *terraria_npc_manager_get_npc(const int runtime_id) {
    const int base = g_npc_base;
    const int nid = runtime_id - base;
    const size_t count = tefstd_vector_size(&g_terraria_npc_registry);
    if (nid < 0 || nid >= (int) count)
        return NULL;
    terraria_npc_handle_t **pp =
            tefstd_vector_at(&g_terraria_npc_registry, (size_t) nid);
    return pp ? *pp : NULL;
}

tefstd_vector_t *terraria_npc_manager_get_npcs(void) {
    return &g_terraria_npc_registry;
}

terraria_npc_handle_t *terraria_npc_manager_find_npc(
        const char *loader, const char *mod, const char *name) {
    if (!loader || !mod || !name)
        return NULL;
    const size_t count = tefstd_vector_size(&g_terraria_npc_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_npc_handle_t **pp =
                tefstd_vector_at(&g_terraria_npc_registry, i);
        if (!pp || !*pp)
            continue;
        if (strcmp((*pp)->parent_modloader_id, loader) == 0 &&
            strcmp((*pp)->parent_id, mod) == 0 &&
            strcmp((*pp)->internal_name, name) == 0)
            return *pp;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* 数组扩容                                                            */
/* ------------------------------------------------------------------ */

/* 用原数组里出现次数最多的元素填充新槽位（NPCID.Sets 里很多集合
 * 的默认值不是 0，例如 TrailCacheLength = 10）。 */
static void fill_new_slots_with_modal(patch_handle_t old_array,
                                      patch_handle_t new_array,
                                      const size_t old_len,
                                      const size_t new_len) {
    enum { MAX_UNIQ = 64, EBUF = 64 };
    if (!patchlib_is_valid(old_array) || !patchlib_is_valid(new_array) ||
        old_len == 0 || new_len <= old_len)
        return;

    uint64_t keys[MAX_UNIQ];
    int counts[MAX_UNIQ];
    uint8_t vals[MAX_UNIQ][EBUF];
    int nu = 0;
    uint8_t buf[EBUF];

    for (size_t i = 0; i < old_len; ++i) {
        memset(buf, 0, EBUF);
        if (!patchlib_array_at(old_array, i, buf))
            continue;
        uint64_t key = 0;
        memcpy(&key, buf, sizeof(key));
        int found = -1;
        for (int j = 0; j < nu; ++j) {
            if (keys[j] == key) {
                found = j;
                break;
            }
        }
        if (found >= 0) {
            counts[found]++;
        } else if (nu < MAX_UNIQ) {
            keys[nu] = key;
            memcpy(vals[nu], buf, EBUF);
            counts[nu] = 1;
            nu++;
        }
    }
    if (nu == 0)
        return;

    int best = 0;
    for (int j = 1; j < nu; ++j)
        if (counts[j] > counts[best])
            best = j;
    for (size_t i = old_len; i < new_len; ++i)
        if (!patchlib_array_set(new_array, i, vals[best]))
            break;
}

static bool field_is_array(patch_handle_t field) {
    patch_handle_t t = patchlib_field_get_type_handle(field);
    if (!patchlib_is_valid(t))
        return false;
    const char *name = patchlib_type_get_name(t);
    bool is_array = false;
    if (name) {
        const size_t len = strlen(name);
        is_array = len >= 2 && strcmp(name + len - 2, "[]") == 0;
    }
    patchlib_free(t);
    return is_array;
}

/* 把按 NPC type 索引的静态数组扩到 new_size；只处理长度正好等于 base 的数组 */
static void resize_static_array(patch_handle_t field, const char *name,
                                const int base, const int new_size,
                                const bool modal, const int int_fill,
                                const bool fill_with_int) {
    if (!patchlib_is_valid(field)) {
        TEKLOG_WARN("npc resize: field %s not found", name ? name : "?");
        return;
    }
    patch_handle_t array = PATCH_NULL;
    patchlib_field_get_value(field, NULL, &array);
    if (!patchlib_is_valid(array))
        return;

    const size_t len = patchlib_array_length(array);
    if (len + 8 < (size_t) base || len >= (size_t) new_size) {
        TEKLOG_DEBUG("npc resize: %s len=%zu != base=%d, skip",
                     name ? name : "?", len, base);
        patchlib_free(array);
        return;
    }

    patch_handle_t bigger = PATCH_NULL;
    if (fill_with_int) {
        int fill = int_fill;
        bigger = patchlib_array_resize(array, (size_t) new_size, &fill);
    } else {
        bigger = patchlib_array_resize(array, (size_t) new_size, NULL);
    }
    if (patchlib_is_valid(bigger)) {
        if (modal)
            fill_new_slots_with_modal(array, bigger, len, (size_t) new_size);
        patchlib_field_set_value(field, NULL, &bigger);
        TEKLOG_DEBUG("npc resize: %s %zu -> %d", name ? name : "?", len, new_size);
        patchlib_free(bigger);
    } else {
        TEKLOG_ERROR("npc resize: %s resize failed", name ? name : "?");
    }
    patchlib_free(array);
}

/* 处理 Sets 里的所有数组成员 */
static int resize_sets_arrays(const char *ns, const char *cls, const char *inner,
                              const int base, const int new_size,
                              const bool exclude_town_groups) {
    patch_handle_t cls_h = patchlib_type_get_type(ns, cls);
    if (!patchlib_is_valid(cls_h))
        return 0;
    patch_handle_t sets = patchlib_type_get_inner_type(cls_h, inner);
    patchlib_free(cls_h);
    if (!patchlib_is_valid(sets))
        return 0;

    tefstd_vector_t fields;
    tefstd_vector_init(&fields, sizeof(patch_handle_t));
    patchlib_type_get_fields(sets, false, &fields);
    int resized = 0;
    for (size_t i = 0; i < fields.size; ++i) {
        patch_handle_t field = *(patch_handle_t *) tefstd_vector_at(&fields, i);
        if (!patchlib_is_valid(field))
            continue;
        const char *name = patchlib_field_get_name(field);
        if (!patchlib_field_is_static(field) || patchlib_field_is_const(field) ||
            !field_is_array(field) || !name) {
            patchlib_free(field);
            continue;
        }
        if (strcmp(name, "Factory") == 0 || strcmp(name, "Count") == 0 ||
            (exclude_town_groups && strcmp(name, "TownNPCsFramingGroups") == 0)) {
            patchlib_free(field);
            continue;
        }

        patch_handle_t array = PATCH_NULL;
        patchlib_field_get_value(field, NULL, &array);
        if (!array) {
            patchlib_free(field);
            continue;
        }
        const size_t len = patchlib_array_length(array);
        if (len + 8 >= (size_t) base && len < (size_t) new_size) {
            patch_handle_t bigger =
                    patchlib_array_resize(array, (size_t) new_size, NULL);
            if (patchlib_is_valid(bigger)) {
                fill_new_slots_with_modal(array, bigger, len, (size_t) new_size);
                patchlib_field_set_value(field, NULL, &bigger);
                resized++;
                patchlib_free(bigger);
            } else {
                TEKLOG_ERROR("npc resize: %s.%s.%s failed",
                             cls, inner, name);
            }
        }
        patchlib_free(array);
        patchlib_free(field);
    }
    tefstd_vector_destroy(&fields);
    patchlib_free(sets);
    return resized;
}

/* 粗暴但有效：把某个类上所有「长度 == base 的静态数组」全部扩到 new_size。
 * 和 item_manager 的 resize_all_class_sets 一个思路，宁可多扩，不要漏扩。 */
static int resize_all_static_arrays(const char *ns, const char *cls,
                                    const int base, const int new_size,
                                    const char *const *excludes,
                                    const size_t n_excludes,
                                    const bool modal) {
    patch_handle_t cls_h = patchlib_type_get_type(ns, cls);
    if (!patchlib_is_valid(cls_h)) return 0;

    tefstd_vector_t fields;
    tefstd_vector_init(&fields, sizeof(patch_handle_t));
    patchlib_type_get_fields(cls_h, false, &fields);
    int done = 0;
    for (size_t i = 0; i < fields.size; ++i) {
        patch_handle_t f = *(patch_handle_t *) tefstd_vector_at(&fields, i);
        if (!patchlib_is_valid(f)) continue;
        const char *name = patchlib_field_get_name(f);
        bool skip = !patchlib_field_is_static(f) ||
                    patchlib_field_is_const(f) || !field_is_array(f) || !name;
        if (!skip) {
            for (size_t j = 0; j < n_excludes; ++j)
                if (excludes && excludes[j] && strcmp(name, excludes[j]) == 0)
                    skip = true;
        }
        if (skip) {
            patchlib_free(f);
            continue;
        }
        patch_handle_t arr = PATCH_NULL;
        patchlib_field_get_value(f, NULL, &arr);
        if (!arr) {
            patchlib_free(f);
            continue;
        }
        if (patchlib_array_length(arr) == (size_t) base) {
            patch_handle_t bigger =
                    patchlib_array_resize(arr, (size_t) new_size, NULL);
            if (patchlib_is_valid(bigger)) {
                if (modal) {
                    const size_t old_len = patchlib_array_length(arr);
                    fill_new_slots_with_modal(arr, bigger, old_len,
                                              (size_t) new_size);
                }
                patchlib_field_set_value(f, NULL, &bigger);
                done++;
                patchlib_free(bigger);
            } else {
                TEKLOG_ERROR("npc resize: %s.%s.%s failed", cls, name, "resize");
            }
        }
        patchlib_free(arr);
        patchlib_free(f);
    }
    tefstd_vector_destroy(&fields);
    patchlib_free(cls_h);
    return done;
}

/* 扩容某个类型上的实例数组（如 Player.npcTypeNoAggro） */
static void resize_player_arrays(const int base, const int new_size) {
    patch_handle_t main_cls = patchlib_type_get_type("Terraria", "Main");
    patch_handle_t player_cls = patchlib_type_get_type("Terraria", "Player");
    if (!patchlib_is_valid(main_cls) || !patchlib_is_valid(player_cls))
        return;
    patch_handle_t f_players = patchlib_type_get_field(main_cls, "player");
    patch_handle_t f_no_aggro =
            patchlib_type_get_field(player_cls, "npcTypeNoAggro");

    if (patchlib_is_valid(f_players) && patchlib_is_valid(f_no_aggro)) {
        patch_handle_t players = PATCH_NULL;
        patchlib_field_get_value(f_players, NULL, &players);
        if (patchlib_is_valid(players)) {
            const size_t n = patchlib_array_length(players);
            int patched = 0;
            for (size_t i = 0; i < n; ++i) {
                patch_handle_t player = PATCH_NULL;
                if (!patchlib_array_at(players, i, &player) || !player)
                    continue;
                patch_handle_t arr = PATCH_NULL;
                patchlib_field_get_value(f_no_aggro, player, &arr);
                if (arr && patchlib_array_length(arr) == (size_t) base) {
                    patch_handle_t bigger =
                            patchlib_array_resize(arr, (size_t) new_size, NULL);
                    if (patchlib_is_valid(bigger)) {
                        patchlib_field_set_value(f_no_aggro, player, &bigger);
                        patched++;
                        patchlib_free(bigger);
                    }
                }
                if (arr) patchlib_free(arr);
                patchlib_free(player);
            }
            TEKLOG_INFO("npc resize: npcTypeNoAggro %d/%zu players", patched, n);
        }
        if (patchlib_is_valid(players)) patchlib_free(players);
    }
    if (patchlib_is_valid(f_players)) patchlib_free(f_players);
    if (patchlib_is_valid(f_no_aggro)) patchlib_free(f_no_aggro);
    patchlib_free(main_cls);
    patchlib_free(player_cls);
}

/* Lang._npcNameCache (LocalizedText[]) 按 NPCID.Count 定长。
 * 新槽位填一个自建的 LocalizedText：不要用原数组元素当 fill ——
 * array_at 拿到的是借用句柄，跨到 array_resize 里会踩坏 GC。 */
static void resize_npc_name_cache(const int base, const int new_size) {
    patch_handle_t lang_cls = patchlib_type_get_type("Terraria", "Lang");
    if (!patchlib_is_valid(lang_cls))
        return;
    patch_handle_t f = patchlib_type_get_field(lang_cls, "_npcNameCache");
    patchlib_free(lang_cls);
    if (!patchlib_is_valid(f))
        return;

    patch_handle_t arr = PATCH_NULL;
    patchlib_field_get_value(f, NULL, &arr);
    if (!patchlib_is_valid(arr) || patchlib_array_length(arr) != (size_t) base) {
        if (patchlib_is_valid(arr)) patchlib_free(arr);
        patchlib_free(f);
        return;
    }

    TEKLOG_INFO("npc resize: Lang._npcNameCache %d -> %d...", base, new_size);
    patch_handle_t text = terraria_language_manager_create_localized_text(
            "NPCName.ModNPC", "Mod NPC");
    /* 不能给 array_resize 传引用型 fill：桌面端这条路会崩。
     * 先 NULL 扩容，再逐个 array_set（和贴图数组同一套写法）。 */
    patch_handle_t bigger =
            patchlib_array_resize(arr, (size_t) new_size, NULL);
    if (patchlib_is_valid(bigger)) {
        patchlib_field_set_value(f, NULL, &bigger);
        if (patchlib_is_valid(text)) {
            for (int i = base; i < new_size; ++i)
                patchlib_array_set(bigger, (size_t) i, &text);
        }
        TEKLOG_INFO("npc resize: Lang._npcNameCache done");
        patchlib_free(bigger);
    }
    if (patchlib_is_valid(text)) patchlib_free(text);
    patchlib_free(arr);
    patchlib_free(f);
}

/* SceneMetrics.ClosestNPCPosition 是每个实例一份、按 NPC type 索引的
 * Vector2[]；SceneMetrics.ScanNPCPositions 会直接 ClosestNPCPosition[npc.type]。
 * Main 上持有 _playerSceneMetrics / _cameraSceneMetrics 两个实例。 */
static void resize_scene_metrics(const int base, const int new_size) {
    patch_handle_t sm_cls = patchlib_type_get_type("Terraria", "SceneMetrics");
    patch_handle_t main_cls = patchlib_type_get_type("Terraria", "Main");
    if (!patchlib_is_valid(sm_cls) || !patchlib_is_valid(main_cls)) {
        if (patchlib_is_valid(sm_cls)) patchlib_free(sm_cls);
        if (patchlib_is_valid(main_cls)) patchlib_free(main_cls);
        return;
    }
    patch_handle_t f_pos = patchlib_type_get_field(sm_cls, "ClosestNPCPosition");
    static const char *inst_fields[] = {"_playerSceneMetrics",
                                        "_cameraSceneMetrics"};
    int done = 0;
    for (size_t i = 0; i < sizeof(inst_fields) / sizeof(inst_fields[0]); ++i) {
        patch_handle_t f_inst = patchlib_type_get_field(main_cls, inst_fields[i]);
        if (!patchlib_is_valid(f_inst)) continue;
        patch_handle_t inst = PATCH_NULL;
        patchlib_field_get_value(f_inst, NULL, &inst);
        patchlib_free(f_inst);
        if (!patchlib_is_valid(inst)) continue;
        patch_handle_t arr = PATCH_NULL;
        patchlib_field_get_value(f_pos, inst, &arr);
        if (patchlib_is_valid(arr) &&
            patchlib_array_length(arr) == (size_t) base) {
            patch_handle_t bigger =
                    patchlib_array_resize(arr, (size_t) new_size, NULL);
            if (patchlib_is_valid(bigger)) {
                patchlib_field_set_value(f_pos, inst, &bigger);
                done++;
                patchlib_free(bigger);
            }
        }
        if (patchlib_is_valid(arr)) patchlib_free(arr);
        patchlib_free(inst);
    }
    if (patchlib_is_valid(f_pos)) patchlib_free(f_pos);
    patchlib_free(sm_cls);
    patchlib_free(main_cls);
    TEKLOG_INFO("npc resize: SceneMetrics.ClosestNPCPosition %d instance(s)", done);
}

/* TownRoomManager._hasRoom 是实例字段、按 NPC type 索引；
 * 除了 .ctor 钩子覆盖的实例外，WorldGen.TownManager 是静态字段，
 * Mod 加载前就已经 new 好（Terraria/WorldGen.cs:4037），钩子管不到它。
 * WorldGen 里存在 for (j < NPCID.Count) { TownManager.HasRoomQuick(j); }
 * 的循环，桌面端 NPCID.Count 抬高后会直接越界。这里补扩现存实例；
 * 之后 WorldGen.ClearWorld（:6900）重建的实例仍由 .ctor 钩子处理。 */
static void resize_town_room_instance(const int base, const int new_size) {
    if (!patchlib_is_valid(f_has_room))
        return;
    patch_handle_t wg_cls = patchlib_type_get_type("Terraria", "WorldGen");
    if (!patchlib_is_valid(wg_cls))
        return;
    patch_handle_t f_tm = patchlib_type_get_field(wg_cls, "TownManager");
    patchlib_free(wg_cls);
    if (!patchlib_is_valid(f_tm))
        return;

    patch_handle_t tm = PATCH_NULL;
    patchlib_field_get_value(f_tm, NULL, &tm);
    patchlib_free(f_tm);
    if (!patchlib_is_valid(tm))
        return;

    patch_handle_t arr = PATCH_NULL;
    patchlib_field_get_value(f_has_room, tm, &arr);
    if (patchlib_is_valid(arr) && patchlib_array_length(arr) == (size_t) base) {
        patch_handle_t bigger =
                patchlib_array_resize(arr, (size_t) new_size, NULL);
        if (patchlib_is_valid(bigger)) {
            patchlib_field_set_value(f_has_room, tm, &bigger);
            patchlib_free(bigger);
            TEKLOG_INFO("npc resize: WorldGen.TownManager._hasRoom -> %d",
                        new_size);
        } else {
            TEKLOG_ERROR("npc resize: WorldGen.TownManager._hasRoom failed");
        }
    }
    if (patchlib_is_valid(arr)) patchlib_free(arr);
    patchlib_free(tm);
}

void terraria_npc_manager_resize(void) {
    const size_t custom = tefstd_vector_size(&g_terraria_npc_registry);
    if (custom == 0)
        return;
    const int base = g_npc_base;
    if (base <= 0)
        return;
    const int new_size = base + (int) custom;

    /* 1) TextureAssets.Npc[] */
    resize_static_array(f_texture_npc, "TextureAssets.Npc", base, new_size,
                        false, 0, false);

    /* 2) Main.npcFrameCount[]（新槽位必须 >= 1，否则绘制时除以 0） */
    resize_static_array(f_npc_frame_count, "Main.npcFrameCount", base, new_size,
                        false, 1, true);

    /* 3) NPCID.Sets.* */
    int resized = resize_sets_arrays("Terraria.ID", "NPCID", "Sets", base,
                                     new_size, true);
    TEKLOG_INFO("npc resize: NPCID.Sets %d array(s) resized (base=%d -> %d)",
                resized, base, new_size);

    /* 4) 粗暴 sweep：Main / NPC 上所有长度 == base 的静态数组
     *    （townNPCCanSpawn / slimeRainNPC / npcCatchable /
     *      ShimmeredTownNPCs / npcsFoundForCheckActive ... 都在里面） */
    static const char *main_excludes[] = {"npcFrameCount"};
    int main_done = resize_all_static_arrays("Terraria", "Main", base, new_size,
                                             main_excludes, 1, false);
    int npc_done = resize_all_static_arrays("Terraria", "NPC", base, new_size,
                                            NULL, 0, false);
    /* 其它按 NPC type 索引的静态表（众数填充，保留 Factory 默认值） */
    int emote_done = resize_all_static_arrays("Terraria.GameContent.UI",
                                              "EmoteBubble", base, new_size,
                                              NULL, 0, true);
    int dialogue_done = resize_all_static_arrays("Terraria.GameContent",
                                                 "ConditionalDialogue", base,
                                                 new_size, NULL, 0, true);
    int critter_done = resize_all_static_arrays(
            "Terraria.GameContent.Tile_Entities", "TECritterAnchor", base,
            new_size, NULL, 0, true);
    int dmg_done = resize_all_static_arrays("Terraria.GameContent",
                                            "NPCDamageTracker", base, new_size,
                                            NULL, 0, true);
    TEKLOG_INFO("npc resize: Main %d, NPC %d, EmoteBubble %d, ConditionalDialogue %d, TECritterAnchor %d, NPCDamageTracker %d",
                main_done, npc_done, emote_done, dialogue_done, critter_done,
                dmg_done);

    /* 5) Player.npcTypeNoAggro（每个玩家一份） */
    resize_player_arrays(base, new_size);

    /* 6) Lang._npcNameCache */
    resize_npc_name_cache(base, new_size);

    /* 7) SceneMetrics.ClosestNPCPosition */
    resize_scene_metrics(base, new_size);

    /* 8) WorldGen.TownManager._hasRoom（.ctor 钩子覆盖不到的既有实例） */
    resize_town_room_instance(base, new_size);

#if !defined(__ANDROID__)
    /* Main.DrawNPCs 的循环条件是 type < NPCID.Count；桌面端 Count 是
     * static readonly，可以运行时抬高。手机端是 const，改不了。 */
    short total = (short) new_size;
    patchlib_field_set_value(npc_id_count, NULL, &total);
    TEKLOG_INFO("npc resize: NPCID.Count %d -> %d (desktop, 供绘制)", base,
                new_size);
#endif
}

/* ------------------------------------------------------------------ */
/* 贴图                                                                */
/* ------------------------------------------------------------------ */

static patch_handle_t make_placeholder_texture(void) {
    static unsigned char px[16 * 16 * 4];
    for (int i = 0; i < 16 * 16; ++i) {
        px[i * 4 + 0] = 255;
        px[i * 4 + 1] = 0;
        px[i * 4 + 2] = 255;
        px[i * 4 + 3] = 255;
    }
    patch_handle_t tex = terraria_texture2d_create(16, 16, TEXTURE_FORMAT_RGBA32,
                                                   px, sizeof(px));
    if (!patchlib_is_valid(tex))
        return PATCH_NULL;
    patch_handle_t cls = terraria_texture2d_get_class();
    patch_handle_t asset = terraria_asset_create(cls, tex);
    if (patchlib_is_valid(cls)) patchlib_free(cls);
    if (patchlib_is_valid(tex)) patchlib_free(tex);
    return asset;
}

void terraria_npc_manager_init_texture2d(void) {
    const size_t custom = tefstd_vector_size(&g_terraria_npc_registry);
    if (custom == 0 || !patchlib_is_valid(f_texture_npc))
        return;

    patch_handle_t array = PATCH_NULL;
    patchlib_field_get_value(f_texture_npc, NULL, &array);
    if (!patchlib_is_valid(array))
        return;

    const size_t len = patchlib_array_length(array);
    patch_handle_t placeholder = PATCH_NULL;
    int done = 0;
    for (size_t i = 0; i < custom; ++i) {
        terraria_npc_handle_t **pp =
                tefstd_vector_at(&g_terraria_npc_registry, i);
        if (!pp || !*pp)
            continue;
        terraria_npc_handle_t *h = *pp;
        if (h->runtime_id < 0 || (size_t) h->runtime_id >= len)
            continue;

        patch_handle_t tex = PATCH_NULL;
        if (h->npc_ops.get_texture)
            tex = h->npc_ops.get_texture(h);
        if (!patchlib_is_valid(tex)) {
            if (!patchlib_is_valid(placeholder)) {
                placeholder = make_placeholder_texture();
                if (!patchlib_is_valid(placeholder))
                    break;
            }
            tex = placeholder;
        }
        const bool tex_ok = patchlib_array_set(array, (size_t) h->runtime_id, &tex);

        /* 读 Asset<T>.Value，确认不是 null Asset */
        patch_handle_t asset_value = PATCH_NULL;
        if (patchlib_is_valid(tex)) {
            patch_handle_t t2d = terraria_texture2d_get_class();
            if (patchlib_is_valid(t2d)) {
                patch_handle_t asset_cls = terraria_asset_get_generic_class(t2d);
                if (patchlib_is_valid(asset_cls)) {
                    asset_value = patchlib_type_get_field(asset_cls,
                                                          "<Value>k__BackingField");
                    patchlib_free(asset_cls);
                }
                patchlib_free(t2d);
            }
        }
        patch_handle_t value = PATCH_NULL;
        if (patchlib_is_valid(asset_value))
            patchlib_field_get_value(asset_value, tex, &value);
        if (patchlib_is_valid(asset_value)) patchlib_free(asset_value);

        TEKLOG_DEBUG("npc texture[%d]: asset=%p value=%p set=%d", h->runtime_id,
                     (void *) tex, (void *) value, (int) tex_ok);

        if (patchlib_is_valid(f_npc_frame_count)) {
            patch_handle_t frames = PATCH_NULL;
            patchlib_field_get_value(f_npc_frame_count, NULL, &frames);
            if (patchlib_is_valid(frames)) {
                int n = h->frames > 0 ? h->frames : 1;
                patchlib_array_set(frames, (size_t) h->runtime_id, &n);
                patchlib_free(frames);
            }
        }
        done++;
    }
    patchlib_free(array);
    TEKLOG_INFO("npc textures: %d set", done);
}

/* ------------------------------------------------------------------ */
/* 钩子                                                                */
/* ------------------------------------------------------------------ */

static terraria_npc_handle_t *custom_from_instance(patch_handle_t inst) {
    if (!inst || !patchlib_is_valid(f_type))
        return NULL;
    int type = 0;
    patchlib_field_get_value(f_type, inst, &type);
    const int nid = type - g_npc_base;
    const size_t count = tefstd_vector_size(&g_terraria_npc_registry);
    if (nid < 0 || nid >= (int) count)
        return NULL;
    terraria_npc_handle_t **pp =
            tefstd_vector_at(&g_terraria_npc_registry, (size_t) nid);
    return (pp && *pp) ? *pp : NULL;
}

/* NPC.SetDefaults(int Type, NPCSpawnParams) 后置 */
static void set_defaults_postfix(patch_handle_t this, void **args, void *result,
                                 const patch_method_signature_t *sig_info) {
    (void) result;
    (void) sig_info;
    if (!this || !args || !args[0] || !patchlib_is_valid(f_type))
        return;

    const int type = *(int *) args[0];
    const int nid = type - g_npc_base;
    const size_t count = tefstd_vector_size(&g_terraria_npc_registry);
    if (nid < 0 || nid >= (int) count)
        return;

    terraria_npc_handle_t **pp =
            tefstd_vector_at(&g_terraria_npc_registry, (size_t) nid);
    if (!pp || !*pp)
        return;
    terraria_npc_handle_t *h = *pp;
    TEKLOG_INFO("npc SetDefaults: type=%d nid=%d name=%s%s", type, nid,
                h->internal_name ? h->internal_name : "?",
                h->npc_ops.set_defaults ? "" : " (no defaults callback)");
    if (h->npc_ops.set_defaults)
        h->npc_ops.set_defaults(h, this);

    /* boss=true 的自定义 NPC：给一个 Boss 头像索引，小地图才画得出来 */
    if (patchlib_is_valid(f_npc_boss) && patchlib_is_valid(f_boss_head_textures)) {
        bool is_boss = false;
        patchlib_field_get_value(f_npc_boss, this, &is_boss);
        if (is_boss) {
            patch_handle_t arr = PATCH_NULL;
            patchlib_field_get_value(f_boss_head_textures, NULL, &arr);
            if (patchlib_is_valid(arr) &&
                patchlib_array_length(arr) > (size_t) type) {
                int cur = -1;
                patchlib_array_at(arr, (size_t) type, &cur);
                if (cur < 0) {
                    int head = 0;   /* 0 = 史莱姆王头像 */
                    patchlib_array_set(arr, (size_t) type, &head);
                    TEKLOG_INFO("npc boss head[%d] = %d", type, head);
                }
            }
            if (patchlib_is_valid(arr)) patchlib_free(arr);
        }
    }
}

/* TownRoomManager..ctor 后置：世界加载时会 new TownRoomManager()，
 * 这里补扩 _hasRoom（按 NPC type 索引的 bool[]）。 */
static void town_room_ctor_postfix(patch_handle_t this, void **args, void *result,
                                   const patch_method_signature_t *sig_info) {
    (void) args;
    (void) result;
    (void) sig_info;
    if (!this || !patchlib_is_valid(f_has_room)) return;
    patch_handle_t arr = PATCH_NULL;
    patchlib_field_get_value(f_has_room, this, &arr);
    if (!patchlib_is_valid(arr)) return;
    const int base = g_npc_base;
    const int new_size = base + (int) tefstd_vector_size(&g_terraria_npc_registry);
    if (patchlib_array_length(arr) == (size_t) base && new_size > base) {
        patch_handle_t bigger = patchlib_array_resize(arr, (size_t) new_size, NULL);
        if (patchlib_is_valid(bigger)) {
            patchlib_field_set_value(f_has_room, this, &bigger);
            patchlib_free(bigger);
            TEKLOG_INFO("npc resize: TownRoomManager._hasRoom -> %d", new_size);
        }
    }
    patchlib_free(arr);
}

/* NPC.NPCLoot() 后置：自定义 NPC 的掉落回调 */
static void npc_loot_postfix(patch_handle_t this, void **args, void *result,
                             const patch_method_signature_t *sig_info) {
    (void) args;
    (void) result;
    (void) sig_info;
    terraria_npc_handle_t *h = custom_from_instance(this);
    if (!h) return;
    int type = -1;
    if (patchlib_is_valid(f_type))
        patchlib_field_get_value(f_type, this, &type);
    TEKLOG_DEBUG("npc loot: type=%d on_death=%d", type,
                 h->npc_ops.on_death ? 1 : 0);
    if (h->npc_ops.on_death)
        h->npc_ops.on_death(h, this);
}

/* NPC.AI() 前置：自定义 NPC 默认跳过原版 AI */
static bool ai_prefix(patch_handle_t this, void **args,
                      const patch_method_signature_t *sig_info, void *result) {
    (void) args;
    (void) sig_info;
    (void) result;
    terraria_npc_handle_t *h = custom_from_instance(this);
    if (!h || !h->npc_ops.ai)
        return false;
    if (h->ai_after_vanilla)
        return false;

    h->npc_ops.ai(h, this);
    return true;
}

static void ai_postfix(patch_handle_t this, void **args, void *result,
                       const patch_method_signature_t *sig_info) {
    (void) args;
    (void) result;
    (void) sig_info;
    terraria_npc_handle_t *h = custom_from_instance(this);
    if (!h || !h->npc_ops.ai || !h->ai_after_vanilla)
        return;
    h->npc_ops.ai(h, this);
}

/* ------------------------------------------------------------------ */
/* 生成                                                                */
/* ------------------------------------------------------------------ */

/* 玩家 Update_NPCCollision 会做 npcTypeNoAggro[Main.npc[i].type]；
 * 玩家实例可能在 Main.Initialize 之后被重新创建，长度又回到 697。
 * 生成前对本地玩家补一次，避免每帧抛异常导致玩家不能移动。 */
static void ensure_local_player_aggro_array(void) {
    static patch_handle_t f_main = PATCH_NULL, f_players = PATCH_NULL;
    static patch_handle_t f_my = PATCH_NULL, f_no_aggro = PATCH_NULL;
    if (f_main == PATCH_NULL) {
        f_main = patchlib_type_get_type("Terraria", "Main");
        patch_handle_t pcls = patchlib_type_get_type("Terraria", "Player");
        if (patchlib_is_valid(f_main)) {
            f_players = patchlib_type_get_field(f_main, "player");
            f_my = patchlib_type_get_field(f_main, "myPlayer");
        }
        if (patchlib_is_valid(pcls))
            f_no_aggro = patchlib_type_get_field(pcls, "npcTypeNoAggro");
        if (patchlib_is_valid(pcls)) patchlib_free(pcls);
    }
    if (!patchlib_is_valid(f_players) || !patchlib_is_valid(f_my) ||
        !patchlib_is_valid(f_no_aggro))
        return;

    int my = 0;
    patchlib_field_get_value(f_my, NULL, &my);
    patch_handle_t players = PATCH_NULL;
    patchlib_field_get_value(f_players, NULL, &players);
    if (!patchlib_is_valid(players)) return;
    patch_handle_t player = PATCH_NULL;
    if (patchlib_array_at(players, (size_t) my, &player) && player) {
        patch_handle_t arr = PATCH_NULL;
        patchlib_field_get_value(f_no_aggro, player, &arr);
        const int base = g_npc_base;
        const int need = base + (int) tefstd_vector_size(&g_terraria_npc_registry);
        if (patchlib_is_valid(arr)) {
            const size_t len = patchlib_array_length(arr);
            TEKLOG_DEBUG("npc spawn: player npcTypeNoAggro len=%zu (need=%d)",
                         len, need);
            if ((int) len < need) {
                patch_handle_t bigger =
                        patchlib_array_resize(arr, (size_t) need, NULL);
                if (patchlib_is_valid(bigger)) {
                    patchlib_field_set_value(f_no_aggro, player, &bigger);
                    patchlib_free(bigger);
                    TEKLOG_INFO("npc spawn: npcTypeNoAggro grown -> %d", need);
                }
            }
            patchlib_free(arr);
        }
        patchlib_free(player);
    }
    patchlib_free(players);
}

int terraria_npc_manager_drop_item(const int item_id, const float x,
                                  const float y, const int count) {
    if (item_id <= 0) return -1;

    const int stack = count > 0 ? count : 1;

#if defined(__ANDROID__)
    /* 手机端 il2cpp 的 Nullable<Vector2> 走 libffi 时结构体布局不可靠，
     * 会把后面的 modifier / noBroadcast 挤错位，托管侧拿到野指针后崩溃。
     * 改用全标量的旧版重载（PE 独有）：
     *   NewItem(int X, int Y, int Width, int Height, int Type,
     *           int Stack, bool noBroadcast, int pfix, NewItemOwnership) */
    static patch_handle_t m_legacy_new_item = PATCH_NULL;
    if (!patchlib_is_valid(m_legacy_new_item)) {
        patch_handle_t cls = patchlib_type_get_type("Terraria", "Item");
        if (patchlib_is_valid(cls)) {
            /* 9 参有两个重载，用第一个参数名 "X" 挑出旧版 */
            const char *names[9] = {"X", NULL, NULL, NULL, NULL,
                                    NULL, NULL, NULL, NULL};
            patch_handle_t m = patchlib_type_get_method_by_param_names(
                    cls, "NewItem", 9, names);
            if (!patchlib_is_valid(m))
                m = patchlib_type_get_method_by_param_count(cls, "NewItem", 9);
            if (patchlib_is_valid(m)) {
                patch_method_signature_t sig;
                if (patchlib_method_get_signature(m, &sig)) {
                    const patch_type_t *t0 = tefstd_vector_at(&sig.arg_types, 0);
                    const int t0v = t0 ? (int) *t0 : -1;
                    patchlib_method_signature_free(&sig);
                    if (t0v == PATCH_INT32)
                        m_legacy_new_item = m;
                    else
                        TEKLOG_ERROR("npc drop: NewItem(9) arg0 type=%d (want INT32)",
                                     t0v);
                }
            }
            patchlib_free(cls);
        }
        if (!patchlib_is_valid(m_legacy_new_item)) {
            TEKLOG_ERROR("npc drop: legacy Item.NewItem(9) not found");
            return -1;
        }
    }

    int x_arg = (int) x;
    int y_arg = (int) y;
    int width = 0;
    int height = 0;
    int type = item_id;
    int st = stack;
    bool no_broadcast = false;
    int prefix = 0;
    int ownership = 0;  /* NewItemOwnership.None */
    void *args[9] = {&x_arg, &y_arg, &width,      &height,
                     &type,  &st,    &no_broadcast, &prefix, &ownership};
    int result = -1;
    TEKLOG_DEBUG("npc drop: invoke legacy NewItem item=%d x%d at (%d,%d)",
                 item_id, stack, x_arg, y_arg);
    const bool ok = patchlib_method_invoke_args(m_legacy_new_item, PATCH_NULL,
                                                &result, args);
    TEKLOG_INFO("npc drop: item=%d x%d at (%.0f,%.0f) -> ok=%d index=%d", item_id,
                stack, x, y, (int) ok, result);
    return ok ? result : -1;
#else
    static patch_handle_t m_new_item = PATCH_NULL;
    static patch_handle_t src_cls = PATCH_NULL;
    if (!patchlib_is_valid(m_new_item)) {
        patch_handle_t cls = patchlib_type_get_type("Terraria", "Item");
        if (patchlib_is_valid(cls)) {
            /* 用 12 参重载，避免和 9 参 (X,Y,W,H,...) 重载混淆：
             * (IEntitySource, X, Y, W, H, type, stack, noBroadcast,
             *  prefix, NewItemOwnership, Vector2?, NewItemModifier) */
            m_new_item = patchlib_type_get_method_by_param_count(cls, "NewItem", 12);
            patchlib_free(cls);
        }
        if (!patchlib_is_valid(m_new_item)) {
            TEKLOG_ERROR("npc drop: Item.NewItem(12) not found");
            return -1;
        }
    }
    if (!patchlib_is_valid(src_cls)) {
        /* EntitySource_Loot 没有无参构造，用 EntitySource_SpawnNPC（隐式无参） */
        src_cls = patchlib_type_get_type("Terraria.DataStructures",
                                         "EntitySource_SpawnNPC");
        if (!patchlib_is_valid(src_cls)) {
            TEKLOG_ERROR("npc drop: EntitySource_SpawnNPC not found");
            return -1;
        }
    }

    patch_handle_t source = patchlib_type_new_instance(src_cls);
    int xi_arg = (int) x;
    int yi_arg = (int) y;
    int w = 0;
    int h = 0;
    int type = item_id;
    int st = stack;
    bool no_broadcast = false;
    int prefix = 0;
    int ownership = 0;                 /* NewItemOwnership.None */
    unsigned char velocity[16] = {0};  /* Nullable<Vector2> = null */
    void *modifier = NULL;

    void *args[12] = {&source, &xi_arg, &yi_arg, &w,        &h,        &type,
                      &st,     &no_broadcast, &prefix, &ownership, velocity,
                      &modifier};
    int result = -1;
    TEKLOG_DEBUG("npc drop: invoke NewItem(12) item=%d x%d at (%.0f,%.0f)",
                 item_id, stack, x, y);
    const bool ok = patchlib_method_invoke_args(m_new_item, PATCH_NULL, &result, args);
    TEKLOG_INFO("npc drop: item=%d x%d at (%.0f,%.0f) -> ok=%d index=%d", item_id,
                stack, x, y, (int) ok, result);
    if (patchlib_is_valid(source)) patchlib_free(source);
    return ok ? result : -1;
#endif
}

int terraria_npc_manager_spawn(const int runtime_id, const float x,
                               const float y) {
    TEKLOG_DEBUG("npc spawn: enter id=%d at (%d,%d)", runtime_id, (int) x, (int) y);
    ensure_local_player_aggro_array();
    if (runtime_id < 0)
        return -1;

    static patch_handle_t new_npc = PATCH_NULL;
    static patch_handle_t source_cls = PATCH_NULL;
    if (!patchlib_is_valid(new_npc)) {
        patch_handle_t cls = patchlib_type_get_type("Terraria", "NPC");
        if (patchlib_is_valid(cls)) {
            new_npc = patchlib_type_get_method_by_param_count(cls, "NewNPC", 10);
            patchlib_free(cls);
        }
        if (!patchlib_is_valid(new_npc)) {
            TEKLOG_ERROR("npc spawn: NPC.NewNPC(10) not found");
            return -1;
        }
    }
    if (!patchlib_is_valid(source_cls)) {
        source_cls = patchlib_type_get_type("Terraria.DataStructures",
                                            "EntitySource_SpawnNPC");
        if (!patchlib_is_valid(source_cls)) {
            TEKLOG_ERROR("npc spawn: EntitySource_SpawnNPC not found");
            return -1;
        }
    }

    patch_handle_t source = patchlib_type_new_instance(source_cls);
    int xi = (int) x;
    int yi = (int) y;
    int type = runtime_id;
    int start = 0;
    float ai0 = 0.0f, ai1 = 0.0f, ai2 = 0.0f, ai3 = 0.0f;
    int target = 255;
    void *args[10] = {&source, &xi, &yi, &type, &start,
                      &ai0, &ai1, &ai2, &ai3, &target};
    int result = -1;
    const bool ok = patchlib_method_invoke_args(new_npc, PATCH_NULL, &result, args);
    TEKLOG_INFO("npc spawn: id=%d at (%d,%d) -> ok=%d index=%d", runtime_id,
                xi, yi, (int) ok, result);

    if (patchlib_is_valid(source)) patchlib_free(source);
    if (!ok)
        return -1;
    return result;
}

/* ------------------------------------------------------------------ */
/* 生命周期                                                            */
/* ------------------------------------------------------------------ */

void terraria_npc_manager_init(void) {
    tefstd_vector_init(&g_terraria_npc_registry, sizeof(terraria_npc_handle_t *));

    npc_class = patchlib_type_get_type("Terraria", "NPC");
    if (!patchlib_is_valid(npc_class)) {
        TEKLOG_ERROR("npc: Terraria.NPC not found");
        return;
    }

    f_type = patchlib_type_get_field(npc_class, "type");
    f_active = patchlib_type_get_field(npc_class, "active");
    f_npc_boss = patchlib_type_get_field(npc_class, "boss");
#if defined(__ANDROID__)
    f_npc_hide = patchlib_type_get_field(npc_class, "hide");
    f_npc_behind_tiles = patchlib_type_get_field(npc_class, "behindTiles");
#endif

    patch_handle_t id_cls = patchlib_type_get_type("Terraria.ID", "NPCID");
    if (patchlib_is_valid(id_cls)) {
        npc_id_count = patchlib_type_get_field(id_cls, "Count");
        short c = 0;
        patchlib_field_get_value(npc_id_count, NULL, &c);
        if (c > 0) g_npc_base = c;
        patch_handle_t sets = patchlib_type_get_inner_type(id_cls, "Sets");
        if (patchlib_is_valid(sets)) {
            f_boss_head_textures =
                    patchlib_type_get_field(sets, "BossHeadTextures");
            patchlib_free(sets);
        }
        patchlib_free(id_cls);
    }

    patch_handle_t tex_cls =
            patchlib_type_get_type("Terraria.GameContent", "TextureAssets");
    if (patchlib_is_valid(tex_cls)) {
        f_texture_npc = patchlib_type_get_field(tex_cls, "Npc");
        patchlib_free(tex_cls);
    }

    patch_handle_t main_cls = patchlib_type_get_type("Terraria", "Main");
    if (patchlib_is_valid(main_cls)) {
        f_npc_frame_count = patchlib_type_get_field(main_cls, "npcFrameCount");
#if defined(__ANDROID__)
        f_main_sprite_batch = patchlib_type_get_field(main_cls, "spriteBatch");
        /* 手机端 screenPosition 是静态属性，不是字段 */
        patch_handle_t sp_prop = patchlib_type_get_property(main_cls, "screenPosition");
        if (patchlib_is_valid(sp_prop)) {
            m_screen_pos_get = patchlib_property_get_get_method(sp_prop);
            patchlib_free(sp_prop);
        }
        f_main_npc_arr = patchlib_type_get_field(main_cls, "npc");
        m_draw_direct = patchlib_type_get_method_by_param_count(
                main_cls, "DrawNPCDirect", 4);
        patch_handle_t draw_npcs = patchlib_type_get_method_by_param_count(
                main_cls, "DrawNPCs", 1);
        if (patchlib_is_valid(draw_npcs) && patchlib_is_valid(m_draw_direct) &&
            patchlib_is_valid(f_main_sprite_batch) &&
            patchlib_is_valid(m_screen_pos_get) &&
            patchlib_is_valid(f_main_npc_arr))
            h_draw_npcs = patchlib_install_prepost_hook(draw_npcs, NULL,
                                                        draw_npcs_postfix);
        TEKLOG_INFO("npc: android DrawNPCs hook=%d drawDirect=%p sb=%p sp=%p arr=%p hide=%p behind=%p",
                    (int) h_draw_npcs, (void *) m_draw_direct,
                    (void *) f_main_sprite_batch, (void *) m_screen_pos_get,
                    (void *) f_main_npc_arr, (void *) f_npc_hide,
                    (void *) f_npc_behind_tiles);
#endif
        patchlib_free(main_cls);
    }

    patch_handle_t set_defaults = patchlib_type_get_method_by_param_count(
            npc_class, "SetDefaults", 2);
    if (patchlib_is_valid(set_defaults)) {
        h_set_defaults = patchlib_install_prepost_hook(set_defaults, NULL,
                                                       set_defaults_postfix);
        TEKLOG_INFO("npc: hooked NPC.SetDefaults (%d)", (int) h_set_defaults);
    } else {
        TEKLOG_ERROR("npc: NPC.SetDefaults(int,NPCSpawnParams) not found");
    }

    patch_handle_t trm_cls = patchlib_type_get_type("Terraria.GameContent",
                                                    "TownRoomManager");
    if (patchlib_is_valid(trm_cls)) {
        f_has_room = patchlib_type_get_field(trm_cls, "_hasRoom");
        patch_handle_t ctor = patchlib_type_get_method_by_param_count(
                trm_cls, ".ctor", 0);
        if (patchlib_is_valid(ctor))
            h_town_room_ctor = patchlib_install_prepost_hook(
                    ctor, NULL, town_room_ctor_postfix);
        patchlib_free(trm_cls);
    }

    patch_handle_t loot = patchlib_type_get_method_by_param_count(npc_class,
                                                                  "NPCLoot", 0);
    if (patchlib_is_valid(loot)) {
        h_npc_loot = patchlib_install_prepost_hook(loot, NULL, npc_loot_postfix);
        TEKLOG_INFO("npc: hooked NPC.NPCLoot (%d)", (int) h_npc_loot);
    }

    patch_handle_t ai = patchlib_type_get_method_by_param_count(npc_class, "AI", 0);
    if (patchlib_is_valid(ai)) {
        h_ai = patchlib_install_prepost_hook(ai, ai_prefix, ai_postfix);
        TEKLOG_INFO("npc: hooked NPC.AI (%d)", (int) h_ai);
    }

    TEKLOG_INFO("npc: init done (base=%d, custom=%zu)",
                g_npc_base,
                tefstd_vector_size(&g_terraria_npc_registry));
}

void terraria_npc_manager_destroy(void) {
    if (h_set_defaults != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(h_set_defaults);
        h_set_defaults = PATCH_HOOK_INVALID_ID;
    }
#if defined(__ANDROID__)
    if (h_draw_npcs != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(h_draw_npcs);
        h_draw_npcs = PATCH_HOOK_INVALID_ID;
    }
#endif
    if (h_npc_loot != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(h_npc_loot);
        h_npc_loot = PATCH_HOOK_INVALID_ID;
    }
    if (h_town_room_ctor != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(h_town_room_ctor);
        h_town_room_ctor = PATCH_HOOK_INVALID_ID;
    }
    if (h_ai != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(h_ai);
        h_ai = PATCH_HOOK_INVALID_ID;
    }
    tefstd_vector_destroy(&g_terraria_npc_registry);
}
