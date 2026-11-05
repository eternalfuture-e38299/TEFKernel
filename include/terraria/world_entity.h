#ifndef TEFKERNEL_WORLD_ENTITY_H
#define TEFKERNEL_WORLD_ENTITY_H
#include "../tef_api.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#if defined(__ANDROID__)
/* Owned native data, not a Terraria TileEntity or a new Tile ID. Binary payloads
 * belong to the caller. All keys persist by stable loader/mod/type/coordinates.
 * Load is transactional; unloading a mod does not discard its saved records.
 * No Lua callbacks run on the game's save/load worker threads. */
typedef struct terraria_entity_scope_t {
    const char *loader;
    const char *mod;
} terraria_entity_scope_t;
typedef struct terraria_entity_key_t {
    terraria_entity_scope_t scope;
    const char *kind;
    int x, y;
} terraria_entity_key_t;
typedef void (*terraria_entity_update_t)(void *user);
/* Optional Update(GameTime) adapter; false means the verified signature/hook
 * is unavailable. Subscribe/unsubscribe only on the game thread. */
DEFINE_FUNCTION(bool, terraria_world_entity_subscribe, const terraria_entity_scope_t *scope,
                terraria_entity_update_t callback, void *user)
DEFINE_FUNCTION(void, terraria_world_entity_unsubscribe, void *user)
#define TERRARIA_ENTITY_DATA_MAX 65536u
DEFINE_FUNCTION(bool, terraria_world_entity_put, const terraria_entity_key_t *key, const void *data,
                size_t size)
/* out_size always receives required size. NULL data/zero capacity probes size. */
DEFINE_FUNCTION(bool, terraria_world_entity_get, const terraria_entity_key_t *key, void *data,
                size_t capacity, size_t *out_size)
DEFINE_FUNCTION(bool, terraria_world_entity_remove, const terraria_entity_key_t *key)
DEFINE_FUNCTION(size_t, terraria_world_entity_count, const terraria_entity_scope_t *scope)
/* Index is zero based, enumerating only the supplied scope. */
DEFINE_FUNCTION(bool, terraria_world_entity_at, const terraria_entity_scope_t *scope, size_t index,
                char *kind, size_t capacity, int *x, int *y)
/* Native lifecycle entry points; load rejects a bad file and keeps the old state.
 * A missing file starts an empty store. NULL path unloads the current world. */
DEFINE_FUNCTION(bool, terraria_world_entity_load, const char *world_path)
DEFINE_FUNCTION(bool, terraria_world_entity_save, const char *world_path)
DEFINE_FUNCTION(bool, terraria_world_entity_active, void)
/* Changes at every world load/unload attempt; use to reject multi-call snapshots. */
DEFINE_FUNCTION(uint64_t, terraria_world_entity_generation, void)
#endif
#ifdef __cplusplus
}
#endif
#endif
