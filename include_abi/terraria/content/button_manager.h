#ifndef TEFKERNEL_BUTTON_MANAGER_H
#define TEFKERNEL_BUTTON_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

#include "../../tef_api.h"

#if defined(__ANDROID__)
#ifdef __cplusplus
extern "C" {
#endif

/* Identity strings are copied. All calls must run on the game thread.
 * Each identity component follows the button id grammar (1..64 characters).
 * Existing consumers must rebuild against this descriptor and scoped API. */
typedef struct terraria_button_scope_t {
    const char *modloader_id;
    const char *mod_id;
} terraria_button_scope_t;

/* Decoded, tightly packed RGBA8, top row first, straight alpha.
 * Pixels are copied during registration; the caller retains its buffer. */
typedef struct terraria_button_image_t {
    const unsigned char *rgba;
    size_t size;
    int width;
    int height;
} terraria_button_image_t;
typedef struct terraria_button_desc_t {
    terraria_button_scope_t scope;
    const char *id;
    const char *name;
    const terraria_button_image_t *icon;
    bool show_label;
} terraria_button_desc_t;

DEFINE_FUNCTION(bool, terraria_button_manager_available, void)
DEFINE_FUNCTION(const char *, terraria_button_manager_error, void)
DEFINE_FUNCTION(void, terraria_button_manager_configure_storage,
                const char *directory)
DEFINE_FUNCTION(void, terraria_button_manager_install, void)
DEFINE_FUNCTION(void, terraria_button_manager_shutdown, void)
DEFINE_FUNCTION(bool, terraria_button_manager_register,
                const terraria_button_desc_t *desc)
DEFINE_FUNCTION(bool, terraria_button_manager_unregister, const terraria_button_scope_t *scope,
                const char *id)
DEFINE_FUNCTION(void, terraria_button_manager_unregister_owner,
                const terraria_button_scope_t *scope)
DEFINE_FUNCTION(bool, terraria_button_manager_down, const terraria_button_scope_t *scope,
                const char *id)
DEFINE_FUNCTION(bool, terraria_button_manager_pressed, const terraria_button_scope_t *scope,
                const char *id)
DEFINE_FUNCTION(bool, terraria_button_manager_released, const terraria_button_scope_t *scope,
                const char *id)
DEFINE_FUNCTION(bool, terraria_button_manager_set_enabled, const terraria_button_scope_t *scope,
                const char *id, bool value)
DEFINE_FUNCTION(bool, terraria_button_manager_set_visible, const terraria_button_scope_t *scope,
                const char *id, bool value)

#ifdef __cplusplus
}
#endif
#endif

#endif
