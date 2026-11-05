#include "world_entity_runtime.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "terraria/world_entity.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#define SUBSCRIBER_MAX 128u
typedef struct subscriber_t {
    char loader[65], mod[65];
    terraria_entity_update_t callback;
    void *user;
    unsigned long long serial;
} subscriber_t;
static subscriber_t subscribers[SUBSCRIBER_MAX];
static unsigned long long next_serial;
static patch_handle_t menu_field, menu_getter;
static bool hooked;
static patch_handle_t active_world_field;
static atomic_uintptr_t loaded_world;
static bool resolve_scene_unlocked(void) {
    if (active_world_field && (menu_field || menu_getter))
        return true;
    patch_handle_t main = patchlib_type_get_type("Terraria", "Main");
    if (!main)
        return false;
    active_world_field = patchlib_type_get_field(main, "ActiveWorldFileData");
    if (active_world_field && patchlib_field_get_type(active_world_field) != PATCH_OBJECT)
        active_world_field = PATCH_NULL;
    menu_field = patchlib_type_get_field(main, "gameMenu");
    if (menu_field && patchlib_field_get_type(menu_field) != PATCH_BOOL)
        menu_field = PATCH_NULL;
    if (!menu_field) {
        menu_getter = patchlib_type_get_method_by_param_count(main, "get_gameMenu", 0);
        if (menu_getter && patchlib_method_get_return_type(menu_getter) != PATCH_BOOL)
            menu_getter = PATCH_NULL;
    }
    return active_world_field && (menu_field || menu_getter);
}
static pthread_mutex_t scene_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool resolve_scene(void) {
    pthread_mutex_lock(&scene_mutex);
    bool ok = resolve_scene_unlocked();
    pthread_mutex_unlock(&scene_mutex);
    return ok;
}
void tef_world_entity_runtime_bind(bool loaded) {
    patch_handle_t world = PATCH_NULL;
    if (loaded && resolve_scene())
        patchlib_field_get_value(active_world_field, PATCH_NULL, &world);
    atomic_store(&loaded_world, (uintptr_t)world);
}
bool tef_world_entity_runtime_accessible(void) {
    if (!resolve_scene())
        return false;
    bool menu = true;
    if (menu_field)
        patchlib_field_get_value(menu_field, PATCH_NULL, &menu);
    else if (menu_getter)
        patchlib_method_invoke_args(menu_getter, PATCH_NULL, &menu, NULL);
    if (menu)
        return false;
    patch_handle_t world = PATCH_NULL;
    patchlib_field_get_value(active_world_field, PATCH_NULL, &world);
    return world && (uintptr_t)world == atomic_load(&loaded_world);
}
static void update_postfix(patch_handle_t instance, void **args, void *result,
                           const patch_method_signature_t *sig) {
    (void)instance;
    (void)args;
    (void)result;
    (void)sig;
    bool menu = true;
    if (menu_field)
        patchlib_field_get_value(menu_field, PATCH_NULL, &menu);
    else if (menu_getter)
        patchlib_method_invoke_args(menu_getter, PATCH_NULL, &menu, NULL);
    if (menu || !terraria_world_entity_active())
        return;
    /* Snapshot serials, not Lua/user pointers: removal/replacement during an
     * earlier callback must prevent calling an unloaded later subscriber. */
    uint64_t generation = terraria_world_entity_generation();
    unsigned long long serials[SUBSCRIBER_MAX];
    for (size_t i = 0; i < SUBSCRIBER_MAX; ++i)
        serials[i] = subscribers[i].serial;
    for (size_t i = 0; i < SUBSCRIBER_MAX && generation == terraria_world_entity_generation(); ++i)
        if (serials[i] && serials[i] == subscribers[i].serial && subscribers[i].callback)
            subscribers[i].callback(subscribers[i].user);
}
static bool install(void) {
    if (hooked)
        return true;
    patch_handle_t main = patchlib_type_get_type("Terraria", "Main");
    patch_handle_t time = patchlib_type_get_type("Microsoft.Xna.Framework", "GameTime");
    if (!main || !time)
        return false;
    patch_handle_t types[] = {time};
    patch_handle_t update = patchlib_type_get_method_by_param_types(main, "Update", 1, types);
    if (!update || patchlib_method_is_static(update) ||
        patchlib_method_get_return_type(update) != PATCH_VOID)
        return false;
    if (!resolve_scene())
        return false;
    hooked = patchlib_install_prepost_hook(update, NULL, update_postfix) != PATCH_HOOK_INVALID_ID;
    return hooked;
}
bool terraria_world_entity_subscribe(const terraria_entity_scope_t *scope,
                                     terraria_entity_update_t callback, void *user) {
    if (!scope || !scope->loader || !scope->mod || !callback || !user ||
        strlen(scope->loader) > 64 || strlen(scope->mod) > 64 || !install())
        return false;
    for (size_t i = 0; i < SUBSCRIBER_MAX; ++i)
        if (subscribers[i].callback && subscribers[i].user == user)
            return false;
    for (size_t i = 0; i < SUBSCRIBER_MAX; ++i)
        if (!subscribers[i].callback) {
            strcpy(subscribers[i].loader, scope->loader);
            strcpy(subscribers[i].mod, scope->mod);
            subscribers[i].callback = callback;
            subscribers[i].user = user;
            subscribers[i].serial = ++next_serial;
            return true;
        }
    return false;
}
void terraria_world_entity_unsubscribe(void *user) {
    for (size_t i = 0; i < SUBSCRIBER_MAX; ++i)
        if (subscribers[i].user == user)
            memset(&subscribers[i], 0, sizeof(subscribers[i]));
}
