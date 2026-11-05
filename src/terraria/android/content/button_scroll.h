#ifndef TEFKERNEL_BUTTON_SCROLL_H
#define TEFKERNEL_BUTTON_SCROLL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct button_scroll_window_t {
    int columns;
    int rows;
    int total;
    int row;
    bool horizontal;
} button_scroll_window_t;

typedef struct button_scroll_gesture_t {
    int cursor;
    int start_row;
    int target_row;
    int cell;
    float start_y;
    float grab;
    bool rail;
    bool dragging;
} button_scroll_gesture_t;

void button_scroll_window_init(button_scroll_window_t *window);
void button_scroll_window_configure(button_scroll_window_t *window, int columns,
                                    int rows, bool horizontal);
int button_scroll_window_capacity(const button_scroll_window_t *window);
int button_scroll_window_max_row(const button_scroll_window_t *window);
void button_scroll_window_clamp(button_scroll_window_t *window);
void button_scroll_window_set_row(button_scroll_window_t *window, int row);
void button_scroll_window_set_fraction(button_scroll_window_t *window,
                                       float value);
int button_scroll_window_source(const button_scroll_window_t *window, int cell);
int button_scroll_window_thumb_size(const button_scroll_window_t *window,
                                    int height);

void button_scroll_gesture_init(button_scroll_gesture_t *gesture);
bool button_scroll_gesture_active(const button_scroll_gesture_t *gesture);
bool button_scroll_gesture_begin(button_scroll_gesture_t *gesture, int cursor,
                                 float y, int row, bool on_rail,
                                 int selected_cell, float thumb_grab);
bool button_scroll_gesture_move(button_scroll_gesture_t *gesture, int cursor,
                                float y, float pitch, float threshold);
bool button_scroll_gesture_release(button_scroll_gesture_t *gesture,
                                   int cursor);
void button_scroll_gesture_cancel(button_scroll_gesture_t *gesture);

#ifdef __cplusplus
}
#endif

#endif
