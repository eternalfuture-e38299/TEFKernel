#include "terraria/content/button_manager.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "button_images.h"
#include "button_hook_args.h"
#include "button_log.h"
#include "button_profile_cache.h"
#include "button_scroll.h"
#include "button_state.h"
#include "button_store.h"
#include "patchlib/field.h"
#include "patchlib/il2cpp_api.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"
#include "patchlib/struct/string.h"
#include "patchlib/type.h"

#define BUTTON_MAX_HOOKS 40
#define BUTTON_MAX_REGIONS 512
#define BUTTON_MAX_CLAIMS 32
#define BUTTON_MAX_RENDER_BINDINGS 512
#define BUTTON_MAX_DRAW_STACK 16
#define BUTTON_MAX_NATIVE_OPTIONS 32
#define BUTTON_MAX_NATIVE_CELLS 512
#define BUTTON_MAX_GROUP_INPUTS 16
#define BUTTON_MAX_GROUP_BINDINGS 64
#define BUTTON_MAX_SCROLL_CLAIMS 32
#define BUTTON_MAX_CHANGES 16

typedef struct button_vec2_t {
    float x;
    float y;
} button_vec2_t;

typedef struct button_rect_t {
    int x;
    int y;
    int width;
    int height;
} button_rect_t;

typedef struct button_color_t {
    unsigned char a;
    unsigned char b;
    unsigned char g;
    unsigned char r;
} button_color_t;

typedef struct button_api_t {
    patch_handle_t main;
    patch_handle_t styles;
    patch_handle_t binding;
    patch_handle_t slot;
    patch_handle_t slot_type;
    patch_handle_t panel;
    patch_handle_t setting;
    patch_handle_t quick;
    patch_handle_t editor;
    patch_handle_t interface_edit;
    patch_handle_t cursor;
    patch_handle_t manager;
    patch_handle_t batch;
    patch_handle_t width;
    patch_handle_t menu;
    patch_handle_t menu_getter;
    patch_handle_t styles_instance;
    patch_handle_t active_profile;
    patch_handle_t profile_id;
    patch_handle_t profile_controls;
    patch_handle_t binding_slot;
    patch_handle_t binding_action;
    patch_handle_t binding_groups;
    patch_handle_t slot_id;
    patch_handle_t slot_style;
    patch_handle_t slot_kind;
    patch_handle_t slot_multi;
    patch_handle_t active_control;
    patch_handle_t action_binding;
    patch_handle_t display_options;
    patch_handle_t selected_position;
    patch_handle_t gui_active;
    patch_handle_t gui_edit;
    patch_handle_t edit_enabled;
    patch_handle_t gui_runtime;
    patch_handle_t ui_open;
    patch_handle_t manager_instance;
    patch_handle_t cursor_id;
    patch_handle_t cursor_pos;
    patch_handle_t cursor_down;
    patch_handle_t cursor_old;
    patch_handle_t cursor_ignore;
    patch_handle_t cursor_world;
    patch_handle_t cursor_layer;
    patch_handle_t group_capture;
    patch_handle_t rect;
    patch_handle_t text;
    patch_handle_t get_region;
    patch_handle_t transaction_region;
    patch_handle_t grid_horizontal;
    patch_handle_t grid_line_count;
    patch_handle_t adjust;
    patch_handle_t count;
    patch_handle_t at;
    patch_handle_t update_value;
    patch_handle_t close_options;
    patch_handle_t is_over_options;
    patch_handle_t add_undo;
    patch_handle_t has_undo;
    patch_handle_t has_redo;
    patch_handle_t control_options;
} button_api_t;

typedef struct button_region_t {
    button_rect_t bounds;
    char key[513];
    char profile[1024];
    char slot[1024];
    unsigned long long tick;
    bool multi;
} button_region_t;

typedef struct button_claim_t {
    int cursor;
    bool cancelled;
    bool released;
    button_region_t region;
} button_claim_t;

typedef struct button_draw_context_t {
    char key[513];
    char slot[1024];
    bool multi;
} button_draw_context_t;

typedef struct button_native_options_t {
    bool used;
    patch_handle_t setting;
    patch_handle_t original;
    patch_handle_t expanded;
    void *setting_pin;
    void *original_pin;
    void *expanded_pin;
    button_scroll_window_t window;
    button_scroll_gesture_t gesture;
    int built_row;
    int built_columns;
    int built_rows;
    int built_total;
    int built_generation;
    bool built_horizontal;
    button_rect_t cells[BUTTON_MAX_NATIVE_CELLS];
    int cell_ids[BUTTON_MAX_NATIVE_CELLS];
    size_t cell_count;
    button_rect_t bounds;
    button_rect_t rail;
    float pitch;
    unsigned long long last_draw;
    unsigned long long block_until;
    char pressed_slot[1024];
    char pressed_profile[1024];
} button_native_options_t;

typedef struct button_native_action_draw_t {
    patch_handle_t setting;
    int cell;
} button_native_action_draw_t;

typedef struct button_quick_draw_t {
    button_draw_context_t context;
    bool radial;
    bool pressed;
} button_quick_draw_t;

typedef struct button_group_draw_t {
    button_draw_context_t entries[4];
    size_t next;
    size_t restore_from;
} button_group_draw_t;

typedef struct button_group_input_t {
    bool used;
    patch_handle_t binding;
    patch_handle_t config;
    int slot;
    char profile[1024];
    char id[1536];
    unsigned long long tick;
    void *binding_pin;
    void *config_pin;
} button_group_input_t;

typedef struct button_render_group_t {
    patch_handle_t array;
    size_t index;
} button_render_group_t;

typedef struct button_scroll_claim_t {
    int cursor;
    patch_handle_t setting;
} button_scroll_claim_t;

typedef struct button_change_t {
    char profile[1024];
    char slot[1024];
} button_change_t;

typedef struct button_radial_held_t {
    char key[513];
    unsigned long long tick;
} button_radial_held_t;

static button_api_t api;
static button_state_t state;
static patch_hook_id_t hooks[BUTTON_MAX_HOOKS];
static size_t hook_count;
static bool ready;
static char reason[256] = "native touch bindings require Android arm64";
static char storage_directory[1024];
static unsigned long long image_serial;
static unsigned long long tick;
static int action_generation;
static button_profile_cache_t profile_cache;

static button_region_t regions[BUTTON_MAX_REGIONS];
static size_t region_count;
static button_claim_t claims[BUTTON_MAX_CLAIMS];
static size_t claim_count;
static button_draw_context_t drawing[BUTTON_MAX_DRAW_STACK];
static size_t drawing_count;
static button_quick_draw_t quick_drawing[BUTTON_MAX_DRAW_STACK];
static size_t quick_drawing_count;
static button_group_draw_t group_drawing[BUTTON_MAX_DRAW_STACK];
static size_t group_drawing_count;
static button_group_input_t group_inputs[BUTTON_MAX_GROUP_INPUTS];
static button_render_group_t render_groups[BUTTON_MAX_GROUP_BINDINGS];
static size_t render_group_count;
static patch_handle_t render_bindings[BUTTON_MAX_RENDER_BINDINGS];
static size_t render_binding_count;
static unsigned runtime_depth;
static bool normal_held[BUTTON_STATE_MAX_ACTIONS];
static button_radial_held_t radial_held[BUTTON_STATE_MAX_ACTIONS];
static size_t radial_held_count;
static button_native_options_t native_options[BUTTON_MAX_NATIVE_OPTIONS];
static button_native_action_draw_t native_action_draws[BUTTON_MAX_DRAW_STACK];
static size_t native_action_draw_count;
static button_scroll_claim_t scroll_claims[BUTTON_MAX_SCROLL_CLAIMS];
static size_t scroll_claim_count;
static bool selecting;
static bool history_transition;
static button_change_t changes[BUTTON_MAX_CHANGES];
static size_t change_count;

static const int unbound_action = 28;
static const int carrier_action = 27;
static const int native_action_base = 65536;
static const int empty_option = -1;

static patch_handle_t need_handle(patch_handle_t handle, const char *name) {
    if (!handle || !patchlib_is_valid(handle)) {
        snprintf(reason, sizeof(reason), "missing %s", name ? name : "handle");
        return PATCH_NULL;
    }
    return handle;
}

static patch_handle_t type_handle(const char *name, const char *namespace_name) {
    return need_handle(patchlib_type_get_type(namespace_name, name), name);
}

static patch_handle_t field_handle(patch_handle_t type, const char *name) {
    return need_handle(patchlib_type_get_field(type, name), name);
}

static patch_handle_t method_handle(patch_handle_t type, const char *name,
                                    int parameter_count) {
    return need_handle(
        patchlib_type_get_method_by_param_count(type, name, parameter_count),
        name);
}

static void copy_text(char *destination, size_t size, const char *source) {
    if (!destination || size == 0) return;
    if (!source) source = "";
    snprintf(destination, size, "%s", source);
}

static bool field_read(patch_handle_t field, patch_handle_t object, void *value,
                       size_t size) {
    if (!field || !value || size == 0) return false;
    void *pointer = patchlib_field_get_pointer(field, object);
    if (!pointer) return false;
    memcpy(value, pointer, size);
    return true;
}

static patch_handle_t field_read_handle(patch_handle_t field,
                                        patch_handle_t object) {
    patch_handle_t value = PATCH_NULL;
    field_read(field, object, &value, sizeof(value));
    return value;
}

static bool field_read_bool(patch_handle_t field, patch_handle_t object) {
    bool value = false;
    field_read(field, object, &value, sizeof(value));
    return value;
}

static int field_read_int(patch_handle_t field, patch_handle_t object) {
    int value = 0;
    field_read(field, object, &value, sizeof(value));
    return value;
}

static bool field_write(patch_handle_t field, patch_handle_t object,
                        const void *value, size_t size) {
    if (!field || !value || size == 0) return false;
    void *pointer = patchlib_field_get_pointer(field, object);
    if (!pointer) return false;
    memcpy(pointer, value, size);
    return true;
}

static bool invoke_value(patch_handle_t method, patch_handle_t object,
                         void *result, size_t result_size, void **arguments) {
    uintptr_t storage[16];
    memset(storage, 0, sizeof(storage));
    if (!method || !patchlib_method_invoke_args(method, object, storage,
                                                arguments)) {
        return false;
    }
    if (result && result_size) memcpy(result, storage, result_size);
    return true;
}

static patch_handle_t invoke_handle(patch_handle_t method, patch_handle_t object,
                                    void **arguments) {
    patch_handle_t result = PATCH_NULL;
    invoke_value(method, object, &result, sizeof(result), arguments);
    return result;
}

static int invoke_int(patch_handle_t method, patch_handle_t object,
                      void **arguments) {
    uintptr_t result = 0;
    invoke_value(method, object, &result, sizeof(result), arguments);
    return (int)result;
}

static bool invoke_bool(patch_handle_t method, patch_handle_t object,
                        void **arguments) {
    uintptr_t result = 0;
    invoke_value(method, object, &result, sizeof(result), arguments);
    return result != 0;
}

static void invoke_void(patch_handle_t method, patch_handle_t object,
                        void **arguments) {
    invoke_value(method, object, NULL, 0, arguments);
}

static char *managed_string(patch_handle_t string) {
    if (!string) return NULL;
    return patchlib_string_cstr(string);
}

static void copy_managed_string(char *destination, size_t size,
                                patch_handle_t string) {
    char *value = managed_string(string);
    copy_text(destination, size, value ? value : "");
    free(value);
}

static patch_handle_t profile_handle(void) {
    patch_handle_t styles = field_read_handle(api.styles_instance, PATCH_NULL);
    return styles ? invoke_handle(api.active_profile, styles, NULL) : PATCH_NULL;
}

static void profile_key_provider(char *output, size_t size, void *user) {
    (void)user;
    copy_text(output, size, "");
    patch_handle_t profile = profile_handle();
    if (profile) copy_managed_string(output, size,
                                     field_read_handle(api.profile_id, profile));
}

static void profile_key(char *output, size_t size) {
    button_profile_cache_get(&profile_cache, tick, profile_key_provider, NULL,
                             output, size);
}

static patch_handle_t gui_object(void) {
    return field_read_handle(api.gui_active, PATCH_NULL);
}

static patch_handle_t edit_object(void) {
    patch_handle_t gui = gui_object();
    return gui ? field_read_handle(api.gui_edit, gui) : PATCH_NULL;
}

static bool in_menu(void) {
    if (api.menu && patchlib_is_valid(api.menu)) {
        return field_read_bool(api.menu, PATCH_NULL);
    }
    return api.menu_getter && invoke_bool(api.menu_getter, PATCH_NULL, NULL);
}

static bool editing(void) {
    patch_handle_t edit = edit_object();
    return edit && field_read_bool(api.edit_enabled, edit);
}

static bool ui_blocked(void) {
    patch_handle_t gui = gui_object();
    patch_handle_t controller =
        gui ? field_read_handle(api.gui_runtime, gui) : PATCH_NULL;
    return !controller || invoke_bool(api.ui_open, controller, NULL);
}

static bool contains(button_rect_t rect, float x, float y) {
    return rect.width > 0 && rect.height > 0 && x >= (float)rect.x &&
           y >= (float)rect.y && x < (float)(rect.x + rect.width) &&
           y < (float)(rect.y + rect.height);
}

static button_color_t make_color(int red, int green, int blue, int alpha) {
    button_color_t color;
    color.a = (unsigned char)alpha;
    color.b = (unsigned char)blue;
    color.g = (unsigned char)green;
    color.r = (unsigned char)red;
    return color;
}

static void group_slot(const char *slot, int index, char *output, size_t size) {
    snprintf(output, size, "%s::group:%d", slot ? slot : "", index);
}

static button_action_t *action_by_index(int index) {
    if (index < 0 || (size_t)index >= state.action_count) return NULL;
    return &state.actions[index];
}

static int action_index_from_native_id(int id) {
    if (id < native_action_base) return -1;
    const int index = id - native_action_base;
    return (size_t)index < state.action_count ? index : -1;
}

static button_action_t *action_from_native_id(int id) {
    const int index = action_index_from_native_id(id);
    return action_by_index(index);
}

static int setting_index(patch_handle_t setting) {
    const int kind = field_read_int(api.action_binding, setting);
    if (kind == 1) return -1;
    if (kind >= 2 && kind <= 5) return kind - 2;
    return -2;
}

static bool render_binding_contains(patch_handle_t binding) {
    for (size_t i = 0; i < render_binding_count; ++i) {
        if (render_bindings[i] == binding) return true;
    }
    return false;
}

static bool render_group_contains(patch_handle_t array, size_t index) {
    for (size_t i = 0; i < render_group_count; ++i) {
        if (render_groups[i].array == array && render_groups[i].index == index) {
            return true;
        }
    }
    return false;
}

static patch_handle_t native_binding(const char *slot) {
    patch_handle_t profile = profile_handle();
    patch_handle_t controls =
        profile ? field_read_handle(api.profile_controls, profile) : PATCH_NULL;
    if (!controls) return PATCH_NULL;
    const size_t count = patchlib_array_length(controls);
    const size_t limit = count < 2048 ? count : 2048;
    for (size_t i = 0; i < limit; ++i) {
        patch_handle_t binding = PATCH_NULL;
        if (!patchlib_array_at(controls, i, &binding) || !binding) continue;
        char binding_slot[1024];
        copy_managed_string(binding_slot, sizeof(binding_slot),
                            field_read_handle(api.binding_slot, binding));
        if (strcmp(binding_slot, slot) == 0) return binding;
    }
    return PATCH_NULL;
}

static void custom_binding(patch_handle_t binding, int group_index, char *output,
                           size_t size) {
    copy_text(output, size, "");
    if (!binding) return;
    char slot[1024];
    copy_managed_string(slot, sizeof(slot),
                        field_read_handle(api.binding_slot, binding));
    if (slot[0] == '\0') return;

    int action = unbound_action;
    char lookup_slot[1024];
    if (group_index >= 0) {
        patch_handle_t groups =
            field_read_handle(api.binding_groups, binding);
        if (!groups || group_index >= 4 ||
            (size_t)group_index >= patchlib_array_length(groups) ||
            !patchlib_array_at(groups, (size_t)group_index, &action)) {
            return;
        }
        const bool temporary =
            render_group_contains(groups, (size_t)group_index);
        if (action != unbound_action &&
            !(temporary && action == carrier_action)) {
            return;
        }
        group_slot(slot, group_index, lookup_slot, sizeof(lookup_slot));
    } else {
        action = field_read_int(api.binding_action, binding);
        if (action != unbound_action &&
            !(action == carrier_action && render_binding_contains(binding))) {
            return;
        }
        copy_text(lookup_slot, sizeof(lookup_slot), slot);
    }

    char profile[1024];
    profile_key(profile, sizeof(profile));
    copy_text(output, size,
              button_state_get_binding(&state, profile, lookup_slot));
}

static void custom_slot(patch_handle_t slot, char *output, size_t size) {
    copy_text(output, size, "");
    if (!slot) return;
    patch_handle_t style = field_read_handle(api.slot_style, slot);
    if (!style || field_read_int(api.slot_kind, style) != 1) return;

    char slot_id[1024];
    copy_managed_string(slot_id, sizeof(slot_id),
                        field_read_handle(api.slot_id, slot));
    patch_handle_t binding = native_binding(slot_id);
    custom_binding(binding, -1, output, size);
}

static patch_handle_t active_control(void) {
    return field_read_handle(api.active_control, PATCH_NULL);
}

static void active_slot(char *output, size_t size) {
    patch_handle_t binding = active_control();
    copy_text(output, size, "");
    if (binding) {
        copy_managed_string(output, size,
                            field_read_handle(api.binding_slot, binding));
    }
}

static void active_key(patch_handle_t setting, char *output, size_t size) {
    const int index = setting ? setting_index(setting) : -1;
    if (index == -2) {
        copy_text(output, size, "");
        return;
    }
    custom_binding(active_control(), index, output, size);
}

static void setting_slot(patch_handle_t setting, char *output, size_t size) {
    char slot[1024];
    active_slot(slot, sizeof(slot));
    const int index = setting_index(setting);
    if (index >= 0 && slot[0] != '\0') {
        group_slot(slot, index, output, size);
    } else {
        copy_text(output, size, slot);
    }
}

static void drawing_clear(void) {
    drawing_count = 0;
    quick_drawing_count = 0;
    group_drawing_count = 0;
}

static void normal_held_clear(void) {
    memset(normal_held, 0, sizeof(normal_held));
}

static void radial_held_clear(void) {
    memset(radial_held, 0, sizeof(radial_held));
    radial_held_count = 0;
}

static void release_group_input(button_group_input_t *input) {
    if (!input) return;
    if (input->binding_pin && il2cpp_gchandle_free) {
        il2cpp_gchandle_free(input->binding_pin);
    }
    if (input->config_pin && il2cpp_gchandle_free) {
        il2cpp_gchandle_free(input->config_pin);
    }
    memset(input, 0, sizeof(*input));
}

static void clear_input(void) {
    region_count = 0;
    claim_count = 0;
    for (size_t i = 0; i < BUTTON_MAX_GROUP_INPUTS; ++i) {
        release_group_input(&group_inputs[i]);
    }
    normal_held_clear();
    radial_held_clear();
    button_state_reset_input(&state);
}

static void sync_input(void) {
    bool held[BUTTON_STATE_MAX_ACTIONS];
    memset(held, 0, sizeof(held));
    for (size_t i = 0; i < state.action_count; ++i) held[i] = normal_held[i];
    for (size_t i = 0; i < radial_held_count;) {
        button_action_t *action =
            button_state_find_action_by_key(&state, radial_held[i].key);
        if (!action || tick - radial_held[i].tick > 1) {
            radial_held[i] = radial_held[--radial_held_count];
            continue;
        }
        const size_t index = (size_t)(action - state.actions);
        held[index] = true;
        ++i;
    }
    if (in_menu() || editing() || ui_blocked()) memset(held, 0, sizeof(held));
    button_state_sync_input(&state, held);
}

static patch_handle_t sprite_batch(void) {
    return field_read_handle(api.batch, PATCH_NULL);
}

static void draw_rectangle(button_rect_t rect, button_color_t color) {
    patch_handle_t batch = sprite_batch();
    if (!batch || rect.width <= 0 || rect.height <= 0) return;
    typedef void (*draw_rect_fn_t)(void *, button_rect_t, button_color_t);
    draw_rect_fn_t draw =
        (draw_rect_fn_t)patchlib_method_get_pointer(api.rect);
    if (draw) draw(batch, rect, color);
}

static int utf8_length(const char *text) {
    int count = 0;
    for (const unsigned char *p = (const unsigned char *)text; p && *p; ++p) {
        if ((*p & 0xc0) != 0x80) ++count;
    }
    return count;
}

static void draw_text(const char *value, button_rect_t rect,
                      button_color_t color, bool center) {
    patch_handle_t batch = sprite_batch();
    if (!batch || !value || *value == '\0') return;
    const int characters = utf8_length(value);
    float scale = (float)(rect.width - 12) /
                  (float)((characters > 1 ? characters : 1) * 26);
    if (scale > 0.85f) scale = 0.85f;
    if (scale < 0.15f) scale = 0.15f;
    float x = (float)(rect.x + 8);
    if (center) {
        x = (float)rect.x + (float)rect.width / 2.0f -
            (float)characters * 26.0f * scale / 2.0f;
    }
    button_vec2_t position = {
        x, (float)rect.y + ((float)rect.height - 28.0f * scale) / 2.0f};
    patch_handle_t string = patchlib_string_create(value);
    if (!string) return;
    typedef button_vec2_t (*draw_text_fn_t)(void *, void *, button_vec2_t,
                                            button_color_t, float, float, float,
                                            int);
    draw_text_fn_t draw =
        (draw_text_fn_t)patchlib_method_get_pointer(api.text);
    if (draw) {
        draw(batch, string, position, color, scale, 0.0f, 0.0f, -1);
    }
    patchlib_free(string);
}

static bool draw_action_icon(const button_action_t *action,
                             button_rect_t rect) {
    if (!action || !action->icon_id[0] || rect.width <= 0 || rect.height <= 0) {
        return false;
    }
    patch_handle_t batch = sprite_batch();
    if (!batch) return false;
    const button_color_t tint =
        action->enabled ? make_color(255, 255, 255, 255)
                        : make_color(130, 135, 150, 255);
    button_image_draw(batch, action->icon_id, (float)rect.x, (float)rect.y,
                      (float)rect.width, (float)rect.height, &tint);
    int width = 0;
    int height = 0;
    return button_image_ready(action->icon_id, &width, &height);
}

static button_rect_t region_from_layout(patch_handle_t layout) {
    button_rect_t result = {0, 0, 0, 0};
    if (!layout || !api.get_region) return result;
    typedef button_rect_t (*region_fn_t)(void *);
    region_fn_t get =
        (region_fn_t)patchlib_method_get_pointer(api.get_region);
    if (get) result = get(layout);
    return result;
}

static button_rect_t transaction_region(patch_handle_t layout) {
    button_rect_t result = {0, 0, 0, 0};
    if (!layout || !api.transaction_region) return result;
    typedef button_rect_t (*region_fn_t)(void *);
    region_fn_t get =
        (region_fn_t)patchlib_method_get_pointer(api.transaction_region);
    if (get) result = get(layout);
    return result;
}

static void add_region(button_rect_t bounds, const char *key, const char *profile,
                       const char *slot, bool multi) {
    if (!key || !profile || !slot || bounds.width <= 0 || bounds.height <= 0) {
        return;
    }
    size_t index = BUTTON_MAX_REGIONS;
    for (size_t i = 0; i < region_count; ++i) {
        if (strcmp(regions[i].key, key) == 0 &&
            strcmp(regions[i].slot, slot) == 0) {
            index = i;
            break;
        }
    }
    if (index == BUTTON_MAX_REGIONS) {
        if (region_count >= BUTTON_MAX_REGIONS) return;
        index = region_count++;
    }
    button_region_t *entry = &regions[index];
    entry->bounds = bounds;
    copy_text(entry->key, sizeof(entry->key), key);
    copy_text(entry->profile, sizeof(entry->profile), profile);
    copy_text(entry->slot, sizeof(entry->slot), slot);
    entry->tick = tick;
    entry->multi = multi;
}

static void invalidate_action(const char *key) {
    normal_held_clear();
    for (size_t i = 0; i < radial_held_count;) {
        if (strcmp(radial_held[i].key, key) == 0) {
            radial_held[i] = radial_held[--radial_held_count];
        } else {
            ++i;
        }
    }
    for (size_t i = 0; i < claim_count; ++i) {
        if (strcmp(claims[i].region.key, key) == 0) claims[i].cancelled = true;
    }
    for (size_t i = 0; i < region_count;) {
        if (strcmp(regions[i].key, key) == 0) {
            regions[i] = regions[--region_count];
        } else {
            ++i;
        }
    }
}

static button_native_options_t *native_options_find(patch_handle_t setting,
                                                    bool create) {
    for (size_t i = 0; i < BUTTON_MAX_NATIVE_OPTIONS; ++i) {
        if (native_options[i].used && native_options[i].setting == setting) {
            return &native_options[i];
        }
    }
    if (!create) return NULL;
    for (size_t i = 0; i < BUTTON_MAX_NATIVE_OPTIONS; ++i) {
        if (!native_options[i].used) {
            memset(&native_options[i], 0, sizeof(native_options[i]));
            native_options[i].used = true;
            native_options[i].setting = setting;
            button_scroll_window_init(&native_options[i].window);
            button_scroll_gesture_init(&native_options[i].gesture);
            return &native_options[i];
        }
    }
    return NULL;
}

static void release_native_option(button_native_options_t *entry) {
    if (!entry) return;
    if (entry->setting_pin && il2cpp_gchandle_free) {
        il2cpp_gchandle_free(entry->setting_pin);
    }
    if (entry->original_pin && il2cpp_gchandle_free) {
        il2cpp_gchandle_free(entry->original_pin);
    }
    if (entry->expanded_pin && il2cpp_gchandle_free) {
        il2cpp_gchandle_free(entry->expanded_pin);
    }
    memset(entry, 0, sizeof(*entry));
}

static void restore_native_options(void) {
    for (size_t i = 0; i < BUTTON_MAX_NATIVE_OPTIONS; ++i) {
        button_native_options_t *entry = &native_options[i];
        if (!entry->used) continue;
        if (entry->original) {
            field_write(api.control_options, entry->setting, &entry->original,
                        sizeof(entry->original));
        }
        release_native_option(entry);
    }
    scroll_claim_count = 0;
}

static void restore_render(void) {
    int value = unbound_action;
    while (render_group_count > 0) {
        button_render_group_t group = render_groups[--render_group_count];
        patchlib_array_set(group.array, group.index, &value);
    }
    for (size_t i = 0; i < render_binding_count; ++i) {
        field_write(api.binding_action, render_bindings[i], &value, sizeof(value));
    }
    render_binding_count = 0;
}

static void refresh_modules(void) {
    ++action_generation;
    restore_native_options();
    clear_input();
}

static bool native_overlay_prefix(patch_handle_t setting, void **arguments,
                                  const patch_method_signature_t *signature,
                                  void *result);
static bool native_option_prefix(patch_handle_t setting, void **arguments,
                                 const patch_method_signature_t *signature,
                                 void *result);
static void native_option_postfix(patch_handle_t setting, void **arguments,
                                  void *result,
                                  const patch_method_signature_t *signature);
static bool native_transaction_prefix(patch_handle_t instance, void **arguments,
                                      const patch_method_signature_t *signature,
                                      void *result);
static void native_transaction_postfix(patch_handle_t instance, void **arguments,
                                       void *result,
                                       const patch_method_signature_t *signature);
static bool scroll_overlay_prefix(patch_handle_t setting, void **arguments,
                                  const patch_method_signature_t *signature,
                                  void *result);
static void scroll_overlay_postfix(patch_handle_t setting, void **arguments,
                                   void *result,
                                   const patch_method_signature_t *signature);
static bool scroll_over_prefix(patch_handle_t setting, void **arguments,
                               const patch_method_signature_t *signature,
                               void *result);
static bool native_name_prefix(patch_handle_t instance, void **arguments,
                               const patch_method_signature_t *signature,
                               void *result);
static bool native_style_prefix(patch_handle_t instance, void **arguments,
                                const patch_method_signature_t *signature,
                                void *result);
static bool native_texture_prefix(patch_handle_t instance, void **arguments,
                                  const patch_method_signature_t *signature,
                                  void *result);
static bool value_prefix(patch_handle_t setting, void **arguments,
                         const patch_method_signature_t *signature,
                         void *result);
static bool visible_prefix(patch_handle_t instance, void **arguments,
                           const patch_method_signature_t *signature,
                           void *result);
static bool slot_prefix(patch_handle_t instance, void **arguments,
                        const patch_method_signature_t *signature, void *result);
static void slot_postfix(patch_handle_t instance, void **arguments, void *result,
                         const patch_method_signature_t *signature);
static bool group_visible_prefix(patch_handle_t instance, void **arguments,
                                 const patch_method_signature_t *signature,
                                 void *result);
static bool group_prefix(patch_handle_t instance, void **arguments,
                         const patch_method_signature_t *signature,
                         void *result);
static void group_postfix(patch_handle_t instance, void **arguments,
                          void *result,
                          const patch_method_signature_t *signature);
static bool group_cursor_prefix(patch_handle_t instance, void **arguments,
                                const patch_method_signature_t *signature,
                                void *result);
static bool quick_prefix(patch_handle_t instance, void **arguments,
                         const patch_method_signature_t *signature,
                         void *result);
static void quick_postfix(patch_handle_t instance, void **arguments,
                          void *result,
                          const patch_method_signature_t *signature);
static bool runtime_prefix(patch_handle_t instance, void **arguments,
                           const patch_method_signature_t *signature,
                           void *result);
static void runtime_postfix(patch_handle_t instance, void **arguments,
                            void *result,
                            const patch_method_signature_t *signature);
static void tick_postfix(patch_handle_t instance, void **arguments, void *result,
                         const patch_method_signature_t *signature);
static void poll_postfix(patch_handle_t instance, void **arguments, void *result,
                         const patch_method_signature_t *signature);
static bool dispatch_prefix(patch_handle_t instance, void **arguments,
                            const patch_method_signature_t *signature,
                            void *result);
static bool update_prefix(patch_handle_t setting, void **arguments,
                          const patch_method_signature_t *signature,
                          void *result);
static void change_postfix(patch_handle_t instance, void **arguments,
                           void *result,
                           const patch_method_signature_t *signature);
static bool remove_slot_prefix(patch_handle_t instance, void **arguments,
                               const patch_method_signature_t *signature,
                               void *result);
static void undo_snapshot(patch_handle_t instance, void **arguments,
                          void *result,
                          const patch_method_signature_t *signature);
static bool undo_prefix(patch_handle_t instance, void **arguments,
                        const patch_method_signature_t *signature, void *result);
static bool redo_prefix(patch_handle_t instance, void **arguments,
                        const patch_method_signature_t *signature, void *result);
static void history_done(patch_handle_t instance, void **arguments, void *result,
                         const patch_method_signature_t *signature);
static void redo_clear(patch_handle_t instance, void **arguments, void *result,
                       const patch_method_signature_t *signature);
static void backup_postfix(patch_handle_t instance, void **arguments,
                           void *result,
                           const patch_method_signature_t *signature);
static void cancel_postfix(patch_handle_t instance, void **arguments,
                           void *result,
                           const patch_method_signature_t *signature);
static void history_postfix(patch_handle_t instance, void **arguments,
                            void *result,
                            const patch_method_signature_t *signature);
static bool save_prefix(patch_handle_t instance, void **arguments,
                        const patch_method_signature_t *signature, void *result);
static void save_postfix(patch_handle_t instance, void **arguments, void *result,
                         const patch_method_signature_t *signature);
static void reset_postfix(patch_handle_t instance, void **arguments,
                          void *result,
                          const patch_method_signature_t *signature);
static void copy_postfix(patch_handle_t instance, void **arguments,
                         void *result,
                         const patch_method_signature_t *signature);
static void paste_postfix(patch_handle_t instance, void **arguments,
                          void *result,
                          const patch_method_signature_t *signature);
static void default_postfix(patch_handle_t instance, void **arguments,
                            void *result,
                            const patch_method_signature_t *signature);

static bool add_hook(patch_handle_t target, prefix_callback_t prefix,
                     postfix_callback_t postfix) {
    if (!target || hook_count >= BUTTON_MAX_HOOKS) {
        copy_text(reason, sizeof(reason), "native hook capacity exhausted");
        return false;
    }
    patch_hook_id_t hook =
        patchlib_install_prepost_hook(target, prefix, postfix);
    if (hook == PATCH_HOOK_INVALID_ID) {
        copy_text(reason, sizeof(reason), "native control hook install failed");
        return false;
    }
    hooks[hook_count++] = hook;
    return true;
}

static bool resolve_api(void) {
    memset(&api, 0, sizeof(api));
    api.main = type_handle("Main", "Terraria");
    api.styles = type_handle("VirtualControlsStyles_Layout", "");
    api.binding = type_handle("VirtualControlsBinding_Layout", "");
    api.slot = type_handle("VirtualControlsHardwareConfigurationMapping_Layout",
                           "");
    api.slot_type = type_handle("VirtualControlsSlotTypeMapping_Layout", "");
    api.panel = type_handle("GUIVirtualInputControllerEdit_ControlPanel", "");
    api.setting =
        need_handle(patchlib_type_get_inner_type(api.panel,
                                                 "ControlPanelActionSetting"),
                    "ControlPanelActionSetting");
    api.quick = type_handle("GUIQuickActionButton", "");
    api.editor = type_handle("GUIVirtualInputControllerEdit", "");
    api.interface_edit = type_handle("GUIInterfaceEdit", "");
    api.cursor = type_handle("Cursor", "");
    api.manager = type_handle("CursorManager", "");

    if (!api.main || !api.styles || !api.binding || !api.slot || !api.slot_type ||
        !api.panel || !api.setting || !api.quick || !api.editor ||
        !api.interface_edit || !api.cursor || !api.manager) {
        return false;
    }

    api.batch = field_handle(api.main, "spriteBatch");
    api.width = method_handle(api.main, "get_screenWidth", 0);
    api.menu = patchlib_type_get_field(api.main, "gameMenu");
    if (!api.menu || !patchlib_is_valid(api.menu)) {
        api.menu = PATCH_NULL;
        api.menu_getter = method_handle(api.main, "get_gameMenu", 0);
    }
    if (!api.batch || !api.width || (!api.menu && !api.menu_getter)) {
        return false;
    }
    api.styles_instance = field_handle(api.styles, "Instance");
    api.active_profile = method_handle(api.styles, "get_ActiveProfile", 0);
    patch_handle_t profile = type_handle("VirtualControlsProfile_Layout", "");
    if (!profile) return false;
    api.profile_id = field_handle(profile, "Id");
    api.profile_controls = field_handle(profile, "Controls");
    api.binding_slot = field_handle(api.binding, "SlotId");
    api.binding_action = field_handle(api.binding, "Action");
    api.binding_groups = field_handle(api.binding, "GroupActions");
    api.slot_id = field_handle(api.slot, "SlotId");
    api.slot_multi = field_handle(api.slot, "MultiTouch");
    api.slot_style = field_handle(api.slot, "Style");
    api.slot_kind = field_handle(api.slot_type, "ControlType");
    api.active_control = field_handle(api.panel, "ActiveControl");
    api.action_binding = field_handle(api.setting, "ActionBinding");
    api.control_options = field_handle(api.setting, "controlOptions");

    patch_handle_t pull_down = type_handle("GUIPulldownSetting", "");
    if (!pull_down) return false;
    api.selected_position = field_handle(pull_down, "selectedPosition");
    api.display_options = field_handle(pull_down, "displayOptions");
    patch_handle_t gui = type_handle("GUIInstance", "");
    if (!gui) return false;
    api.gui_active = field_handle(gui, "Active");
    api.gui_edit = field_handle(gui, "GUIInterfaceEdit");
    api.edit_enabled = field_handle(api.interface_edit, "Enabled");
    api.gui_runtime = field_handle(gui, "GUIVirtualInputController");
    patch_handle_t controller = type_handle("GUIVirtualInputController", "");
    if (!controller) return false;
    api.ui_open = method_handle(controller, "get_InGameUIOpen", 0);

    api.manager_instance = field_handle(api.manager, "Instance");
    api.cursor_id = field_handle(api.cursor, "Id");
    api.cursor_pos = field_handle(api.cursor, "Position");
    api.cursor_down = field_handle(api.cursor, "Down");
    api.cursor_old = field_handle(api.cursor, "WasDown");
    api.cursor_ignore = field_handle(api.cursor, "Ignore");
    api.cursor_world = field_handle(api.cursor, "IgnoreForWorld");
    api.cursor_layer = field_handle(api.cursor, "InputLayer");
    api.group_capture =
        method_handle(controller, "UpdateCursorGroupControl", 7);

    patch_handle_t utils = type_handle("Utils", "Terraria");
    if (!utils) return false;
    api.rect = method_handle(utils, "DrawInvBG", 3);
    api.text = method_handle(utils, "DrawBorderString", 8);
    api.get_region = method_handle(api.quick, "GetRegionWithoutPicking", 1);
    patch_handle_t transaction = type_handle("GUITransactionButton", "");
    if (!transaction) return false;
    api.transaction_region = method_handle(transaction, "GetRegion", 1);
    patch_handle_t grid = type_handle("ItemGrid_Layout", "");
    if (!grid) return false;
    api.grid_horizontal = field_handle(grid, "Horizontal");
    api.grid_line_count = field_handle(grid, "ItemLineCount");
    patch_handle_t xna_runner = type_handle("XNAUnityRunner", "");
    if (!xna_runner) return false;
    api.adjust = method_handle(xna_runner, "AdjustRealScreenForUI", 1);
    api.count = method_handle(api.manager, "GetNumCursors", 0);
    api.at = method_handle(api.manager, "GetCursor", 1);
    api.update_value = method_handle(api.setting, "UpdateValue", 1);
    api.close_options = method_handle(pull_down, "CloseOptions", 0);
    api.is_over_options = method_handle(api.setting, "IsOverOptions", 1);
    api.add_undo = method_handle(api.interface_edit, "AddControlUndo", 0);
    api.has_undo = method_handle(api.interface_edit, "HasUndo", 0);
    api.has_redo = method_handle(api.interface_edit, "HasRedo", 0);

    return reason[0] == '\0' || strcmp(reason, "native touch bindings require Android arm64") == 0;
}

static bool native_overlay_prefix(patch_handle_t setting, void **arguments,
                                  const patch_method_signature_t *signature,
                                  void *result) {
    (void)arguments;
    (void)signature;
    if (!ready || setting_index(setting) == -2) return false;
    patch_handle_t options =
        field_read_handle(api.control_options, setting);
    if (!options) return false;

    button_native_options_t *entry = native_options_find(setting, true);
    if (!entry) return false;
    if (!entry->setting_pin) {
        entry->setting_pin = il2cpp_gchandle_new(setting, false);
        entry->original_pin = il2cpp_gchandle_new(options, false);
        if (!entry->setting_pin || !entry->original_pin) return false;
        entry->original = options;
    }

    if (options != entry->original && options != entry->expanded) {
        if (entry->original_pin && il2cpp_gchandle_free) {
            il2cpp_gchandle_free(entry->original_pin);
        }
        entry->original = options;
        entry->original_pin = il2cpp_gchandle_new(options, false);
        if (!entry->original_pin) return false;
        entry->cell_count = 0;
        button_scroll_gesture_cancel(&entry->gesture);
        entry->last_draw = 0;
    }

    const size_t original_count = patchlib_array_length(entry->original);
    entry->window.total = (int)original_count + (int)state.action_count;
    button_scroll_window_clamp(&entry->window);

    if (entry->expanded && entry->built_row == entry->window.row &&
        entry->built_columns == entry->window.columns &&
        entry->built_rows == entry->window.rows &&
        entry->built_total == entry->window.total &&
        entry->built_generation == action_generation &&
        entry->built_horizontal == entry->window.horizontal) {
        field_write(api.control_options, setting, &entry->expanded,
                    sizeof(entry->expanded));
        char key[513];
        active_key(setting, key, sizeof(key));
        button_action_t *action = button_state_find_action_by_key(&state, key);
        if (action && result) {
            *(int *)result = native_action_base + (int)(action - state.actions);
            return true;
        }
        return false;
    }

    int length = button_scroll_window_capacity(&entry->window);
    if (length > entry->window.total) length = entry->window.total;
    patch_handle_t expanded =
        patchlib_array_resize(entry->original, (size_t)length, NULL);
    if (!expanded) return false;

    for (int cell = 0; cell < length; ++cell) {
        const int source = button_scroll_window_source(&entry->window, cell);
        int value = empty_option;
        if (source >= 0 && (size_t)source < original_count) {
            if (!patchlib_array_at(entry->original, (size_t)source, &value)) {
                return false;
            }
        } else if (source >= 0 &&
                   (size_t)(source - (int)original_count) < state.action_count) {
            value = native_action_base + source - (int)original_count;
        }
        if (!patchlib_array_set(expanded, (size_t)cell, &value)) return false;
    }

    void *pin = il2cpp_gchandle_new(expanded, false);
    if (!pin) return false;
    if (entry->expanded_pin && il2cpp_gchandle_free) {
        il2cpp_gchandle_free(entry->expanded_pin);
    }
    entry->expanded = expanded;
    entry->expanded_pin = pin;
    entry->built_row = entry->window.row;
    entry->built_columns = entry->window.columns;
    entry->built_rows = entry->window.rows;
    entry->built_total = entry->window.total;
    entry->built_generation = action_generation;
    entry->built_horizontal = entry->window.horizontal;
    field_write(api.control_options, setting, &expanded, sizeof(expanded));

    char key[513];
    active_key(setting, key, sizeof(key));
    button_action_t *action = button_state_find_action_by_key(&state, key);
    if (action && result) {
        *(int *)result = native_action_base + (int)(action - state.actions);
        return true;
    }
    return false;
}

static void push_change(const char *profile, const char *slot) {
    if (change_count >= BUTTON_MAX_CHANGES) {
        memmove(&changes[0], &changes[1],
                (BUTTON_MAX_CHANGES - 1) * sizeof(changes[0]));
        --change_count;
    }
    button_change_t *change = &changes[change_count++];
    copy_text(change->profile, sizeof(change->profile), profile);
    copy_text(change->slot, sizeof(change->slot), slot);
}

static bool update_prefix(patch_handle_t setting, void **arguments,
                          const patch_method_signature_t *signature,
                          void *result) {
    (void)signature;
    (void)result;
    if (arguments && arguments[0]) {
        button_action_t *action =
            action_from_native_id(*(int *)arguments[0]);
        if (action) {
            char profile[1024];
            char slot[1024];
            profile_key(profile, sizeof(profile));
            setting_slot(setting, slot, sizeof(slot));
            push_change("", "");
            if (profile[0] == '\0' || slot[0] == '\0') return true;
            patch_handle_t edit = edit_object();
            if (edit) invoke_void(api.add_undo, edit, NULL);
            int value = unbound_action;
            void *nested = &value;
            selecting = true;
            invoke_void(api.update_value, setting, &nested);
            selecting = false;
            button_state_set_binding(&state, profile, slot, action->key);
            return true;
        }
    }

    if (!selecting && setting_index(setting) != -2) {
        char profile[1024];
        char slot[1024];
        profile_key(profile, sizeof(profile));
        setting_slot(setting, slot, sizeof(slot));
        push_change(profile, slot);
    }
    return false;
}

static void change_postfix(patch_handle_t instance, void **arguments,
                           void *result,
                           const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    if (change_count == 0) return;
    const button_change_t change = changes[--change_count];
    if (change.profile[0] == '\0') return;
    button_state_erase_binding(&state, change.profile, change.slot);
    for (int i = 0; i < 4; ++i) {
        char slot[1024];
        group_slot(change.slot, i, slot, sizeof(slot));
        button_state_erase_binding(&state, change.profile, slot);
    }
}

static bool remove_slot_prefix(patch_handle_t instance, void **arguments,
                               const patch_method_signature_t *signature,
                               void *result) {
    (void)instance;
    (void)signature;
    (void)result;
    char slot[1024] = "";
    if (arguments && arguments[0]) {
        const int index = *(int *)arguments[0];
        patch_handle_t profile = profile_handle();
        patch_handle_t controls =
            profile ? field_read_handle(api.profile_controls, profile)
                    : PATCH_NULL;
        if (controls && index >= 0 &&
            (size_t)index < patchlib_array_length(controls)) {
            patch_handle_t binding = PATCH_NULL;
            patchlib_array_at(controls, (size_t)index, &binding);
            if (binding) {
                copy_managed_string(
                    slot, sizeof(slot),
                    field_read_handle(api.binding_slot, binding));
            }
        }
    }
    if (slot[0] != '\0') {
        char profile[1024];
        profile_key(profile, sizeof(profile));
        push_change(profile, slot);
    } else {
        push_change("", "");
    }
    return false;
}

static void undo_snapshot(patch_handle_t instance, void **arguments,
                          void *result,
                          const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    if (!history_transition) button_state_snapshot(&state);
}

static bool undo_prefix(patch_handle_t instance, void **arguments,
                        const patch_method_signature_t *signature, void *result) {
    (void)arguments;
    (void)signature;
    (void)result;
    history_transition = true;
    if (invoke_bool(api.has_undo, instance, NULL)) button_state_undo(&state);
    return false;
}

static bool redo_prefix(patch_handle_t instance, void **arguments,
                        const patch_method_signature_t *signature, void *result) {
    (void)arguments;
    (void)signature;
    (void)result;
    history_transition = true;
    if (invoke_bool(api.has_redo, instance, NULL)) button_state_redo(&state);
    return false;
}

static void history_done(patch_handle_t instance, void **arguments, void *result,
                         const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    history_transition = false;
}

static void redo_clear(patch_handle_t instance, void **arguments, void *result,
                       const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    if (!history_transition) button_state_clear_redo(&state);
}

static void backup_postfix(patch_handle_t instance, void **arguments,
                           void *result,
                           const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    button_state_begin_edit(&state);
    clear_input();
}

static void cancel_postfix(patch_handle_t instance, void **arguments,
                           void *result,
                           const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    button_state_cancel_edit(&state);
    clear_input();
}

static void history_postfix(patch_handle_t instance, void **arguments,
                            void *result,
                            const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    button_state_clear_history(&state);
}

static bool save_prefix(patch_handle_t instance, void **arguments,
                        const patch_method_signature_t *signature, void *result) {
    (void)instance;
    (void)arguments;
    (void)signature;
    (void)result;
    restore_render();
    return false;
}

static void save_postfix(patch_handle_t instance, void **arguments, void *result,
                         const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    if (storage_directory[0] &&
        !button_store_save(&state, storage_directory)) {
        BUTTON_LOG_WARN("[button-manager] binding save failed");
    }
    button_profile_cache_invalidate(&profile_cache);
}

static void reset_postfix(patch_handle_t instance, void **arguments,
                          void *result,
                          const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    button_state_clear_bindings(&state);
    button_state_clear_history(&state);
    clear_input();
    if (storage_directory[0]) button_store_save(&state, storage_directory);
    button_profile_cache_invalidate(&profile_cache);
}

static void copy_postfix(patch_handle_t instance, void **arguments,
                         void *result,
                         const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    char profile[1024];
    profile_key(profile, sizeof(profile));
    button_state_copy_profile(&state, profile);
}

static void paste_postfix(patch_handle_t instance, void **arguments,
                          void *result,
                          const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    char profile[1024];
    profile_key(profile, sizeof(profile));
    button_state_paste_profile(&state, profile);
    clear_input();
    button_profile_cache_invalidate(&profile_cache);
}

static void default_postfix(patch_handle_t instance, void **arguments,
                            void *result,
                            const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    char profile[1024];
    profile_key(profile, sizeof(profile));
    button_state_clear_profile(&state, profile);
    clear_input();
    button_profile_cache_invalidate(&profile_cache);
}

static void uninstall_hooks(void) {
    while (hook_count > 0) {
        patchlib_uninstall_hook(hooks[--hook_count]);
    }
}

static void install_manager(void) {
#if !defined(__aarch64__)
    copy_text(reason, sizeof(reason),
              "native touch bindings require Android arm64");
    return;
#else
    if (ready) return;
    reason[0] = '\0';
    button_profile_cache_init(&profile_cache);
    if (!il2cpp_gchandle_new || !il2cpp_gchandle_free) {
        copy_text(reason, sizeof(reason),
                  "IL2CPP GC retention API unavailable");
        return;
    }
    if (!resolve_api()) {
        if (reason[0] == '\0') {
            copy_text(reason, sizeof(reason), "native control API unavailable");
        }
        return;
    }

    patch_handle_t setting_type = api.setting;
    patch_handle_t transaction = type_handle("GUITransactionButton", "");
    patch_handle_t editor = api.editor;
    patch_handle_t controller = type_handle("GUIVirtualInputController", "");
    patch_handle_t cursor_layer = type_handle("CursorInputLayer", "");
    if (!transaction || !controller || !cursor_layer) {
        uninstall_hooks();
        return;
    }

    bool ok =
        add_hook(method_handle(setting_type, "GetControlType", 0),
                 native_overlay_prefix, NULL) &&
        add_hook(method_handle(setting_type, "PulldownOptionDraw", 4),
                 native_option_prefix, native_option_postfix) &&
        add_hook(method_handle(transaction, "Draw", 9),
                 native_transaction_prefix, native_transaction_postfix) &&
        add_hook(method_handle(setting_type, "DrawOverlay", 0),
                 scroll_overlay_prefix, scroll_overlay_postfix) &&
        add_hook(api.is_over_options, scroll_over_prefix, NULL) &&
        add_hook(method_handle(editor, "GetControlName", 1),
                 native_name_prefix, NULL) &&
        add_hook(method_handle(editor, "GetButtonControlSubStyle", 1),
                 native_style_prefix, NULL) &&
        add_hook(method_handle(editor, "GetControlTextureFrame", 3),
                 native_texture_prefix, NULL) &&
        add_hook(method_handle(setting_type, "GetValue", 0), value_prefix,
                 NULL) &&
        add_hook(api.update_value, update_prefix, change_postfix) &&
        add_hook(method_handle(editor, "RemoveSlot", 1), remove_slot_prefix,
                 change_postfix) &&
        add_hook(method_handle(controller, "Draw", 0), runtime_prefix,
                 runtime_postfix) &&
        add_hook(method_handle(controller, "UpdateControlVisibilityRegions", 1),
                 runtime_prefix, runtime_postfix) &&
        add_hook(method_handle(controller, "DetermineButtonVisible", 3),
                 visible_prefix, NULL) &&
        add_hook(method_handle(controller, "DrawButtonControl", 4), slot_prefix,
                 slot_postfix) &&
        add_hook(method_handle(controller, "DrawGroupControl", 4), group_prefix,
                 group_postfix) &&
        add_hook(method_handle(controller, "DetermineGroupVisible", 3),
                 group_visible_prefix, NULL) &&
        add_hook(method_handle(controller, "UpdateCursor", 1),
                 group_cursor_prefix, NULL) &&
        add_hook(method_handle(api.quick, "Draw", 6), quick_prefix,
                 quick_postfix) &&
        add_hook(method_handle(api.manager, "UpdateCursorList", 0), NULL,
                 poll_postfix) &&
        add_hook(method_handle(cursor_layer, "UpdateCursor", 1),
                 dispatch_prefix, NULL) &&
        add_hook(method_handle(api.main, "Update", 1), NULL, tick_postfix) &&
        add_hook(api.add_undo, NULL, undo_snapshot) &&
        add_hook(method_handle(api.interface_edit, "ControlUndo", 0),
                 undo_prefix, history_done) &&
        add_hook(method_handle(api.interface_edit, "ControlRedo", 0),
                 redo_prefix, history_done) &&
        add_hook(method_handle(api.interface_edit, "Backup", 0), NULL,
                 backup_postfix) &&
        add_hook(method_handle(api.interface_edit, "CancelChanges", 0), NULL,
                 cancel_postfix) &&
        add_hook(method_handle(api.interface_edit, "ClearControlUndoRedo", 0),
                 NULL, history_postfix) &&
        add_hook(method_handle(api.interface_edit, "ClearControlRedo", 0), NULL,
                 redo_clear) &&
        add_hook(method_handle(api.interface_edit, "RestoreBackup", 0), NULL,
                 cancel_postfix) &&
        add_hook(method_handle(api.interface_edit, "CopyProfile", 0), NULL,
                 copy_postfix) &&
        add_hook(method_handle(api.interface_edit, "PasteProfile", 0), NULL,
                 paste_postfix) &&
        add_hook(method_handle(api.interface_edit, "RestoreDefaults", 0), NULL,
                 default_postfix) &&
        add_hook(method_handle(api.styles, "SaveConfiguration", 1), save_prefix,
                 save_postfix) &&
        add_hook(method_handle(api.styles, "SetAllConfigurationsToDefault", 0),
                 NULL, reset_postfix);

    if (!ok) {
        uninstall_hooks();
        restore_render();
        clear_input();
        button_image_clear();
        return;
    }
    ready = true;
    reason[0] = '\0';
    BUTTON_LOG_INFO("[button-manager] installed %zu native hooks", hook_count);
#endif
}

static void shutdown_manager(void) {
    uninstall_hooks();
    ready = false;
    restore_render();
    restore_native_options();
    clear_input();
    drawing_clear();
    native_action_draw_count = 0;
    change_count = 0;
    runtime_depth = 0;
    history_transition = false;
    selecting = false;
    button_profile_cache_invalidate(&profile_cache);
    button_image_clear();
    reason[0] = '\0';
    copy_text(reason, sizeof(reason), "native touch bindings are shut down");
    button_state_destroy(&state);
    button_state_init(&state);
}

static void configure_storage(const char *directory) {
    if (!directory || *directory == '\0') return;
    copy_text(storage_directory, sizeof(storage_directory), directory);
    button_state_clear_interaction_state(&state);
    if (!button_store_load(&state, storage_directory)) {
        BUTTON_LOG_WARN("[button-manager] binding configuration ignored");
    }
}

bool terraria_button_manager_available(void) {
    return ready;
}

const char *terraria_button_manager_error(void) {
    return reason;
}

void terraria_button_manager_configure_storage(const char *directory) {
    configure_storage(directory);
}

void terraria_button_manager_install(void) {
    install_manager();
}

void terraria_button_manager_shutdown(void) {
    shutdown_manager();
}

/* Colon cannot occur in either validated component, so scopes cannot alias. */
static bool button_scope_key(const terraria_button_scope_t *scope, char output[130]) {
    if (!scope || !button_state_valid_id(scope->modloader_id) ||
        !button_state_valid_id(scope->mod_id)) return false;
    snprintf(output, 130, "%s:%s", scope->modloader_id, scope->mod_id);
    return true;
}

bool terraria_button_manager_register(const terraria_button_desc_t *desc) {
    char owner[130];
    if (!desc || !button_scope_key(&desc->scope, owner)) return false;
    if (!ready || !desc->id || !desc->name ||
        !button_state_valid_id(desc->id) ||
        button_state_find_action(&state, owner, desc->id)) {
        return false;
    }

    char icon_id[768] = "";
    if (desc->icon) {
        snprintf(icon_id, sizeof(icon_id), "%s@buttons.%s.%llu", owner,
                 desc->id, ++image_serial);
        if (!button_image_request(icon_id, desc->icon->rgba, desc->icon->size,
                                  desc->icon->width, desc->icon->height)) {
            copy_text(reason, sizeof(reason), "invalid or full button icon cache");
            return false;
        }
    }

    if (!button_state_register_action(&state, owner, desc->id,
                                      desc->name, "", icon_id,
                                      desc->show_label)) {
        if (icon_id[0]) button_image_release(icon_id);
        return false;
    }
    button_action_t *registered = button_state_find_action(&state, owner, desc->id);
    copy_text(registered->modloader_id, sizeof(registered->modloader_id), desc->scope.modloader_id);
    copy_text(registered->mod_id, sizeof(registered->mod_id), desc->scope.mod_id);
    refresh_modules();
    return true;
}

bool terraria_button_manager_unregister(const terraria_button_scope_t *scope, const char *id) {
    char owner[130];
    if (!button_scope_key(scope, owner)) return false;
    if (!id) return false;
    button_action_t *action = button_state_find_action(&state, owner, id);
    if (!action) return false;
    char key[513];
    copy_text(key, sizeof(key), action->key);
    if (action->icon_id[0]) button_image_release(action->icon_id);
    invalidate_action(key);
    if (!button_state_unregister_action(&state, owner, id)) return false;
    refresh_modules();
    return true;
}

void terraria_button_manager_unregister_owner(const terraria_button_scope_t *scope) {
    char owner[130];
    if (!button_scope_key(scope, owner)) return;
    for (size_t i = 0; i < state.action_count;) {
        if (strcmp(state.actions[i].owner, owner) == 0) {
            char id[65];
            copy_text(id, sizeof(id), state.actions[i].id);
            terraria_button_manager_unregister(scope, id);
        } else {
            ++i;
        }
    }
}

bool terraria_button_manager_down(const terraria_button_scope_t *scope, const char *id) {
    char owner[130];
    if (!button_scope_key(scope, owner)) return false;
    if (!ready) return false;
    return button_state_is_held(
        button_state_find_action(&state, owner, id));
}

bool terraria_button_manager_pressed(const terraria_button_scope_t *scope, const char *id) {
    char owner[130];
    if (!button_scope_key(scope, owner)) return false;
    if (!ready) return false;
    return button_state_consume_pressed(
        button_state_find_action(&state, owner, id));
}

bool terraria_button_manager_released(const terraria_button_scope_t *scope, const char *id) {
    char owner[130];
    if (!button_scope_key(scope, owner)) return false;
    if (!ready) return false;
    return button_state_consume_released(
        button_state_find_action(&state, owner, id));
}

bool terraria_button_manager_set_enabled(const terraria_button_scope_t *scope, const char *id,
                                         bool value) {
    char owner[130];
    if (!button_scope_key(scope, owner)) return false;
    button_action_t *action = button_state_find_action(&state, owner, id);
    if (!button_state_set_action_enabled(action, value)) return false;
    if (!value) invalidate_action(action->key);
    return true;
}

bool terraria_button_manager_set_visible(const terraria_button_scope_t *scope, const char *id,
                                         bool value) {
    char owner[130];
    if (!button_scope_key(scope, owner)) return false;
    button_action_t *action = button_state_find_action(&state, owner, id);
    if (!button_state_set_action_visible(action, value)) return false;
    if (!value) invalidate_action(action->key);
    return true;
}

static bool native_option_prefix(patch_handle_t setting, void **arguments,
                                 const patch_method_signature_t *signature,
                                 void *result) {
    (void)signature;
    (void)result;
    const int cell = arguments && arguments[1] ? *(int *)arguments[1] : -1;
    if (native_action_draw_count < BUTTON_MAX_DRAW_STACK) {
        native_action_draws[native_action_draw_count++] =
            (button_native_action_draw_t){setting, cell};
    }
    button_native_options_t *entry = native_options_find(setting, false);
    if (!entry || cell < 0) return false;

    patch_handle_t grid =
        arguments && arguments[0] ? *(patch_handle_t *)arguments[0] : PATCH_NULL;
    if (grid && cell == 0) {
        const bool horizontal = field_read_bool(api.grid_horizontal, grid);
        int lines = field_read_int(api.grid_line_count, grid);
        if (lines < 1) lines = 1;
        if (lines > 27) lines = 27;
        const int columns = horizontal ? lines : (27 / lines > 0 ? 27 / lines : 1);
        const int rows = horizontal ? (27 / lines > 0 ? 27 / lines : 1) : lines;
        if (columns != entry->window.columns || rows != entry->window.rows ||
            horizontal != entry->window.horizontal) {
            button_scroll_window_configure(&entry->window, columns, rows,
                                           horizontal);
            button_scroll_gesture_cancel(&entry->gesture);
            entry->last_draw = 0;
        }
    }

    int action = empty_option;
    patch_handle_t array =
        field_read_handle(api.control_options, setting);
    if (array && (size_t)cell < patchlib_array_length(array)) {
        patchlib_array_at(array, (size_t)cell, &action);
    }
    if ((size_t)cell >= BUTTON_MAX_NATIVE_CELLS) return false;
    if ((size_t)cell >= entry->cell_count) {
        memset(&entry->cells[entry->cell_count], 0,
               ((size_t)cell + 1 - entry->cell_count) * sizeof(entry->cells[0]));
        for (size_t i = entry->cell_count; i <= (size_t)cell; ++i) {
            entry->cell_ids[i] = empty_option;
        }
        entry->cell_count = (size_t)cell + 1;
    }
    entry->cell_ids[cell] = action;
    return action == empty_option;
}

static void native_option_postfix(patch_handle_t setting, void **arguments,
                                  void *result,
                                  const patch_method_signature_t *signature) {
    (void)arguments;
    (void)result;
    (void)signature;
    if (native_action_draw_count == 0) return;
    button_native_action_draw_t draw =
        native_action_draws[--native_action_draw_count];
    button_native_options_t *entry = native_options_find(draw.setting, false);
    if (entry && entry->block_until >= tick) {
        const int selected = 0;
        field_write(api.selected_position, draw.setting, &selected,
                    sizeof(selected));
    }
    (void)setting;
}

static bool native_transaction_prefix(patch_handle_t instance, void **arguments,
                                      const patch_method_signature_t *signature,
                                      void *result) {
    (void)instance;
    (void)signature;
    (void)result;
    if (native_action_draw_count == 0 || !arguments || !arguments[0]) {
        return false;
    }
    button_native_action_draw_t draw =
        native_action_draws[native_action_draw_count - 1];
    button_native_options_t *entry = native_options_find(draw.setting, false);
    if (!entry || draw.cell < 0 ||
        (size_t)draw.cell >= entry->cell_count) {
        return false;
    }
    patch_handle_t layout = *(patch_handle_t *)arguments[0];
    entry->cells[draw.cell] = transaction_region(layout);
    return false;
}

static void native_transaction_postfix(patch_handle_t instance, void **arguments,
                                       void *result,
                                       const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)signature;
    if (!result || native_action_draw_count == 0) return;
    button_native_action_draw_t draw =
        native_action_draws[native_action_draw_count - 1];
    button_native_options_t *entry = native_options_find(draw.setting, false);
    if (entry && entry->block_until >= tick) *(int *)result = 1;
}

static bool scroll_overlay_prefix(patch_handle_t setting, void **arguments,
                                  const patch_method_signature_t *signature,
                                  void *result) {
    (void)arguments;
    (void)signature;
    (void)result;
    button_native_options_t *entry = native_options_find(setting, false);
    if (entry) {
        entry->cell_count = 0;
        if (!field_read_bool(api.display_options, setting)) {
            button_scroll_gesture_cancel(&entry->gesture);
            entry->last_draw = 0;
        }
    }
    return false;
}

static void scroll_overlay_postfix(patch_handle_t setting, void **arguments,
                                   void *result,
                                   const patch_method_signature_t *signature) {
    (void)arguments;
    (void)result;
    (void)signature;
    button_native_options_t *entry = native_options_find(setting, false);
    if (!entry) return;
    if (!field_read_bool(api.display_options, setting) ||
        entry->cell_count == 0) {
        button_scroll_gesture_cancel(&entry->gesture);
        entry->last_draw = 0;
        return;
    }

    button_rect_t bounds = {0, 0, 0, 0};
    for (size_t i = 0; i < entry->cell_count; ++i) {
        const button_rect_t cell = entry->cells[i];
        if (cell.width <= 0 || cell.height <= 0) continue;
        if (bounds.width == 0) {
            bounds = cell;
        } else {
            const int right =
                bounds.x + bounds.width > cell.x + cell.width
                    ? bounds.x + bounds.width
                    : cell.x + cell.width;
            const int bottom =
                bounds.y + bounds.height > cell.y + cell.height
                    ? bounds.y + bounds.height
                    : cell.y + cell.height;
            bounds.x = bounds.x < cell.x ? bounds.x : cell.x;
            bounds.y = bounds.y < cell.y ? bounds.y : cell.y;
            bounds.width = right - bounds.x;
            bounds.height = bottom - bounds.y;
        }
    }
    if (bounds.width <= 0 || bounds.height <= 0) {
        entry->last_draw = 0;
        return;
    }

    entry->bounds = bounds;
    entry->last_draw = tick;
    entry->pitch = (float)bounds.height / (float)entry->window.rows;
    for (size_t i = 1; i < entry->cell_count; ++i) {
        if (entry->cells[i].y > entry->cells[0].y &&
            entry->cells[i].x == entry->cells[0].x) {
            entry->pitch =
                (float)(entry->cells[i].y - entry->cells[0].y);
            break;
        }
    }
    if (button_scroll_window_max_row(&entry->window) == 0) {
        memset(&entry->rail, 0, sizeof(entry->rail));
        return;
    }

    int width = bounds.height / entry->window.rows / 5;
    if (width < 8) width = 8;
    if (width > 16) width = 16;
    const int screen = invoke_int(api.width, PATCH_NULL, NULL);
    int rail_x = bounds.x + bounds.width + 4;
    if (rail_x < 0) rail_x = 0;
    if (rail_x > screen - width && screen > width) rail_x = screen - width;
    entry->rail = (button_rect_t){rail_x, bounds.y, width, bounds.height};
    draw_rectangle(entry->rail, make_color(28, 38, 68, 235));

    const int thumb =
        button_scroll_window_thumb_size(&entry->window, entry->rail.height);
    const int travel = entry->rail.height - thumb;
    const int max_row = button_scroll_window_max_row(&entry->window);
    const int y =
        entry->rail.y +
        (int)lroundf((float)travel * (float)entry->window.row / (float)max_row);
    const button_color_t thumb_color =
        button_scroll_gesture_active(&entry->gesture)
            ? make_color(149, 198, 245, 255)
            : make_color(99, 146, 207, 255);
    draw_rectangle(
        (button_rect_t){entry->rail.x + 1, y, width > 2 ? width - 2 : 1, thumb},
        thumb_color);
}

static bool scroll_over_prefix(patch_handle_t setting, void **arguments,
                               const patch_method_signature_t *signature,
                               void *result) {
    (void)signature;
    button_native_options_t *entry = native_options_find(setting, false);
    if (!entry || !result || entry->last_draw == 0 ||
        tick - entry->last_draw > 2 ||
        button_scroll_window_max_row(&entry->window) == 0 ||
        !field_read_bool(api.display_options, setting)) {
        return false;
    }

    bool over = entry->block_until >= tick;
    if (arguments && arguments[0]) {
        const button_vec2_t position = *(button_vec2_t *)arguments[0];
        over = over || contains(entry->rail, position.x, position.y);
    }
    if (!over) return false;
    *(bool *)result = true;
    return true;
}

static button_native_options_t *scroll_options_on_rail(
    button_vec2_t position, bool *on_rail, int *cell, float *grab) {
    for (size_t i = 0; i < BUTTON_MAX_NATIVE_OPTIONS; ++i) {
        button_native_options_t *entry = &native_options[i];
        if (!entry->used || !entry->last_draw || tick - entry->last_draw > 2 ||
            !field_read_bool(api.display_options, entry->setting) ||
            button_scroll_window_max_row(&entry->window) == 0) {
            continue;
        }
        const bool rail = contains(entry->rail, position.x, position.y);
        if (!rail && !contains(entry->bounds, position.x, position.y)) continue;
        int selected = -1;
        for (size_t j = 0; j < entry->cell_count; ++j) {
            if (contains(entry->cells[j], position.x, position.y)) {
                selected = (int)j;
                break;
            }
        }
        const int thumb =
            button_scroll_window_thumb_size(&entry->window, entry->rail.height);
        const float top =
            entry->rail.y + (float)(entry->rail.height - thumb) *
                                (float)entry->window.row /
                                (float)button_scroll_window_max_row(&entry->window);
        *on_rail = rail;
        *cell = selected;
        *grab = rail && position.y >= top &&
                        position.y < top + (float)thumb
                    ? position.y - top
                    : (float)thumb / 2.0f;
        return entry;
    }
    return NULL;
}

static void poll_native_scroll(void) {
    patch_handle_t manager =
        field_read_handle(api.manager_instance, PATCH_NULL);
    if (!manager) return;
    typedef button_vec2_t (*adjust_fn_t)(button_vec2_t);
    adjust_fn_t adjust =
        (adjust_fn_t)patchlib_method_get_pointer(api.adjust);
    int count = invoke_int(api.count, manager, NULL);
    if (count < 0) count = 0;
    if (count > 32) count = 32;
    bool live[BUTTON_MAX_SCROLL_CLAIMS];
    memset(live, 0, sizeof(live));

    for (int index = 0; index < count; ++index) {
        void *argument = &index;
        patch_handle_t cursor = invoke_handle(api.at, manager, &argument);
        if (!cursor) continue;
        const int cursor_id = field_read_int(api.cursor_id, cursor);
        button_vec2_t position = {0.0f, 0.0f};
        field_read(api.cursor_pos, cursor, &position, sizeof(position));
        if (adjust) position = adjust(position);
        const bool down = field_read_bool(api.cursor_down, cursor);
        const bool old = field_read_bool(api.cursor_old, cursor);

        size_t claim_index = BUTTON_MAX_SCROLL_CLAIMS;
        for (size_t i = 0; i < scroll_claim_count; ++i) {
            if (scroll_claims[i].cursor == cursor_id) {
                claim_index = i;
                live[i] = true;
                break;
            }
        }

        if (claim_index == BUTTON_MAX_SCROLL_CLAIMS && down && !old &&
            editing() && !in_menu()) {
            bool on_rail = false;
            int cell = -1;
            float grab = 0.0f;
            button_native_options_t *entry =
                scroll_options_on_rail(position, &on_rail, &cell, &grab);
            if (entry && scroll_claim_count < BUTTON_MAX_SCROLL_CLAIMS &&
                button_scroll_gesture_begin(
                    &entry->gesture, cursor_id, position.y, entry->window.row,
                    on_rail, cell, grab)) {
                setting_slot(entry->setting, entry->pressed_slot,
                             sizeof(entry->pressed_slot));
                profile_key(entry->pressed_profile,
                            sizeof(entry->pressed_profile));
                scroll_claims[scroll_claim_count++] =
                    (button_scroll_claim_t){cursor_id, entry->setting};
                claim_index = scroll_claim_count - 1;
                live[claim_index] = true;
            }
        }
        if (claim_index == BUTTON_MAX_SCROLL_CLAIMS) continue;

        field_write(api.cursor_ignore, cursor, &(bool){true}, sizeof(bool));
        field_write(api.cursor_world, cursor, &(bool){true}, sizeof(bool));
        button_native_options_t *entry =
            native_options_find(scroll_claims[claim_index].setting, false);
        if (entry) {
            entry->block_until = tick + 1;
            char current_slot[1024];
            char current_profile[1024];
            setting_slot(entry->setting, current_slot, sizeof(current_slot));
            profile_key(current_profile, sizeof(current_profile));
            const bool valid =
                editing() && !in_menu() &&
                field_read_bool(api.display_options, entry->setting) &&
                tick - entry->last_draw <= 2 &&
                strcmp(current_slot, entry->pressed_slot) == 0 &&
                strcmp(current_profile, entry->pressed_profile) == 0;
            if (!valid) button_scroll_gesture_cancel(&entry->gesture);
            if (entry->gesture.cursor == cursor_id) {
                if (entry->gesture.rail) {
                    const int thumb = button_scroll_window_thumb_size(
                        &entry->window, entry->rail.height);
                    const int travel = entry->rail.height - thumb;
                    if (travel > 0) {
                        button_scroll_window_set_fraction(
                            &entry->window,
                            (position.y - entry->rail.y -
                             entry->gesture.grab) /
                                (float)travel);
                    }
                } else if (button_scroll_gesture_move(
                               &entry->gesture, cursor_id, position.y,
                               entry->pitch,
                               entry->pitch * 0.15f > 8.0f
                                   ? entry->pitch * 0.15f
                                   : 8.0f)) {
                    button_scroll_window_set_row(&entry->window,
                                                 entry->gesture.target_row);
                }
                if (!down) {
                    const int cell = entry->gesture.cell;
                    const bool tap =
                        button_scroll_gesture_release(&entry->gesture, cursor_id);
                    if (tap && cell >= 0 &&
                        (size_t)cell < entry->cell_count &&
                        contains(entry->cells[cell], position.x, position.y)) {
                        int action = entry->cell_ids[cell];
                        if (action != empty_option &&
                            (action < native_action_base ||
                             action_from_native_id(action))) {
                            void *argument = &action;
                            invoke_void(api.update_value, entry->setting,
                                        &argument);
                            invoke_void(api.close_options, entry->setting,
                                        NULL);
                            entry->last_draw = 0;
                        }
                    }
                }
            }
        }
        if (!down) {
            if (claim_index + 1 < scroll_claim_count) {
                scroll_claims[claim_index] =
                    scroll_claims[--scroll_claim_count];
            } else {
                --scroll_claim_count;
            }
        }
    }
    for (size_t i = 0; i < scroll_claim_count;) {
        if (!live[i]) {
            scroll_claims[i] = scroll_claims[--scroll_claim_count];
        } else {
            ++i;
        }
    }
}

static bool native_name_prefix(patch_handle_t instance, void **arguments,
                               const patch_method_signature_t *signature,
                               void *result) {
    (void)instance;
    (void)signature;
    if (!arguments || !arguments[0] || !result) return false;
    const int id = *(int *)arguments[0];
    if (id == empty_option) {
        *(patch_handle_t *)result = patchlib_string_create("");
        return true;
    }
    button_action_t *action = action_from_native_id(id);
    if (!action) return false;
    *(patch_handle_t *)result = patchlib_string_create(action->name);
    return true;
}

static bool native_style_prefix(patch_handle_t instance, void **arguments,
                                const patch_method_signature_t *signature,
                                void *result) {
    (void)instance;
    (void)signature;
    if (!arguments || !arguments[0] || !result) return false;
    const int id = *(int *)arguments[0];
    if (id != empty_option && !action_from_native_id(id)) return false;
    *(int *)result = 1;
    return true;
}

static bool native_texture_prefix(patch_handle_t instance, void **arguments,
                                  const patch_method_signature_t *signature,
                                  void *result) {
    (void)instance;
    (void)signature;
    (void)result;
    if (!arguments || !arguments[0] || !arguments[1] || !arguments[2]) {
        return false;
    }
    int id = *(int *)arguments[0];
    if (id == empty_option) {
        *(int *)arguments[0] = carrier_action;
        return false;
    }
    button_action_t *action = action_from_native_id(id);
    if (!action) return false;

    patch_handle_t *out_texture = *(patch_handle_t **)arguments[1];
    button_rect_t *out_rect = *(button_rect_t **)arguments[2];
    if (!action->icon_id[0]) {
        *(int *)arguments[0] = carrier_action;
        return false;
    }
    int width = 0;
    int height = 0;
    patch_handle_t texture =
        button_image_texture(action->icon_id, &width, &height);
    if (!texture) {
        texture = button_image_texture("@builtin-white", &width, &height);
    }
    /* The original icon drawing path dereferences the texture without checking
     * it. If both uploads fail, let GetControlTextureFrame resolve a built-in
     * icon instead of claiming that we supplied a valid custom texture. */
    if (!texture || width <= 0 || height <= 0 || !out_texture || !out_rect) {
        *(int *)arguments[0] = carrier_action;
        return false;
    }
    if (out_texture) *out_texture = texture;
    if (out_rect) *out_rect = (button_rect_t){0, 0, width, height};
    return true;
}

static bool value_prefix(patch_handle_t setting, void **arguments,
                         const patch_method_signature_t *signature,
                         void *result) {
    (void)arguments;
    (void)signature;
    if (!result || setting_index(setting) == -2) return false;
    char key[513];
    active_key(setting, key, sizeof(key));
    button_action_t *action = button_state_find_action_by_key(&state, key);
    if (!action) return false;
    *(patch_handle_t *)result = patchlib_string_create(action->name);
    return true;
}

static bool visible_prefix(patch_handle_t instance, void **arguments,
                           const patch_method_signature_t *signature,
                           void *result) {
    (void)instance;
    (void)signature;
    if (!arguments || !arguments[2] || !result) return false;
    char key[513];
    custom_slot(*(patch_handle_t *)arguments[2], key, sizeof(key));
    button_action_t *action = button_state_find_action_by_key(&state, key);
    if (!action) return false;
    *(bool *)result = action->visible && !ui_blocked();
    return true;
}

static bool slot_prefix(patch_handle_t instance, void **arguments,
                        const patch_method_signature_t *signature, void *result) {
    (void)instance;
    (void)signature;
    (void)result;
    if (drawing_count >= BUTTON_MAX_DRAW_STACK) return false;
    patch_handle_t slot =
        arguments && arguments[1] ? *(patch_handle_t *)arguments[1] : PATCH_NULL;
    button_draw_context_t *context = &drawing[drawing_count++];
    memset(context, 0, sizeof(*context));
    custom_slot(slot, context->key, sizeof(context->key));
    if (slot) {
        copy_managed_string(context->slot, sizeof(context->slot),
                            field_read_handle(api.slot_id, slot));
        context->multi = field_read_bool(api.slot_multi, slot);
    }
    return false;
}

static void slot_postfix(patch_handle_t instance, void **arguments, void *result,
                         const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    if (drawing_count > 0) --drawing_count;
}

static button_group_input_t *group_input_find(const char *id) {
    for (size_t i = 0; i < BUTTON_MAX_GROUP_INPUTS; ++i) {
        if (group_inputs[i].used && strcmp(group_inputs[i].id, id) == 0) {
            return &group_inputs[i];
        }
    }
    return NULL;
}

static button_group_input_t *group_input_allocate(const char *id) {
    for (size_t i = 0; i < BUTTON_MAX_GROUP_INPUTS; ++i) {
        if (!group_inputs[i].used) {
            memset(&group_inputs[i], 0, sizeof(group_inputs[i]));
            group_inputs[i].used = true;
            copy_text(group_inputs[i].id, sizeof(group_inputs[i].id), id);
            return &group_inputs[i];
        }
    }
    return NULL;
}

static bool group_visible_prefix(patch_handle_t instance, void **arguments,
                                 const patch_method_signature_t *signature,
                                 void *result) {
    (void)instance;
    (void)signature;
    if (!arguments || !arguments[1] || !result ||
        (!editing() && ui_blocked())) return false;
    patch_handle_t binding = *(patch_handle_t *)arguments[1];
    for (int i = 0; i < 4; ++i) {
        char key[513];
        custom_binding(binding, i, key, sizeof(key));
        button_action_t *action = button_state_find_action_by_key(&state, key);
        if (action && action->visible) {
            *(bool *)result = true;
            return true;
        }
    }
    return false;
}

static bool group_prefix(patch_handle_t instance, void **arguments,
                         const patch_method_signature_t *signature,
                         void *result) {
    (void)instance;
    (void)signature;
    (void)result;
    if (group_drawing_count >= BUTTON_MAX_DRAW_STACK) return false;
    button_group_draw_t *draw = &group_drawing[group_drawing_count++];
    memset(draw, 0, sizeof(*draw));
    draw->restore_from = render_group_count;

    patch_handle_t binding =
        arguments && arguments[0] ? *(patch_handle_t *)arguments[0] : PATCH_NULL;
    patch_handle_t config =
        arguments && arguments[1] ? *(patch_handle_t *)arguments[1] : PATCH_NULL;
    const int slot_index =
        arguments && arguments[2] ? *(int *)arguments[2] : 0;
    patch_handle_t groups =
        binding ? field_read_handle(api.binding_groups, binding) : PATCH_NULL;
    if (!groups || patchlib_array_length(groups) < 4) return false;

    char slot[1024];
    char profile[1024];
    copy_managed_string(slot, sizeof(slot),
                        field_read_handle(api.binding_slot, binding));
    profile_key(profile, sizeof(profile));
    char input_id[1536];
    snprintf(input_id, sizeof(input_id), "%s\n%s", profile, slot);
    button_group_input_t *input = group_input_find(input_id);
    if (input && (input->binding != binding || input->config != config)) {
        release_group_input(input);
        input = NULL;
    }
    if (input) {
        input->tick = tick;
        input->slot = slot_index;
    } else {
        input = group_input_allocate(input_id);
        if (input) {
            input->binding = binding;
            input->config = config;
            input->slot = slot_index;
            copy_text(input->profile, sizeof(input->profile), profile);
            input->tick = tick;
            input->binding_pin =
                il2cpp_gchandle_new ? il2cpp_gchandle_new(binding, false) : NULL;
            input->config_pin =
                il2cpp_gchandle_new ? il2cpp_gchandle_new(config, false) : NULL;
            if (!input->binding_pin || !input->config_pin) {
                release_group_input(input);
                input = NULL;
            }
        }
    }

    for (int i = 0; i < 4; ++i) {
        char key[513];
        char subslot[1024];
        custom_binding(binding, i, key, sizeof(key));
        group_slot(slot, i, subslot, sizeof(subslot));
        button_action_t *action = button_state_find_action_by_key(&state, key);
        int value = unbound_action;
        if (!patchlib_array_at(groups, (size_t)i, &value)) continue;

        if (action && action->visible && (editing() || !ui_blocked()) &&
            render_group_count < BUTTON_MAX_GROUP_BINDINGS) {
            render_groups[render_group_count++] =
                (button_render_group_t){groups, (size_t)i};
            int carrier = carrier_action;
            patchlib_array_set(groups, (size_t)i, &carrier);
            value = carrier;
        } else {
            key[0] = '\0';
        }
        if (value != unbound_action && draw->next < 4) {
            button_draw_context_t *context = &draw->entries[draw->next++];
            copy_text(context->key, sizeof(context->key), key);
            copy_text(context->slot, sizeof(context->slot), subslot);
            context->multi =
                config ? field_read_bool(api.slot_multi, config) : false;
        }
    }
    /* Building entries advances next; GUIQuickActionButton.Draw consumes the
     * same entries afterward. Start consumption at the first outer button. */
    draw->next = 0;
    return false;
}

static void restore_groups(size_t from) {
    int value = unbound_action;
    while (render_group_count > from) {
        button_render_group_t group = render_groups[--render_group_count];
        patchlib_array_set(group.array, group.index, &value);
    }
}

static void group_postfix(patch_handle_t instance, void **arguments,
                          void *result,
                          const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    if (group_drawing_count == 0) return;
    const size_t from =
        group_drawing[--group_drawing_count].restore_from;
    restore_groups(from);
    if (ready) {
        sync_input();
    } else {
        clear_input();
    }
}

static button_vec2_t adjust_position(button_vec2_t position) {
    typedef button_vec2_t (*adjust_fn_t)(button_vec2_t);
    adjust_fn_t adjust =
        (adjust_fn_t)patchlib_method_get_pointer(api.adjust);
    return adjust ? adjust(position) : position;
}

static bool group_cursor_prefix(patch_handle_t controller, void **arguments,
                                const patch_method_signature_t *signature,
                                void *result) {
    (void)signature;
    (void)result;
    patch_handle_t cursor =
        (patch_handle_t)button_hook_group_cursor_argument(arguments);
    if (!cursor || in_menu() || editing() || ui_blocked() ||
        !field_read_bool(api.cursor_down, cursor) ||
        field_read_bool(api.cursor_old, cursor) ||
        field_read_bool(api.cursor_ignore, cursor) ||
        field_read_handle(api.cursor_layer, cursor)) {
        return false;
    }

    button_vec2_t position = {0.0f, 0.0f};
    field_read(api.cursor_pos, cursor, &position, sizeof(position));
    position = adjust_position(position);
    char profile[1024];
    profile_key(profile, sizeof(profile));
    typedef void (*capture_fn_t)(void *, button_vec2_t, void *, int, void *,
                                 void *, bool, float *);
    capture_fn_t capture =
        (capture_fn_t)patchlib_method_get_pointer(api.group_capture);
    if (!capture) return false;

    for (size_t input_index = 0; input_index < BUTTON_MAX_GROUP_INPUTS;
         ++input_index) {
        button_group_input_t *input = &group_inputs[input_index];
        if (!input->used || strcmp(input->profile, profile) != 0 ||
            tick - input->tick > 1) {
            continue;
        }
        patch_handle_t groups =
            field_read_handle(api.binding_groups, input->binding);
        if (!groups || patchlib_array_length(groups) < 4) continue;
        bool all_unbound = true;
        bool custom = false;
        for (int i = 0; i < 4; ++i) {
            int value = unbound_action;
            patchlib_array_at(groups, (size_t)i, &value);
            all_unbound = all_unbound && value == unbound_action;
            char key[513];
            custom_binding(input->binding, i, key, sizeof(key));
            button_action_t *action =
                button_state_find_action_by_key(&state, key);
            custom = custom || (action && action->visible);
        }
        if (!all_unbound || !custom) continue;

        const size_t from = render_group_count;
        for (int i = 0; i < 4; ++i) {
            char key[513];
            custom_binding(input->binding, i, key, sizeof(key));
            button_action_t *action =
                button_state_find_action_by_key(&state, key);
            if (!action || !action->visible ||
                render_group_count >= BUTTON_MAX_GROUP_BINDINGS) {
                continue;
            }
            render_groups[render_group_count++] =
                (button_render_group_t){groups, (size_t)i};
            int carrier = carrier_action;
            patchlib_array_set(groups, (size_t)i, &carrier);
        }
        float size = 0.0f;
        capture(controller, position, cursor, input->slot, input->binding,
                input->config, false, &size);
        restore_groups(from);
        if (field_read_handle(api.cursor_layer, cursor)) break;
    }
    return false;
}

static bool quick_prefix(patch_handle_t instance, void **arguments,
                         const patch_method_signature_t *signature,
                         void *result) {
    (void)instance;
    (void)signature;
    (void)result;
    if (quick_drawing_count >= BUTTON_MAX_DRAW_STACK) return false;
    button_quick_draw_t *draw = &quick_drawing[quick_drawing_count++];
    memset(draw, 0, sizeof(*draw));
    if (group_drawing_count > 0) {
        button_group_draw_t *group = &group_drawing[group_drawing_count - 1];
        draw->radial = true;
        if (group->next < 4) draw->context = group->entries[group->next++];
    } else if (drawing_count > 0) {
        draw->context = drawing[drawing_count - 1];
    }
    button_action_t *action =
        button_state_find_action_by_key(&state, draw->context.key);
    if (!action || !arguments) return false;
    if (arguments[2]) {
        if (draw->radial) {
            draw->pressed = *(bool *)arguments[2];
        } else {
            *(bool *)arguments[2] = action->held;
        }
    }
    if (arguments[4]) *(bool *)arguments[4] = true;
    if (arguments[5]) *(bool *)arguments[5] = true;
    return false;
}

static void quick_postfix(patch_handle_t instance, void **arguments,
                          void *result,
                          const patch_method_signature_t *signature) {
    (void)instance;
    (void)result;
    (void)signature;
    if (quick_drawing_count == 0) return;
    button_quick_draw_t draw = quick_drawing[--quick_drawing_count];
    if (!ready || !arguments || !arguments[1]) return;
    button_action_t *action =
        button_state_find_action_by_key(&state, draw.context.key);
    if (!action || !action->visible) return;
    button_rect_t bounds =
        region_from_layout(*(patch_handle_t *)arguments[1]);
    if (bounds.width <= 0 || bounds.height <= 0) return;

    const int icon_size =
        (bounds.width < bounds.height ? bounds.width : bounds.height) * 3 / 4;
    button_rect_t icon = {
        bounds.x + (bounds.width - icon_size) / 2,
        bounds.y + (bounds.height - icon_size) / 2, icon_size, icon_size};
    const bool has_icon = draw_action_icon(action, icon);
    if (!has_icon && action->show_label) {
        draw_text(action->name, bounds,
                  action->enabled ? make_color(245, 247, 255, 255)
                                  : make_color(130, 135, 150, 255),
                  true);
    }

    char profile[1024];
    profile_key(profile, sizeof(profile));
    if (draw.radial) {
        if (draw.pressed && action->enabled &&
            radial_held_count < BUTTON_STATE_MAX_ACTIONS) {
            copy_text(radial_held[radial_held_count].key,
                      sizeof(radial_held[radial_held_count].key), action->key);
            radial_held[radial_held_count].tick = tick;
            ++radial_held_count;
        } else {
            for (size_t i = 0; i < radial_held_count; ++i) {
                if (strcmp(radial_held[i].key, action->key) == 0 &&
                    radial_held[i].tick < tick) {
                    radial_held[i] = radial_held[--radial_held_count];
                    break;
                }
            }
        }
    } else {
        add_region(bounds, action->key, profile, draw.context.slot,
                   draw.context.multi);
    }
}

static bool runtime_prefix(patch_handle_t instance, void **arguments,
                           const patch_method_signature_t *signature,
                           void *result) {
    (void)instance;
    (void)arguments;
    (void)signature;
    (void)result;
    ++runtime_depth;
    if (runtime_depth != 1) return false;
    patch_handle_t profile = profile_handle();
    patch_handle_t controls =
        profile ? field_read_handle(api.profile_controls, profile) : PATCH_NULL;
    if (!controls) return false;
    const size_t count = patchlib_array_length(controls);
    const size_t limit = count < 2048 ? count : 2048;
    for (size_t i = 0; i < limit; ++i) {
        patch_handle_t binding = PATCH_NULL;
        if (!patchlib_array_at(controls, i, &binding) || !binding) continue;
        char key[513];
        custom_binding(binding, -1, key, sizeof(key));
        button_action_t *action = button_state_find_action_by_key(&state, key);
        if (action && action->visible && !ui_blocked() &&
            render_binding_count < BUTTON_MAX_RENDER_BINDINGS) {
            render_bindings[render_binding_count++] = binding;
            const int carrier = carrier_action;
            field_write(api.binding_action, binding, &carrier, sizeof(carrier));
        }
    }
    return false;
}

static void runtime_postfix(patch_handle_t instance, void **arguments,
                            void *result,
                            const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    if (runtime_depth > 0) --runtime_depth;
    if (runtime_depth == 0) {
        restore_render();
        drawing_clear();
    }
}

static void tick_postfix(patch_handle_t instance, void **arguments, void *result,
                         const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    ++tick;
    button_image_begin_frame();
    for (size_t i = 0; i < region_count;) {
        if (tick - regions[i].tick > 2) {
            regions[i] = regions[--region_count];
        } else {
            ++i;
        }
    }
    for (size_t i = 0; i < BUTTON_MAX_GROUP_INPUTS; ++i) {
        if (group_inputs[i].used && tick - group_inputs[i].tick > 2) {
            release_group_input(&group_inputs[i]);
        }
    }
    if (in_menu() || editing()) button_state_sync_input(&state, NULL);
}

static void poll_postfix(patch_handle_t instance, void **arguments, void *result,
                         const patch_method_signature_t *signature) {
    (void)instance;
    (void)arguments;
    (void)result;
    (void)signature;
    poll_native_scroll();
    if (in_menu()) {
        clear_input();
        return;
    }
    patch_handle_t manager =
        field_read_handle(api.manager_instance, PATCH_NULL);
    if (!manager) {
        clear_input();
        return;
    }
    const bool in_editor = editing();
    const bool blocked = !in_editor && ui_blocked();
    if (blocked) {
        normal_held_clear();
        radial_held_clear();
        button_state_reset_input(&state);
        for (size_t i = 0; i < claim_count; ++i) claims[i].cancelled = true;
    }

    for (size_t i = 0; i < claim_count;) {
        if (claims[i].released) {
            claims[i] = claims[--claim_count];
        } else {
            ++i;
        }
    }

    char profile[1024];
    profile_key(profile, sizeof(profile));
    bool held[BUTTON_STATE_MAX_ACTIONS];
    memset(held, 0, sizeof(held));
    bool live[BUTTON_MAX_CLAIMS];
    memset(live, 0, sizeof(live));
    int count = invoke_int(api.count, manager, NULL);
    if (count < 0) count = 0;
    if (count > 32) count = 32;

    for (int index = 0; index < count; ++index) {
        void *argument = &index;
        patch_handle_t cursor = invoke_handle(api.at, manager, &argument);
        if (!cursor) continue;
        const int cursor_id = field_read_int(api.cursor_id, cursor);
        button_vec2_t position = {0.0f, 0.0f};
        field_read(api.cursor_pos, cursor, &position, sizeof(position));
        position = adjust_position(position);
        const bool down = field_read_bool(api.cursor_down, cursor);
        const bool old = field_read_bool(api.cursor_old, cursor);

        size_t claim_index = BUTTON_MAX_CLAIMS;
        for (size_t i = 0; i < claim_count; ++i) {
            if (claims[i].cursor == cursor_id) {
                claim_index = i;
                live[i] = true;
                break;
            }
        }

        if (claim_index == BUTTON_MAX_CLAIMS && down && !old &&
            !in_editor && !blocked &&
            !field_read_bool(api.cursor_ignore, cursor) &&
            claim_count < BUTTON_MAX_CLAIMS) {
            for (size_t i = 0; i < region_count; ++i) {
                button_region_t *region = &regions[i];
                button_action_t *action =
                    button_state_find_action_by_key(&state, region->key);
                if (!action || !action->visible ||
                    strcmp(region->profile, profile) != 0 ||
                    tick - region->tick > 2 ||
                    !contains(region->bounds, position.x, position.y)) {
                    continue;
                }
                bool cancelled = false;
                if (!region->multi) {
                    for (size_t other_index = 0; other_index < claim_count;
                         ++other_index) {
                        const button_claim_t *other = &claims[other_index];
                        if (!other->released && !other->cancelled &&
                            strcmp(other->region.profile, region->profile) == 0 &&
                            strcmp(other->region.slot, region->slot) == 0) {
                            cancelled = true;
                            break;
                        }
                    }
                }
                button_claim_t *claim = &claims[claim_count++];
                memset(claim, 0, sizeof(*claim));
                claim->cursor = cursor_id;
                claim->cancelled = cancelled;
                claim->region = *region;
                claim_index = claim_count - 1;
                live[claim_index] = true;
                break;
            }
        }
        if (claim_index == BUTTON_MAX_CLAIMS) continue;

        button_claim_t *claim = &claims[claim_index];
        field_write(api.cursor_ignore, cursor, &(bool){true}, sizeof(bool));
        field_write(api.cursor_world, cursor, &(bool){true}, sizeof(bool));
        if (!contains(claim->region.bounds, position.x, position.y)) {
            claim->cancelled = true;
        }
        button_action_t *action =
            button_state_find_action_by_key(&state, claim->region.key);
        if (down && !claim->cancelled && !in_editor && !blocked && action &&
            strcmp(claim->region.profile, profile) == 0) {
            held[action - state.actions] = true;
        }
        if (!down) claim->released = true;
    }

    for (size_t i = 0; i < claim_count;) {
        if (!live[i]) {
            claims[i] = claims[--claim_count];
        } else {
            ++i;
        }
    }
    memcpy(normal_held, held, sizeof(normal_held));
    sync_input();
}

static bool dispatch_prefix(patch_handle_t instance, void **arguments,
                            const patch_method_signature_t *signature,
                            void *result) {
    (void)instance;
    (void)signature;
    (void)result;
    if (!arguments || !arguments[0]) return false;
    patch_handle_t cursor = *(patch_handle_t *)arguments[0];
    if (!cursor) return false;
    const int cursor_id = field_read_int(api.cursor_id, cursor);
    for (size_t i = 0; i < claim_count; ++i) {
        if (claims[i].cursor == cursor_id) return true;
    }
    for (size_t i = 0; i < scroll_claim_count; ++i) {
        if (scroll_claims[i].cursor == cursor_id) return true;
    }
    return false;
}
