#include "terraria/item_slot.h"
#include "patchlib/field.h"
#include "patchlib/il2cpp_api.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"
#include <stdlib.h>
#include <string.h>
#define SLOT_LIMIT 256u
#define CONTAINER_LIMIT 128u
struct terraria_item_container_t {
    patch_handle_t array;
    void *root;
    size_t count;
    struct terraria_item_container_t *next;
};
static terraria_item_container_t *containers;
static size_t container_count;
static patch_handle_t item_type, ctor, f_type, f_stack, f_max, f_prefix, f_cursor, handle_slot;
static patch_handle_t set_value;
static patch_handle_t net_mode;
static bool ready;
static patch_handle_t field(patch_handle_t type, const char *name, patch_type_t expected) {
    patch_handle_t f = patchlib_type_get_field(type, name);
    return f && patchlib_field_get_type(f) == expected ? f : PATCH_NULL;
}
static bool resolve(void) {
    if (ready)
        return true;
    item_type = patchlib_type_get_type("Terraria", "Item");
    patch_handle_t main = patchlib_type_get_type("Terraria", "Main");
    if (!item_type || !main)
        return false;
    f_type = field(item_type, "type", PATCH_INT32);
    f_stack = field(item_type, "stack", PATCH_INT32);
    f_max = field(item_type, "maxStack", PATCH_INT32);
    f_prefix = field(item_type, "prefix", PATCH_UINT8);
    f_cursor = field(main, "mouseItem", PATCH_OBJECT);
    net_mode = field(main, "netMode", PATCH_INT32);
    ctor = patchlib_type_get_method_by_param_count(item_type, ".ctor", 2);
    patch_handle_t array_type = patchlib_type_get_type("System", "Array");
    patch_handle_t setter_types[] = {patchlib_get_basic_type(PATCH_OBJECT),
                                     patchlib_get_basic_type(PATCH_INT32)};
    if (array_type && setter_types[0] && setter_types[1])
        set_value =
            patchlib_type_get_method_by_param_types(array_type, "SetValue", 2, setter_types);
    patch_method_signature_t signature = {0};
    bool constructor_ok = ctor && patchlib_method_get_signature(ctor, &signature);
    if (constructor_ok) {
        constructor_ok = signature.is_instance && tefstd_vector_size(&signature.arg_types) == 2;
        if (constructor_ok) {
            patch_type_t *first = tefstd_vector_at(&signature.arg_types, 0);
            patch_type_t *second = tefstd_vector_at(&signature.arg_types, 1);
            constructor_ok = first && second && *first == PATCH_INT32 && *second == PATCH_OBJECT;
        }
        patchlib_method_signature_free(&signature);
    }
    ready = f_type && f_stack && f_max && f_prefix && constructor_ok && set_value;
    return ready;
}
/* System.Array.SetValue performs the runtime's managed reference write barrier.
 * patchlib_array_set currently copies bytes and must not be used for Item[]. */
static bool set_item(patch_handle_t array, size_t slot, patch_handle_t item) {
    if (slot > 255 || !set_value)
        return false;
    int index = (int)slot;
    void *args[] = {&item, &index};
    return patchlib_method_invoke_args(set_value, array, NULL, args);
}
static bool known(const terraria_item_container_t *c) {
    for (const terraria_item_container_t *p = containers; p; p = p->next)
        if (p == c)
            return true;
    return false;
}
static bool read_item(patch_handle_t item, terraria_item_view_t *v) {
    if (!item || !v || !resolve())
        return false;
    memset(v, 0, sizeof(*v));
    patchlib_field_get_value(f_type, item, &v->type);
    patchlib_field_get_value(f_stack, item, &v->stack);
    patchlib_field_get_value(f_max, item, &v->max_stack);
    patchlib_field_get_value(f_prefix, item, &v->prefix);
    return true;
}
terraria_item_container_t *terraria_item_container_create(size_t slots) {
    if (!slots || slots > SLOT_LIMIT || container_count >= CONTAINER_LIMIT || !resolve())
        return NULL;
    terraria_item_container_t *c = calloc(1, sizeof(*c));
    if (!c)
        return NULL;
    c->array = patchlib_array_create(slots, item_type);
    if (!c->array) {
        free(c);
        return NULL;
    }
    c->root = patchlib_object_root(c->array);
    if (!c->root) {
        free(c);
        return NULL;
    }
    c->count = slots;
    for (size_t i = 0; i < slots; ++i) {
        int id = 0;
        patch_handle_t variant = PATCH_NULL, item = PATCH_NULL;
        void *args[] = {&id, &variant};
        if (!patchlib_constructor_invoke(ctor, &item, args) || !item ||
            !set_item(c->array, i, item)) {
            patchlib_object_unroot(c->root);
            free(c);
            return NULL;
        }
    }
    c->next = containers;
    containers = c;
    ++container_count;
    return c;
}
void terraria_item_container_destroy(terraria_item_container_t *c) {
    terraria_item_container_t **p = &containers;
    while (*p && *p != c)
        p = &(*p)->next;
    if (!*p)
        return;
    *p = c->next;
    patchlib_object_unroot(c->root);
    free(c);
    --container_count;
}
size_t terraria_item_container_size(const terraria_item_container_t *c) {
    return known(c) ? c->count : 0;
}
patch_handle_t terraria_item_container_array(terraria_item_container_t *c) {
    return known(c) ? c->array : PATCH_NULL;
}
bool terraria_item_container_read(terraria_item_container_t *c, size_t slot,
                                  terraria_item_view_t *out) {
    if (!known(c) || slot >= c->count || !out)
        return false;
    patch_handle_t item = PATCH_NULL;
    return patchlib_array_at(c->array, slot, &item) && read_item(item, out);
}
bool terraria_item_container_swap(terraria_item_container_t *a, size_t ai,
                                  terraria_item_container_t *b, size_t bi) {
    if (!known(a) || !known(b) || ai >= a->count || bi >= b->count)
        return false;
    if (a == b && ai == bi)
        return true;
    patch_handle_t av = PATCH_NULL, bv = PATCH_NULL;
    if (!patchlib_array_at(a->array, ai, &av) || !patchlib_array_at(b->array, bi, &bv))
        return false;
    /* Both objects stay rooted by the two arrays, including between assignments. */
    void *pin = av ? patchlib_object_root(av) : NULL;
    if (av && !pin)
        return false;
    bool ok = set_item(a->array, ai, bv);
    if (ok && !set_item(b->array, bi, av)) {
        set_item(a->array, ai, av);
        ok = false;
    }
    if (pin)
        patchlib_object_unroot(pin);
    return ok;
}
bool terraria_item_container_interact(terraria_item_container_t *c, size_t slot, int context) {
    if (!known(c) || slot >= c->count || context != 0)
        return false;
    int mode = -1;
    if (net_mode)
        patchlib_field_get_value(net_mode, PATCH_NULL, &mode);
    /* This transient native container has no server-authoritative transport. */
    if (mode != 0)
        return false;
    if (!handle_slot) {
        patch_handle_t type = patchlib_type_get_type("Terraria.UI", "ItemSlot");
        patch_handle_t integer = patchlib_get_basic_type(PATCH_INT32);
        patch_handle_t actual_array = il2cpp_object_get_class(c->array);
        if (!actual_array || !integer)
            return false;
        patch_handle_t parameter_types[] = {actual_array, integer, integer};
        patch_handle_t m =
            patchlib_type_get_method_by_param_types(type, "Handle", 3, parameter_types);
        patch_method_signature_t sig = {0};
        if (!m || !patchlib_method_get_signature(m, &sig))
            return false;
        bool ok = !sig.is_instance && sig.return_type == PATCH_VOID &&
                  tefstd_vector_size(&sig.arg_types) == 3;
        patch_type_t expected[] = {PATCH_OBJECT, PATCH_INT32, PATCH_INT32};
        for (size_t i = 0; ok && i < 3; ++i) {
            patch_type_t *t = tefstd_vector_at(&sig.arg_types, i);
            ok = t && *t == expected[i];
        }
        patchlib_method_signature_free(&sig);
        if (!ok)
            return false;
        handle_slot = m;
    }
    int i = (int)slot;
    void *args[] = {&c->array, &context, &i};
    return patchlib_method_invoke_args(handle_slot, PATCH_NULL, NULL, args);
}
bool terraria_item_slot_cursor(terraria_item_view_t *out) {
    if (!out || !resolve() || !f_cursor)
        return false;
    patch_handle_t item = PATCH_NULL;
    patchlib_field_get_value(f_cursor, PATCH_NULL, &item);
    return read_item(item, out);
}
