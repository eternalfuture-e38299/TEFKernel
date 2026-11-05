#ifndef TEFKERNEL_BUTTON_STORE_H
#define TEFKERNEL_BUTTON_STORE_H

#include <stdbool.h>

#include "button_state.h"

#ifdef __cplusplus
extern "C" {
#endif

bool button_store_save(const button_state_t *state, const char *directory);
bool button_store_load(button_state_t *state, const char *directory);

#ifdef __cplusplus
}
#endif

#endif
