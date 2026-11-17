/*******************************************************************************
 * tefkernel - npc_manager
 * Copyright (C) 2026 eternalfuture-e38299
 *
 * 自定义 NPC：注册 / ID 分配 / 贴图 / SetDefaults / AI 分发 / 生成。
 * 结构与 projectile_manager 同构。
 *******************************************************************************/

#include "internal/terraria/content/npc_manager.h"
#include "internal/log.h"
#include "internal/terraria/content/item_manager.h"
#include "internal/terraria/entity.h"

#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/property.h"
#include "patchlib/struct/array.h"
#include "patchlib/struct/dictionary.h"
#include "patchlib/struct/string.h"
#include "patchlib/il2cpp_api.h"
#include "terraria/asset.h"
#include "terraria/language_manager.h"
#include "terraria/texture2d.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

tefstd_vector_t g_terraria_npc_registry;

static patch_handle_t npc_class = PATCH_NULL;
static patch_handle_t f_type = PATCH_NULL;
static patch_handle_t f_active = PATCH_NULL;

/* NPC.type 字段偏移（dump.cs 确认实际偏移为 0x164），初始化时尝试通过 IL2CPP API 动态获取 */
static int g_npc_type_offset = 0x164;

static inline int get_npc_type(void *instance) {
    if (!instance) return -1;
#if defined(__ANDROID__)
    /* Android 的 patch_handle_t 就是对象指针，可以直接按偏移读 */
    if (g_npc_type_offset >= 0) {
        return *(const int *)((const char *)instance + g_npc_type_offset);
    }
#endif
    /* 桌面端 patch_handle_t 是 GCHandle，必须先解引用成对象再读字段；
     * 直接算偏移会读到 GCHandle 结构里，fault addr 落在很小的地址上。 */
    int type = -1;
    if (patchlib_is_valid(f_type)) {
        patchlib_field_get_value(f_type, instance, &type);
    }
    return type;
}

static patch_handle_t npc_id_count = PATCH_NULL;       /* NPCID.Count (short) */
static patch_handle_t f_texture_npc = PATCH_NULL;      /* TextureAssets.Npc */
static patch_handle_t f_npc_frame_count = PATCH_NULL;  /* Main.npcFrameCount */
static patch_hook_id_t h_set_defaults = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_town_room_ctor = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_npc_loot = PATCH_HOOK_INVALID_ID;
static patch_handle_t f_has_room = PATCH_NULL;  /* TownRoomManager._hasRoom */
static patch_hook_id_t h_ai = PATCH_HOOK_INVALID_ID;
#if defined(__ANDROID__)
static patch_hook_id_t h_lang_get_npc_name = PATCH_HOOK_INVALID_ID;
#endif
static bool g_ids_assigned = false;

typedef struct npc_bestiary_record_t {
    const terraria_npc_handle_t *owner;
    terraria_npc_bestiary_t cfg;
    char **tags_copy;
    int tag_count;
    terraria_npc_loot_entry_t *drops_copy;
    char **drop_names_copy;
    int drop_count;
    char *banner_item_name_copy;
} npc_bestiary_record_t;

static tefstd_vector_t g_npc_bestiary_records;
static bool g_bestiary_records_inited = false;

static void free_bestiary_record(npc_bestiary_record_t *rec) {
    if (!rec) return;
    if (rec->tags_copy) {
        for (int i = 0; i < rec->tag_count; ++i) {
            free(rec->tags_copy[i]);
        }
        free(rec->tags_copy);
    }
    if (rec->drop_names_copy) {
        for (int i = 0; i < rec->drop_count; ++i) {
            free(rec->drop_names_copy[i]);
        }
        free(rec->drop_names_copy);
    }
    free(rec->drops_copy);
    free(rec->banner_item_name_copy);
    memset(rec, 0, sizeof(*rec));
}

static npc_bestiary_record_t *find_bestiary_record(const terraria_npc_handle_t *owner) {
    if (!g_bestiary_records_inited || !owner) return NULL;
    const size_t n = tefstd_vector_size(&g_npc_bestiary_records);
    for (size_t i = 0; i < n; ++i) {
        npc_bestiary_record_t *r = tefstd_vector_at(&g_npc_bestiary_records, i);
        if (r && r->owner == owner) return r;
    }
    return NULL;
}

static void remove_bestiary_record(const terraria_npc_handle_t *owner) {
    if (!g_bestiary_records_inited || !owner) return;
    for (size_t i = tefstd_vector_size(&g_npc_bestiary_records); i-- > 0;) {
        npc_bestiary_record_t *r = tefstd_vector_at(&g_npc_bestiary_records, i);
        if (r && r->owner == owner) {
            free_bestiary_record(r);
            tefstd_vector_erase(&g_npc_bestiary_records, i, NULL);
        }
    }
}

static char *dup_cstr(const char *s) {
    if (!s) return NULL;
    const size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

bool terraria_npc_manager_set_bestiary(int runtime_id,
                                       const terraria_npc_bestiary_t *bestiary) {
    terraria_npc_handle_t *h = terraria_npc_manager_get_npc(runtime_id);
    if (!h || !bestiary) return false;
    if (!g_bestiary_records_inited) {
        tefstd_vector_init(&g_npc_bestiary_records, sizeof(npc_bestiary_record_t));
        g_bestiary_records_inited = true;
    }

    npc_bestiary_record_t next;
    memset(&next, 0, sizeof(next));
    next.owner = h;
    next.cfg = *bestiary;
    next.cfg.tags = NULL;
    next.cfg.drops = NULL;
    next.banner_item_name_copy = dup_cstr(bestiary->banner_item_name);
    next.cfg.banner_item_name = next.banner_item_name_copy;

    if (bestiary->tags && bestiary->tag_count > 0) {
        next.tags_copy = (char **)calloc((size_t)bestiary->tag_count, sizeof(char *));
        if (next.tags_copy) {
            next.tag_count = bestiary->tag_count;
            for (int i = 0; i < bestiary->tag_count; ++i) {
                next.tags_copy[i] = dup_cstr(bestiary->tags[i]);
            }
            next.cfg.tags = (const char *const *)next.tags_copy;
        }
    }

    if (bestiary->drops && bestiary->drop_count > 0) {
        next.drops_copy = (terraria_npc_loot_entry_t *)calloc(
                (size_t)bestiary->drop_count, sizeof(terraria_npc_loot_entry_t));
        next.drop_names_copy = (char **)calloc(
                (size_t)bestiary->drop_count, sizeof(char *));
        if (next.drops_copy && next.drop_names_copy) {
            next.drop_count = bestiary->drop_count;
            for (int i = 0; i < bestiary->drop_count; ++i) {
                next.drops_copy[i] = bestiary->drops[i];
                next.drop_names_copy[i] = dup_cstr(bestiary->drops[i].item_name);
                next.drops_copy[i].item_name = next.drop_names_copy[i];
            }
            next.cfg.drops = next.drops_copy;
        }
    }

    npc_bestiary_record_t *existing = find_bestiary_record(h);
    if (existing) {
        free_bestiary_record(existing);
        *existing = next;
        return true;
    }
    return tefstd_vector_push_back(&g_npc_bestiary_records, &next);
}

/* 原版 NPCID.Count（697）。桌面端之后会把 NPCID.Count 抬高用于绘制，
 * 所以内核内部一律用这个缓存值做 base，避免 nid 计算被改掉。 */
static int g_npc_base = 697;

/* NPC.boss / NPCID.Sets.BossHeadTextures：boss=true 的自定义 NPC 在
 * 小地图/大地图上会画一个 Boss 头像（BossHeadTextures 默认 -1 = 不画）。 */
static patch_handle_t f_npc_boss = PATCH_NULL;
static patch_handle_t f_boss_head_textures = PATCH_NULL;
static patch_handle_t f_npc_head_boss = PATCH_NULL;

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
            remove_bestiary_record(*pp);
            const bool removed =
                    tefstd_vector_erase(&g_terraria_npc_registry, i, NULL);
            if (removed)
                terraria_npc_manager_assign_ids(g_npc_base);
            return removed;
        }
    }
    return false;
}

bool terraria_npc_manager_unregister_npc(terraria_npc_handle_t *npc_handle) {
    if (!npc_handle)
        return false;
    const size_t count = tefstd_vector_size(&g_terraria_npc_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_npc_handle_t **pp =
                tefstd_vector_at(&g_terraria_npc_registry, i);
        if (!pp || *pp != npc_handle)
            continue;
        remove_bestiary_record(npc_handle);
        const bool removed =
                tefstd_vector_erase(&g_terraria_npc_registry, i, NULL);
        if (removed && g_ids_assigned)
            terraria_npc_manager_assign_ids(g_npc_base);
        return removed;
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
    if (g_npc_base > 0)
        return g_npc_base;
    short count = 0;
    if (patchlib_is_valid(npc_id_count)) {
        patchlib_field_get_value(npc_id_count, NULL, &count);
        if (count > 0)
            g_npc_base = count;
    }
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

/* NPCID.Sets.Factory._size 是 SetFactory 创建新集合时使用的长度。
 * 它不会随 Sets 里的静态数组一起被扩到 NPCID.Count + custom，
 * 所以 Mod 之后用 Factory.Create*Set 自建集合仍只有基础长度。
 */
static void resize_npc_set_factory(const int new_size) {
    patch_handle_t npcid = patchlib_type_get_type("Terraria.ID", "NPCID");
    if (!patchlib_is_valid(npcid)) return;
    patch_handle_t sets = patchlib_type_get_inner_type(npcid, "Sets");
    patchlib_free(npcid);
    if (!patchlib_is_valid(sets)) return;

    patch_handle_t f_factory = patchlib_type_get_field(sets, "Factory");
    patchlib_free(sets);
    if (!patchlib_is_valid(f_factory)) return;

    patch_handle_t factory = PATCH_NULL;
    patchlib_field_get_value(f_factory, NULL, &factory);
    patchlib_free(f_factory);
    if (!patchlib_is_valid(factory)) return;

    patch_handle_t sf = patchlib_type_get_type("Terraria.ID", "SetFactory");
    if (!patchlib_is_valid(sf)) return;
    patch_handle_t f_size = patchlib_type_get_field(sf, "_size");
    patchlib_free(sf);
    if (!patchlib_is_valid(f_size)) return;

    int size = new_size;
    patchlib_field_set_value(f_size, factory, &size);
    patchlib_free(f_size);
    patchlib_free(factory);
    TEKLOG_INFO("npc resize: NPCID.Sets.Factory._size -> %d", new_size);
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

    /* 9) NPCID.Sets.Factory._size（Mod 后续自建按 type 索引的集合用） */
    resize_npc_set_factory(new_size);

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
    if (!inst)
        return NULL;
    const int type = get_npc_type(inst);
    const int base = terraria_npc_id_get_count();
    const int nid = type - base;
    const size_t count = tefstd_vector_size(&g_terraria_npc_registry);
    if (nid < 0 || nid >= (int) count)
        return NULL;
    terraria_npc_handle_t **pp =
            tefstd_vector_at(&g_terraria_npc_registry, (size_t) nid);
    return (pp && *pp) ? *pp : NULL;
}

/* 把自定义 Boss 头像 Asset 追加到 TextureAssets.NpcHeadBoss，
 * 返回新索引；失败返回 -1。 */
static int append_boss_head_asset(patch_handle_t asset) {
    if (!patchlib_is_valid(f_npc_head_boss) || !patchlib_is_valid(asset))
        return -1;

    patch_handle_t arr = PATCH_NULL;
    patchlib_field_get_value(f_npc_head_boss, NULL, &arr);
    if (!patchlib_is_valid(arr)) return -1;

    const size_t len = patchlib_array_length(arr);
    patch_handle_t bigger = patchlib_array_resize(arr, len + 1, NULL);
    if (!patchlib_is_valid(bigger)) {
        patchlib_free(arr);
        return -1;
    }
    patchlib_array_set(bigger, len, &asset);
    patchlib_field_set_value(f_npc_head_boss, NULL, &bigger);
    patchlib_free(bigger);
    patchlib_free(arr);
    return (int) len;
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
                    int head = 0;   /* 0 = 克苏鲁之眼头像（默认） */
                    if (h->npc_ops.get_boss_head) {
                        patch_handle_t asset = h->npc_ops.get_boss_head(h);
                        if (patchlib_is_valid(asset)) {
                            int custom = append_boss_head_asset(asset);
                            if (custom >= 0) head = custom;
                            patchlib_free(asset);
                        }
                    }
                    patchlib_array_set(arr, (size_t) type, &head);
                    TEKLOG_INFO("npc boss head[%d] = %d%s", type, head,
                                head > 0 ? " (custom)" : "");
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

/* 获取 Main.BestiaryTracker.Kills 实例（调用方负责释放返回句柄） */
static patch_handle_t get_bestiary_kills_tracker(void) {
    static patch_handle_t main_cls = PATCH_NULL;
    static patch_handle_t f_tracker = PATCH_NULL;
    static patch_handle_t tracker_cls = PATCH_NULL;
    static patch_handle_t f_kills = PATCH_NULL;

    if (!patchlib_is_valid(f_kills)) {
        main_cls = patchlib_type_get_type("Terraria", "Main");
        if (patchlib_is_valid(main_cls)) {
            f_tracker = patchlib_type_get_field(main_cls, "BestiaryTracker");
        }
        tracker_cls = patchlib_type_get_type(
                "Terraria.GameContent.Bestiary", "BestiaryUnlocksTracker");
        if (patchlib_is_valid(tracker_cls)) {
            f_kills = patchlib_type_get_field(tracker_cls, "Kills");
        }
    }
    if (!patchlib_is_valid(f_tracker) || !patchlib_is_valid(f_kills)) {
        return PATCH_NULL;
    }

    patch_handle_t tracker = PATCH_NULL;
    patchlib_field_get_value(f_tracker, PATCH_NULL, &tracker);
    if (!patchlib_is_valid(tracker)) {
        return PATCH_NULL;
    }
    patch_handle_t kills = PATCH_NULL;
    patchlib_field_get_value(f_kills, tracker, &kills);
    patchlib_free(tracker);
    return kills;
}

#if defined(__ANDROID__)
/* 手机端 Lang.GetNPCName 仅判断 netID < 697，超出就返回 Empty，导致自定义 NPC 名字不显示。 */
static bool lang_get_npc_name_prefix(patch_handle_t this, void **args,
                                     const patch_method_signature_t *sig_info,
                                     void *result) {
    (void)this;
    (void)sig_info;
    if (!args || !args[0] || !result) {
        return false;
    }
    const int net_id = *(const int *)args[0];
    if (net_id < g_npc_base) {
        return false;
    }

    terraria_npc_handle_t *h = terraria_npc_manager_get_npc(net_id);
    if (!h) {
        return false;
    }

    static patch_handle_t lang_cls = PATCH_NULL;
    static patch_handle_t f_cache = PATCH_NULL;
    if (!patchlib_is_valid(f_cache)) {
        lang_cls = patchlib_type_get_type("Terraria", "Lang");
        if (patchlib_is_valid(lang_cls)) {
            f_cache = patchlib_type_get_field(lang_cls, "_npcNameCache");
        }
    }

    patch_handle_t cache = PATCH_NULL;
    patch_handle_t text = PATCH_NULL;
    if (patchlib_is_valid(f_cache)) {
        patchlib_field_get_value(f_cache, PATCH_NULL, &cache);
    }
    if (patchlib_is_valid(cache) &&
        (size_t)net_id < patchlib_array_length(cache)) {
        patchlib_array_at(cache, (size_t)net_id, &text);
    }
    if (patchlib_is_valid(cache)) {
        patchlib_free(cache);
    }
    if (!patchlib_is_valid(text)) {
        char *key = terraria_entity_make_key(TEF_ENTITY(h), "npc_name");
        if (key) {
            text = terraria_language_manager_get_localized_text(key);
            free(key);
        }
    }
    if (!patchlib_is_valid(text)) {
        return false;
    }

    *(patch_handle_t *)result = text;
    return true;
}
#endif

void terraria_npc_manager_init_localized_text(void) {
    const size_t custom = tefstd_vector_size(&g_terraria_npc_registry);
    if (custom == 0) {
        return;
    }

    patch_handle_t lang_cls = patchlib_type_get_type("Terraria", "Lang");
    if (!patchlib_is_valid(lang_cls)) {
        return;
    }
    patch_handle_t f_cache = patchlib_type_get_field(lang_cls, "_npcNameCache");
    patch_handle_t cache = PATCH_NULL;
    if (patchlib_is_valid(f_cache)) {
        patchlib_field_get_value(f_cache, PATCH_NULL, &cache);
    }
    if (patchlib_is_valid(cache)) {
        const size_t cache_len = patchlib_array_length(cache);
        int applied = 0;
        for (size_t i = 0; i < custom; ++i) {
            terraria_npc_handle_t **pp =
                    tefstd_vector_at(&g_terraria_npc_registry, i);
            if (!pp || !*pp || (*pp)->runtime_id < 0 ||
                (size_t)(*pp)->runtime_id >= cache_len) {
                continue;
            }
            char *key = terraria_entity_make_key(TEF_ENTITY(*pp), "npc_name");
            if (!key) {
                continue;
            }
            patch_handle_t lt = terraria_language_manager_get_localized_text(key);
            if (patchlib_is_valid(lt)) {
                patchlib_array_set(cache, (size_t)(*pp)->runtime_id, &lt);
                patchlib_free(lt);
                applied++;
            }
            free(key);
        }
        TEKLOG_INFO("npc l10n: %d custom NPC name(s) written to Lang._npcNameCache",
                    applied);
        patchlib_free(cache);
    }
    if (patchlib_is_valid(f_cache)) {
        patchlib_free(f_cache);
    }
    patchlib_free(lang_cls);
}

static patch_handle_t find_bestiary_spawn_tag(patch_handle_t *tag_groups,
                                              size_t group_count,
                                              const char *name) {
    if (!tag_groups || !name || !*name) {
        return PATCH_NULL;
    }
    for (size_t i = 0; i < group_count; ++i) {
        if (!patchlib_is_valid(tag_groups[i])) {
            continue;
        }
        patch_handle_t f = patchlib_type_get_field(tag_groups[i], name);
        if (!patchlib_is_valid(f)) {
            continue;
        }
        patch_handle_t elem = PATCH_NULL;
        patchlib_field_get_value(f, PATCH_NULL, &elem);
        patchlib_free(f);
        if (patchlib_is_valid(elem)) {
            return elem;
        }
    }
    return PATCH_NULL;
}

void terraria_npc_manager_init_bestiary(void) {
    const size_t custom = tefstd_vector_size(&g_terraria_npc_registry);
    if (custom == 0) {
        return;
    }

    patch_handle_t cs_cls = patchlib_type_get_type("Terraria.ID", "ContentSamples");
    patch_handle_t main_cls = patchlib_type_get_type("Terraria", "Main");
    patch_handle_t be_cls = patchlib_type_get_type(
            "Terraria.GameContent.Bestiary", "BestiaryEntry");
    patch_handle_t db_cls = patchlib_type_get_type(
            "Terraria.GameContent.Bestiary", "BestiaryDatabase");
    patch_handle_t prov_cls = patchlib_type_get_type(
            "Terraria.GameContent.Bestiary", "CommonEnemyUICollectionInfoProvider");
    patch_handle_t flavor_cls = patchlib_type_get_type(
            "Terraria.GameContent.Bestiary", "FlavorTextBestiaryInfoElement");
    patch_handle_t idb_cls = patchlib_type_get_type(
            "Terraria.GameContent.ItemDropRules", "ItemDropDatabase");
    patch_handle_t drop_cls = patchlib_type_get_type(
            "Terraria.GameContent.ItemDropRules", "CommonDrop");

    if (!patchlib_is_valid(cs_cls) || !patchlib_is_valid(main_cls) ||
        !patchlib_is_valid(be_cls) || !patchlib_is_valid(db_cls)) {
        TEKLOG_WARN("npc bestiary: core Bestiary types missing");
        return;
    }

    patch_handle_t f_npcs_by_id = patchlib_type_get_field(cs_cls, "NpcsByNetId");
    patch_handle_t f_persist_by_id =
            patchlib_type_get_field(cs_cls, "NpcPersistentIdsByNetIds");
    patch_handle_t f_credit_by_id =
            patchlib_type_get_field(cs_cls, "NpcBestiaryCreditIdsByNpcNetIds");
    patch_handle_t f_id_by_persist =
            patchlib_type_get_field(cs_cls, "NpcNetIdsByPersistentIds");
    patch_handle_t f_stars =
            patchlib_type_get_field(cs_cls, "NpcBestiaryRarityStars");
    patch_handle_t f_sorting =
            patchlib_type_get_field(cs_cls, "NpcBestiarySortingId");
    patch_handle_t m_rebuild_sorting = patchlib_type_get_method_by_param_count(
            cs_cls, "RebuildBestiarySortingIDsByBestiaryDatabaseContents", 1);

    patch_handle_t dict_npcs = PATCH_NULL, t_npcs = PATCH_NULL;
    patch_handle_t dict_persist = PATCH_NULL, t_persist = PATCH_NULL;
    patch_handle_t dict_credit = PATCH_NULL, t_credit = PATCH_NULL;
    patch_handle_t dict_rev = PATCH_NULL, t_rev = PATCH_NULL;
    patch_handle_t dict_stars = PATCH_NULL, t_stars = PATCH_NULL;
    patch_handle_t dict_sorting = PATCH_NULL, t_sorting = PATCH_NULL;

    if (patchlib_is_valid(f_npcs_by_id)) {
        patchlib_field_get_value(f_npcs_by_id, PATCH_NULL, &dict_npcs);
        t_npcs = patchlib_field_get_type_handle(f_npcs_by_id);
    }
    if (patchlib_is_valid(f_persist_by_id)) {
        patchlib_field_get_value(f_persist_by_id, PATCH_NULL, &dict_persist);
        t_persist = patchlib_field_get_type_handle(f_persist_by_id);
    }
    if (patchlib_is_valid(f_credit_by_id)) {
        patchlib_field_get_value(f_credit_by_id, PATCH_NULL, &dict_credit);
        t_credit = patchlib_field_get_type_handle(f_credit_by_id);
    }
    if (patchlib_is_valid(f_id_by_persist)) {
        patchlib_field_get_value(f_id_by_persist, PATCH_NULL, &dict_rev);
        t_rev = patchlib_field_get_type_handle(f_id_by_persist);
    }
    if (patchlib_is_valid(f_stars)) {
        patchlib_field_get_value(f_stars, PATCH_NULL, &dict_stars);
        t_stars = patchlib_field_get_type_handle(f_stars);
    }
    if (patchlib_is_valid(f_sorting)) {
        patchlib_field_get_value(f_sorting, PATCH_NULL, &dict_sorting);
        t_sorting = patchlib_field_get_type_handle(f_sorting);
    }

    patch_handle_t f_bestiary_db = patchlib_type_get_field(main_cls, "BestiaryDB");
    patch_handle_t bestiary_db = PATCH_NULL;
    if (patchlib_is_valid(f_bestiary_db)) {
        patchlib_field_get_value(f_bestiary_db, PATCH_NULL, &bestiary_db);
    }
    if (!patchlib_is_valid(bestiary_db)) {
        TEKLOG_WARN("npc bestiary: Main.BestiaryDB is null");
        return;
    }

    patch_handle_t npc_ctor =
            patchlib_type_get_method_by_param_count(npc_class, ".ctor", 0);
    patch_handle_t f_net_id = patchlib_type_get_field(npc_class, "netID");

    patch_handle_t m_enemy = patchlib_type_get_method_by_param_count(be_cls, "Enemy", 1);
    patch_handle_t m_critter = patchlib_type_get_method_by_param_count(be_cls, "Critter", 1);
    patch_handle_t m_town = patchlib_type_get_method_by_param_count(be_cls, "TownNPC", 1);
    patch_handle_t m_get_info = patchlib_type_get_method_by_param_count(be_cls, "get_Info", 0);
    patch_handle_t f_ui_provider = patchlib_type_get_field(be_cls, "UIInfoProvider");

    patch_handle_t m_db_register =
            patchlib_type_get_method_by_param_count(db_cls, "Register", 1);
    patch_handle_t m_db_extract =
            patchlib_type_get_method_by_param_count(db_cls, "ExtractDropsForNPC", 2);

    patch_handle_t m_prov_ctor = patchlib_is_valid(prov_cls)
            ? patchlib_type_get_method_by_param_count(prov_cls, ".ctor", 2) : PATCH_NULL;
    patch_handle_t m_flavor_ctor = patchlib_is_valid(flavor_cls)
            ? patchlib_type_get_method_by_param_count(flavor_cls, ".ctor", 1) : PATCH_NULL;

    patch_handle_t m_idb_ctor = patchlib_is_valid(idb_cls)
            ? patchlib_type_get_method_by_param_count(idb_cls, ".ctor", 0) : PATCH_NULL;
    patch_handle_t m_idb_reg = patchlib_is_valid(idb_cls)
            ? patchlib_type_get_method_by_param_count(idb_cls, "RegisterToNPC", 2) : PATCH_NULL;
    patch_handle_t m_drop_ctor = patchlib_is_valid(drop_cls)
            ? patchlib_type_get_method_by_param_count(drop_cls, ".ctor", 5) : PATCH_NULL;

    patch_method_signature_t info_sig;
    const bool info_sig_ok = patchlib_is_valid(m_get_info) &&
            patchlib_method_get_signature(m_get_info, &info_sig);
    patch_handle_t m_info_add = (info_sig_ok && patchlib_is_valid(info_sig.return_type_handle))
            ? patchlib_type_get_method_by_param_count(info_sig.return_type_handle, "Add", 1)
            : PATCH_NULL;

    /* 收集 SpawnConditions 下的标签静态组（Biomes / Times / Events / Invasions / Visuals） */
    patch_handle_t tag_groups[5] = {PATCH_NULL, PATCH_NULL, PATCH_NULL, PATCH_NULL, PATCH_NULL};
    static const char *const tag_group_names[5] = {
        "Biomes", "Times", "Events", "Invasions", "Visuals"
    };
    patch_handle_t pop_cls = patchlib_type_get_type(
            "Terraria.GameContent.Bestiary", "BestiaryDatabaseNPCsPopulator");
    patch_handle_t common_tags_cls = patchlib_is_valid(pop_cls)
            ? patchlib_type_get_inner_type(pop_cls, "CommonTags") : PATCH_NULL;
    patch_handle_t spawn_cond_cls = patchlib_is_valid(common_tags_cls)
            ? patchlib_type_get_inner_type(common_tags_cls, "SpawnConditions")
            : PATCH_NULL;
    if (patchlib_is_valid(spawn_cond_cls)) {
        for (size_t gi = 0; gi < 5; ++gi) {
            tag_groups[gi] = patchlib_type_get_inner_type(
                    spawn_cond_cls, tag_group_names[gi]);
#if defined(__ANDROID__)
            if (patchlib_is_valid(tag_groups[gi]) && il2cpp_runtime_class_init) {
                il2cpp_runtime_class_init((void *)tag_groups[gi]);
            }
#endif
        }
    }

    int registered_entries = 0;
    for (size_t i = 0; i < custom; ++i) {
        terraria_npc_handle_t **pp = tefstd_vector_at(&g_terraria_npc_registry, i);
        if (!pp || !*pp || (*pp)->runtime_id < g_npc_base) {
            continue;
        }
        terraria_npc_handle_t *h = *pp;
        int id = h->runtime_id;
        npc_bestiary_record_t *rec = find_bestiary_record(h);
        terraria_npc_bestiary_t *cfg = rec ? &rec->cfg : NULL;

        /* 延后解析同 Mod 旗帜物品 id */
        if (cfg && cfg->banner_item_id <= 0 && cfg->banner_item_name) {
            terraria_item_handle_t *banner = terraria_item_manager_find_item(
                    h->parent_modloader_id, h->parent_id, cfg->banner_item_name);
            if (banner && banner->runtime_id > 0) {
                cfg->banner_item_id = banner->runtime_id;
            }
        }

        const char *loader = h->parent_modloader_id ? h->parent_modloader_id : "?";
        const char *mod = h->parent_id ? h->parent_id : "?";
        const char *name = h->internal_name ? h->internal_name : "?";
        const size_t plen = strlen(loader) + 1 + strlen(mod) + 1 + strlen(name) + 1;
        char *persist_c = (char *)malloc(plen);
        if (!persist_c) {
            continue;
        }
        snprintf(persist_c, plen, "%s:%s:%s", loader, mod, name);
        patch_handle_t persist_str = patchlib_string_create(persist_c);
        free(persist_c);

        if (patchlib_is_valid(persist_str)) {
            if (patchlib_is_valid(dict_persist) && patchlib_is_valid(t_persist)) {
                patchlib_dictionary_set_value_typed(
                        dict_persist, t_persist, &id, &persist_str);
            }
            if (patchlib_is_valid(dict_credit) && patchlib_is_valid(t_credit)) {
                patchlib_dictionary_set_value_typed(
                        dict_credit, t_credit, &id, &persist_str);
            }
            if (patchlib_is_valid(dict_rev) && patchlib_is_valid(t_rev)) {
                patchlib_dictionary_set_value_typed(
                        dict_rev, t_rev, &persist_str, &id);
            }
        }

        int stars = (cfg && cfg->stars > 0) ? cfg->stars : 1;
        if (stars < 1) stars = 1;
        if (stars > 5) stars = 5;
        if (patchlib_is_valid(dict_stars) && patchlib_is_valid(t_stars)) {
            patchlib_dictionary_set_value_typed(dict_stars, t_stars, &id, &stars);
        }

        if (cfg && cfg->exclude) {
            if (patchlib_is_valid(dict_sorting) && patchlib_is_valid(t_sorting)) {
                int zero_sort = 0;
                patchlib_dictionary_set_value_typed(
                        dict_sorting, t_sorting, &id, &zero_sort);
            }
            if (patchlib_is_valid(persist_str)) {
                patchlib_free(persist_str);
            }
            continue;
        }

        /* 构造 ContentSamples.NpcsByNetId[id]（不依赖 NPCSpawnParams 结构体参数） */
        patch_handle_t sample_npc = PATCH_NULL;
        if (patchlib_is_valid(npc_ctor)) {
            patchlib_constructor_invoke(npc_ctor, &sample_npc, NULL);
        } else {
            sample_npc = patchlib_type_new_instance(npc_class);
        }
        if (patchlib_is_valid(sample_npc)) {
            if (patchlib_is_valid(f_type)) {
                patchlib_field_set_value(f_type, sample_npc, &id);
            }
            if (patchlib_is_valid(f_net_id)) {
                patchlib_field_set_value(f_net_id, sample_npc, &id);
            }
            if (h->npc_ops.set_defaults) {
                h->npc_ops.set_defaults(h, sample_npc);
            }
            if (patchlib_is_valid(dict_npcs) && patchlib_is_valid(t_npcs)) {
                patchlib_dictionary_set_value_typed(
                        dict_npcs, t_npcs, &id, &sample_npc);
            }
            patchlib_free(sample_npc);
        }

        patch_handle_t m_factory = m_enemy;
        if (cfg && cfg->kind == TERRARIA_NPC_BESTIARY_CRITTER &&
            patchlib_is_valid(m_critter)) {
            m_factory = m_critter;
        } else if (cfg && cfg->kind == TERRARIA_NPC_BESTIARY_TOWN_NPC &&
                   patchlib_is_valid(m_town)) {
            m_factory = m_town;
        }

        patch_handle_t entry = PATCH_NULL;
        if (patchlib_is_valid(m_factory)) {
            void *eargs[1] = {&id};
            patchlib_method_invoke_args(m_factory, PATCH_NULL, &entry, eargs);
        }

        if (patchlib_is_valid(entry)) {
            if (cfg && cfg->quick_unlock && patchlib_is_valid(persist_str) &&
                patchlib_is_valid(m_prov_ctor) &&
                patchlib_is_valid(f_ui_provider)) {
                patch_handle_t provider = PATCH_NULL;
                bool quick = true;
                void *pargs[2] = {&persist_str, &quick};
                if (patchlib_constructor_invoke(m_prov_ctor, &provider, pargs) &&
                    patchlib_is_valid(provider)) {
                    patchlib_field_set_value(f_ui_provider, entry, &provider);
                    patchlib_free(provider);
                }
            }

            patch_handle_t info_list = PATCH_NULL;
            if (patchlib_is_valid(m_get_info)) {
                patchlib_method_invoke_args(m_get_info, entry, &info_list, NULL);
            }
            if (patchlib_is_valid(info_list) && patchlib_is_valid(m_info_add)) {
                if (cfg && cfg->tags && cfg->tag_count > 0) {
                    for (int ti = 0; ti < cfg->tag_count; ++ti) {
                        patch_handle_t tag_elem = find_bestiary_spawn_tag(
                                tag_groups, 5, cfg->tags[ti]);
                        if (patchlib_is_valid(tag_elem)) {
                            void *aargs[1] = {&tag_elem};
                            patchlib_method_invoke_args(
                                    m_info_add, info_list, NULL, aargs);
                            patchlib_free(tag_elem);
                        } else {
                            TEKLOG_WARN("npc bestiary: unknown spawn tag '%s' on %s",
                                        cfg->tags[ti] ? cfg->tags[ti] : "?", name);
                        }
                    }
                }

                if (cfg && cfg->has_flavor_text && patchlib_is_valid(m_flavor_ctor)) {
                    char *fkey = terraria_entity_make_key(TEF_ENTITY(h), "npc_bestiary");
                    if (fkey) {
                        patch_handle_t fstr = patchlib_string_create(fkey);
                        free(fkey);
                        if (patchlib_is_valid(fstr)) {
                            patch_handle_t flavor = PATCH_NULL;
                            void *fargs[1] = {&fstr};
                            if (patchlib_constructor_invoke(
                                        m_flavor_ctor, &flavor, fargs) &&
                                patchlib_is_valid(flavor)) {
                                void *aargs[1] = {&flavor};
                                patchlib_method_invoke_args(
                                        m_info_add, info_list, NULL, aargs);
                                patchlib_free(flavor);
                            }
                            patchlib_free(fstr);
                        }
                    }
                }
            }
            if (patchlib_is_valid(info_list)) {
                patchlib_free(info_list);
            }

            if (patchlib_is_valid(m_db_register)) {
                void *rargs[1] = {&entry};
                patchlib_method_invoke_args(m_db_register, bestiary_db, NULL, rargs);
                registered_entries++;
            }

            if (cfg && cfg->drops && cfg->drop_count > 0 &&
                patchlib_is_valid(m_idb_ctor) && patchlib_is_valid(m_idb_reg) &&
                patchlib_is_valid(m_drop_ctor) && patchlib_is_valid(m_db_extract)) {
                patch_handle_t temp_db = PATCH_NULL;
                if (patchlib_constructor_invoke(m_idb_ctor, &temp_db, NULL) &&
                    patchlib_is_valid(temp_db)) {
                    int added_drops = 0;
                    for (int di = 0; di < cfg->drop_count; ++di) {
                        int item_id = cfg->drops[di].item_id;
                        if (item_id <= 0 && cfg->drops[di].item_name) {
                            terraria_item_handle_t *dit =
                                    terraria_item_manager_find_item(
                                            h->parent_modloader_id, h->parent_id,
                                            cfg->drops[di].item_name);
                            if (dit && dit->runtime_id > 0) {
                                item_id = dit->runtime_id;
                            }
                        }
                        if (item_id <= 0) {
                            continue;
                        }
                        int denom = cfg->drops[di].chance_denominator > 0
                                ? cfg->drops[di].chance_denominator : 1;
                        int min_st = cfg->drops[di].min_stack > 0
                                ? cfg->drops[di].min_stack : 1;
                        int max_st = cfg->drops[di].max_stack >= min_st
                                ? cfg->drops[di].max_stack : min_st;
                        int numer = cfg->drops[di].chance_numerator > 0
                                ? cfg->drops[di].chance_numerator : 1;

                        patch_handle_t rule = PATCH_NULL;
                        void *dargs[5] = {&item_id, &denom, &min_st, &max_st, &numer};
                        if (patchlib_constructor_invoke(m_drop_ctor, &rule, dargs) &&
                            patchlib_is_valid(rule)) {
                            void *reg_args[2] = {&id, &rule};
                            patchlib_method_invoke_args(
                                    m_idb_reg, temp_db, NULL, reg_args);
                            patchlib_free(rule);
                            added_drops++;
                        }
                    }
                    if (added_drops > 0) {
                        void *ex_args[2] = {&temp_db, &id};
                        patchlib_method_invoke_args(
                                m_db_extract, bestiary_db, NULL, ex_args);
                    }
                    patchlib_free(temp_db);
                }
            }
            patchlib_free(entry);
        }

        if (patchlib_is_valid(persist_str)) {
            patchlib_free(persist_str);
        }
    }

    if (patchlib_is_valid(m_rebuild_sorting)) {
        void *sargs[1] = {&bestiary_db};
        patchlib_method_invoke_args(m_rebuild_sorting, PATCH_NULL, NULL, sargs);
    }

    TEKLOG_INFO("npc bestiary: registered %d custom entry(s)", registered_entries);

    if (patchlib_is_valid(m_info_add)) patchlib_free(m_info_add);
    if (info_sig_ok) patchlib_method_signature_free(&info_sig);
    for (size_t gi = 0; gi < 5; ++gi) {
        if (patchlib_is_valid(tag_groups[gi])) patchlib_free(tag_groups[gi]);
    }
    if (patchlib_is_valid(spawn_cond_cls)) patchlib_free(spawn_cond_cls);
    if (patchlib_is_valid(common_tags_cls)) patchlib_free(common_tags_cls);
    if (patchlib_is_valid(pop_cls)) patchlib_free(pop_cls);

    if (patchlib_is_valid(m_drop_ctor)) patchlib_free(m_drop_ctor);
    if (patchlib_is_valid(m_idb_reg)) patchlib_free(m_idb_reg);
    if (patchlib_is_valid(m_idb_ctor)) patchlib_free(m_idb_ctor);
    if (patchlib_is_valid(m_flavor_ctor)) patchlib_free(m_flavor_ctor);
    if (patchlib_is_valid(m_prov_ctor)) patchlib_free(m_prov_ctor);
    if (patchlib_is_valid(m_db_extract)) patchlib_free(m_db_extract);
    if (patchlib_is_valid(m_db_register)) patchlib_free(m_db_register);
    if (patchlib_is_valid(f_ui_provider)) patchlib_free(f_ui_provider);
    if (patchlib_is_valid(m_get_info)) patchlib_free(m_get_info);
    if (patchlib_is_valid(m_town)) patchlib_free(m_town);
    if (patchlib_is_valid(m_critter)) patchlib_free(m_critter);
    if (patchlib_is_valid(m_enemy)) patchlib_free(m_enemy);
    if (patchlib_is_valid(f_net_id)) patchlib_free(f_net_id);
    if (patchlib_is_valid(npc_ctor)) patchlib_free(npc_ctor);
    patchlib_free(bestiary_db);
    if (patchlib_is_valid(f_bestiary_db)) patchlib_free(f_bestiary_db);

    if (patchlib_is_valid(dict_npcs)) patchlib_free(dict_npcs);
    if (patchlib_is_valid(dict_persist)) patchlib_free(dict_persist);
    if (patchlib_is_valid(dict_credit)) patchlib_free(dict_credit);
    if (patchlib_is_valid(dict_rev)) patchlib_free(dict_rev);
    if (patchlib_is_valid(dict_stars)) patchlib_free(dict_stars);
    if (patchlib_is_valid(dict_sorting)) patchlib_free(dict_sorting);
    if (patchlib_is_valid(t_npcs)) patchlib_free(t_npcs);
    if (patchlib_is_valid(t_persist)) patchlib_free(t_persist);
    if (patchlib_is_valid(t_credit)) patchlib_free(t_credit);
    if (patchlib_is_valid(t_rev)) patchlib_free(t_rev);
    if (patchlib_is_valid(t_stars)) patchlib_free(t_stars);
    if (patchlib_is_valid(t_sorting)) patchlib_free(t_sorting);

    if (patchlib_is_valid(m_rebuild_sorting)) patchlib_free(m_rebuild_sorting);
    if (patchlib_is_valid(f_sorting)) patchlib_free(f_sorting);
    if (patchlib_is_valid(f_stars)) patchlib_free(f_stars);
    if (patchlib_is_valid(f_id_by_persist)) patchlib_free(f_id_by_persist);
    if (patchlib_is_valid(f_credit_by_id)) patchlib_free(f_credit_by_id);
    if (patchlib_is_valid(f_persist_by_id)) patchlib_free(f_persist_by_id);
    if (patchlib_is_valid(f_npcs_by_id)) patchlib_free(f_npcs_by_id);

    if (patchlib_is_valid(drop_cls)) patchlib_free(drop_cls);
    if (patchlib_is_valid(idb_cls)) patchlib_free(idb_cls);
    if (patchlib_is_valid(flavor_cls)) patchlib_free(flavor_cls);
    if (patchlib_is_valid(prov_cls)) patchlib_free(prov_cls);
    patchlib_free(db_cls);
    patchlib_free(be_cls);
    patchlib_free(main_cls);
    patchlib_free(cs_cls);
}

/* NPC.NPCLoot() 后置：自定义 NPC 的击杀统计 / 旗帜 / 掉落回调 */
static void npc_loot_postfix(patch_handle_t this, void **args, void *result,
                             const patch_method_signature_t *sig_info) {
    (void) args;
    (void) result;
    (void) sig_info;
    terraria_npc_handle_t *h = custom_from_instance(this);
    if (!h) return;

    npc_bestiary_record_t *rec = find_bestiary_record(h);
    const bool excluded = rec && rec->cfg.exclude;

    static patch_handle_t kills_cls = PATCH_NULL;
#if defined(__ANDROID__)
    static patch_handle_t m_reg_kill = PATCH_NULL;
#endif
    static patch_handle_t m_get_kills = PATCH_NULL;
    static patch_handle_t entity_cls = PATCH_NULL;
    static patch_handle_t f_entity_pos = PATCH_NULL;
    if (!patchlib_is_valid(kills_cls)) {
        kills_cls = patchlib_type_get_type(
                "Terraria.GameContent.Bestiary", "NPCKillsTracker");
        if (patchlib_is_valid(kills_cls)) {
#if defined(__ANDROID__)
            m_reg_kill = patchlib_type_get_method_by_param_count(
                    kills_cls, "RegisterKill", 1);
#endif
            m_get_kills = patchlib_type_get_method_by_param_types(
                    kills_cls, "GetKillCount", 1, &npc_class);
        }
        entity_cls = patchlib_type_get_type("Terraria", "Entity");
        if (patchlib_is_valid(entity_cls)) {
            f_entity_pos = patchlib_type_get_field(entity_cls, "position");
        }
    }

    if (!excluded) {
        patch_handle_t kills_tracker = get_bestiary_kills_tracker();
        if (patchlib_is_valid(kills_tracker)) {
#if defined(__ANDROID__)
            /* 手机端 NPC.NPCLoot 在 type >= 697 时提前返回，跳过 RegisterKill(this) */
            if (patchlib_is_valid(m_reg_kill)) {
                void *kargs[1] = {&this};
                patchlib_method_invoke_args(m_reg_kill, kills_tracker, NULL, kargs);
            }
#endif
            if (rec && rec->cfg.banner_item_id > 0 &&
                patchlib_is_valid(m_get_kills)) {
                int count = 0;
                void *qargs[1] = {&this};
                if (patchlib_method_invoke_args(
                            m_get_kills, kills_tracker, &count, qargs)) {
                    const int step = rec->cfg.kills_to_banner > 0
                            ? rec->cfg.kills_to_banner : 50;
                    if (count > 0 && (count % step) == 0) {
                        struct { float x; float y; } pos = {0.0f, 0.0f};
                        if (patchlib_is_valid(f_entity_pos)) {
                            patchlib_field_get_value(f_entity_pos, this, &pos);
                        }
                        terraria_npc_manager_drop_item(
                                rec->cfg.banner_item_id, pos.x, pos.y, 1);
                    }
                }
            }
            patchlib_free(kills_tracker);
        }
    }

    int type = -1;
    if (patchlib_is_valid(f_type))
        patchlib_field_get_value(f_type, this, &type);
    TEKLOG_DEBUG("npc loot: type=%d on_death=%d", type,
                 h->npc_ops.on_death ? 1 : 0);
    if (h->npc_ops.on_death)
        h->npc_ops.on_death(h, this);
}

#if defined(__ANDROID__)
typedef void (*npc_ai_fn)(void *this);
static npc_ai_fn g_orig_npc_ai = NULL;

static void npc_ai_detour(void *this) {
    if (!this) return;
    const int type = get_npc_type(this);
    const int base = terraria_npc_id_get_count();
    const int nid = type - base;
    const size_t count = tefstd_vector_size(&g_terraria_npc_registry);
    if (nid < 0 || nid >= (int) count) {
        /* 原版 NPC：超快速旁路，直接调用原函数 */
        g_orig_npc_ai(this);
        return;
    }

    terraria_npc_handle_t **pp = tefstd_vector_at(&g_terraria_npc_registry, (size_t) nid);
    if (!pp || !*pp) {
        g_orig_npc_ai(this);
        return;
    }
    terraria_npc_handle_t *h = *pp;
    if (!h->npc_ops.ai) {
        g_orig_npc_ai(this);
        return;
    }

    if (h->ai_after_vanilla) {
        g_orig_npc_ai(this);
        h->npc_ops.ai(h, this);
        return;
    }

    h->npc_ops.ai(h, this);
}
#endif

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
        const int base = terraria_npc_id_get_count();
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
#if defined(__ANDROID__)
    if (patchlib_is_valid(f_type)) {
        const size_t off = il2cpp_field_get_offset(f_type);
        if (off > 0) g_npc_type_offset = (int) off;
    }
#endif
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
        f_npc_head_boss = patchlib_type_get_field(tex_cls, "NpcHeadBoss");
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
#if defined(__ANDROID__)
        if (patchlib_install_typed_hook(ai, (void *) npc_ai_detour,
                                        (void **) &g_orig_npc_ai)) {
            TEKLOG_INFO("npc: fast-path typed hook installed for NPC.AI");
        } else {
            h_ai = patchlib_install_prepost_hook(ai, ai_prefix, ai_postfix);
            TEKLOG_INFO("npc: hooked NPC.AI via prepost (%d)", (int) h_ai);
        }
#else
        h_ai = patchlib_install_prepost_hook(ai, ai_prefix, ai_postfix);
        TEKLOG_INFO("npc: hooked NPC.AI (%d)", (int) h_ai);
#endif
    }

#if defined(__ANDROID__)
    {
        patch_handle_t lang_cls = patchlib_type_get_type("Terraria", "Lang");
        if (patchlib_is_valid(lang_cls)) {
            patch_handle_t get_name = patchlib_type_get_method_by_param_count(
                    lang_cls, "GetNPCName", 1);
            if (patchlib_is_valid(get_name)) {
                h_lang_get_npc_name = patchlib_install_prepost_hook(
                        get_name, lang_get_npc_name_prefix, NULL);
                patchlib_free(get_name);
                TEKLOG_INFO("npc: hooked Lang.GetNPCName (%d)",
                            (int)h_lang_get_npc_name);
            }
            patchlib_free(lang_cls);
        }
    }
#endif

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
    if (h_lang_get_npc_name != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(h_lang_get_npc_name);
        h_lang_get_npc_name = PATCH_HOOK_INVALID_ID;
    }
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
#if defined(__ANDROID__)
    if (g_orig_npc_ai) {
        patch_handle_t ai = patchlib_type_get_method_by_param_count(npc_class, "AI", 0);
        if (patchlib_is_valid(ai)) {
            patchlib_uninstall_typed_hook(ai);
            patchlib_free(ai);
        }
        g_orig_npc_ai = NULL;
    }
#endif
    if (h_ai != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(h_ai);
        h_ai = PATCH_HOOK_INVALID_ID;
    }
    if (g_bestiary_records_inited) {
        const size_t n = tefstd_vector_size(&g_npc_bestiary_records);
        for (size_t i = 0; i < n; ++i) {
            npc_bestiary_record_t *r =
                    tefstd_vector_at(&g_npc_bestiary_records, i);
            free_bestiary_record(r);
        }
        tefstd_vector_destroy(&g_npc_bestiary_records);
        g_bestiary_records_inited = false;
    }
    tefstd_vector_destroy(&g_terraria_npc_registry);
}
