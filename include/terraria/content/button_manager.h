#ifndef TEFKERNEL_BUTTON_MANAGER_H
#define TEFKERNEL_BUTTON_MANAGER_H
#include <stdbool.h>
#include "../../tef_api.h"
#if defined(__ANDROID__)
#ifdef __cplusplus
extern "C" {
#endif
/* Strings are copied at registration. root is the owner's private asset directory.
 * Calls must run on the game thread. Events are consumed by pressed/released. */
typedef struct terraria_button_desc_t {
    const char *owner, *id, *name, *root, *icon;
    bool show_label;
} terraria_button_desc_t;
DEFINE_FUNCTION(bool, terraria_button_manager_available, void)
DEFINE_FUNCTION(const char*, terraria_button_manager_error, void)
DEFINE_FUNCTION(void, terraria_button_manager_configure_storage, const char *directory)
DEFINE_FUNCTION(void, terraria_button_manager_install, void)
DEFINE_FUNCTION(void, terraria_button_manager_shutdown, void)
DEFINE_FUNCTION(bool, terraria_button_manager_register, const terraria_button_desc_t *desc)
DEFINE_FUNCTION(bool, terraria_button_manager_unregister, const char *owner, const char *id)
DEFINE_FUNCTION(void, terraria_button_manager_unregister_owner, const char *owner)
DEFINE_FUNCTION(bool, terraria_button_manager_down, const char *owner, const char *id)
DEFINE_FUNCTION(bool, terraria_button_manager_pressed, const char *owner, const char *id)
DEFINE_FUNCTION(bool, terraria_button_manager_released, const char *owner, const char *id)
DEFINE_FUNCTION(bool, terraria_button_manager_set_enabled, const char *owner, const char *id, bool value)
DEFINE_FUNCTION(bool, terraria_button_manager_set_visible, const char *owner, const char *id, bool value)
#ifdef __cplusplus
}
#endif
#endif
#endif
