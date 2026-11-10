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
#include <string.h>

#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/il2cpp_api.h"
#include "patchlib/method.h"
#include "patchlib/property.h"
#include "patchlib/struct/dictionary.h"
#include "patchlib/struct/list.h"
#include "patchlib/struct/string.h"
#include "patchlib/type.h"

#define KEYBIND_MAX_ENTRIES 128
#define KEYBIND_MAX_ID 64
#define KEYBIND_MAX_NAME 128
#define KEYBIND_MAX_STORED 256

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
static bool g_installed;
static bool g_ready;
static char g_error[256] = "keybind manager not initialized";

static patch_hook_id_t g_update_hook = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t g_menu_hook = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t g_player_init_hook = PATCH_HOOK_INVALID_ID;
static patch_hook_id_t g_player_load_hook = PATCH_HOOK_INVALID_ID;

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

static patch_handle_t t_string;
static patch_handle_t t_ui_manage_controls;

/* 前向声明：钩子回调与 Triggers 重建在 resolve_all 之后定义。 */
static void keybind_on_update_postfix(patch_handle_t instance, void **args, void *result,
                                      const patch_method_signature_t *sig);
static bool keybind_on_create_binding_group_prefix(patch_handle_t instance, void **args,
                                                   const patch_method_signature_t *sig,
                                                   void *result);
static bool keybind_on_player_init_prefix(patch_handle_t instance, void **args,
                                          const patch_method_signature_t *sig, void *result);
static void keybind_on_player_init_postfix(patch_handle_t instance, void **args, void *result,
                                           const patch_method_signature_t *sig);
static void keybind_on_player_load_postfix(patch_handle_t instance, void **args, void *result,
                                           const patch_method_signature_t *sig);
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
    uint8_t value = 0;
    patchlib_field_get_value(field, PATCH_NULL, &value);
    return value != 0;
}

/* 读取静态引用字段；返回的句柄由调用方 patchlib_free。 */
static patch_handle_t read_static_object(patch_handle_t field) {
    patch_handle_t value = PATCH_NULL;
    if (!is_valid_handle(field))
        return PATCH_NULL;
    patchlib_field_get_value(field, PATCH_NULL, &value);
    return value;
}


/* List<T>.Contains(value) —— value 需为对应元素的句柄指针（如 string 句柄）。 */
static bool list_contains(patch_handle_t list, patch_handle_t value) {
    if (!is_valid_handle(list) || !is_valid_handle(value))
        return false;

    patch_handle_t klass = il2cpp_object_get_class(list);
    if (!is_valid_handle(klass))
        return false;

    patch_handle_t method = patchlib_type_get_method_by_param_count(klass, "Contains", 1);
    bool found = false;
    if (is_valid_handle(method)) {
        void *args[1] = {&value};
        patchlib_method_invoke_args(method, list, &found, args);
        patchlib_free(method);
    }
    patchlib_free(klass);
    return found;
}

/* Dictionary<string, T>.ContainsKey(key) */
static bool dictionary_contains_key(patch_handle_t dict, patch_handle_t key) {
    if (!is_valid_handle(dict) || !is_valid_handle(key))
        return false;

    patch_handle_t klass = il2cpp_object_get_class(dict);
    if (!is_valid_handle(klass))
        return false;

    patch_handle_t method = patchlib_type_get_method_by_param_count(klass, "ContainsKey", 1);
    bool found = false;
    if (is_valid_handle(method)) {
        void *args[1] = {&key};
        patchlib_method_invoke_args(method, dict, &found, args);
        patchlib_free(method);
    }
    patchlib_free(klass);
    return found;
}

/* Dictionary<string, bool>.TryGetValue(key, out value) */
static bool dictionary_try_get_bool(patch_handle_t dict, patch_handle_t key, bool *out_value) {
    if (out_value)
        *out_value = false;
    if (!is_valid_handle(dict) || !is_valid_handle(key) || !out_value)
        return false;

    patch_handle_t klass = il2cpp_object_get_class(dict);
    if (!is_valid_handle(klass))
        return false;

    patch_handle_t method = patchlib_type_get_method_by_param_count(klass, "TryGetValue", 2);
    bool result = false;
    if (is_valid_handle(method)) {
        uint8_t value = 0;
        void *args[2] = {&key, &value};
        if (patchlib_method_invoke_args(method, dict, &result, args) && result)
            *out_value = value != 0;
        patchlib_free(method);
    }
    patchlib_free(klass);
    return result;
}

/* 读取 PlayerInput.CurrentProfile（属性 getter 优先，退回 _currentProfile 字段）。 */
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
        if (!patchlib_list_add(list, &trigger))
            ok = false;
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

    if (dictionary_contains_key(key_status, trigger))
        return true;

    patch_handle_t list = patchlib_list_create(0, t_string);
    if (!is_valid_handle(list))
        return false;

    if (default_key && default_key[0]) {
        patch_handle_t key = patchlib_string_create(default_key);
        if (is_valid_handle(key)) {
            patchlib_list_add(list, &key);
            patchlib_free(key);
        }
    }

    const bool ok = patchlib_dictionary_add(key_status, &trigger, &list);
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
        if (!patchlib_dictionary_get_value(modes, &mode, &config) || !is_valid_handle(config))
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
        &f_profile_input_modes, &t_key_config,   &f_config_key_status,     &t_string,
        &t_ui_manage_controls,
    };
    for (size_t i = 0; i < sizeof(handles) / sizeof(handles[0]); ++i) {
        if (is_valid_handle(*handles[i]))
            patchlib_free(*handles[i]);
        *handles[i] = PATCH_NULL;
    }
}

static bool resolve_all(void) {
    release_handles();

    t_main = patchlib_type_get_type("Terraria", "Main");
    t_player_input = patchlib_type_get_type("Terraria.GameInput", "PlayerInput");
    t_triggers_pack = patchlib_type_get_type("Terraria.GameInput", "TriggersPack");
    t_triggers_set = patchlib_type_get_type("Terraria.GameInput", "TriggersSet");
    t_profile = patchlib_type_get_type("Terraria.GameInput", "PlayerInputProfile");
    t_key_config = patchlib_type_get_type("Terraria.GameInput", "KeyConfiguration");
    t_string = patchlib_type_get_type("System", "String");
    t_ui_manage_controls = patchlib_type_get_type("Terraria.GameContent.UI.States",
                                                   "UIManageControls");

    if (!is_valid_handle(t_main) || !is_valid_handle(t_player_input) ||
        !is_valid_handle(t_triggers_pack) || !is_valid_handle(t_triggers_set) ||
        !is_valid_handle(t_profile) || !is_valid_handle(t_key_config) ||
        !is_valid_handle(t_string) || !is_valid_handle(t_ui_manage_controls)) {
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

    if (!is_valid_handle(f_drawing_chat) || !is_valid_handle(f_triggers) ||
        !is_valid_handle(f_known_triggers) || !is_valid_handle(f_trig_current) ||
        !is_valid_handle(f_trig_just_pressed) || !is_valid_handle(f_trig_just_released) ||
        !is_valid_handle(f_trigger_key_status) || !is_valid_handle(f_profile_input_modes) ||
        !is_valid_handle(f_config_key_status) ||
        (!is_valid_handle(f_current_profile) && !is_valid_handle(m_current_profile_getter))) {
        set_error("required input members not found");
        return false;
    }

    patch_handle_t m_update = resolve_method(t_main, "Update", 1);
    if (!is_valid_handle(m_update))
        m_update = resolve_method(t_main, "DoUpdate", 1);
    patch_handle_t m_binding_group =
            resolve_method(t_ui_manage_controls, "CreateBindingGroup", 3);
    patch_handle_t m_player_init = resolve_method(t_player_input, "Initialize", 0);
    patch_handle_t m_player_load = resolve_method(t_player_input, "Load", 0);

    if (!is_valid_handle(m_update) || !is_valid_handle(m_binding_group) ||
        !is_valid_handle(m_player_init) || !is_valid_handle(m_player_load)) {
        set_error("required input methods not found");
        patchlib_free(m_update);
        patchlib_free(m_binding_group);
        patchlib_free(m_player_init);
        patchlib_free(m_player_load);
        return false;
    }

    g_update_hook = patchlib_install_prepost_hook(m_update, NULL, keybind_on_update_postfix);
    g_menu_hook = patchlib_install_prepost_hook(m_binding_group,
                                                keybind_on_create_binding_group_prefix, NULL);
    g_player_init_hook = patchlib_install_prepost_hook(m_player_init,
                                                       keybind_on_player_init_prefix,
                                                       keybind_on_player_init_postfix);
    g_player_load_hook = patchlib_install_prepost_hook(m_player_load, NULL,
                                                       keybind_on_player_load_postfix);

    patchlib_free(m_update);
    patchlib_free(m_binding_group);
    patchlib_free(m_player_init);
    patchlib_free(m_player_load);

    if (g_update_hook == PATCH_HOOK_INVALID_ID || g_menu_hook == PATCH_HOOK_INVALID_ID ||
        g_player_init_hook == PATCH_HOOK_INVALID_ID ||
        g_player_load_hook == PATCH_HOOK_INVALID_ID) {
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
    g_installed = true;
    if (g_ready)
        return;
    g_ready = resolve_all();
    if (g_ready)
        TEKLOG_INFO("keybind: PC native keybind injection ready");
    else
        TEKLOG_ERROR("keybind: initialization failed: %s", g_error);
}

bool terraria_keybind_manager_available(void) {
    if (!g_ready)
        terraria_keybind_manager_init();
    return g_ready;
}

const char *terraria_keybind_manager_error(void) {
    return g_error;
}

void terraria_keybind_manager_install(void) {
    terraria_keybind_manager_init();
}

void terraria_keybind_manager_shutdown(void) {
    if (g_update_hook != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(g_update_hook);
        g_update_hook = PATCH_HOOK_INVALID_ID;
    }
    if (g_menu_hook != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(g_menu_hook);
        g_menu_hook = PATCH_HOOK_INVALID_ID;
    }
    if (g_player_init_hook != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(g_player_init_hook);
        g_player_init_hook = PATCH_HOOK_INVALID_ID;
    }
    if (g_player_load_hook != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(g_player_load_hook);
        g_player_load_hook = PATCH_HOOK_INVALID_ID;
    }

    for (int i = 0; i < KEYBIND_MAX_ENTRIES; ++i) {
        if (is_valid_handle(g_entries[i].trigger))
            patchlib_free(g_entries[i].trigger);
        g_entries[i] = (keybind_entry_t) {0};
    }

    release_handles();
    g_ready = false;
    g_installed = false;
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
    if (!g_ready) {
        set_error("keybind manager unavailable");
        return false;
    }

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

    /* 进入 KnownTriggers + 重建 TriggersSet，再写入当前 Profile。 */
    const bool known = ensure_known_trigger(trigger);
    if (known) {
        rebuild_trigger_sets();
        inject_into_current_profile();
    }

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

        dictionary_try_get_bool(current, entry->trigger, &entry->down);
        if (!suppress) {
            dictionary_try_get_bool(just_pressed, entry->trigger, &entry->pressed);
            dictionary_try_get_bool(just_released, entry->trigger, &entry->released);
        }

        if (!entry->enabled)
            continue;

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

    for (int i = 0; i < KEYBIND_MAX_ENTRIES; ++i) {
        keybind_entry_t *entry = &g_entries[i];
        if (!entry->used || !is_valid_handle(entry->trigger))
            continue;
        if (list_contains(bindings, entry->trigger))
            continue;
        patchlib_list_add(bindings, &entry->trigger);
    }
    return false;
}

/* PlayerInput.Initialize 前置：确保早期注册的 trigger 在 Profile 构建前进入 KnownTriggers。 */
static bool keybind_on_player_init_prefix(patch_handle_t instance, void **args,
                                          const patch_method_signature_t *sig, void *result) {
    (void) instance;
    (void) args;
    (void) sig;
    (void) result;
    if (g_ready)
        inject_all_known_triggers();
    return false;
}

/* PlayerInput.Initialize 后置：兜底补齐 Profile / TriggersSet（前缀可能因类未初始化而拿不到列表）。 */
static void keybind_on_player_init_postfix(patch_handle_t instance, void **args, void *result,
                                           const patch_method_signature_t *sig) {
    (void) instance;
    (void) args;
    (void) result;
    (void) sig;
    if (!g_ready)
        return;
    inject_all_known_triggers();
    inject_into_current_profile();
    rebuild_trigger_sets();
}

/* PlayerInput.Load 后置：读档后重新把 trigger 注入新的 CurrentProfile。 */
static void keybind_on_player_load_postfix(patch_handle_t instance, void **args, void *result,
                                           const patch_method_signature_t *sig) {
    (void) instance;
    (void) args;
    (void) result;
    (void) sig;
    if (!g_ready)
        return;
    inject_all_known_triggers();
    inject_into_current_profile();
    rebuild_trigger_sets();
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

