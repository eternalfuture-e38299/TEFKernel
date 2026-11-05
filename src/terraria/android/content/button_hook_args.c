#include "button_hook_args.h"

#include <stddef.h>

void *button_hook_group_cursor_argument(void **arguments) {
    if (!arguments || !arguments[0]) return NULL;
    return *(void **)arguments[0];
}
