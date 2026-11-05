#ifndef TEFKERNEL_BUTTON_IMAGES_H
#define TEFKERNEL_BUTTON_IMAGES_H

#include <stdbool.h>
#include <stddef.h>

#include "patchlib/type.h"

#ifdef __cplusplus
extern "C" {
#endif

bool button_image_request(const char *id, const unsigned char *rgba, size_t size,
                          int width, int height);
bool button_image_ready(const char *id, int *width, int *height);
patch_handle_t button_image_texture(const char *id, int *width, int *height);
void button_image_draw(void *sprite_batch, const char *id, float x, float y,
                       float width, float height, const void *color);
void button_image_release(const char *id);
void button_image_begin_frame(void);
void button_image_clear(void);

#ifdef __cplusplus
}
#endif

#endif
