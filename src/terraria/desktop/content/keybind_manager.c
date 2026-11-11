/*******************************************************************************
 * tefkernel - keybind_manager (PC)
 *
 * 把 Mod 自定义按键注入 Terraria PC 原生「设置 -> 控制」菜单：
 *   1. 注册 trigger 名到 PlayerInput.KnownTriggers；
 *   2. 注入 PlayerInputProfile.InputModes[*].KeyStatus；
 *   3. 在 UIManageControls.CreateBindingGroup 组装菜单时追加条目；
 *   4. 每帧读取 PlayerInput.Triggers 的 Current/JustPressed/JustReleased 派发回调。
 *
 * 这样玩家在游戏原生菜单里改键后，Mod 无需自行维护键码映射。
 *******************************************************************************/

#include "internal/terraria/content/keybind_manager.h"

#if !defined(__ANDROID__)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/kernel_state.h"
#include "internal/log.h"
#include "internal/terraria/save/tef_save_format.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/property.h"
#include "patchlib/struct/list.h"
#include "patchlib/struct/string.h"
#include "patchlib/type.h"

#define KEYBIND_MAX_ENTRIES 128
#define KEYBIND_MAX_ID 64
#define KEYBIND_MAX_NAME 128
#define KEYBIND_MAX_STORED 256

typedef struct keybind_saved_t {
    char modloader_id[KEYBIND_MAX_ID + 1];
    char mod_id[KEYBIND_MAX_ID + 1];
    char id[KEYBIND_MAX_ID + 1];
    char key[KEYBIND_MAX_STORED + 1];
} keybind_saved_t;

typedef struct keybind_entry_t {
    bool used;
    bool enabled;
    char modloader_id[KEYBIND_MAX_ID + 1];
    char mod_id[KEYBIND_MAX_ID + 1];
    char id[KEYBIND_MAX_ID + 1];
    char name[KEYBIND_MAX_NAME + 1];
    char default_key[KEYBIND_MAX_STORED + 1];
    terraria_keybind_callback_t on_press;
    terraria_keybind_callback_t on_release;
    void *user;
    patch_handle_t trigger; /* 缓存的 C# string 句柄（GCHandle） */
    bool down;
    bool pressed;
    bool released;
} keybind_entry_t;

static keybind_entry_t g_entries[KEYBIND_MAX_ENTRIES];
static keybind_saved_t g_saved[KEYBIND_MAX_ENTRIES];
static int g_saved_count;
static bool g_store_loaded;
static bool g_installed;
static bool g_shutdown; /* 管理器已 shutdown：禁止 available/register 触发重新初始化 */
static bool g_ready;
static bool g_injected; /* 是否已把已注册 trigger 注入当前 Profile / Triggers */
static bool g_dirty;    /* 是否需要在下一帧重新注入（新注册/切换 Profile） */
static char g_error[256] = "keybind manager not initialized";

static patch_hook_id_t g_update_hook = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t g_menu_hook = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t g_save_hook = PATCH_HOOK_INVALID_ID;

/* ---- 类型/成员句柄（保持到 shutdown） ---- */
static patch_handle_t t_main;
static patch_handle_t f_drawing_chat;

static patch_handle_t t_player_input;
static patch_handle_t f_triggers;
static patch_handle_t f_writing_text;
static patch_handle_t f_current_profile;
static patch_handle_t f_known_triggers;
static patch_handle_t m_current_profile_getter;

static patch_handle_t t_triggers_pack;
static patch_handle_t f_trig_current;
static patch_handle_t f_trig_just_pressed;
static patch_handle_t f_trig_just_released;
static patch_handle_t m_triggers_initialize;

static patch_handle_t t_triggers_set;
static patch_handle_t f_trigger_key_status;

static patch_handle_t t_profile;
static patch_handle_t f_profile_input_modes;

static patch_handle_t t_key_config;
static patch_handle_t f_config_key_status;

static patch_handle_t t_ui_manage_controls;

/* 闭合泛型类型与方法句柄（桌面端不能用 il2cpp_object_get_class 取泛型实例的类）。 */
static patch_handle_t t_key_status_type;      /* Dictionary<string, List<string>> */
static patch_handle_t t_trigger_status_type;  /* Dictionary<string, bool> */
static patch_handle_t t_input_modes_type;     /* Dictionary<InputMode, KeyConfiguration> */
static patch_handle_t t_list_string_type;     /* List<string> */
static patch_handle_t m_ks_contains;
static patch_handle_t m_ks_add;
static patch_handle_t m_ks_get;
static patch_handle_t m_ts_contains;
static patch_handle_t m_ts_get;
static patch_handle_t m_im_get;
static patch_handle_t m_list_contains;
static patch_handle_t m_list_add;
static patch_handle_t m_list_index_of;
static patch_handle_t m_list_insert;
static patch_handle_t m_list_ctor;
static patch_handle_t m_list_count;
static patch_handle_t m_list_get;
static patch_handle_t str_reset_gameplay; /* "sp9"：游戏菜单里的“重置为默认” */

/* 前向声明：钩子回调与 Triggers 重建在 resolve_all 之后定义。 */
static void keybind_on_update_postfix(patch_handle_t instance, void **args, void *result,
                                      const patch_method_signature_t *sig);
static bool keybind_on_create_binding_group_prefix(patch_handle_t instance, void **args,
                                                   const patch_method_signature_t *sig,
                                                   void *result);
static void keybind_on_player_save_postfix(patch_handle_t instance, void **args, void *result,
                                           const patch_method_signature_t *sig);
static patch_handle_t get_current_profile(void);
static patch_handle_t read_object_field(patch_handle_t field, patch_handle_t instance);
static void ensure_game_state(void);
static void rebuild_trigger_sets(void);

/* ------------------------------------------------------------------ */
/* 基础工具                                                            */
/* ------------------------------------------------------------------ */

static void set_error(const char *message) {
    if (!message) {
        g_error[0] = '\0';
        return;
    }
    snprintf(g_error, sizeof(g_error), "%s", message);
}

static void copy_str(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0)
        return;
    if (!src)
        src = "";
    snprintf(dst, dst_size, "%s", src);
}

static bool is_valid_handle(patch_handle_t handle) {
    return handle != PATCH_NULL && patchlib_is_valid(handle);
}

static bool read_bool_field(patch_handle_t field) {
    if (!is_valid_handle(field))
        return false;
    /* 内核可能按 4/8 字节写入返回值，必须用足够大的缓冲，否则踩坏栈。 */
    unsigned char value[64] = {0};
    patchlib_field_get_value(field, PATCH_NULL, value);
    return value[0] != 0;
}

/* 读取静态引用字段；返回的句柄由调用方 patchlib_free。 */
static patch_handle_t read_static_object(patch_handle_t field) {
    patch_handle_t value = PATCH_NULL;
    if (!is_valid_handle(field))
        return PATCH_NULL;
    patchlib_field_get_value(field, PATCH_NULL, &value);
    return value;
}


/* List<string>.Contains(value) */
static bool list_contains(patch_handle_t list, patch_handle_t value) {
    if (!is_valid_handle(list) || !is_valid_handle(value) || !is_valid_handle(m_list_contains))
        return false;
    unsigned char result[64] = {0};
    void *args[1] = {&value};
    return patchlib_method_invoke_args(m_list_contains, list, result, args) && result[0] != 0;
}

/* List<string>.Add(value) */
static void list_add(patch_handle_t list, patch_handle_t value) {
    if (!is_valid_handle(list) || !is_valid_handle(value) || !is_valid_handle(m_list_add))
        return;
    void *args[1] = {&value};
    patchlib_method_invoke_args(m_list_add, list, NULL, args);
}

static patch_handle_t create_string_list(void) {
    if (!is_valid_handle(m_list_ctor))
        return PATCH_NULL;
    int capacity = 0;
    patch_handle_t list = PATCH_NULL;
    void *args[1] = {&capacity};
    if (!patchlib_constructor_invoke(m_list_ctor, &list, args))
        return PATCH_NULL;
    return list;
}

/* Dictionary<string, List<string>>.ContainsKey(key) */
static bool key_status_contains(patch_handle_t dict, patch_handle_t key) {
    if (!is_valid_handle(dict) || !is_valid_handle(key) || !is_valid_handle(m_ks_contains))
        return false;
    unsigned char result[64] = {0};
    void *args[1] = {&key};
    return patchlib_method_invoke_args(m_ks_contains, dict, result, args) && result[0] != 0;
}

/* Dictionary<string, List<string>>.Add(key, value) */
static bool key_status_add(patch_handle_t dict, patch_handle_t key, patch_handle_t value) {
    if (!is_valid_handle(dict) || !is_valid_handle(key) || !is_valid_handle(value) ||
        !is_valid_handle(m_ks_add))
        return false;
    void *args[2] = {&key, &value};
    return patchlib_method_invoke_args(m_ks_add, dict, NULL, args);
}

/* List<string>.IndexOf(value) */
static int list_index_of(patch_handle_t list, patch_handle_t value) {
    if (!is_valid_handle(list) || !is_valid_handle(value) || !is_valid_handle(m_list_index_of))
        return -1;
    unsigned char result[64] = {0};
    void *args[1] = {&value};
    if (!patchlib_method_invoke_args(m_list_index_of, list, result, args))
        return -1;
    int index = -1;
    memcpy(&index, result, sizeof(index));
    return index;
}

/* List<string>.Insert(index, value) */
static void list_insert(patch_handle_t list, int index, patch_handle_t value) {
    if (!is_valid_handle(list) || !is_valid_handle(value) || !is_valid_handle(m_list_insert))
        return;
    void *args[2] = {&index, &value};
    patchlib_method_invoke_args(m_list_insert, list, NULL, args);
}

/* Dictionary<string, bool>.ContainsKey(key) */
static bool trigger_status_contains(patch_handle_t dict, patch_handle_t key) {
    if (!is_valid_handle(dict) || !is_valid_handle(key) || !is_valid_handle(m_ts_contains))
        return false;
    unsigned char result[64] = {0};
    void *args[1] = {&key};
    return patchlib_method_invoke_args(m_ts_contains, dict, result, args) && result[0] != 0;
}

/* Dictionary<string, bool>.get_Item(key) —— 不用 TryGetValue：
 * tefloader 的 il2cpp_method_invoke 不会把 out 参数写回。 */
static bool trigger_status_try_get(patch_handle_t dict, patch_handle_t key, bool *out_value) {
    if (out_value)
        *out_value = false;
    if (!is_valid_handle(dict) || !is_valid_handle(key) || !out_value ||
        !trigger_status_contains(dict, key) || !is_valid_handle(m_ts_get))
        return false;

    unsigned char value[64] = {0};
    void *args[1] = {&key};
    if (!patchlib_method_invoke_args(m_ts_get, dict, value, args))
        return false;
    *out_value = value[0] != 0;
    return true;
}

/* Dictionary<InputMode, KeyConfiguration>.get_Item(mode) */
static bool input_modes_get(patch_handle_t dict, int mode, patch_handle_t *out_config) {
    if (out_config)
        *out_config = PATCH_NULL;
    if (!is_valid_handle(dict) || !out_config || !is_valid_handle(m_im_get))
        return false;

    patch_handle_t config = PATCH_NULL;
    void *args[1] = {&mode};
    const bool ok = patchlib_method_invoke_args(m_im_get, dict, &config, args);
    if (ok && is_valid_handle(config))
        *out_config = config;
    return ok;
}

/* Dictionary<string, List<string>>.get_Item(key) */
static bool key_status_get(patch_handle_t dict, patch_handle_t key, patch_handle_t *out_list) {
    if (out_list)
        *out_list = PATCH_NULL;
    if (!is_valid_handle(dict) || !is_valid_handle(key) || !out_list ||
        !is_valid_handle(m_ks_get) || !key_status_contains(dict, key))
        return false;
    patch_handle_t list = PATCH_NULL;
    void *args[1] = {&key};
    if (!patchlib_method_invoke_args(m_ks_get, dict, &list, args) || !is_valid_handle(list))
        return false;
    *out_list = list;
    return true;
}

/* List<string>.get_Count */
static int list_count(patch_handle_t list) {
    if (!is_valid_handle(list) || !is_valid_handle(m_list_count))
        return 0;
    unsigned char result[64] = {0};
    if (!patchlib_method_invoke_args(m_list_count, list, result, NULL))
        return 0;
    int count = 0;
    memcpy(&count, result, sizeof(count));
    return count;
}

/* List<string>.get_Item(index) -> UTF-8 拷贝 */
static bool list_get_string(patch_handle_t list, int index, char *out, size_t out_size) {
    if (!is_valid_handle(list) || !out || out_size == 0 || !is_valid_handle(m_list_get))
        return false;
    out[0] = '\0';
    patch_handle_t str = PATCH_NULL;
    void *args[1] = {&index};
    if (!patchlib_method_invoke_args(m_list_get, list, &str, args) || !is_valid_handle(str))
        return false;
    char *text = patchlib_string_cstr(str);
    if (text) {
        copy_str(out, out_size, text);
        free(text);
    }
    patchlib_free(str);
    return out[0] != '\0';
}

/* 读取某个键位在当前 Profile 里的实际绑定（Keyboard 优先，其次 KeyboardUI）。 */
static bool current_key_of(const keybind_entry_t *entry, char *out, size_t out_size) {
    if (!entry || !out || out_size == 0 || !is_valid_handle(entry->trigger))
        return false;
    out[0] = '\0';

    patch_handle_t profile = get_current_profile();
    if (!is_valid_handle(profile))
        return false;
    patch_handle_t modes = read_object_field(f_profile_input_modes, profile);
    patchlib_free(profile);
    if (!is_valid_handle(modes))
        return false;

    static const int modes_to_try[] = {0 /* Keyboard */, 1 /* KeyboardUI */};
    bool found = false;
    for (size_t m = 0; m < sizeof(modes_to_try) / sizeof(modes_to_try[0]) && !out[0]; ++m) {
        patch_handle_t config = PATCH_NULL;
        if (!input_modes_get(modes, modes_to_try[m], &config) || !is_valid_handle(config))
            continue;
        patch_handle_t key_status = read_object_field(f_config_key_status, config);
        patchlib_free(config);
        if (!is_valid_handle(key_status))
            continue;
        if (key_status_contains(key_status, entry->trigger))
            found = true;
        patch_handle_t list = PATCH_NULL;
        if (key_status_get(key_status, entry->trigger, &list) && is_valid_handle(list)) {
            found = true;
            if (list_count(list) > 0)
                list_get_string(list, 0, out, out_size);
            patchlib_free(list);
        }
        patchlib_free(key_status);
    }
    patchlib_free(modes);
    return found;
}
/* ---------------- 键位持久化（独立 .tef 配置） ---------------- */

static bool keybind_store_path(char *buf, size_t n) {
    if (!buf || n == 0 || !tefkernel_working_dir || !tefkernel_working_dir[0])
        return false;
    const size_t len = strlen(tefkernel_working_dir);
    const char last = len ? tefkernel_working_dir[len - 1] : '\0';
    const char *sep = (last == '/' || last == '\\') ? "" : "/";
    snprintf(buf, n, "%s%s%s", tefkernel_working_dir, sep, "tefkernel.keybinds.tef");
    return true;
}

static const keybind_saved_t *saved_find(const char *loader, const char *mod, const char *id) {
    if (!loader || !mod || !id)
        return NULL;
    for (int i = 0; i < g_saved_count; ++i) {
        if (strcmp(g_saved[i].modloader_id, loader) == 0 &&
            strcmp(g_saved[i].mod_id, mod) == 0 && strcmp(g_saved[i].id, id) == 0)
            return &g_saved[i];
    }
    return NULL;
}

static void keybind_store_load(void) {
    if (g_store_loaded)
        return;
    g_store_loaded = true;
    g_saved_count = 0;

    char path[1400];
    if (!keybind_store_path(path, sizeof path))
        return;

    tef_save_file_t file;
    if (!tef_save_file_open(path, &file))
        return;
    if (file.kind != TEF_SAVE_KIND_KEYBINDS) {
        tef_save_file_close(&file);
        return;
    }

    for (int si = 0; si < file.seg_count; ++si) {
        if (file.segs[si].id != TEF_SAVE_SEG_KEYBINDS)
            continue;
        tef_save_rd_t seg = tef_save_file_seg(&file, si);
        tef_save_tlv_t t;
        while (tef_save_rd_next(&seg, &t)) {
            if (t.tag != TEF_SAVE_KEYBINDS_ARRAY)
                continue;
            tef_save_rd_t elems;
            uint32_t count = 0;
            if (!tef_save_rd_array_begin(&t, &elems, &count))
                continue;
            for (uint32_t i = 0; i < count && g_saved_count < KEYBIND_MAX_ENTRIES; ++i) {
                tef_save_tlv_t obj;
                if (!tef_save_rd_next(&elems, &obj) || obj.type != TEF_SAVE_T_OBJECT)
                    continue;
                tef_save_rd_t os;
                if (!tef_save_rd_enter(&obj, &os))
                    continue;

                keybind_saved_t saved = {0};
                tef_save_tlv_t f;
                while (tef_save_rd_next(&os, &f)) {
                    switch (f.tag) {
                        case TEF_SAVE_KEYBIND_LOADER:
                            copy_str(saved.modloader_id, sizeof saved.modloader_id,
                                     tef_save_rd_str(&f));
                            break;
                        case TEF_SAVE_KEYBIND_MOD:
                            copy_str(saved.mod_id, sizeof saved.mod_id, tef_save_rd_str(&f));
                            break;
                        case TEF_SAVE_KEYBIND_ID:
                            copy_str(saved.id, sizeof saved.id, tef_save_rd_str(&f));
                            break;
                        case TEF_SAVE_KEYBIND_KEY:
                            copy_str(saved.key, sizeof saved.key, tef_save_rd_str(&f));
                            break;
                        default:
                            break;
                    }
                }
                if (saved.modloader_id[0] && saved.mod_id[0] && saved.id[0])
                    g_saved[g_saved_count++] = saved;
            }
        }
    }
    tef_save_file_close(&file);
    if (g_saved_count > 0)
        TEKLOG_INFO("keybind: loaded %d saved binding(s) from %s", g_saved_count, path);
}

static void keybind_store_save(void) {
    if (!g_ready)
        return;
    char path[1400];
    if (!keybind_store_path(path, sizeof path))
        return;

    tef_save_wbuf_t seg = {.ok = true};
    const size_t arr = tef_save_wb_begin_array(&seg, TEF_SAVE_KEYBINDS_ARRAY);
    int count = 0;
    int active = 0;

    for (int i = 0; i < KEYBIND_MAX_ENTRIES; ++i) {
        keybind_entry_t *entry = &g_entries[i];
        if (!entry->used)
            continue;
        ++active;

        /* 读不到当前 Profile 时不要用默认键覆盖玩家已有配置。 */
        char key[KEYBIND_MAX_STORED + 1] = {0};
        if (!current_key_of(entry, key, sizeof key))
            continue;

        const size_t elem = tef_save_wb_begin_elem(&seg);
        tef_save_wb_tlv_str(&seg, TEF_SAVE_KEYBIND_LOADER, entry->modloader_id);
        tef_save_wb_tlv_str(&seg, TEF_SAVE_KEYBIND_MOD, entry->mod_id);
        tef_save_wb_tlv_str(&seg, TEF_SAVE_KEYBIND_ID, entry->id);
        tef_save_wb_tlv_str(&seg, TEF_SAVE_KEYBIND_NAME, entry->name);
        tef_save_wb_tlv_str(&seg, TEF_SAVE_KEYBIND_KEY, key);
        tef_save_wb_end(&seg, elem);
        ++count;
    }

    if (active > 0 && count == 0) {
        TEKLOG_WARN("keybind: skip saving (current profile unavailable)");
        tef_save_wb_free(&seg);
        return;
    }
    tef_save_wb_end_array(&seg, arr);

    const tef_save_seg_out_t out = {.id = TEF_SAVE_SEG_KEYBINDS, .data = seg.data,
                                    .len = (uint32_t) seg.len};
    if (seg.ok && tef_save_file_write(path, TEF_SAVE_KIND_KEYBINDS, &out, 1))
        TEKLOG_INFO("keybind: saved %d binding(s) to %s", count, path);
    else
        TEKLOG_WARN("keybind: failed to save bindings to %s", path);

    tef_save_wb_free(&seg);
}

static patch_handle_t get_current_profile(void) {
    patch_handle_t profile = PATCH_NULL;
    if (is_valid_handle(m_current_profile_getter)) {
        patchlib_method_invoke_args(m_current_profile_getter, PATCH_NULL, &profile, NULL);
        if (is_valid_handle(profile))
            return profile;
    }
    if (is_valid_handle(f_current_profile)) {
        patchlib_field_get_value(f_current_profile, PATCH_NULL, &profile);
        return profile;
    }
    return PATCH_NULL;
}

/* 读取 PlayerInput.Triggers 对象。 */
static patch_handle_t get_triggers_pack(void) {
    return read_static_object(f_triggers);
}

/* 读取 PlayerInput.KnownTriggers 列表。 */
static patch_handle_t get_known_triggers(void) {
    return read_static_object(f_known_triggers);
}

/* 把 trigger 加入 PlayerInput.KnownTriggers（不存在时）。 */
static bool ensure_known_trigger(patch_handle_t trigger) {
    patch_handle_t list = get_known_triggers();
    if (!is_valid_handle(list))
        return false;

    bool ok = true;
    if (!list_contains(list, trigger)) {
        list_add(list, trigger);
    }
    patchlib_free(list);
    return ok;
}


/* 读取实例引用字段；返回的句柄由调用方 patchlib_free。 */
static patch_handle_t read_object_field(patch_handle_t field, patch_handle_t instance) {
    patch_handle_t value = PATCH_NULL;
    if (!is_valid_handle(field) || !is_valid_handle(instance))
        return PATCH_NULL;
    patchlib_field_get_value(field, instance, &value);
    return value;
}

/* 向 KeyStatus(Dictionary<string, List<string>>) 写入默认键（不存在时）。
 * 已存在则保留玩家当前绑定，不覆盖。 */
static bool key_status_add_default(patch_handle_t key_status, patch_handle_t trigger,
                                   const char *default_key) {
    if (!is_valid_handle(key_status) || !is_valid_handle(trigger))
        return false;

    if (key_status_contains(key_status, trigger))
        return true;

    patch_handle_t list = create_string_list();
    if (!is_valid_handle(list))
        return false;

    if (default_key && default_key[0]) {
        patch_handle_t key = patchlib_string_create(default_key);
        if (is_valid_handle(key)) {
            list_add(list, key);
            patchlib_free(key);
        }
    }

    const bool ok = key_status_add(key_status, trigger, list);
    patchlib_free(list);
    return ok;
}

/* 把已注册的 trigger 注入当前 Profile 的 Keyboard / KeyboardUI KeyStatus。 */
static void inject_into_current_profile(void) {
    patch_handle_t profile = get_current_profile();
    if (!is_valid_handle(profile))
        return;

    patch_handle_t modes = read_object_field(f_profile_input_modes, profile);
    if (!is_valid_handle(modes)) {
        patchlib_free(profile);
        return;
    }

    static const int accepted_modes[] = {0 /* Keyboard */, 1 /* KeyboardUI */};
    for (size_t m = 0; m < sizeof(accepted_modes) / sizeof(accepted_modes[0]); ++m) {
        int mode = accepted_modes[m];
        patch_handle_t config = PATCH_NULL;
        if (!input_modes_get(modes, mode, &config) || !is_valid_handle(config))
            continue;

        patch_handle_t key_status = read_object_field(f_config_key_status, config);
        if (is_valid_handle(key_status)) {
            for (int i = 0; i < KEYBIND_MAX_ENTRIES; ++i) {
                keybind_entry_t *entry = &g_entries[i];
                if (!entry->used || !is_valid_handle(entry->trigger))
                    continue;
                key_status_add_default(key_status, entry->trigger, entry->default_key);
            }
            patchlib_free(key_status);
        }
        patchlib_free(config);
    }

    patchlib_free(modes);
    patchlib_free(profile);
}


static void inject_all_known_triggers(void) {
    for (int i = 0; i < KEYBIND_MAX_ENTRIES; ++i) {
        keybind_entry_t *entry = &g_entries[i];
        if (entry->used && is_valid_handle(entry->trigger))
            ensure_known_trigger(entry->trigger);
    }
}

/* 重建 PlayerInput.Triggers 的四个 TriggersSet，使新增 trigger 立刻进入字典。 */
static void rebuild_trigger_sets(void) {
    patch_handle_t pack = get_triggers_pack();
    if (!is_valid_handle(pack))
        return;
    if (is_valid_handle(m_triggers_initialize))
        patchlib_method_invoke_args(m_triggers_initialize, pack, NULL, NULL);
    patchlib_free(pack);
}

/* ------------------------------------------------------------------ */
/* 类型解析与钩子安装                                                  */
/* ------------------------------------------------------------------ */

static patch_handle_t resolve_method(patch_handle_t type, const char *name, int param_count) {
    if (!is_valid_handle(type) || !name)
        return PATCH_NULL;
    patch_handle_t method = patchlib_type_get_method_by_param_count(type, name, param_count);
    return is_valid_handle(method) ? method : PATCH_NULL;
}

static void release_handles(void) {
    patch_handle_t *handles[] = {
        &t_main,          &f_drawing_chat,       &t_player_input,          &f_triggers,
        &f_writing_text,  &f_known_triggers,     &m_current_profile_getter, &f_current_profile,
        &t_triggers_pack, &f_trig_current,       &f_trig_just_pressed,     &f_trig_just_released,
        &m_triggers_initialize, &t_triggers_set, &f_trigger_key_status,    &t_profile,
        &f_profile_input_modes, &t_key_config,   &f_config_key_status,
        &t_ui_manage_controls, &t_key_status_type, &t_trigger_status_type,
        &t_input_modes_type, &t_list_string_type, &m_ks_contains, &m_ks_add, &m_ks_get,
        &m_ts_contains, &m_ts_get, &m_im_get, &m_list_contains, &m_list_add,
        &m_list_index_of, &m_list_insert, &m_list_ctor, &m_list_count, &m_list_get,
        &str_reset_gameplay,
    };
    for (size_t i = 0; i < sizeof(handles) / sizeof(handles[0]); ++i) {
        if (is_valid_handle(*handles[i]))
            patchlib_free(*handles[i]);
        *handles[i] = PATCH_NULL;
    }
}

static bool resolve_all(void) {
    release_handles();
    TEKLOG_INFO("keybind: resolving Terraria input types...");

    t_main = patchlib_type_get_type("Terraria", "Main");
    t_player_input = patchlib_type_get_type("Terraria.GameInput", "PlayerInput");
    t_triggers_pack = patchlib_type_get_type("Terraria.GameInput", "TriggersPack");
    t_triggers_set = patchlib_type_get_type("Terraria.GameInput", "TriggersSet");
    t_profile = patchlib_type_get_type("Terraria.GameInput", "PlayerInputProfile");
    t_key_config = patchlib_type_get_type("Terraria.GameInput", "KeyConfiguration");
    t_ui_manage_controls = patchlib_type_get_type("Terraria.GameContent.UI.States",
                                                   "UIManageControls");

    if (!is_valid_handle(t_main) || !is_valid_handle(t_player_input) ||
        !is_valid_handle(t_triggers_pack) || !is_valid_handle(t_triggers_set) ||
        !is_valid_handle(t_profile) || !is_valid_handle(t_key_config) ||
        !is_valid_handle(t_ui_manage_controls)) {
        set_error("required Terraria input types not found");
        return false;
    }

    f_drawing_chat = patchlib_type_get_field(t_main, "drawingPlayerChat");

    f_triggers = patchlib_type_get_field(t_player_input, "Triggers");
    f_writing_text = patchlib_type_get_field(t_player_input, "WritingText");
    f_known_triggers = patchlib_type_get_field(t_player_input, "KnownTriggers");
    f_current_profile = patchlib_type_get_field(t_player_input, "_currentProfile");
    if (!is_valid_handle(f_current_profile)) {
        patch_handle_t prop = patchlib_type_get_property(t_player_input, "CurrentProfile");
        if (is_valid_handle(prop)) {
            m_current_profile_getter = patchlib_property_get_get_method(prop);
            patchlib_free(prop);
        }
    }

    f_trig_current = patchlib_type_get_field(t_triggers_pack, "Current");
    f_trig_just_pressed = patchlib_type_get_field(t_triggers_pack, "JustPressed");
    f_trig_just_released = patchlib_type_get_field(t_triggers_pack, "JustReleased");
    m_triggers_initialize = resolve_method(t_triggers_pack, "Initialize", 0);
    f_trigger_key_status = patchlib_type_get_field(t_triggers_set, "KeyStatus");

    f_profile_input_modes = patchlib_type_get_field(t_profile, "InputModes");
    f_config_key_status = patchlib_type_get_field(t_key_config, "KeyStatus");

    /* 闭合泛型类型：直接取自字段声明类型，避免桌面端 il2cpp_object_get_class。 */
    t_key_status_type = patchlib_field_get_type_handle(f_config_key_status);
    t_trigger_status_type = patchlib_field_get_type_handle(f_trigger_key_status);
    t_input_modes_type = patchlib_field_get_type_handle(f_profile_input_modes);
    t_list_string_type = patchlib_field_get_type_handle(f_known_triggers);
    m_ks_contains = resolve_method(t_key_status_type, "ContainsKey", 1);
    m_ks_add = resolve_method(t_key_status_type, "Add", 2);
    m_ks_get = resolve_method(t_key_status_type, "get_Item", 1);
    m_ts_contains = resolve_method(t_trigger_status_type, "ContainsKey", 1);
    m_ts_get = resolve_method(t_trigger_status_type, "get_Item", 1);
    m_im_get = resolve_method(t_input_modes_type, "get_Item", 1);
    m_list_contains = resolve_method(t_list_string_type, "Contains", 1);
    m_list_add = resolve_method(t_list_string_type, "Add", 1);
    m_list_index_of = resolve_method(t_list_string_type, "IndexOf", 1);
    m_list_insert = resolve_method(t_list_string_type, "Insert", 2);
    m_list_count = resolve_method(t_list_string_type, "get_Count", 0);
    m_list_get = resolve_method(t_list_string_type, "get_Item", 1);
    str_reset_gameplay = patchlib_string_create("sp9");
    {
        const char *ctor_args[] = {"capacity"};
        m_list_ctor = patchlib_type_get_method_by_param_names(t_list_string_type, ".ctor", 1,
                                                              ctor_args);
    }

    if (!is_valid_handle(f_drawing_chat) || !is_valid_handle(f_triggers) ||
        !is_valid_handle(f_known_triggers) || !is_valid_handle(f_trig_current) ||
        !is_valid_handle(f_trig_just_pressed) || !is_valid_handle(f_trig_just_released) ||
        !is_valid_handle(f_trigger_key_status) || !is_valid_handle(f_profile_input_modes) ||
        !is_valid_handle(f_config_key_status) ||
        (!is_valid_handle(f_current_profile) && !is_valid_handle(m_current_profile_getter))) {
        set_error("required input members not found");
        return false;
    }

    if (!is_valid_handle(t_key_status_type) || !is_valid_handle(t_trigger_status_type) ||
        !is_valid_handle(t_input_modes_type) || !is_valid_handle(t_list_string_type) ||
        !is_valid_handle(m_ks_contains) || !is_valid_handle(m_ks_add) ||
        !is_valid_handle(m_ks_get) ||
        !is_valid_handle(m_ts_contains) || !is_valid_handle(m_ts_get) ||
        !is_valid_handle(m_im_get) || !is_valid_handle(m_list_contains) ||
        !is_valid_handle(m_list_add) || !is_valid_handle(m_list_index_of) ||
        !is_valid_handle(m_list_insert) || !is_valid_handle(m_list_ctor) ||
        !is_valid_handle(m_list_count) || !is_valid_handle(m_list_get) ||
        !is_valid_handle(str_reset_gameplay)) {
        set_error("required generic input methods not found");
        return false;
    }

    patch_handle_t m_update = resolve_method(t_main, "Update", 1);
    if (!is_valid_handle(m_update))
        m_update = resolve_method(t_main, "DoUpdate", 1);
    patch_handle_t m_binding_group =
            resolve_method(t_ui_manage_controls, "CreateBindingGroup", 3);
    patch_handle_t m_player_save = resolve_method(t_player_input, "Save", 0);

    if (!is_valid_handle(m_update) || !is_valid_handle(m_binding_group) ||
        !is_valid_handle(m_player_save)) {
        set_error("required input methods not found");
        patchlib_free(m_update);
        patchlib_free(m_binding_group);
        patchlib_free(m_player_save);
        return false;
    }

    g_update_hook = patchlib_install_prepost_hook(m_update, NULL, keybind_on_update_postfix);
    g_menu_hook = patchlib_install_prepost_hook(m_binding_group,
                                                keybind_on_create_binding_group_prefix, NULL);
    g_save_hook = patchlib_install_prepost_hook(m_player_save, NULL,
                                                keybind_on_player_save_postfix);
    TEKLOG_INFO("keybind: hooks installed (update=%d, menu=%d, save=%d)", (int) g_update_hook,
                (int) g_menu_hook, (int) g_save_hook);

    patchlib_free(m_update);
    patchlib_free(m_binding_group);
    patchlib_free(m_player_save);

    if (g_update_hook == PATCH_HOOK_INVALID_ID || g_menu_hook == PATCH_HOOK_INVALID_ID ||
        g_save_hook == PATCH_HOOK_INVALID_ID) {
        set_error("failed to install keybind hooks");
        return false;
    }

    set_error(NULL);
    return true;
}


/* ------------------------------------------------------------------ */
/* 公共 API                                                           */
/* ------------------------------------------------------------------ */

void terraria_keybind_manager_init(void) {
    if (g_shutdown)
        return;
    g_installed = true;
    if (g_ready)
        return;
    keybind_store_load();
    g_ready = resolve_all();
    if (g_ready)
        TEKLOG_INFO("keybind: PC native keybind injection ready");
    else
        TEKLOG_ERROR("keybind: initialization failed: %s", g_error);
}

bool terraria_keybind_manager_available(void) {
    if (g_shutdown)
        return false;
    if (!g_ready)
        terraria_keybind_manager_init();
    return g_ready;
}

const char *terraria_keybind_manager_error(void) {
    return g_error;
}

void terraria_keybind_manager_install(void) {
    g_shutdown = false;
    terraria_keybind_manager_init();
}

void terraria_keybind_manager_shutdown(void) {
    /* 退出前把当前实际绑定写盘。 */
    keybind_store_save();

    if (g_update_hook != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(g_update_hook);
        g_update_hook = PATCH_HOOK_INVALID_ID;
    }
    if (g_menu_hook != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(g_menu_hook);
        g_menu_hook = PATCH_HOOK_INVALID_ID;
    }
    if (g_save_hook != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(g_save_hook);
        g_save_hook = PATCH_HOOK_INVALID_ID;
    }

    for (int i = 0; i < KEYBIND_MAX_ENTRIES; ++i) {
        if (is_valid_handle(g_entries[i].trigger))
            patchlib_free(g_entries[i].trigger);
        g_entries[i] = (keybind_entry_t) {0};
    }

    release_handles();
    g_ready = false;
    g_installed = false;
    g_shutdown = true;
    g_injected = false;
    g_dirty = false;
    set_error("keybind manager shut down");
}

static keybind_entry_t *find_entry(const terraria_keybind_scope_t *scope, const char *id) {
    if (!scope || !scope->modloader_id || !scope->mod_id || !id)
        return NULL;
    for (int i = 0; i < KEYBIND_MAX_ENTRIES; ++i) {
        keybind_entry_t *entry = &g_entries[i];
        if (!entry->used)
            continue;
        if (strcmp(entry->modloader_id, scope->modloader_id) == 0 &&
            strcmp(entry->mod_id, scope->mod_id) == 0 && strcmp(entry->id, id) == 0) {
            return entry;
        }
    }
    return NULL;
}

static bool valid_id(const char *value) {
    if (!value || !value[0] || strlen(value) > KEYBIND_MAX_ID)
        return false;
    for (const char *p = value; *p; ++p) {
        const char c = *p;
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        if (!ok)
            return false;
    }
    return true;
}

bool terraria_keybind_manager_register(const terraria_keybind_desc_t *desc) {
    if (!desc || !desc->id || !desc->scope.modloader_id || !desc->scope.mod_id) {
        set_error("invalid keybind descriptor");
        return false;
    }
    if (!valid_id(desc->id) || strlen(desc->scope.modloader_id) > KEYBIND_MAX_ID ||
        strlen(desc->scope.mod_id) > KEYBIND_MAX_ID) {
        set_error("keybind id/scope must be 1-64 ASCII letters, digits, _, - or .");
        return false;
    }

    const char *name = (desc->name && desc->name[0]) ? desc->name : desc->id;
    if (strlen(name) > KEYBIND_MAX_NAME) {
        set_error("keybind name must be at most 128 UTF-8 bytes");
        return false;
    }
    const char *default_key = desc->default_key ? desc->default_key : "";
    if (strlen(default_key) > KEYBIND_MAX_STORED) {
        set_error("keybind default_key is too long");
        return false;
    }

    terraria_keybind_manager_init();
    if (!g_ready || g_shutdown) {
        set_error("keybind manager unavailable");
        return false;
    }

    /* 有持久化记录时，用玩家上次保存的键覆盖默认键（空串表示“未绑定”）。 */
    const keybind_saved_t *saved =
            saved_find(desc->scope.modloader_id, desc->scope.mod_id, desc->id);
    if (saved)
        default_key = saved->key;

    /* 同一 Mod 重复注册同一 id：就地更新，不新建条目。 */
    keybind_entry_t *entry = find_entry(&desc->scope, desc->id);
    if (entry) {
        entry->on_press = desc->on_press;
        entry->on_release = desc->on_release;
        entry->user = desc->user;
        entry->enabled = desc->enabled;
        inject_into_current_profile();
        return true;
    }

    /* name 是游戏菜单里的 trigger 键，必须全局唯一。 */
    for (int i = 0; i < KEYBIND_MAX_ENTRIES; ++i) {
        if (g_entries[i].used && strcmp(g_entries[i].name, name) == 0) {
            set_error("keybind name already registered");
            return false;
        }
    }

    int slot = -1;
    for (int i = 0; i < KEYBIND_MAX_ENTRIES; ++i) {
        if (!g_entries[i].used) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        set_error("keybind table is full");
        return false;
    }

    patch_handle_t trigger = patchlib_string_create(name);
    if (!is_valid_handle(trigger)) {
        set_error("failed to create trigger string");
        return false;
    }

    entry = &g_entries[slot];
    memset(entry, 0, sizeof(*entry));
    entry->used = true;
    entry->enabled = desc->enabled;
    copy_str(entry->modloader_id, sizeof(entry->modloader_id), desc->scope.modloader_id);
    copy_str(entry->mod_id, sizeof(entry->mod_id), desc->scope.mod_id);
    copy_str(entry->id, sizeof(entry->id), desc->id);
    copy_str(entry->name, sizeof(entry->name), name);
    copy_str(entry->default_key, sizeof(entry->default_key), default_key);
    entry->on_press = desc->on_press;
    entry->on_release = desc->on_release;
    entry->user = desc->user;
    entry->trigger = trigger;
    g_dirty = true;

    TEKLOG_INFO("keybind: registered %s/%s id=%s name=%s key=%s", entry->modloader_id,
                entry->mod_id, entry->id, entry->name,
                entry->default_key[0] ? entry->default_key : "(unbound)");
    set_error(NULL);
    return true;
}

/* 从 TriggersPack 读取某个 TriggersSet 的 KeyStatus(Dictionary<string,bool>)。
 * 返回的句柄由调用方 patchlib_free。 */
static patch_handle_t read_trigger_key_status(patch_handle_t pack, patch_handle_t set_field) {
    patch_handle_t set = read_object_field(set_field, pack);
    if (!is_valid_handle(set))
        return PATCH_NULL;
    patch_handle_t dict = read_object_field(f_trigger_key_status, set);
    patchlib_free(set);
    return dict;
}

/* Main.Update 后置：读取游戏已经计算好的按键状态并派发回调。
 * 聊天/文本输入期间抑制触发，避免打字误触。 */
static void keybind_on_update_postfix(patch_handle_t instance, void **args, void *result,
                                      const patch_method_signature_t *sig) {
    (void) instance;
    (void) args;
    (void) result;
    (void) sig;

    if (!g_ready)
        return;

    ensure_game_state();

    const bool suppress = read_bool_field(f_drawing_chat) || read_bool_field(f_writing_text);

    patch_handle_t pack = get_triggers_pack();
    patch_handle_t current = PATCH_NULL;
    patch_handle_t just_pressed = PATCH_NULL;
    patch_handle_t just_released = PATCH_NULL;
    if (is_valid_handle(pack)) {
        current = read_trigger_key_status(pack, f_trig_current);
        just_pressed = read_trigger_key_status(pack, f_trig_just_pressed);
        just_released = read_trigger_key_status(pack, f_trig_just_released);
    }

    for (int i = 0; i < KEYBIND_MAX_ENTRIES; ++i) {
        keybind_entry_t *entry = &g_entries[i];
        if (!entry->used || !is_valid_handle(entry->trigger)) {
            continue;
        }

        entry->down = false;
        entry->pressed = false;
        entry->released = false;

        trigger_status_try_get(current, entry->trigger, &entry->down);
        if (!suppress) {
            trigger_status_try_get(just_pressed, entry->trigger, &entry->pressed);
            trigger_status_try_get(just_released, entry->trigger, &entry->released);
        }

        if (!entry->enabled)
            continue;

        if (entry->pressed)
            TEKLOG_INFO("keybind: '%s' pressed", entry->name);
        if (entry->released)
            TEKLOG_INFO("keybind: '%s' released", entry->name);

        terraria_keybind_scope_t scope = {entry->modloader_id, entry->mod_id};
        if (entry->pressed && entry->on_press)
            entry->on_press(&scope, entry->id, entry->user);
        if (entry->released && entry->on_release)
            entry->on_release(&scope, entry->id, entry->user);
    }

    if (is_valid_handle(current))
        patchlib_free(current);
    if (is_valid_handle(just_pressed))
        patchlib_free(just_pressed);
    if (is_valid_handle(just_released))
        patchlib_free(just_released);
    if (is_valid_handle(pack))
        patchlib_free(pack);
}

/* UIManageControls.CreateBindingGroup 前置：把自定义 trigger 追加到游戏菜单列表。
 * 只处理 elementIndex==0（Gameplay）且 Keyboard / KeyboardUI 两种输入模式。 */
static bool keybind_on_create_binding_group_prefix(patch_handle_t instance, void **args,
                                                   const patch_method_signature_t *sig,
                                                   void *result) {
    (void) instance;
    (void) sig;
    (void) result;

    if (!g_ready || !args || !args[0] || !args[1] || !args[2])
        return false;

    const int element_index = *((const int *) args[0]);
    patch_handle_t bindings = *((patch_handle_t *) args[1]);
    const int input_mode = *((const int *) args[2]);
    if (element_index != 0 || (input_mode != 0 && input_mode != 1))
        return false;
    if (!is_valid_handle(bindings))
        return false;

    /* 菜单打开时兜底：处理运行期新注册或切换 Profile 的情况。 */
    ensure_game_state();
    inject_all_known_triggers();
    inject_into_current_profile();

    int reset_index = list_index_of(bindings, str_reset_gameplay);
    for (int i = 0; i < KEYBIND_MAX_ENTRIES; ++i) {
        keybind_entry_t *entry = &g_entries[i];
        if (!entry->used || !is_valid_handle(entry->trigger))
            continue;
        if (list_contains(bindings, entry->trigger))
            continue;
        if (reset_index >= 0) {
            /* 插在“重置为默认(sp9)”之前，避免出现在重置按钮下面。 */
            list_insert(bindings, reset_index, entry->trigger);
            ++reset_index;
        } else {
            list_add(bindings, entry->trigger);
        }
    }
    return false;
}

/* PlayerInput.Save 后置：游戏保存设置时同步写入键位配置。 */
static void keybind_on_player_save_postfix(patch_handle_t instance, void **args, void *result,
                                           const patch_method_signature_t *sig) {
    (void) instance;
    (void) args;
    (void) result;
    (void) sig;
    keybind_store_save();
}

/* 第一帧 Main.Update 后惰性注入：此时 PlayerInput / Profiles 已就绪。
 * 不在 PlayerInput.Initialize / Load 阶段做任何事，避免窗口初始化期崩溃。 */
static void ensure_game_state(void) {
    if (!g_ready || (!g_dirty && g_injected))
        return;

    patch_handle_t profile = get_current_profile();
    if (!is_valid_handle(profile))
        return;
    patchlib_free(profile);

    inject_all_known_triggers();
    inject_into_current_profile();
    rebuild_trigger_sets();

    int count = 0;
    for (int i = 0; i < KEYBIND_MAX_ENTRIES; ++i) {
        if (g_entries[i].used)
            ++count;
    }
    TEKLOG_INFO("keybind: injected %d binding(s) into current profile", count);

    g_injected = true;
    g_dirty = false;
}

/* ------------------------------------------------------------------ */
/* 查询 / 注销                                                         */
/* ------------------------------------------------------------------ */

bool terraria_keybind_manager_unregister(const terraria_keybind_scope_t *scope, const char *id) {
    keybind_entry_t *entry = find_entry(scope, id);
    if (!entry)
        return false;
    if (is_valid_handle(entry->trigger))
        patchlib_free(entry->trigger);
    memset(entry, 0, sizeof(*entry));
    return true;
}

void terraria_keybind_manager_unregister_owner(const terraria_keybind_scope_t *scope) {
    if (!scope || !scope->modloader_id || !scope->mod_id)
        return;
    for (int i = 0; i < KEYBIND_MAX_ENTRIES; ++i) {
        keybind_entry_t *entry = &g_entries[i];
        if (!entry->used)
            continue;
        if (strcmp(entry->modloader_id, scope->modloader_id) == 0 &&
            strcmp(entry->mod_id, scope->mod_id) == 0) {
            if (is_valid_handle(entry->trigger))
                patchlib_free(entry->trigger);
            memset(entry, 0, sizeof(*entry));
        }
    }
}

bool terraria_keybind_manager_down(const terraria_keybind_scope_t *scope, const char *id) {
    keybind_entry_t *entry = find_entry(scope, id);
    return entry && entry->down;
}

bool terraria_keybind_manager_pressed(const terraria_keybind_scope_t *scope, const char *id) {
    keybind_entry_t *entry = find_entry(scope, id);
    return entry && entry->pressed;
}

bool terraria_keybind_manager_released(const terraria_keybind_scope_t *scope, const char *id) {
    keybind_entry_t *entry = find_entry(scope, id);
    return entry && entry->released;
}

bool terraria_keybind_manager_set_enabled(const terraria_keybind_scope_t *scope, const char *id,
                                          bool value) {
    keybind_entry_t *entry = find_entry(scope, id);
    if (!entry)
        return false;
    entry->enabled = value;
    if (!value) {
        entry->pressed = false;
        entry->released = false;
    }
    return true;
}

#endif /* !__ANDROID__ */

