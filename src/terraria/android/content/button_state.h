#ifndef TEFKERNEL_BUTTON_STATE_H
#define TEFKERNEL_BUTTON_STATE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUTTON_STATE_MAX_ACTIONS 256
#define BUTTON_STATE_MAX_BINDINGS 2048
#define BUTTON_STATE_HISTORY_DEPTH 8

typedef struct button_action_t {
    char *key;
    char *owner; /* Internal modloader_id:mod_id namespace. */
    char modloader_id[65];
    char mod_id[65];
    char *id;
    char *name;
    char *icon_path;
    char *icon_id;
    bool enabled;
    bool visible;
    bool held;
    bool show_label;
    unsigned presses;
    unsigned releases;
} button_action_t;

typedef struct button_binding_t {
    char *profile;
    char *slot;
    char *key;
} button_binding_t;

typedef struct button_bindings_t {
    button_binding_t *items;
    size_t count;
    size_t capacity;
} button_bindings_t;

typedef struct button_state_t {
    button_action_t actions[BUTTON_STATE_MAX_ACTIONS];
    size_t action_count;
    button_bindings_t bindings;
    button_bindings_t backup;
    button_bindings_t undo[BUTTON_STATE_HISTORY_DEPTH];
    button_bindings_t redo[BUTTON_STATE_HISTORY_DEPTH];
    size_t undo_count;
    size_t redo_count;
    button_bindings_t clipboard;
    bool has_backup;
    bool has_clipboard;
} button_state_t;

void button_state_init(button_state_t *state);
void button_state_destroy(button_state_t *state);

bool button_state_valid_id(const char *id);
bool button_state_register_action(button_state_t *state, const char *owner,
                                  const char *id, const char *name,
                                  const char *icon_path, const char *icon_id,
                                  bool show_label);
bool button_state_unregister_action(button_state_t *state, const char *owner,
                                    const char *id);
void button_state_unregister_owner(button_state_t *state, const char *owner);
button_action_t *button_state_find_action(button_state_t *state,
                                          const char *owner, const char *id);
button_action_t *button_state_find_action_by_key(button_state_t *state,
                                                 const char *key);
int button_state_find_action_index(const button_state_t *state,
                                   const char *owner, const char *id);
void button_state_reset_input(button_state_t *state);
void button_state_sync_input(button_state_t *state,
                             const bool held[BUTTON_STATE_MAX_ACTIONS]);
bool button_state_is_held(const button_action_t *action);
bool button_state_consume_pressed(button_action_t *action);
bool button_state_consume_released(button_action_t *action);
bool button_state_set_action_enabled(button_action_t *action, bool enabled);
bool button_state_set_action_visible(button_action_t *action, bool visible);

const char *button_state_get_binding(const button_state_t *state,
                                     const char *profile, const char *slot);
bool button_state_set_binding(button_state_t *state, const char *profile,
                              const char *slot, const char *key);
void button_state_erase_binding(button_state_t *state, const char *profile,
                                const char *slot);
void button_state_clear_bindings(button_state_t *state);
void button_state_replace_bindings(button_state_t *state,
                                   button_bindings_t *replacement);
void button_state_clear_interaction_state(button_state_t *state);
bool button_state_add_binding(button_state_t *state, const char *profile,
                              const char *slot, const char *key);

void button_state_snapshot(button_state_t *state);
void button_state_undo(button_state_t *state);
void button_state_redo(button_state_t *state);
void button_state_begin_edit(button_state_t *state);
void button_state_cancel_edit(button_state_t *state);
void button_state_clear_history(button_state_t *state);
void button_state_clear_redo(button_state_t *state);
void button_state_copy_profile(button_state_t *state, const char *profile);
void button_state_paste_profile(button_state_t *state, const char *profile);
void button_state_clear_profile(button_state_t *state, const char *profile);

#ifdef __cplusplus
}
#endif

#endif
