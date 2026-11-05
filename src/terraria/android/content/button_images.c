#include "button_images.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "button_log.h"
#include "patchlib/field.h"
#include "patchlib/il2cpp_api.h"
#include "patchlib/method.h"
#include "terraria/texture2d.h"

#define BUTTON_IMAGE_MAX 256
#define BUTTON_IMAGE_MAX_BYTES (24u * 1024u * 1024u)

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

typedef struct button_image_entry_t {
    char id[768];
    unsigned char *pixels;
    int pixel_width;
    int pixel_height;
    size_t pixel_bytes;
    patch_handle_t texture;
    void *pin;
    int width;
    int height;
    size_t bytes;
    unsigned long long last_use;
    int state;
} button_image_entry_t;

static button_image_entry_t images[BUTTON_IMAGE_MAX];
static size_t image_count;
static size_t texture_bytes;
static size_t pixel_bytes;
static unsigned long long frame_number;
static patch_handle_t draw_method;
static void *native_draw;
static bool draw_resolved;
static patch_handle_t cached_dispose;

static void copy_text(char *destination, size_t size, const char *source) {
    if (!destination || size == 0) return;
    if (!source) source = "";
    snprintf(destination, size, "%s", source);
}

static button_image_entry_t *find_image(const char *id) {
    if (!id) return NULL;
    for (size_t i = 0; i < image_count; ++i) {
        if (strcmp(images[i].id, id) == 0) return &images[i];
    }
    return NULL;
}

static void resolve_draw(void) {
    if (draw_resolved) return;
    draw_resolved = true;

    patch_handle_t sprite_batch =
        patchlib_type_get_type("Microsoft.Xna.Framework.Graphics", "SpriteBatch");
    patch_handle_t texture =
        patchlib_type_get_type("Microsoft.Xna.Framework.Graphics", "Texture2D");
    patch_handle_t rectangle =
        patchlib_type_get_type("Microsoft.Xna.Framework", "Rectangle");
    patch_handle_t color =
        patchlib_type_get_type("Microsoft.Xna.Framework.Graphics", "Color");
    patch_handle_t vector =
        patchlib_type_get_type("Microsoft.Xna.Framework", "Vector2");
    patch_handle_t single = patchlib_type_get_type("System", "Single");
    patch_handle_t effects = patchlib_type_get_type(
        "Microsoft.Xna.Framework.Graphics", "SpriteEffects");

    patch_handle_t types[] = {texture, vector, rectangle, color, single, vector,
                              vector, effects, single};
    /* Match the ref Rectangle overload, not Nullable<Rectangle> sourceRectangle.
     * Its native ARM64 ABI takes Rectangle*, which draw_part supplies below. */
    const char *names[] = {"texture", "position", "srcRect", "color",
                           "rotation", "origin", "scale", "effects",
                           "layerDepth"};
    draw_method = patchlib_type_get_method_by_signature(sprite_batch, "Draw", 9,
                                                        types, names);
    if (draw_method) {
        for (unsigned i = 0; i < 9; ++i) {
            void *parameter = il2cpp_method_get_param(draw_method, i);
            if (!parameter || il2cpp_type_is_byref(parameter) != (i == 2)) {
                draw_method = PATCH_NULL;
                return;
            }
        }
        native_draw = patchlib_method_get_pointer(draw_method);
        if (native_draw) {
            BUTTON_LOG_INFO("[button-image] Draw(ref Rectangle) resolved");
        }
    }
}

static patch_handle_t dispose_method(void) {
    if (cached_dispose) return cached_dispose;
    patch_handle_t type =
        patchlib_type_get_type("Microsoft.Xna.Framework.Graphics", "Texture2D");
    for (int i = 0; type && i < 8; ++i) {
        patch_handle_t method =
            patchlib_type_get_method_by_param_count(type, "Dispose", 0);
        if (method) {
            cached_dispose = method;
            return method;
        }
        type = patchlib_type_get_parent(type);
    }
    return PATCH_NULL;
}

static void destroy_texture(patch_handle_t texture) {
    if (!texture) return;
    patch_handle_t dispose = dispose_method();
    if (dispose) {
        patchlib_method_invoke_args(dispose, texture, NULL, NULL);
    }
}

static void unload_image(button_image_entry_t *entry) {
    if (!entry) return;
    destroy_texture(entry->texture);
    if (entry->pin && il2cpp_gchandle_free) il2cpp_gchandle_free(entry->pin);
    if (texture_bytes >= entry->bytes) texture_bytes -= entry->bytes;
    entry->texture = PATCH_NULL;
    entry->pin = NULL;
    entry->width = 0;
    entry->height = 0;
    entry->bytes = 0;
    entry->state = 0;
}

static bool make_room(size_t bytes, button_image_entry_t *protect) {
    if (bytes > BUTTON_IMAGE_MAX_BYTES) return false;
    while (texture_bytes + bytes > BUTTON_IMAGE_MAX_BYTES) {
        button_image_entry_t *oldest = NULL;
        for (size_t i = 0; i < image_count; ++i) {
            button_image_entry_t *entry = &images[i];
            if (entry == protect || !entry->texture ||
                strcmp(entry->id, "@builtin-white") == 0 ||
                entry->last_use + 2 >= frame_number) {
                continue;
            }
            if (!oldest || entry->last_use < oldest->last_use) oldest = entry;
        }
        if (!oldest) return false;
        unload_image(oldest);
    }
    return true;
}

static bool load_image(button_image_entry_t *entry) {
    if (!entry) return false;
    if (entry->state == 1) return true;
    if (entry->state == -1) return false;

    resolve_draw();
    if (!native_draw) {
        entry->state = -1;
        BUTTON_LOG_WARN("[button-image] drawing or texture API unavailable");
        return false;
    }

    unsigned char white[4] = {255, 255, 255, 255};
    const bool builtin = strcmp(entry->id, "@builtin-white") == 0;
    const int width = builtin ? 1 : entry->pixel_width;
    const int height = builtin ? 1 : entry->pixel_height;
    const size_t bytes = builtin ? 4 : entry->pixel_bytes;
    if (!make_room(bytes, entry)) return false;
    patch_handle_t texture = terraria_texture2d_create(
        width, height, TEXTURE_FORMAT_RGBA32,
        builtin ? white : entry->pixels, bytes);
    if (!texture || !il2cpp_gchandle_new) {
        destroy_texture(texture);
        entry->state = -1;
        BUTTON_LOG_WARN("[button-image] texture upload failed: %s", entry->id);
        return false;
    }

    void *pin = il2cpp_gchandle_new(texture, false);
    if (!pin) {
        destroy_texture(texture);
        entry->state = -1;
        return false;
    }
    entry->texture = texture;
    entry->pin = pin;
    entry->width = width;
    entry->height = height;
    entry->bytes = bytes;
    entry->state = 1;
    texture_bytes += bytes;
    return true;
}

static void draw_part(void *sprite_batch, button_image_entry_t *entry, float x,
                      float y, float width, float height,
                      const button_color_t *color, button_rect_t source) {
    if (!sprite_batch || !entry || !entry->texture || !color || !native_draw || width <= 0.0f ||
        height <= 0.0f || source.width <= 0 || source.height <= 0) {
        return;
    }

    button_color_t tint = *color;
    tint.b = (unsigned char)((tint.b * tint.a + 127) / 255);
    tint.g = (unsigned char)((tint.g * tint.a + 127) / 255);
    tint.r = (unsigned char)((tint.r * tint.a + 127) / 255);
    uint32_t packed;
    memcpy(&packed, &tint, sizeof(packed));
    typedef void (*draw_fn_t)(void *, patch_handle_t, button_vec2_t,
                              button_rect_t *, uint32_t, float, button_vec2_t,
                              button_vec2_t, int, float, const void *);
    draw_fn_t draw = (draw_fn_t)native_draw;
    draw(sprite_batch, entry->texture, (button_vec2_t){x, y}, &source, packed,
         0.0f, (button_vec2_t){0.0f, 0.0f},
         (button_vec2_t){width / source.width, height / source.height}, 0, 0.0f,
         NULL);
}

bool button_image_request(const char *id, const unsigned char *rgba, size_t size,
                          int width, int height) {
    if (!id || !*id || strlen(id) >= sizeof(images[0].id) ||
        !rgba || width <= 0 || height <= 0 || width > 2048 || height > 2048 ||
        size != (size_t)width * (size_t)height * 4 ||
        size > BUTTON_IMAGE_MAX_BYTES - pixel_bytes ||
        find_image(id) || image_count >= BUTTON_IMAGE_MAX) return false;
    unsigned char *pixels = malloc(size);
    if (!pixels) return false;
    /* Match Texture2D's existing upload orientation and premultiplied alpha. */
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const unsigned char *src = rgba + ((size_t)y * width + x) * 4;
            unsigned char *dst = pixels + ((size_t)(height - y - 1) * width + x) * 4;
            for (int c = 0; c < 3; ++c)
                dst[c] = (unsigned char)((src[c] * src[3] + 127) / 255);
            dst[3] = src[3];
        }
    }
    button_image_entry_t *entry = &images[image_count++];
    memset(entry, 0, sizeof(*entry));
    copy_text(entry->id, sizeof(entry->id), id);
    entry->pixels = pixels;
    entry->pixel_width = width;
    entry->pixel_height = height;
    entry->pixel_bytes = size;
    pixel_bytes += size;
    return true;
}

bool button_image_ready(const char *id, int *width, int *height) {
    button_image_entry_t *entry = find_image(id);
    if (!entry) return false;
    if (entry->state == 0) load_image(entry);
    if (width) *width = entry->width;
    if (height) *height = entry->height;
    return entry->state == 1;
}

patch_handle_t button_image_texture(const char *id, int *width, int *height) {
    button_image_entry_t *entry = find_image(id);
    if (!entry && id && strcmp(id, "@builtin-white") == 0) {
        if (image_count >= BUTTON_IMAGE_MAX) return PATCH_NULL;
        entry = &images[image_count++];
        memset(entry, 0, sizeof(*entry));
        copy_text(entry->id, sizeof(entry->id), id);
    }
    if (!entry) return PATCH_NULL;
    entry->last_use = frame_number;
    if (!load_image(entry)) return PATCH_NULL;
    if (width) *width = entry->width;
    if (height) *height = entry->height;
    return entry->texture;
}

void button_image_draw(void *sprite_batch, const char *id, float x, float y,
                       float width, float height, const void *color) {
    if (!isfinite(x) || !isfinite(y) || !isfinite(width) ||
        !isfinite(height) || width <= 0.0f || height <= 0.0f || !color) {
        return;
    }
    button_image_entry_t *entry = find_image(id);
    if (!entry || !sprite_batch) return;
    entry->last_use = frame_number;
    if (!load_image(entry)) return;
    draw_part(sprite_batch, entry, x, y, width, height,
              (const button_color_t *)color,
              (button_rect_t){0, 0, entry->width, entry->height});
}

void button_image_release(const char *id) {
    button_image_entry_t *entry = find_image(id);
    if (!entry) return;
    unload_image(entry);
    free(entry->pixels);
    pixel_bytes -= entry->pixel_bytes;
    const size_t index = (size_t)(entry - images);
    if (index + 1 < image_count) {
        memmove(&images[index], &images[index + 1],
                (image_count - index - 1) * sizeof(images[0]));
    }
    --image_count;
    memset(&images[image_count], 0, sizeof(images[0]));
}

void button_image_begin_frame(void) {
    ++frame_number;
}

void button_image_clear(void) {
    for (size_t i = 0; i < image_count; ++i) {
        unload_image(&images[i]);
        free(images[i].pixels);
    }
    pixel_bytes = 0;
    memset(images, 0, sizeof(images));
    image_count = 0;
    texture_bytes = 0;
    frame_number = 0;
    draw_method = PATCH_NULL;
    native_draw = NULL;
    draw_resolved = false;
    cached_dispose = PATCH_NULL;
}
