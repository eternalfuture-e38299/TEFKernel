#include "button_scroll.h"

static int round_float(float value) {
    return (int)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

static float absolute_float(float value) {
    return value < 0.0f ? -value : value;
}

static int clamp_int(int value, int minimum, int maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

void button_scroll_window_init(button_scroll_window_t *window) {
    if (!window) return;
    window->columns = 3;
    window->rows = 9;
    window->total = 0;
    window->row = 0;
    window->horizontal = false;
}

void button_scroll_window_configure(button_scroll_window_t *window, int columns,
                                    int rows, bool horizontal) {
    if (!window) return;
    window->columns = clamp_int(columns, 1, 27);
    window->rows = clamp_int(rows, 1, 27 / window->columns);
    window->horizontal = horizontal;
    button_scroll_window_clamp(window);
}

int button_scroll_window_capacity(const button_scroll_window_t *window) {
    if (!window) return 0;
    return window->columns * window->rows;
}

int button_scroll_window_max_row(const button_scroll_window_t *window) {
    if (!window || window->columns <= 0) return 0;
    const int logical_rows = (window->total + window->columns - 1) /
                             window->columns;
    return logical_rows > window->rows ? logical_rows - window->rows : 0;
}

void button_scroll_window_clamp(button_scroll_window_t *window) {
    if (!window) return;
    window->row = clamp_int(window->row, 0,
                            button_scroll_window_max_row(window));
}

void button_scroll_window_set_row(button_scroll_window_t *window, int row) {
    if (!window) return;
    window->row = row;
    button_scroll_window_clamp(window);
}

void button_scroll_window_set_fraction(button_scroll_window_t *window,
                                       float value) {
    if (!window) return;
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    button_scroll_window_set_row(
        window, round_float(value * (float)button_scroll_window_max_row(window)));
}

int button_scroll_window_source(const button_scroll_window_t *window, int cell) {
    if (!window || cell < 0 ||
        cell >= button_scroll_window_capacity(window)) {
        return -1;
    }

    const int column = window->horizontal ? cell % window->columns
                                          : cell / window->rows;
    const int line = (window->horizontal ? cell / window->columns
                                         : cell % window->rows) +
                     window->row;
    int index;
    if (line < window->rows) {
        index = window->horizontal ? line * window->columns + column
                                   : column * window->rows + line;
    } else {
        index = button_scroll_window_capacity(window) +
                (line - window->rows) * window->columns + column;
    }
    return index < window->total ? index : -1;
}

int button_scroll_window_thumb_size(const button_scroll_window_t *window,
                                    int height) {
    if (!window || height <= 0) return 0;
    const int max_row = button_scroll_window_max_row(window);
    if (max_row == 0) return height;

    int thumb = height * window->rows / (window->rows + max_row);
    if (thumb < 24) thumb = 24;
    return thumb < height ? thumb : height;
}

void button_scroll_gesture_init(button_scroll_gesture_t *gesture) {
    if (!gesture) return;
    gesture->cursor = -1;
    gesture->start_row = 0;
    gesture->target_row = 0;
    gesture->cell = -1;
    gesture->start_y = 0.0f;
    gesture->grab = 0.0f;
    gesture->rail = false;
    gesture->dragging = false;
}

bool button_scroll_gesture_active(const button_scroll_gesture_t *gesture) {
    return gesture && gesture->cursor != -1;
}

bool button_scroll_gesture_begin(button_scroll_gesture_t *gesture, int cursor,
                                 float y, int row, bool on_rail,
                                 int selected_cell, float thumb_grab) {
    if (!gesture || button_scroll_gesture_active(gesture)) return false;
    gesture->cursor = cursor;
    gesture->start_y = y;
    gesture->start_row = row;
    gesture->target_row = row;
    gesture->rail = on_rail;
    gesture->dragging = on_rail;
    gesture->cell = selected_cell;
    gesture->grab = thumb_grab;
    return true;
}

bool button_scroll_gesture_move(button_scroll_gesture_t *gesture, int cursor,
                                float y, float pitch, float threshold) {
    if (!gesture || cursor != gesture->cursor || gesture->rail || pitch <= 0.0f) {
        return false;
    }

    const float delta = gesture->start_y - y;
    if (absolute_float(delta) >= threshold) gesture->dragging = true;
    if (!gesture->dragging) return false;
    gesture->target_row =
        gesture->start_row + round_float(delta / pitch);
    return true;
}

bool button_scroll_gesture_release(button_scroll_gesture_t *gesture,
                                   int cursor) {
    if (!gesture || cursor != gesture->cursor) return false;
    const bool tap = !gesture->dragging && !gesture->rail;
    button_scroll_gesture_cancel(gesture);
    return tap;
}

void button_scroll_gesture_cancel(button_scroll_gesture_t *gesture) {
    if (!gesture) return;
    gesture->cursor = -1;
    gesture->dragging = false;
    gesture->rail = false;
    gesture->cell = -1;
}
