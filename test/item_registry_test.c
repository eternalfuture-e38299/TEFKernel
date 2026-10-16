#include <assert.h>
#include <stdio.h>
#include "internal/terraria/item_manager.h"
#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/struct/string.h"
#include "terraria/texture2d.h"
#include "terraria/asset.h"
#include "terraria/recipe_manager.h"
#include "terraria/language_manager.h"

static int recipe_callback_count, language_callback_count;

// Game-facing calls are not executed by registry tests.
void tefkernel_log_write_ex(tefkernel_log_level_t level, const char *file, int line,
                           const char *func, const char *fmt, ...) {}
void tefkernel_log_write(tefkernel_log_level_t level, const char *fmt, ...) {}
bool patchlib_is_valid(patch_handle_t h) { return h != NULL; }
void patchlib_free(patch_handle_t h) {}
patch_handle_t patchlib_type_get_type(const char *ns, const char *name) { assert(0); return NULL; }
patch_handle_t patchlib_type_get_field(patch_handle_t type, const char *name) { assert(0); return NULL; }
patch_handle_t patchlib_string_create(const char *str) { assert(0); return NULL; }
void patchlib_field_set_value(patch_handle_t f, patch_handle_t i, void *v) { assert(0); }
void patchlib_field_get_value(patch_handle_t f, patch_handle_t i, void *v) { *(short *)v = 6000; }
patch_handle_t terraria_texture2d_create(int w, int h, texture_format_t format, void *data, size_t size) { assert(0); return NULL; }
patch_handle_t terraria_texture2d_get_class(void) { assert(0); return NULL; }
patch_handle_t terraria_asset_create(patch_handle_t type, patch_handle_t value) { assert(0); return NULL; }
void terraria_recipe_set_result(int id, int stack) { assert(0); }
void terraria_recipe_set_material(int *mats, int count) { assert(0); }
void terraria_recipe_add(void) { assert(0); }
void terraria_recipe_manager_register_callback(terraria_recipe_manager_addrecipes_t cb) { ++recipe_callback_count; }
void terraria_language_manager_register_callback(terraria_languager_manager_get_localized_text_t cb) { ++language_callback_count; }

int main(void) {
    terraria_item_manager_unknown_item()->item_ops.early_init(terraria_item_manager_unknown_item());
    assert(recipe_callback_count == 0 && language_callback_count == 1);
    assert(tefstd_vector_init(&g_terraria_item_registry, sizeof(terraria_item_handle_t *)));
    terraria_item_handle_t a = {.parent_modloader_id="lua",.parent_id="test",.internal_name="A"};
    terraria_item_handle_t b = {.parent_modloader_id="lua",.parent_id="test",.internal_name="B"};
    assert(terraria_item_manager_register_item(&a));
    assert(terraria_item_manager_register_item(&b));
    assert(a.runtime_id == -1 && b.runtime_id == -1);
    assert(terraria_item_manager_unregister_item(&a));
    assert(tefstd_vector_size(&g_terraria_item_registry) == 2);
    assert(!terraria_item_manager_unregister_item(&a));
    assert(terraria_item_manager_register_item(&a));
    assert(terraria_item_manager_assign_ids(6000));
    assert(a.runtime_id == 6000 && b.runtime_id == 6001);
    assert(terraria_item_manager_unregister_item_by_id(a.runtime_id));
    assert(b.runtime_id == 6001);
    assert(terraria_item_manager_get_item(6001) == &b);
    assert(terraria_item_manager_get_item(6000) != &a);
    assert(!terraria_item_manager_unregister_item_by_id(-1));
    terraria_item_handle_t replacement = {.parent_modloader_id="lua",.parent_id="test",.internal_name="A"};
    assert(terraria_item_manager_register_item(&replacement));
    assert(replacement.runtime_id == 6000);
    assert(terraria_item_manager_get_item(6000) == &replacement);
    terraria_item_handle_t late = {.parent_modloader_id="lua",.parent_id="test",.internal_name="Late"};
    assert(!terraria_item_manager_register_item(&late));
    for (int i = 0; i < 100; ++i) {
        assert(terraria_item_manager_unregister_item(&replacement));
        assert(terraria_item_manager_register_item(&replacement));
        assert(replacement.runtime_id == 6000 && b.runtime_id == 6001);
    }
    assert(terraria_item_manager_unregister_item(&replacement));
    terraria_item_manager_destroy();
    puts("item registry tests passed");
}
