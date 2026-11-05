#include "button_state.h"

#include <stdlib.h>
#include <string.h>

static char *copy_string(const char *value) {
    if (!value) return NULL;
    const size_t length = strlen(value);
    char *copy = (char *)malloc(length + 1);
    if (!copy) return NULL;
    memcpy(copy, value, length + 1);
    return copy;
}

static void clear_bindings(button_bindings_t *bindings) {
    if (!bindings) return;
    for (size_t i = 0; i < bindings->count; ++i) {
        free(bindings->items[i].profile);
        free(bindings->items[i].slot);
        free(bindings->items[i].key);
    }
    free(bindings->items);
    memset(bindings, 0, sizeof(*bindings));
}

static bool reserve_bindings(button_bindings_t *bindings, size_t capacity) {
    if (!bindings || capacity > BUTTON_STATE_MAX_BINDINGS) return false;
    if (bindings->capacity >= capacity) return true;
    size_t next = bindings->capacity ? bindings->capacity * 2 : 32;
    if (next < capacity) next = capacity;
    if (next > BUTTON_STATE_MAX_BINDINGS) next = BUTTON_STATE_MAX_BINDINGS;
    button_binding_t *items =
        (button_binding_t *)realloc(bindings->items, next * sizeof(*items));
    if (!items) return false;
    bindings->items = items;
    bindings->capacity = next;
    return true;
}

static bool append_binding_owned(button_bindings_t *bindings, char *profile,
                                 char *slot, char *key) {
    if (!bindings || !profile || !slot || !key) return false;
    if (bindings->count >= BUTTON_STATE_MAX_BINDINGS ||
        !reserve_bindings(bindings, bindings->count + 1)) {
        return false;
    }
    button_binding_t *binding = &bindings->items[bindings->count++];
    binding->profile = profile;
    binding->slot = slot;
    binding->key = key;
    return true;
}

static bool copy_bindings(button_bindings_t *destination,
                          const button_bindings_t *source) {
    clear_bindings(destination);
    if (!source || source->count == 0) return true;
    if (!reserve_bindings(destination, source->count)) return false;

    for (size_t i = 0; i < source->count; ++i) {
        char *profile = copy_string(source->items[i].profile);
        char *slot = copy_string(source->items[i].slot);
        char *key = copy_string(source->items[i].key);
        if (!profile || !slot || !key ||
            !append_binding_owned(destination, profile, slot, key)) {
            free(profile);
            free(slot);
            free(key);
            clear_bindings(destination);
            return false;
        }
    }
    return true;
}

static int find_binding_index(const button_bindings_t *bindings,
                              const char *profile, const char *slot) {
    if (!bindings || !profile || !slot) return -1;
    for (size_t i = 0; i < bindings->count; ++i) {
        if (strcmp(bindings->items[i].profile, profile) == 0 &&
            strcmp(bindings->items[i].slot, slot) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void erase_binding_at(button_bindings_t *bindings, size_t index) {
    if (!bindings || index >= bindings->count) return;
    free(bindings->items[index].profile);
    free(bindings->items[index].slot);
    free(bindings->items[index].key);
    if (index + 1 < bindings->count) {
        memmove(&bindings->items[index], &bindings->items[index + 1],
                (bindings->count - index - 1) * sizeof(*bindings->items));
    }
    --bindings->count;
}

static void push_history(button_bindings_t *history, size_t *count,
                         const button_bindings_t *current) {
    if (*count == BUTTON_STATE_HISTORY_DEPTH) {
        clear_bindings(&history[0]);
        memmove(&history[0], &history[1],
                (BUTTON_STATE_HISTORY_DEPTH - 1) * sizeof(*history));
        --*count;
        /* The last slot now aliases the moved entry; ownership moved with it. */
        memset(&history[*count], 0, sizeof(history[*count]));
    }
    if (copy_bindings(&history[*count], current)) ++*count;
}

static void pop_history(button_bindings_t *history, size_t *count,
                        button_bindings_t *destination) {
    if (*count == 0) return;
    clear_bindings(destination);
    *destination = history[*count - 1];
    memset(&history[*count - 1], 0, sizeof(history[*count - 1]));
    --*count;
}

static char *make_key(const char *owner, const char *id) {
    if (!owner || !id) return NULL;
    const size_t owner_length = strlen(owner);
    const size_t id_length = strlen(id);
    char *key = (char *)malloc(owner_length + id_length + 2);
    if (!key) return NULL;
    memcpy(key, owner, owner_length);
    key[owner_length] = ':';
    memcpy(key + owner_length + 1, id, id_length + 1);
    return key;
}

void button_state_init(button_state_t *state) {
    if (state) memset(state, 0, sizeof(*state));
}

void button_state_destroy(button_state_t *state) {
    if (!state) return;
    for (size_t i = 0; i < state->action_count; ++i) {
        button_action_t *action = &state->actions[i];
        free(action->key);
        free(action->owner);
        free(action->id);
        free(action->name);
        free(action->icon_path);
        free(action->icon_id);
    }
    clear_bindings(&state->bindings);
    clear_bindings(&state->backup);
    clear_bindings(&state->clipboard);
    for (size_t i = 0; i < BUTTON_STATE_HISTORY_DEPTH; ++i) {
        clear_bindings(&state->undo[i]);
        clear_bindings(&state->redo[i]);
    }
    memset(state, 0, sizeof(*state));
}

bool button_state_valid_id(const char *id) {
    if (!id) return false;
    const size_t length = strlen(id);
    if (length == 0 || length > 64) return false;
    for (const unsigned char *p = (const unsigned char *)id; *p; ++p) {
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' ||
              *p == '.')) {
            return false;
        }
    }
    return true;
}

bool button_state_register_action(button_state_t *state, const char *owner,
                                  const char *id, const char *name,
                                  const char *icon_path, const char *icon_id,
                                  bool show_label) {
    if (!state || !owner || *owner == '\0' || strlen(owner) > 129 ||
        !button_state_valid_id(id) || !name || *name == '\0' ||
        strlen(name) > 128 || state->action_count >= BUTTON_STATE_MAX_ACTIONS ||
        (icon_path && strlen(icon_path) > 512) ||
        (icon_id && strlen(icon_id) > 768) ||
        button_state_find_action(state, owner, id)) {
        return false;
    }

    button_action_t action;
    memset(&action, 0, sizeof(action));
    action.key = make_key(owner, id);
    action.owner = copy_string(owner);
    action.id = copy_string(id);
    action.name = copy_string(name);
    action.icon_path = copy_string(icon_path ? icon_path : "");
    action.icon_id = copy_string(icon_id ? icon_id : "");
    action.enabled = true;
    action.visible = true;
    action.show_label = show_label;
    if (!action.key || !action.owner || !action.id || !action.name ||
        !action.icon_path || !action.icon_id) {
        free(action.key);
        free(action.owner);
        free(action.id);
        free(action.name);
        free(action.icon_path);
        free(action.icon_id);
        return false;
    }
    state->actions[state->action_count++] = action;
    return true;
}

bool button_state_unregister_action(button_state_t *state, const char *owner,
                                    const char *id) {
    if (!state || !owner || !id) return false;
    const int index = button_state_find_action_index(state, owner, id);
    if (index < 0) return false;

    button_action_t *action = &state->actions[index];
    free(action->key);
    free(action->owner);
    free(action->id);
    free(action->name);
    free(action->icon_path);
    free(action->icon_id);
    if ((size_t)index + 1 < state->action_count) {
        memmove(&state->actions[index], &state->actions[index + 1],
                (state->action_count - (size_t)index - 1) *
                    sizeof(state->actions[0]));
    }
    --state->action_count;
    memset(&state->actions[state->action_count], 0,
           sizeof(state->actions[0]));
    return true;
}

void button_state_unregister_owner(button_state_t *state, const char *owner) {
    if (!state || !owner) return;
    for (size_t i = 0; i < state->action_count;) {
        if (strcmp(state->actions[i].owner, owner) == 0) {
            char id[65];
            strncpy(id, state->actions[i].id, sizeof(id) - 1);
            id[sizeof(id) - 1] = '\0';
            button_state_unregister_action(state, owner, id);
        } else {
            ++i;
        }
    }
}

button_action_t *button_state_find_action(button_state_t *state,
                                          const char *owner, const char *id) {
    const int index = button_state_find_action_index(state, owner, id);
    return index < 0 ? NULL : &state->actions[index];
}

button_action_t *button_state_find_action_by_key(button_state_t *state,
                                                 const char *key) {
    if (!state || !key) return NULL;
    for (size_t i = 0; i < state->action_count; ++i) {
        if (strcmp(state->actions[i].key, key) == 0) {
            return &state->actions[i];
        }
    }
    return NULL;
}

int button_state_find_action_index(const button_state_t *state,
                                   const char *owner, const char *id) {
    if (!state || !owner || !id) return -1;
    for (size_t i = 0; i < state->action_count; ++i) {
        if (strcmp(state->actions[i].owner, owner) == 0 &&
            strcmp(state->actions[i].id, id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

void button_state_reset_input(button_state_t *state) {
    if (!state) return;
    for (size_t i = 0; i < state->action_count; ++i) {
        state->actions[i].held = false;
        state->actions[i].presses = 0;
        state->actions[i].releases = 0;
    }
}

void button_state_sync_input(button_state_t *state,
                             const bool held[BUTTON_STATE_MAX_ACTIONS]) {
    if (!state) return;
    for (size_t i = 0; i < state->action_count; ++i) {
        button_action_t *action = &state->actions[i];
        const bool next =
            held && held[i] && action->enabled && action->visible;
        if (next && !action->held && action->presses < 64) {
            ++action->presses;
        }
        if (!next && action->held && action->releases < 64) {
            ++action->releases;
        }
        action->held = next;
    }
}

bool button_state_is_held(const button_action_t *action) {
    return action && action->held && action->enabled && action->visible;
}

bool button_state_consume_pressed(button_action_t *action) {
    if (!action || !action->enabled || !action->visible || action->presses == 0) {
        return false;
    }
    --action->presses;
    return true;
}

bool button_state_consume_released(button_action_t *action) {
    if (!action || !action->enabled || !action->visible || action->releases == 0) {
        return false;
    }
    --action->releases;
    return true;
}

bool button_state_set_action_enabled(button_action_t *action, bool enabled) {
    if (!action) return false;
    action->enabled = enabled;
    if (!enabled) {
        action->held = false;
        action->presses = 0;
        action->releases = 0;
    }
    return true;
}

bool button_state_set_action_visible(button_action_t *action, bool visible) {
    if (!action) return false;
    action->visible = visible;
    if (!visible) {
        action->held = false;
        action->presses = 0;
        action->releases = 0;
    }
    return true;
}

const char *button_state_get_binding(const button_state_t *state,
                                     const char *profile, const char *slot) {
    const int index = find_binding_index(&state->bindings, profile, slot);
    return index < 0 ? "" : state->bindings.items[index].key;
}

bool button_state_add_binding(button_state_t *state, const char *profile,
                              const char *slot, const char *key) {
    if (!state || !profile || *profile == '\0' || !slot || *slot == '\0' ||
        !key || *key == '\0' || state->bindings.count >= BUTTON_STATE_MAX_BINDINGS) {
        return false;
    }
    char *profile_copy = copy_string(profile);
    char *slot_copy = copy_string(slot);
    char *key_copy = copy_string(key);
    if (!profile_copy || !slot_copy || !key_copy ||
        !append_binding_owned(&state->bindings, profile_copy, slot_copy,
                              key_copy)) {
        free(profile_copy);
        free(slot_copy);
        free(key_copy);
        return false;
    }
    return true;
}

bool button_state_set_binding(button_state_t *state, const char *profile,
                              const char *slot, const char *key) {
    if (!state || !profile || !slot) return false;
    if (!key || *key == '\0') {
        button_state_erase_binding(state, profile, slot);
        return true;
    }

    const int existing = find_binding_index(&state->bindings, profile, slot);
    if (existing < 0) return button_state_add_binding(state, profile, slot, key);

    char *key_copy = copy_string(key);
    if (!key_copy) return false;
    free(state->bindings.items[existing].key);
    state->bindings.items[existing].key = key_copy;
    return true;
}

void button_state_erase_binding(button_state_t *state, const char *profile,
                                const char *slot) {
    if (!state) return;
    const int index = find_binding_index(&state->bindings, profile, slot);
    if (index >= 0) erase_binding_at(&state->bindings, (size_t)index);
}

void button_state_clear_bindings(button_state_t *state) {
    if (state) clear_bindings(&state->bindings);
}

void button_state_replace_bindings(button_state_t *state,
                                   button_bindings_t *replacement) {
    if (!state || !replacement) return;
    clear_bindings(&state->bindings);
    state->bindings = *replacement;
    memset(replacement, 0, sizeof(*replacement));
}

void button_state_clear_interaction_state(button_state_t *state) {
    if (!state) return;
    clear_bindings(&state->bindings);
    clear_bindings(&state->backup);
    clear_bindings(&state->clipboard);
    button_state_clear_history(state);
    state->has_backup = false;
    state->has_clipboard = false;
    button_state_reset_input(state);
}

void button_state_snapshot(button_state_t *state) {
    if (!state) return;
    push_history(state->undo, &state->undo_count, &state->bindings);
    for (size_t i = 0; i < state->redo_count; ++i) {
        clear_bindings(&state->redo[i]);
    }
    state->redo_count = 0;
}

void button_state_undo(button_state_t *state) {
    if (!state || state->undo_count == 0) return;
    push_history(state->redo, &state->redo_count, &state->bindings);
    pop_history(state->undo, &state->undo_count, &state->bindings);
    button_state_reset_input(state);
}

void button_state_redo(button_state_t *state) {
    if (!state || state->redo_count == 0) return;
    push_history(state->undo, &state->undo_count, &state->bindings);
    pop_history(state->redo, &state->redo_count, &state->bindings);
    button_state_reset_input(state);
}

void button_state_begin_edit(button_state_t *state) {
    if (!state) return;
    copy_bindings(&state->backup, &state->bindings);
    state->has_backup = true;
    button_state_clear_history(state);
    button_state_reset_input(state);
}

void button_state_cancel_edit(button_state_t *state) {
    if (!state || !state->has_backup) return;
    copy_bindings(&state->bindings, &state->backup);
    button_state_clear_history(state);
    button_state_reset_input(state);
}

void button_state_clear_history(button_state_t *state) {
    if (!state) return;
    for (size_t i = 0; i < state->undo_count; ++i) {
        clear_bindings(&state->undo[i]);
    }
    for (size_t i = 0; i < state->redo_count; ++i) {
        clear_bindings(&state->redo[i]);
    }
    state->undo_count = 0;
    state->redo_count = 0;
}

void button_state_clear_redo(button_state_t *state) {
    if (!state) return;
    for (size_t i = 0; i < state->redo_count; ++i) {
        clear_bindings(&state->redo[i]);
    }
    state->redo_count = 0;
}

void button_state_copy_profile(button_state_t *state, const char *profile) {
    if (!state || !profile) return;
    clear_bindings(&state->clipboard);
    for (size_t i = 0; i < state->bindings.count; ++i) {
        const button_binding_t *binding = &state->bindings.items[i];
        if (strcmp(binding->profile, profile) == 0) {
            char *profile_copy = copy_string(binding->profile);
            char *slot = copy_string(binding->slot);
            char *key = copy_string(binding->key);
            if (!profile_copy || !slot || !key ||
                !append_binding_owned(&state->clipboard, profile_copy, slot,
                                      key)) {
                free(profile_copy);
                free(slot);
                free(key);
                break;
            }
        }
    }
    state->has_clipboard = true;
}

void button_state_paste_profile(button_state_t *state, const char *profile) {
    if (!state || !profile || !state->has_clipboard) return;
    button_state_clear_profile(state, profile);
    for (size_t i = 0; i < state->clipboard.count; ++i) {
        const button_binding_t *binding = &state->clipboard.items[i];
        button_state_set_binding(state, profile, binding->slot, binding->key);
    }
    button_state_reset_input(state);
}

void button_state_clear_profile(button_state_t *state, const char *profile) {
    if (!state || !profile) return;
    for (size_t i = 0; i < state->bindings.count;) {
        if (strcmp(state->bindings.items[i].profile, profile) == 0) {
            erase_binding_at(&state->bindings, i);
        } else {
            ++i;
        }
    }
}
