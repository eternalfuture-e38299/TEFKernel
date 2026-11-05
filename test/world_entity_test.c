#include "terraria/world_entity.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static terraria_entity_key_t a = {{"loader.a", "mod"}, "machine", 10, 20};
static void *writer(void *ignored) {
    (void)ignored;
    for (int i = 0; i < 200; ++i) {
        assert(terraria_world_entity_put(&a, &i, sizeof(i)));
    }
    return NULL;
}
int main(void) {
    char folder[] = "/tmp/tef-entity-test-XXXXXX";
    assert(mkdtemp(folder));
    char world[512], other[512], file[550];
    snprintf(world, sizeof(world), "%s/world.wld", folder);
    snprintf(other, sizeof(other), "%s/other.wld", folder);
    snprintf(file, sizeof(file), "%s.tefentities", world);
    assert(!terraria_world_entity_active());
    assert(!terraria_world_entity_put(&a, "a", 1));
    assert(terraria_world_entity_load(world));
    assert(terraria_world_entity_active());
    terraria_entity_key_t b = {{"loader.b", "mod"}, "machine", 10, 20};
    const unsigned char data[] = {1, 0, 2, 255};
    assert(terraria_world_entity_put(&a, data, sizeof(data)));
    assert(terraria_world_entity_put(&b, "B", 1));
    assert(terraria_world_entity_count(&a.scope) == 1);
    size_t n = 0;
    assert(terraria_world_entity_get(&a, NULL, 0, &n) && n == sizeof(data));
    unsigned char out[4] = {0};
    assert(!terraria_world_entity_get(&a, out, 1, &n) && n == 4);
    assert(out[0] == 0);
    assert(terraria_world_entity_get(&a, out, sizeof(out), &n) && !memcmp(out, data, 4));
    assert(!terraria_world_entity_put(&a, NULL, 1));
    assert(!terraria_world_entity_put(&a, data, TERRARIA_ENTITY_DATA_MAX + 1));
    terraria_entity_key_t invalid = a;
    invalid.kind = "../escape";
    assert(!terraria_world_entity_put(&invalid, data, 4));
    char kind[65];
    int x = -1, y = -1;
    assert(terraria_world_entity_at(&a.scope, 0, kind, sizeof(kind), &x, &y));
    assert(!strcmp(kind, "machine") && x == 10 && y == 20);
    assert(!terraria_world_entity_at(&a.scope, 0, kind, 2, &x, &y));
    assert(!terraria_world_entity_save(other));
    assert(terraria_world_entity_save(world));
    assert(terraria_world_entity_load(other));
    assert(terraria_world_entity_count(&a.scope) == 0);
    assert(terraria_world_entity_load(world));
    assert(terraria_world_entity_get(&a, out, sizeof(out), &n) && !memcmp(out, data, 4));
    /* Corrupt bytes cannot replace the live store or be overwritten on save. */
    FILE *f = fopen(file, "r+b");
    assert(f);
    assert(!fseek(f, -1, SEEK_END));
    int last = fgetc(f);
    assert(!fseek(f, -1, SEEK_END));
    fputc(last ^ 1, f);
    fclose(f);
    assert(!terraria_world_entity_load(world));
    assert(!terraria_world_entity_active());
    assert(!terraria_world_entity_save(world));
    assert(!terraria_world_entity_get(&a, out, 4, &n));
    f = fopen(file, "r+b");
    assert(f);
    assert(!fseek(f, -1, SEEK_END));
    fputc(last, f);
    fclose(f);
    assert(terraria_world_entity_load(world));
    pthread_t t;
    assert(!pthread_create(&t, NULL, writer, NULL));
    for (int i = 0; i < 30; ++i)
        assert(terraria_world_entity_save(world));
    assert(!pthread_join(t, NULL));
    int actual = -1;
    assert(terraria_world_entity_get(&a, &actual, sizeof(actual), &n) && actual == 199);
    assert(terraria_world_entity_remove(&a));
    assert(!terraria_world_entity_remove(&a));
    assert(terraria_world_entity_count(&b.scope) == 1);
    assert(terraria_world_entity_put(&a, NULL, 0));
    assert(terraria_world_entity_get(&a, NULL, 0, &n) && n == 0);
    assert(terraria_world_entity_save(world));
    assert(terraria_world_entity_load(world));
    assert(terraria_world_entity_get(&a, NULL, 0, &n) && n == 0);
    assert(terraria_world_entity_load(NULL));
    assert(!terraria_world_entity_active());
    unlink(file);
    assert(!rmdir(folder));
    puts("PASS: entity scope, binary data, bounds, atomic save/load, corrupt-data guard, world "
         "switch, concurrent save");
}

bool tef_world_entity_runtime_accessible(void) { return true; }
void tef_world_entity_runtime_bind(bool loaded) { (void)loaded; }
