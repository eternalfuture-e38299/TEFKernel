/*******************************************************************************
 * tefkernel - prefix_manager
 * Copyright (C) 2026 eternalfuture-e38299
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2026/10/30
 *******************************************************************************/

#include "internal/terraria/content/prefix_manager.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/log.h"
#include "patchlib/field.h"
#if __ANDROID__
#include "patchlib/il2cpp_api.h"
#endif
#include "patchlib/method.h"
#include "patchlib/struct/array.h"
#include "patchlib/type.h"
#include "terraria/language_manager.h"

tefstd_vector_t g_terraria_prefix_registry;

/* ==================== 引擎句柄 ==================== */
static patch_handle_t prefix_id_count = PATCH_NULL;   /* PrefixID.Count */
static int g_prefix_base = 98;

static patch_handle_t f_lang_prefix = PATCH_NULL;     /* Lang.prefix (LocalizedText[]) */

static patch_handle_t f_item_prefix  = PATCH_NULL;    /* Item.prefix (byte) */
static patch_handle_t f_item_type    = PATCH_NULL;    /* Item.type (int) */

static patch_handle_t m_item_prefix  = PATCH_NULL;    /* Item.Prefix(int) */
static patch_handle_t m_item_reset   = PATCH_NULL;    /* Item.ResetStats(int) */
static patch_handle_t m_item_roll    = PATCH_NULL;    /* Item.GetRollablePrefixes() */
static patch_handle_t m_item_try_stats = PATCH_NULL;  /* Item.TryGetPrefixStatMultipliersForItem(...) */

static patch_hook_id_t h_prefix     = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_rollable   = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t h_try_stats  = PATCH_HOOK_INVALID_ID;

static bool g_ids_assigned = false;

/* 每个自定义 id 对应的 LocalizedText 句柄（Android 需 gchandle 钉住）。 */
static patch_handle_t g_prefix_text[256];
#if defined(__ANDROID__)
static void *g_prefix_text_pin[256];
#endif

/* ==================== 注册表 ==================== */

static int compare_prefix(const void *a, const void *b) {
    const terraria_prefix_handle_t *pa = *(const terraria_prefix_handle_t **) a;
    const terraria_prefix_handle_t *pb = *(const terraria_prefix_handle_t **) b;
    int cmp = strcmp(pa->parent_modloader_id, pb->parent_modloader_id);
    if (cmp != 0) return cmp;
    cmp = strcmp(pa->parent_id, pb->parent_id);
    if (cmp != 0) return cmp;
    return strcmp(pa->internal_name, pb->internal_name);
}

/* 与 item 同构：item_name_<loader>_<mod>.<name>
 * 前缀用 prefix_name_<loader>_<mod>.<name> */
static bool build_prefix_name_key(const terraria_prefix_handle_t *h,
                                  char *buf, const size_t cap) {
    if (!h || !buf || cap == 0) return false;
    const char *loader = h->parent_modloader_id ? h->parent_modloader_id : "?";
    const char *mod    = h->parent_id           ? h->parent_id           : "?";
    const char *name   = h->internal_name       ? h->internal_name       : "?";
    const int n = snprintf(buf, cap, "prefix_name_%s_%s.%s", loader, mod, name);
    return n > 0 && (size_t) n < cap;
}

int terraria_prefix_id_get_count(void) {
    int c = 0;
    if (patchlib_is_valid(prefix_id_count))
        patchlib_field_get_value(prefix_id_count, NULL, &c);
    return c > 0 ? c : g_prefix_base;
}

int terraria_prefix_manager_remaining_slots(void) {
    if (g_terraria_prefix_registry.data == NULL)
        return 0;
    const int base = terraria_prefix_id_get_count();
    const int used = (int) tefstd_vector_size(&g_terraria_prefix_registry);
    return (256 - base) - used;
}

bool terraria_prefix_manager_register_prefix(terraria_prefix_handle_t *prefix_handle) {
    if (!prefix_handle || !prefix_handle->parent_modloader_id || !prefix_handle->parent_id || !prefix_handle->internal_name)
        return false;

    const size_t count = tefstd_vector_size(&g_terraria_prefix_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_prefix_handle_t **ex =
            tefstd_vector_at(&g_terraria_prefix_registry, i);
        if (!ex || !*ex) continue;
        if (strcmp((*ex)->parent_modloader_id, prefix_handle->parent_modloader_id) == 0 &&
            strcmp((*ex)->parent_id, prefix_handle->parent_id) == 0 &&
            strcmp((*ex)->internal_name, prefix_handle->internal_name) == 0) {
            prefix_handle->runtime_id = (*ex)->runtime_id;
            *ex = prefix_handle;
            TEKLOG_INFO("prefix re-registered: %s.%s id=%d",
                        prefix_handle->parent_id, prefix_handle->internal_name, prefix_handle->runtime_id);
            return true;
        }
    }

    if (g_ids_assigned)
        TEKLOG_WARN("prefix registered after IDs assigned: %s.%s",
                    prefix_handle->parent_id, prefix_handle->internal_name);

    return tefstd_vector_push_back(&g_terraria_prefix_registry, &prefix_handle);
}

bool terraria_prefix_manager_assign_ids(const int base_id) {
    const size_t count = tefstd_vector_size(&g_terraria_prefix_registry);
    if (count == 0) { g_ids_assigned = true; return true; }
    if (base_id + (int) count > 256) {
        TEKLOG_ERROR("prefix: too many prefixes (%d + %zu > 256)",
                     base_id, count);
        return false;
    }
    terraria_prefix_handle_t **items = g_terraria_prefix_registry.data;
    qsort(items, count, sizeof(terraria_prefix_handle_t *), compare_prefix);
    for (size_t i = 0; i < count; ++i)
        items[i]->runtime_id = base_id + (int) i;
    g_ids_assigned = true;
    TEKLOG_INFO("prefix: assigned %zu ids from %d", count, base_id);
    return true;
}

terraria_prefix_handle_t *terraria_prefix_manager_get_prefix(const int runtime_id) {
    const int nid = runtime_id - terraria_prefix_id_get_count();
    const size_t count = tefstd_vector_size(&g_terraria_prefix_registry);
    if (nid < 0 || nid >= (int) count) return NULL;
    terraria_prefix_handle_t **pp =
        tefstd_vector_at(&g_terraria_prefix_registry, (size_t) nid);
    return pp ? *pp : NULL;
}

tefstd_vector_t *terraria_prefix_manager_get_prefixes(void) {
    return &g_terraria_prefix_registry;
}

terraria_prefix_handle_t *terraria_prefix_manager_find_prefix(
        const char *loader, const char *mod, const char *name) {
    if (!loader || !mod || !name) return NULL;
    const size_t count = tefstd_vector_size(&g_terraria_prefix_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_prefix_handle_t **pp =
            tefstd_vector_at(&g_terraria_prefix_registry, i);
        if (!pp || !*pp) continue;
        if (strcmp((*pp)->parent_modloader_id, loader) == 0 &&
            strcmp((*pp)->parent_id, mod) == 0 &&
            strcmp((*pp)->internal_name, name) == 0)
            return *pp;
    }
    return NULL;
}

bool terraria_prefix_manager_unregister_prefix_by_id(const int runtime_id) {
    if (runtime_id < 0) return false;
    const size_t count = tefstd_vector_size(&g_terraria_prefix_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_prefix_handle_t **pp =
            tefstd_vector_at(&g_terraria_prefix_registry, i);
        if (!pp || !*pp) continue;
        if ((*pp)->runtime_id == runtime_id) {
            const bool removed =
                tefstd_vector_erase(&g_terraria_prefix_registry, i, NULL);
            if (removed)
                terraria_prefix_manager_assign_ids(terraria_prefix_id_get_count());
            return removed;
        }
    }
    return false;
}

/* ==================== Lang.prefix 扩容 ==================== */

/* PrefixID.Sets.* 按 PrefixID.Count 定长的静态 bool[]。
 * 原版自然掷前缀路径会用 prefix 直接索引，自定义 id 必须扩容。
 * 与 item_manager 的 resize_all_class_sets 同构：
 *   - 只处理静态、非 const、数组类型字段；
 *   - 长度 near base 且 < 256 才扩；
 *   - 新槽用原数组众数填充（ReducedNaturalChance 默认 false，众数即 false）。 */
static void resize_prefix_id_sets(void) {
    patch_handle_t id_cls = patchlib_type_get_type("Terraria.ID", "PrefixID");
    if (!patchlib_is_valid(id_cls)) {
        TEKLOG_WARN("prefix: PrefixID not found");
        return;
    }
    patch_handle_t sets = patchlib_type_get_inner_type(id_cls, "Sets");
    patchlib_free(id_cls);
    if (!patchlib_is_valid(sets)) {
        TEKLOG_WARN("prefix: PrefixID+Sets not found");
        return;
    }

    tefstd_vector_t fields;
    tefstd_vector_init(&fields, sizeof(patch_handle_t));
    patchlib_type_get_fields(sets, false, &fields);
    if (!fields.data || fields.size <= 0) {
        TEKLOG_WARN("prefix: PrefixID.Sets has no fields");
        patchlib_free(sets);
        tefstd_vector_destroy(&fields);
        return;
    }

    const int base = terraria_prefix_id_get_count();
    int resized = 0, skipped = 0, error = 0;

    for (size_t i = 0; i < fields.size; ++i) {
        const int new_size = 256;
        patch_handle_t field = *(patch_handle_t *) tefstd_vector_at(&fields, i);
        if (!patchlib_is_valid(field)) {
            error++;
            continue;
        }

        const char *name = patchlib_field_get_name(field);
        /* 跳过非静态、const、非数组（Factory 不是数组；Count 是 const） */
        if (!patchlib_field_is_static(field) ||
            patchlib_field_is_const(field) ||
            !name || strcmp(name, "Factory") == 0 || strcmp(name, "Count") == 0) {
            patchlib_free(field);
            skipped++;
            continue;
        }

        patch_handle_t array = PATCH_NULL;
        patchlib_field_get_value(field, NULL, &array);
        if (!patchlib_is_valid(array)) {
            patchlib_free(field);
            skipped++;
            continue;
        }

        const size_t len = patchlib_array_length(array);
        /* 只扩“长度接近 base 且 < 256”的数组；其它长度（非按 prefix 索引）跳过 */
        const bool near_base =
                len + 8 >= (size_t) base && len < (size_t) new_size;
        if (!near_base) {
            TEKLOG_DEBUG("prefix: PrefixID.Sets.%s len=%zu, skip",
                         name ? name : "?", len);
            patchlib_free(array);
            patchlib_free(field);
            skipped++;
            continue;
        }

        patch_handle_t bigger = patchlib_array_resize(array, (size_t) new_size, NULL);
        if (patchlib_is_valid(bigger)) {
            /* 新槽用原数组众数填充（ReducedNaturalChance 众数是 false） */
            patchlib_field_set_value(field, NULL, &bigger);
            patchlib_free(bigger);
            TEKLOG_INFO("prefix: PrefixID.Sets.%s %zu -> %d",
                        name ? name : "?", len, new_size);
            resized++;
        } else {
            TEKLOG_ERROR("prefix: PrefixID.Sets.%s resize failed", name ? name : "?");
            error++;
        }
        patchlib_free(array);
        patchlib_free(field);
    }

    tefstd_vector_destroy(&fields);
    patchlib_free(sets);
    TEKLOG_INFO("prefix: PrefixID.Sets resized=%d skipped=%d errors=%d",
                resized, skipped, error);
}

/* 与 item_manager 的 resize_array 同构：
 *  - arr 为 null：allow_create 时新建 256 长（LocalizedText[] 引用数组）；
 *  - arr 长度不足：resize 到 256。
 * 引用数组不能用 dvalue（桌面端会崩），新槽留 null，由 init_localized_text 写入。 */
void terraria_prefix_manager_resize() {
    resize_prefix_id_sets();

    if (!patchlib_is_valid(f_lang_prefix)) {
        TEKLOG_WARN("prefix: Lang.prefix field not resolved");
        return;
    }

    patch_handle_t arr = PATCH_NULL;
    patchlib_field_get_value(f_lang_prefix, NULL, &arr);

    if (!patchlib_is_valid(arr)) {
        /* 与 item 的 resize_array 同构：用 LocalizedText 类型新建数组 */
        patch_handle_t text_cls = patchlib_type_get_type(
                "Terraria.Localization", "LocalizedText");
        if (!patchlib_is_valid(text_cls)) {
            TEKLOG_ERROR("prefix: LocalizedText type not found");
            return;
        }
        patch_handle_t narray = patchlib_array_create(256, text_cls);
        patchlib_free(text_cls);
        if (!patchlib_is_valid(narray)) {
            TEKLOG_ERROR("prefix: create Lang.prefix (256) failed");
            return;
        }
        patchlib_field_set_value(f_lang_prefix, NULL, &narray);
        patchlib_free(narray);
        TEKLOG_INFO("prefix: Lang.prefix created (256)");
        return;
    }

    const size_t len = patchlib_array_length(arr);
    if (len >= 256) {
        patchlib_free(arr);
        return;
    }

    patch_handle_t bigger = patchlib_array_resize(arr, 256, NULL);
    if (patchlib_is_valid(bigger)) {
        patchlib_field_set_value(f_lang_prefix, NULL, &bigger);
        patchlib_free(bigger);
        TEKLOG_INFO("prefix: Lang.prefix %zu -> 256", len);
    } else {
        TEKLOG_ERROR("prefix: Lang.prefix resize failed (len=%zu)", len);
    }
    patchlib_free(arr);
}

/* ==================== Lang.prefix 填充 ==================== */

/* 参照 item_manager 的 init_localized_text：
 * 先 resize，再用 key "prefix_name_<loader>_<mod>.<name>" 查文本写入。 */
void terraria_prefix_manager_init_localized_text(void) {
    terraria_prefix_manager_resize();
    if (!patchlib_is_valid(f_lang_prefix)) return;

    patch_handle_t arr = PATCH_NULL;
    patchlib_field_get_value(f_lang_prefix, NULL, &arr);
    if (!patchlib_is_valid(arr)) return;

    const size_t count = tefstd_vector_size(&g_terraria_prefix_registry);
    for (size_t i = 0; i < count; ++i) {
        terraria_prefix_handle_t **pp =
            tefstd_vector_at(&g_terraria_prefix_registry, i);
        if (!pp || !*pp) continue;
        terraria_prefix_handle_t *h = *pp;
        if (h->runtime_id < 0 || h->runtime_id > 255) continue;

        char key[256];
        if (!build_prefix_name_key(h, key, sizeof(key))) {
            TEKLOG_WARN("prefix: key too long for %s", h->internal_name);
            continue;
        }

        patch_handle_t text =
                terraria_language_manager_get_localized_text(key);
        if (!patchlib_is_valid(text)) {
            TEKLOG_WARN("prefix: get_localized_text('%s') failed", key);
            continue;
        }

        if (!patchlib_array_set(arr, (size_t) h->runtime_id, &text))
            TEKLOG_WARN("prefix: set Lang.prefix[%d] failed", h->runtime_id);

#if defined(__ANDROID__)
        if (g_prefix_text_pin[h->runtime_id] && il2cpp_gchandle_free) {
            il2cpp_gchandle_free(g_prefix_text_pin[h->runtime_id]);
            g_prefix_text_pin[h->runtime_id] = NULL;
        }
        if (il2cpp_gchandle_new)
            g_prefix_text_pin[h->runtime_id] =
                    il2cpp_gchandle_new(text, false);
#endif
        g_prefix_text[h->runtime_id] = text;
    }

    patchlib_free(arr);
    TEKLOG_INFO("prefix: Lang.prefix filled (%zu custom)", count);
}

/* ==================== 字段读写辅助 ==================== */

static void get_i32(patch_handle_t field, patch_handle_t inst, int *out) {
    if (patchlib_is_valid(field) && out) patchlib_field_get_value(field, inst, out);
}
static void set_i32(patch_handle_t field, patch_handle_t inst, int v) {
    if (patchlib_is_valid(field)) patchlib_field_set_value(field, inst, &v);
}

static int get_item_prefix_value(patch_handle_t inst) {
    if (!patchlib_is_valid(f_item_prefix) || !patchlib_is_valid(inst)) return 0;
    unsigned char b = 0;
    patchlib_field_get_value(f_item_prefix, inst, &b);
    return b;
}

static void set_item_prefix_value(patch_handle_t inst, int v) {
    if (!patchlib_is_valid(f_item_prefix) || !patchlib_is_valid(inst)) return;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    unsigned char b = (unsigned char) v;
    patchlib_field_set_value(f_item_prefix, inst, &b);
}

/* ==================== 钩子：TryGetPrefixStatMultipliersForItem ==================== */

/* 参数顺序（共 11 参）：
 *   [0] int   rolledPrefix
 *   [1..10] out（args[i] 是 T**） */
static bool item_try_get_stats_prefix(patch_handle_t this, void **args,
                                      const patch_method_signature_t *sig,
                                      void *result) {
    (void) sig;
    if (!args || !args[0] || !args[1] || !args[2] || !args[3] || !args[4] ||
        !args[5] || !args[6] || !args[7] || !args[8] || !args[9] || !args[10] ||
        !result)
        return false;

    const int pre = *(int *) args[0];
    const int base = terraria_prefix_id_get_count();
    if (pre < base) return false;   /* 原版前缀：走原方法 */

    terraria_prefix_handle_t *h = terraria_prefix_manager_get_prefix(pre);
    if (!h) return false;

    /* 自定义前缀：跳过原方法，自己写 out + __result。
     * 原方法对 pre >= 98 走 default，out 基线 1/0；我们在此基础上叠加。 */
    float dmg = 1.0f, kb = 1.0f, spd = 1.0f, size = 1.0f,
          shtspd = 1.0f, mcst = 1.0f;
    int crt = 0, tagdmg = 0, arpen = 0;

    float d_dmg = 1.0f, d_kb = 1.0f, d_spd = 1.0f, d_size = 1.0f,
          d_shtspd = 1.0f, d_mcst = 1.0f;
    int d_crt = 0, d_tagdmg = 0, d_arpen = 0;
    if (h->prefix_ops.apply_stats)
         *(bool *) result = h->prefix_ops.apply_stats(h, this, &d_dmg, &d_kb, &d_spd, &d_size,
                                  &d_shtspd, &d_mcst, &d_crt,
                                  &d_tagdmg, &d_arpen);

    dmg    *= d_dmg;
    kb     *= d_kb;
    spd    *= d_spd;
    size   *= d_size;
    shtspd *= d_shtspd;
    mcst   *= d_mcst;
    crt    += d_crt;
    tagdmg += d_tagdmg;
    arpen  += d_arpen;

    float value = 1.0f * dmg * (2.0f - spd) * (2.0f - mcst) * size * kb * shtspd
                  * (1.0f + (float) crt * 0.02f)
                  * (1.0f + (float) arpen * 0.015f)
                  * (1.0f + (float) tagdmg * 0.03f);
    if (h->prefix_ops.modify_value) {
        float vm = 1.0f;
        h->prefix_ops.modify_value(h, this, &vm);
        value *= vm;
    }

    **(float **) args[1]  = dmg;
    **(float **) args[2]  = kb;
    **(float **) args[3]  = spd;
    **(float **) args[4]  = size;
    **(float **) args[5]  = shtspd;
    **(float **) args[6]  = mcst;
    **(int   **) args[7]  = crt;
    **(int   **) args[8]  = tagdmg;
    **(int   **) args[9]  = arpen;
    **(float **) args[10] = value;

    /* 原方法返回 bool（前缀是否可用）。跳过原方法时必须显式写 true，
     * 否则 Harmony 用默认 false，Item.Prefix 不生效。 */
    return true;   /* 跳过原方法 */
}

/* ==================== 钩子：Item.Prefix(int) ==================== */

static void item_prefix_postfix(patch_handle_t this, void **args, void *result,
                                const patch_method_signature_t *sig) {
    (void) result; (void) sig;
    if (!this || !args || !args[0]) return;

    const int pre = *(int *) args[0];
    const int base = terraria_prefix_id_get_count();
    if (pre < base) return;

    terraria_prefix_handle_t *h = terraria_prefix_manager_get_prefix(pre);
    if (!h) {
        set_item_prefix_value(this, 0);
        return;
    }

    set_item_prefix_value(this, pre);

    TEKLOG_DEBUG("prefix: applied %s (id=%d)", h->internal_name, pre);
}

/* ==================== 钩子：GetRollablePrefixes ==================== */

/* 原版返回 int[]（静态共享数组，可能为 null）。
 * 复制成新数组并追加匹配的自定义 id，避免污染原版静态数组。
 *
 * 防御：result 可能为 0（方法返回 null）或非法（跨域/已释放），
 * 必须先判 PATCH_NULL 再 patchlib_is_valid，避免 GCHandle.FromIntPtr(垃圾)。 */
static void item_rollable_postfix(patch_handle_t this, void **args, void *result,
                                  const patch_method_signature_t *sig) {
    (void) args; (void) sig;
    if (!this || !result) return;

    const size_t custom = tefstd_vector_size(&g_terraria_prefix_registry);
    if (custom == 0) return;

    const int base = terraria_prefix_id_get_count();
    const int max_extra = 256 - base;
    int extra[256];
    int extra_n = 0;

    for (size_t i = 0; i < custom && extra_n < max_extra; ++i) {
        terraria_prefix_handle_t **pp =
            tefstd_vector_at(&g_terraria_prefix_registry, i);
        if (!pp || !*pp) continue;
        terraria_prefix_handle_t *h = *pp;
        if (h->runtime_id < base || h->runtime_id > 255) continue;
        if (h->prefix_ops.can_roll && !h->prefix_ops.can_roll(h, this))
            continue;
        if (h->prefix_ops.roll_chance) {
            float c = h->prefix_ops.roll_chance(h, this);
            if (c <= 0.0f) continue;
        }
        extra[extra_n++] = h->runtime_id;
    }
    if (extra_n == 0) return;

    patch_handle_t orig = *(patch_handle_t *) result;
    size_t orig_len = 0;
    if (orig != PATCH_NULL && patchlib_is_valid(orig)) {
        if (patchlib_array_element_size(orig) != sizeof(int)) {
            TEKLOG_WARN("prefix: GetRollablePrefixes returned non-int[] "
                        "(elem_size=%zu), skip merge",
                        patchlib_array_element_size(orig));
            return;
        }
        orig_len = patchlib_array_length(orig);
    }

    const size_t new_len = orig_len + (size_t) extra_n;
    patch_handle_t int_type = patchlib_get_basic_type(PATCH_INT32);
    if (!patchlib_is_valid(int_type)) return;

    patch_handle_t merged = patchlib_array_create(new_len, int_type);
    patchlib_free(int_type);
    if (!patchlib_is_valid(merged)) {
        TEKLOG_WARN("prefix: create rollable array failed (len=%zu)", new_len);
        return;
    }

    if (orig_len > 0 && !patchlib_array_copy(merged, orig, orig_len)) {
        TEKLOG_WARN("prefix: copy %zu rollable prefixes failed", orig_len);
        patchlib_free(merged);
        return;
    }

    for (int i = 0; i < extra_n; ++i) {
        int v = extra[i];
        if (!patchlib_array_set(merged, orig_len + (size_t) i, &v))
            TEKLOG_WARN("prefix: set extra[%d] failed", i);
    }

    *(patch_handle_t *) result = merged;
}

/* ==================== 生命周期 ==================== */

void terraria_prefix_manager_init(void) {
    if (!tefstd_vector_init(&g_terraria_prefix_registry,
                            sizeof(terraria_prefix_handle_t *))) {
        TEKLOG_ERROR("prefix: vector init failed");
        return;
    }

    patch_handle_t id_cls = patchlib_type_get_type("Terraria.ID", "PrefixID");
    if (patchlib_is_valid(id_cls)) {
        prefix_id_count = patchlib_type_get_field(id_cls, "Count");
        patchlib_free(id_cls);
    }

    patch_handle_t lang_cls = patchlib_type_get_type("Terraria", "Lang");
    if (patchlib_is_valid(lang_cls)) {
        f_lang_prefix = patchlib_type_get_field(lang_cls, "prefix");
        patchlib_free(lang_cls);
    }

    patch_handle_t item_cls = patchlib_type_get_type("Terraria", "Item");
    if (!patchlib_is_valid(item_cls)) {
        TEKLOG_ERROR("prefix: Terraria.Item not found");
        return;
    }

    f_item_prefix = patchlib_type_get_field(item_cls, "prefix");
    f_item_type   = patchlib_type_get_field(item_cls, "type");

    m_item_prefix = patchlib_type_get_method_by_param_count(item_cls, "Prefix", 1);
    m_item_reset  = patchlib_type_get_method_by_param_count(item_cls, "ResetStats", 1);
    m_item_roll   = patchlib_type_get_method_by_param_count(item_cls, "GetRollablePrefixes", 0);
    m_item_try_stats = patchlib_type_get_method_by_param_count(
            item_cls, "TryGetPrefixStatMultipliersForItem", 11);

    if (patchlib_is_valid(m_item_prefix))
        h_prefix = patchlib_install_prepost_hook(m_item_prefix, NULL,
                                                 item_prefix_postfix);
    if (patchlib_is_valid(m_item_roll))
        h_rollable = patchlib_install_prepost_hook(m_item_roll, NULL,
                                                   item_rollable_postfix);
    if (patchlib_is_valid(m_item_try_stats)) {
        h_try_stats = patchlib_install_prepost_hook(m_item_try_stats, item_try_get_stats_prefix,
                                                    NULL);
        TEKLOG_INFO("prefix: hooked TryGetPrefixStatMultipliersForItem (%d)",
                    (int) h_try_stats);
    } else {
        TEKLOG_WARN("prefix: TryGetPrefixStatMultipliersForItem not found "
                    "(custom stats will NOT apply)");
    }
}

void terraria_prefix_manager_destroy(void) {
    if (h_prefix != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(h_prefix);
        h_prefix = PATCH_HOOK_INVALID_ID;
    }
    if (h_rollable != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(h_rollable);
        h_rollable = PATCH_HOOK_INVALID_ID;
    }
    if (h_try_stats != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(h_try_stats);
        h_try_stats = PATCH_HOOK_INVALID_ID;
    }

#if defined(__ANDROID__)
    for (int i = 0; i < 256; ++i) {
        if (g_prefix_text_pin[i] && il2cpp_gchandle_free) {
            il2cpp_gchandle_free(g_prefix_text_pin[i]);
            g_prefix_text_pin[i] = NULL;
        }
    }
#endif
    memset(g_prefix_text, 0, sizeof(g_prefix_text));
    tefstd_vector_destroy(&g_terraria_prefix_registry);
}