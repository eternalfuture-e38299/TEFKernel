#ifndef TEFKERNEL_ITEM_SLOT_H
#define TEFKERNEL_ITEM_SLOT_H
#include "../patchlib/type.h"
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#if defined(__ANDROID__)
typedef struct terraria_item_container_t terraria_item_container_t;
typedef struct terraria_item_view_t {
    int type, stack, max_stack;
    unsigned char prefix;
} terraria_item_view_t;
/* Containers own rooted Terraria Item[] instances. This is not ChestItem[].
 * All operations must run on the game thread. Destroy before unloading caller.
 * Native cursor interaction is single-player inventory context 0 only.
 * Containers are transient: persistence and network policy belong to the mod. */
DEFINE_FUNCTION(terraria_item_container_t *, terraria_item_container_create, size_t slots)
DEFINE_FUNCTION(void, terraria_item_container_destroy, terraria_item_container_t *container)
DEFINE_FUNCTION(size_t, terraria_item_container_size, const terraria_item_container_t *container)
DEFINE_FUNCTION(bool, terraria_item_container_read, terraria_item_container_t *container,
                size_t slot, terraria_item_view_t *out)
/* Swap whole Item references, preserving prefixes and arbitrary instance data.
 * Partial stacks and cursor drag behaviour use the original ItemSlot.Handle. */
DEFINE_FUNCTION(bool, terraria_item_container_swap, terraria_item_container_t *a, size_t a_slot,
                terraria_item_container_t *b, size_t b_slot)
DEFINE_FUNCTION(bool, terraria_item_container_interact, terraria_item_container_t *container,
                size_t slot, int context)
/* Read-only query: never consumes Main.mouseItem or modifies its stack. */
DEFINE_FUNCTION(bool, terraria_item_slot_cursor, terraria_item_view_t *out)
/* Borrowed handle valid only while container exists; caller must not retain it. */
DEFINE_FUNCTION(patch_handle_t, terraria_item_container_array, terraria_item_container_t *container)
#endif
#ifdef __cplusplus
}
#endif
#endif
