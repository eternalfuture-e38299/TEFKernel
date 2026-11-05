/* Controlled managed-runtime boundary for the real C support implementations. */
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define H(n) ((void *)(uintptr_t)(n))
typedef struct item_t {
    int type, stack, maximum;
    unsigned char prefix;
    int marker;
} item_t;
typedef struct array_t {
    size_t length, element;
    bool reference;
    void *data;
    struct array_t *next;
} array_t;
typedef struct recipe_t {
    item_t *result;
    array_t *materials, *stations;
    bool water;
} recipe_t;
static item_t results[3] = {{100, 1, 999, 0, 0}, {200, 2, 999, 0, 0}, {100, 4, 999, 0, 0}};
static item_t ingredients[2] = {{5, 2, 999, 0, 0}, {7, 3, 999, 0, 0}};
static item_t *material_data[3][2] = {
    {&ingredients[0], &ingredients[1]}, {&ingredients[0], NULL}, {&ingredients[1], NULL}};
static int station_data[3][2] = {{13, -1}, {17, -1}, {-1, -1}};
static array_t material_arrays[3], station_arrays[3], recipe_array;
static recipe_t recipes[3];
static recipe_t *recipe_data[3];
static item_t cursor = {42, 8, 999, 3, 876};
static array_t *allocations;
static int roots, interactions, updates, recipe_count = 3;
static bool menu;
static postfix_callback_t update_callback;
static void *active_world = (void *)50;
void support_fixture_world(void *value) { active_world = value; }
void support_fixture_init(void) {
    for (int i = 0; i < 3; ++i) {
        material_arrays[i] = (array_t){2, sizeof(void *), true, material_data[i], NULL};
        station_arrays[i] = (array_t){2, sizeof(int), false, station_data[i], NULL};
        recipes[i] = (recipe_t){&results[i], &material_arrays[i], &station_arrays[i], i == 0};
        recipe_data[i] = &recipes[i];
    }
    recipe_array = (array_t){3, sizeof(void *), true, recipe_data, NULL};
}
void support_fixture_cleanup(void) {
    assert(roots == 0);
    while (allocations) {
        array_t *a = allocations;
        allocations = a->next;
        if (a->reference)
            for (size_t i = 0; i < a->length; ++i)
                free(((void **)a->data)[i]);
        free(a->data);
        free(a);
    }
}
int support_fixture_roots(void) { return roots; }
int support_fixture_interactions(void) { return interactions; }
void support_fixture_tick(void) {
    assert(update_callback);
    ++updates;
    update_callback(NULL, NULL, NULL, NULL);
}
void support_fixture_menu(bool value) { menu = value; }
void support_fixture_recipe_result(int id) { results[0].type = id; }
patch_handle_t patchlib_type_get_type(const char *ns, const char *name) {
    (void)ns;
    if (!strcmp(name, "Main"))
        return H(1);
    if (!strcmp(name, "Recipe"))
        return H(2);
    if (!strcmp(name, "Item"))
        return H(3);
    if (!strcmp(name, "ItemSlot"))
        return H(4);
    if (!strcmp(name, "Array"))
        return H(7);
    if (!strcmp(name, "GameTime"))
        return H(5);
    return NULL;
}
patch_handle_t patchlib_type_get_field(patch_handle_t t, const char *name) {
    if (t == H(1)) {
        if (!strcmp(name, "netMode"))
            return H(15);
        if (!strcmp(name, "ActiveWorldFileData"))
            return H(14);
        if (!strcmp(name, "recipe"))
            return H(11);
        if (!strcmp(name, "mouseItem"))
            return H(12);
        if (!strcmp(name, "gameMenu"))
            return H(13);
    }
    if (t == H(2)) {
        if (!strcmp(name, "numRecipes"))
            return H(21);
        if (!strcmp(name, "createItem"))
            return H(22);
        if (!strcmp(name, "requiredItem"))
            return H(23);
        if (!strcmp(name, "requiredTile"))
            return H(24);
        if (!strcmp(name, "needWater"))
            return H(25);
    }
    if (t == H(3)) {
        if (!strcmp(name, "type"))
            return H(31);
        if (!strcmp(name, "stack"))
            return H(32);
        if (!strcmp(name, "maxStack"))
            return H(33);
        if (!strcmp(name, "prefix"))
            return H(34);
    }
    return NULL;
}
patch_type_t patchlib_field_get_type(patch_handle_t f) {
    switch ((uintptr_t)f) {
    case 11:
    case 14:
    case 12:
    case 22:
    case 23:
    case 24:
        return PATCH_OBJECT;
    case 13:
    case 25:
        return PATCH_BOOL;
    case 34:
        return PATCH_UINT8;
    default:
        return PATCH_INT32;
    }
}
void patchlib_field_get_value(patch_handle_t f, patch_handle_t instance, void *out) {
    recipe_t *r = instance;
    item_t *i = instance;
    switch ((uintptr_t)f) {
    case 15:
        *(int *)out = 0;
        break;
    case 14:
        *(void **)out = active_world;
        break;
    case 11:
        *(void **)out = &recipe_array;
        break;
    case 12:
        *(void **)out = &cursor;
        break;
    case 13:
        *(bool *)out = menu;
        break;
    case 21:
        *(int *)out = recipe_count;
        break;
    case 22:
        *(void **)out = r->result;
        break;
    case 23:
        *(void **)out = r->materials;
        break;
    case 24:
        *(void **)out = r->stations;
        break;
    case 25:
        *(bool *)out = r->water;
        break;
    case 31:
        *(int *)out = i->type;
        break;
    case 32:
        *(int *)out = i->stack;
        break;
    case 33:
        *(int *)out = i->maximum;
        break;
    case 34:
        *(unsigned char *)out = i->prefix;
        break;
    default:
        assert(0);
    }
}
patch_handle_t patchlib_type_get_method_by_param_count(patch_handle_t t, const char *name, int n) {
    if (t == H(3) && !strcmp(name, ".ctor") && n == 2)
        return H(41);
    if (t == H(4) && !strcmp(name, "Handle") && n == 3)
        return H(42);
    return NULL;
}
patch_handle_t patchlib_type_get_method_by_param_types(patch_handle_t t, const char *name, int n,
                                                       patch_handle_t const *types) {
    if (t == H(1) && !strcmp(name, "Update") && n == 1)
        return H(43);
    if (t == H(7) && !strcmp(name, "SetValue") && n == 2 && types[0] == H(8) && types[1] == H(9))
        return H(44);
    if (t == H(4) && !strcmp(name, "Handle") && n == 3 && types[0] == H(6) && types[1] == H(9) &&
        types[2] == H(9))
        return H(42);
    return NULL;
}
patch_handle_t patchlib_get_basic_type(patch_type_t t) {
    return t == PATCH_OBJECT ? H(8) : t == PATCH_INT32 ? H(9) : NULL;
}
void *il2cpp_object_get_class(void *object) {
    assert(object);
    return H(6);
}
bool patchlib_method_is_static(patch_handle_t m) { return m == H(42); }
patch_type_t patchlib_method_get_return_type(patch_handle_t m) {
    (void)m;
    return PATCH_VOID;
}
patch_hook_id_t patchlib_install_prepost_hook(patch_handle_t m, prefix_callback_t pre,
                                              postfix_callback_t post) {
    (void)pre;
    assert(m == H(43));
    update_callback = post;
    return 0;
}
bool patchlib_method_get_signature(patch_handle_t m, patch_method_signature_t *s) {
    static patch_type_t handle_types[] = {PATCH_OBJECT, PATCH_INT32, PATCH_INT32};
    static patch_type_t ctor_types[] = {PATCH_INT32, PATCH_OBJECT};
    assert(m == H(41) || m == H(42));
    memset(s, 0, sizeof(*s));
    s->is_instance = m == H(41);
    s->arg_types = (tefstd_vector_t){m == H(41) ? ctor_types : handle_types, m == H(41) ? 2 : 3, 3,
                                     sizeof(patch_type_t)};
    s->return_type = PATCH_VOID;
    return true;
}
bool patchlib_method_signature_free(patch_method_signature_t *s) {
    (void)s;
    return true;
}
size_t tefstd_vector_size(const tefstd_vector_t *v) { return v->size; }
void *tefstd_vector_at(const tefstd_vector_t *v, size_t i) {
    return i < v->size ? (char *)v->data + i * v->elem_size : NULL;
}
patch_handle_t patchlib_array_create(size_t n, patch_handle_t t) {
    assert(t == H(3));
    array_t *a = calloc(1, sizeof(*a));
    assert(a);
    a->length = n;
    a->element = sizeof(void *);
    a->reference = true;
    a->data = calloc(n, sizeof(void *));
    assert(a->data);
    a->next = allocations;
    allocations = a;
    return a;
}
void *patchlib_object_root(patch_handle_t object) {
    assert(object);
    ++roots;
    return object;
}
void patchlib_object_unroot(void *root) {
    assert(root);
    --roots;
    assert(roots >= 0);
}
bool patchlib_constructor_invoke(patch_handle_t ctor, patch_handle_t *out, void **args) {
    assert(ctor == H(41) && *(int *)args[0] == 0);
    item_t *i = calloc(1, sizeof(*i));
    assert(i);
    i->maximum = 999;
    *out = i;
    return true;
}
size_t patchlib_array_length(patch_handle_t p) { return ((array_t *)p)->length; }
size_t patchlib_array_element_size(patch_handle_t p) { return ((array_t *)p)->element; }
bool patchlib_array_element_is_reference(patch_handle_t p) { return ((array_t *)p)->reference; }
bool patchlib_array_at(patch_handle_t p, size_t index, void *out) {
    array_t *a = p;
    if (index >= a->length)
        return false;
    memcpy(out, (char *)a->data + index * a->element, a->element);
    return true;
}
bool patchlib_array_set(patch_handle_t p, size_t index, void *value) {
    array_t *a = p;
    if (index >= a->length)
        return false;
    memcpy((char *)a->data + index * a->element, value, a->element);
    return true;
}
bool patchlib_method_invoke_args(patch_handle_t m, patch_handle_t instance, void *result,
                                 void **args) {
    (void)instance;
    (void)result;
    if (m == H(44)) {
        array_t *a = instance;
        int i = *(int *)args[1];
        assert(a && (size_t)i < a->length);
        ((void **)a->data)[i] = *(void **)args[0];
        return true;
    }
    assert(m == H(42));
    array_t *a = *(void **)args[0];
    int context = *(int *)args[1], slot = *(int *)args[2];
    assert(context >= 0 && (size_t)slot < a->length);
    item_t *i = ((item_t **)a->data)[slot];
    item_t temporary = *i;
    *i = cursor;
    cursor = temporary;
    ++interactions;
    return true;
}
