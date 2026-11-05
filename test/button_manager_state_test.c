#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "terraria/android/content/button_scroll.h"
#include "terraria/android/content/button_hook_args.h"
#include "terraria/android/content/button_profile_cache.h"
#include "terraria/android/content/button_state.h"
#include "terraria/android/content/button_store.h"

static void test_actions(void) {
    button_state_t state;
    button_state_init(&state);

    assert(button_state_register_action(
        &state, "mod.a", "attack", "Attack", "icons/attack.png",
        "mod.a@buttons.attack.1", true));
    assert(button_state_register_action(
        &state, "mod.a", "jump", "Jump", "", "", true));
    assert(!button_state_register_action(
        &state, "mod.a", "attack", "Duplicate", "", "", true));
    assert(state.action_count == 2);

    button_action_t *attack = button_state_find_action(&state, "mod.a", "attack");
    button_action_t *jump = button_state_find_action(&state, "mod.a", "jump");
    assert(attack != NULL);
    assert(jump != NULL);
    assert(strcmp(attack->name, "Attack") == 0);

    bool held[BUTTON_STATE_MAX_ACTIONS] = {0};
    held[0] = true;
    button_state_sync_input(&state, held);
    assert(button_state_consume_pressed(attack));
    assert(button_state_consume_pressed(attack) == false);
    assert(button_state_is_held(attack));

    button_state_sync_input(&state, held);
    assert(button_state_consume_pressed(attack) == false);
    held[0] = false;
    button_state_sync_input(&state, held);
    assert(button_state_consume_released(attack));
    assert(button_state_is_held(attack) == false);

    assert(button_state_set_action_enabled(attack, false));
    held[0] = true;
    button_state_sync_input(&state, held);
    assert(button_state_is_held(attack) == false);
    assert(button_state_consume_pressed(attack) == false);

    button_state_unregister_owner(&state, "mod.a");
    assert(state.action_count == 0);
    button_state_destroy(&state);
}

static void test_bindings_and_history(void) {
    button_state_t state;
    button_state_init(&state);

    assert(button_state_set_binding(&state, "profile-a", "slot-1",
                                    "mod.a:attack"));
    assert(strcmp(button_state_get_binding(&state, "profile-a", "slot-1"),
                  "mod.a:attack") == 0);
    assert(button_state_set_binding(&state, "profile-a", "slot-1",
                                    "mod.a:jump"));
    assert(state.bindings.count == 1);

    button_state_snapshot(&state);
    button_state_set_binding(&state, "profile-a", "slot-2", "mod.a:attack");
    assert(state.bindings.count == 2);
    button_state_undo(&state);
    assert(state.bindings.count == 1);
    assert(strcmp(button_state_get_binding(&state, "profile-a", "slot-1"),
                  "mod.a:jump") == 0);
    button_state_redo(&state);
    assert(state.bindings.count == 2);

    button_state_begin_edit(&state);
    button_state_set_binding(&state, "profile-b", "slot-1", "mod.b:use");
    button_state_cancel_edit(&state);
    assert(button_state_get_binding(&state, "profile-b", "slot-1")[0] == '\0');

    button_state_erase_binding(&state, "profile-a", "slot-1");
    assert(button_state_get_binding(&state, "profile-a", "slot-1")[0] == '\0');

    button_state_copy_profile(&state, "profile-a");
    button_state_paste_profile(&state, "profile-c");
    assert(strcmp(button_state_get_binding(&state, "profile-c", "slot-2"),
                  "mod.a:attack") == 0);
    button_state_clear_profile(&state, "profile-c");
    assert(button_state_get_binding(&state, "profile-c", "slot-2")[0] == '\0');
    button_state_clear_interaction_state(&state);
    assert(state.bindings.count == 0);
    assert(state.undo_count == 0);
    assert(state.redo_count == 0);
    button_state_destroy(&state);
}

static void test_history_overflow(void) {
    button_state_t state;
    button_state_init(&state);
    for (int i = 0; i < 24; ++i) {
        char key[32];
        snprintf(key, sizeof(key), "mod:action%d", i);
        button_state_snapshot(&state);
        assert(button_state_set_binding(&state, "profile", "slot", key));
    }
    assert(state.undo_count == BUTTON_STATE_HISTORY_DEPTH);
    for (int i = 22; i >= 15; --i) {
        char expected[32];
        snprintf(expected, sizeof(expected), "mod:action%d", i);
        button_state_undo(&state);
        assert(strcmp(button_state_get_binding(&state, "profile", "slot"), expected) == 0);
    }
    for (int i = 16; i <= 23; ++i) {
        char expected[32];
        snprintf(expected, sizeof(expected), "mod:action%d", i);
        button_state_redo(&state);
        assert(strcmp(button_state_get_binding(&state, "profile", "slot"), expected) == 0);
    }
    button_state_destroy(&state);
}

static void test_binding_container_transfer(void) {
    button_state_t source;
    button_state_t destination;
    button_state_init(&source);
    button_state_init(&destination);
    button_state_add_binding(&source, "profile-z", "slot-z", "mod.z:fire");
    button_state_add_binding(&destination, "old", "old", "mod.old:use");

    button_state_replace_bindings(&destination, &source.bindings);
    assert(strcmp(button_state_get_binding(&destination, "profile-z", "slot-z"),
                  "mod.z:fire") == 0);
    assert(button_state_get_binding(&destination, "old", "old")[0] == '\0');
    assert(source.bindings.count == 0);

    button_state_destroy(&destination);
    button_state_destroy(&source);
}

static void test_scroll(void) {
    button_scroll_window_t window;
    button_scroll_window_init(&window);
    button_scroll_window_configure(&window, 3, 9, false);
    window.total = 30;
    button_scroll_window_clamp(&window);
    assert(window.row == 0);
    assert(button_scroll_window_source(&window, 0) == 0);
    assert(button_scroll_window_source(&window, 8) == 8);
    window.row = 1;
    assert(button_scroll_window_source(&window, 0) == 1);
    assert(button_scroll_window_source(&window, 1) == 2);
    assert(button_scroll_window_source(&window, 8) == 27);

    button_scroll_gesture_t gesture;
    button_scroll_gesture_init(&gesture);
    assert(button_scroll_gesture_begin(&gesture, 4, 100.0f, 1, false, 2, 0.0f));
    assert(!button_scroll_gesture_move(&gesture, 4, 98.0f, 20.0f, 5.0f));
    assert(button_scroll_gesture_move(&gesture, 4, 80.0f, 20.0f, 5.0f));
    assert(gesture.target_row == 2);
    assert(!button_scroll_gesture_release(&gesture, 4));
}

static void test_group_update_cursor_single_managed_argument(void) {
    void *cursor = (void *)0x1234;
    /* A wrong-index regression fails here instead of reading past the array. */
    void *trailing_null = NULL;
    void *arguments[] = {&cursor, &trailing_null};
    assert(button_hook_group_cursor_argument(arguments) == cursor);
}

static int profile_provider_calls;

static void profile_provider(char *output, size_t size, void *user) {
    (void)user;
    ++profile_provider_calls;
    snprintf(output, size, "profile-%d", profile_provider_calls);
}

static void test_profile_cache(void) {
    button_profile_cache_t cache;
    char profile[64];
    button_profile_cache_init(&cache);
    profile_provider_calls = 0;

    button_profile_cache_get(&cache, 10, profile_provider, NULL, profile,
                             sizeof(profile));
    assert(strcmp(profile, "profile-1") == 0);
    button_profile_cache_get(&cache, 10, profile_provider, NULL, profile,
                             sizeof(profile));
    assert(strcmp(profile, "profile-1") == 0);
    assert(profile_provider_calls == 1);

    button_profile_cache_get(&cache, 11, profile_provider, NULL, profile,
                             sizeof(profile));
    assert(strcmp(profile, "profile-2") == 0);
    assert(profile_provider_calls == 2);

    button_profile_cache_invalidate(&cache);
    button_profile_cache_get(&cache, 11, profile_provider, NULL, profile,
                             sizeof(profile));
    assert(strcmp(profile, "profile-3") == 0);
    assert(profile_provider_calls == 3);
}

static void test_store(void) {
    button_state_t state;
    button_state_init(&state);
    button_state_set_binding(&state, "profile\tone", "slot%one",
                             "mod.a:action");

    assert(button_store_save(&state, "./"));

    button_state_t loaded;
    button_state_init(&loaded);
    assert(button_store_load(&loaded, "./"));
    assert(strcmp(button_state_get_binding(&loaded, "profile\tone", "slot%one"),
                  "mod.a:action") == 0);

    button_state_destroy(&loaded);
    button_state_set_binding(&state, "profile\tone", "slot%one",
                             "mod.a:updated");
    assert(button_store_save(&state, "./"));
    button_state_init(&loaded);
    assert(button_store_load(&loaded, "./"));
    assert(strcmp(button_state_get_binding(&loaded, "profile\tone", "slot%one"),
                  "mod.a:updated") == 0);

    button_state_destroy(&state);
    button_state_destroy(&loaded);
}

int main(void) {
    test_actions();
    test_bindings_and_history();
    test_history_overflow();
    test_binding_container_transfer();
    test_scroll();
    test_group_update_cursor_single_managed_argument();
    test_profile_cache();
    test_store();
    puts("button manager state tests passed");
    return 0;
}
