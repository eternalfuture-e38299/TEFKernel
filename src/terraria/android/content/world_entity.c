#include "terraria/world_entity.h"
#include "world_entity_runtime.h"
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* A bounded, independently versioned sidecar. Never overwrite the game's wld.
 * The lock covers only native buffers and file IO, never managed/Lua calls. */
#define ENTITY_MAX 4096u
#define ENTITY_BYTES_MAX (16u * 1024u * 1024u)
typedef struct entity_t {
    char loader[65], mod[65], kind[65];
    int x, y;
    size_t size;
    unsigned char *data;
} entity_t;
static entity_t *entries;
static size_t count, bytes;
static char *current_path;
static bool writable;
static atomic_ullong epoch;
uint64_t terraria_world_entity_generation(void) { return atomic_load(&epoch); }
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static bool identifier(const char *s) {
    if (!s || !*s || strlen(s) > 64)
        return false;
    for (; *s; ++s)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') ||
              *s == '_' || *s == '.' || *s == '-'))
            return false;
    return true;
}
static bool valid_key(const terraria_entity_key_t *k) {
    return k && identifier(k->scope.loader) && identifier(k->scope.mod) && identifier(k->kind) &&
           k->x >= 0 && k->y >= 0 && k->x < INT_MAX && k->y < INT_MAX;
}
static bool scope_matches(const entity_t *e, const terraria_entity_scope_t *s) {
    return s && identifier(s->loader) && identifier(s->mod) && !strcmp(e->loader, s->loader) &&
           !strcmp(e->mod, s->mod);
}
static size_t find(const terraria_entity_key_t *k) {
    for (size_t i = 0; i < count; ++i)
        if (scope_matches(&entries[i], &k->scope) && entries[i].x == k->x && entries[i].y == k->y &&
            !strcmp(entries[i].kind, k->kind))
            return i;
    return count;
}
static void dispose(entity_t *list, size_t length) {
    for (size_t i = 0; i < length; ++i)
        free(list[i].data);
    free(list);
}
bool terraria_world_entity_active(void) {
    unsigned long long expected = atomic_load(&epoch);
    pthread_mutex_lock(&mutex);
    bool ok = current_path && writable && expected == atomic_load(&epoch) &&
              tef_world_entity_runtime_accessible();
    pthread_mutex_unlock(&mutex);
    return ok;
}
bool terraria_world_entity_put(const terraria_entity_key_t *k, const void *data, size_t size) {
    if (!valid_key(k) || size > TERRARIA_ENTITY_DATA_MAX || (!data && size))
        return false;
    unsigned long long expected = atomic_load(&epoch);
    pthread_mutex_lock(&mutex);
    if (!current_path || !writable || expected != atomic_load(&epoch) ||
        !tef_world_entity_runtime_accessible()) {
        pthread_mutex_unlock(&mutex);
        return false;
    }
    size_t i = find(k), old = i < count ? entries[i].size : 0;
    if ((i == count && count >= ENTITY_MAX) || size > ENTITY_BYTES_MAX - (bytes - old)) {
        pthread_mutex_unlock(&mutex);
        return false;
    }
    unsigned char *copy = size ? malloc(size) : NULL;
    if (size && !copy) {
        pthread_mutex_unlock(&mutex);
        return false;
    }
    if (size)
        memcpy(copy, data, size);
    if (i == count) {
        entity_t *next = realloc(entries, (count + 1) * sizeof(*next));
        if (!next) {
            free(copy);
            pthread_mutex_unlock(&mutex);
            return false;
        }
        entries = next;
        memset(&entries[count], 0, sizeof(entries[count]));
        strcpy(entries[i].loader, k->scope.loader);
        strcpy(entries[i].mod, k->scope.mod);
        strcpy(entries[i].kind, k->kind);
        entries[i].x = k->x;
        entries[i].y = k->y;
        ++count;
    }
    free(entries[i].data);
    entries[i].data = copy;
    entries[i].size = size;
    bytes = bytes - old + size;
    pthread_mutex_unlock(&mutex);
    return true;
}
bool terraria_world_entity_get(const terraria_entity_key_t *k, void *data, size_t capacity,
                               size_t *size) {
    if (size)
        *size = 0;
    if (!valid_key(k) || !size || (!data && capacity))
        return false;
    unsigned long long expected = atomic_load(&epoch);
    pthread_mutex_lock(&mutex);
    size_t i = find(k);
    bool ok = current_path && writable && i < count && expected == atomic_load(&epoch) &&
              tef_world_entity_runtime_accessible();
    if (ok) {
        *size = entries[i].size;
        if (data) {
            ok = capacity >= *size;
            if (ok && *size)
                memcpy(data, entries[i].data, *size);
        }
    }
    pthread_mutex_unlock(&mutex);
    return ok;
}
bool terraria_world_entity_remove(const terraria_entity_key_t *k) {
    if (!valid_key(k))
        return false;
    unsigned long long expected = atomic_load(&epoch);
    pthread_mutex_lock(&mutex);
    size_t i = find(k);
    bool ok = current_path && writable && i < count && expected == atomic_load(&epoch) &&
              tef_world_entity_runtime_accessible();
    if (ok) {
        bytes -= entries[i].size;
        free(entries[i].data);
        entries[i] = entries[--count];
    }
    pthread_mutex_unlock(&mutex);
    return ok;
}
size_t terraria_world_entity_count(const terraria_entity_scope_t *s) {
    unsigned long long expected = atomic_load(&epoch);
    pthread_mutex_lock(&mutex);
    size_t n = 0;
    if (current_path && writable && expected == atomic_load(&epoch) &&
        tef_world_entity_runtime_accessible())
        for (size_t i = 0; i < count; ++i)
            if (scope_matches(&entries[i], s))
                ++n;
    pthread_mutex_unlock(&mutex);
    return n;
}
bool terraria_world_entity_at(const terraria_entity_scope_t *s, size_t index, char *kind,
                              size_t capacity, int *x, int *y) {
    if (!s || !kind || !x || !y || capacity == 0)
        return false;
    unsigned long long expected = atomic_load(&epoch);
    pthread_mutex_lock(&mutex);
    bool ok = false;
    if (current_path && writable && expected == atomic_load(&epoch) &&
        tef_world_entity_runtime_accessible())
        for (size_t i = 0; i < count; ++i)
            if (scope_matches(&entries[i], s)) {
                if (index-- == 0) {
                    size_t len = strlen(entries[i].kind);
                    if (capacity > len) {
                        memcpy(kind, entries[i].kind, len + 1);
                        *x = entries[i].x;
                        *y = entries[i].y;
                        ok = true;
                    }
                    break;
                }
            }
    pthread_mutex_unlock(&mutex);
    return ok;
}
static char *sidecar(const char *path, const char *suffix) {
    if (!path || !*path || strlen(path) > 4096)
        return NULL;
    size_t n = strlen(path) + strlen(suffix) + 1;
    char *out = malloc(n);
    if (out)
        snprintf(out, n, "%s%s", path, suffix);
    return out;
}
static bool write_u32(FILE *f, uint32_t n) {
    unsigned char b[4] = {(unsigned char)n, (unsigned char)(n >> 8), (unsigned char)(n >> 16),
                          (unsigned char)(n >> 24)};
    return fwrite(b, 1, 4, f) == 4;
}
static bool read_u32(FILE *f, uint32_t *n) {
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4)
        return false;
    *n = (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
    return true;
}
static bool write_id(FILE *f, const char *s) {
    size_t n = strlen(s);
    return fputc((int)n, f) != EOF && fwrite(s, 1, n, f) == n;
}
static bool read_id(FILE *f, char s[65]) {
    int n = fgetc(f);
    if (n < 1 || n > 64 || fread(s, 1, (size_t)n, f) != (size_t)n)
        return false;
    s[n] = 0;
    return strlen(s) == (size_t)n && identifier(s);
}
/* FNV checksum catches accidental payload damage; it is not an authentication code. */
static uint32_t hash_bytes(uint32_t h, const void *p, size_t n) {
    const unsigned char *b = p;
    for (size_t i = 0; i < n; ++i)
        h = (h ^ b[i]) * 16777619u;
    return h;
}
static uint32_t entry_hash(const entity_t *e) {
    uint32_t h = 2166136261u;
    h = hash_bytes(h, e->loader, strlen(e->loader) + 1);
    h = hash_bytes(h, e->mod, strlen(e->mod) + 1);
    h = hash_bytes(h, e->kind, strlen(e->kind) + 1);
    uint32_t ns[3] = {(uint32_t)e->x, (uint32_t)e->y, (uint32_t)e->size};
    for (size_t i = 0; i < 3; ++i) {
        unsigned char b[4] = {(unsigned char)ns[i], (unsigned char)(ns[i] >> 8),
                              (unsigned char)(ns[i] >> 16), (unsigned char)(ns[i] >> 24)};
        h = hash_bytes(h, b, 4);
    }
    return hash_bytes(h, e->data, e->size);
}
bool terraria_world_entity_save(const char *world_path) {
    char *path = sidecar(world_path, ".tefentities"),
         *temporary = sidecar(world_path, ".tefentities.tmp");
    if (!path || !temporary) {
        free(path);
        free(temporary);
        return false;
    }
    pthread_mutex_lock(&mutex);
    bool ok = current_path && writable && !strcmp(current_path, world_path);
    FILE *f = ok ? fopen(temporary, "wb") : NULL;
    ok = ok && f;
    if (ok)
        ok = fwrite("TEFE\1\0\0\0", 1, 8, f) == 8 && write_u32(f, (uint32_t)count);
    for (size_t i = 0; ok && i < count; ++i) {
        entity_t *e = &entries[i];
        ok = write_id(f, e->loader) && write_id(f, e->mod) && write_id(f, e->kind) &&
             write_u32(f, (uint32_t)e->x) && write_u32(f, (uint32_t)e->y) &&
             write_u32(f, (uint32_t)e->size) && write_u32(f, entry_hash(e)) &&
             (!e->size || fwrite(e->data, 1, e->size, f) == e->size);
    }
    if (f) {
        if (fflush(f) || fsync(fileno(f)))
            ok = false;
        if (fclose(f))
            ok = false;
    }
    if (ok)
        ok = rename(temporary, path) == 0;
    if (!ok && f)
        remove(temporary);
    pthread_mutex_unlock(&mutex);
    free(path);
    free(temporary);
    return ok;
}
bool terraria_world_entity_load(const char *world_path) {
    if (!world_path) {
        tef_world_entity_runtime_bind(false);
        pthread_mutex_lock(&mutex);
        atomic_fetch_add(&epoch, 1);
        dispose(entries, count);
        entries = NULL;
        count = bytes = 0;
        free(current_path);
        current_path = NULL;
        writable = false;
        pthread_mutex_unlock(&mutex);
        return true;
    }
    char *path = sidecar(world_path, ".tefentities"), *identity = sidecar(world_path, "");
    if (!path || !identity) {
        free(path);
        free(identity);
        return false;
    }
    pthread_mutex_lock(&mutex);
    atomic_fetch_add(&epoch, 1);
    FILE *f = fopen(path, "rb");
    bool missing = !f && errno == ENOENT, ok = missing;
    entity_t *list = NULL;
    size_t length = 0, total = 0;
    uint32_t n = 0;
    if (f) {
        unsigned char magic[8];
        ok = fread(magic, 1, 8, f) == 8 && !memcmp(magic, "TEFE\1\0\0\0", 8) && read_u32(f, &n) &&
             n <= ENTITY_MAX;
        if (ok && n) {
            list = calloc(n, sizeof(*list));
            ok = list != NULL;
        }
        for (uint32_t i = 0; ok && i < n; ++i) {
            entity_t *e = &list[i];
            uint32_t x = 0, y = 0, size = 0, checksum = 0;
            ok = read_id(f, e->loader) && read_id(f, e->mod) && read_id(f, e->kind) &&
                 read_u32(f, &x) && read_u32(f, &y) && read_u32(f, &size) &&
                 read_u32(f, &checksum) && x < INT_MAX && y < INT_MAX &&
                 size <= TERRARIA_ENTITY_DATA_MAX && size <= ENTITY_BYTES_MAX - total;
            if (!ok)
                break;
            e->x = (int)x;
            e->y = (int)y;
            e->size = size;
            length = i + 1;
            if (size) {
                e->data = malloc(size);
                ok = e->data && fread(e->data, 1, size, f) == size;
            }
            if (ok)
                ok = entry_hash(e) == checksum;
            for (uint32_t j = 0; ok && j < i; ++j)
                if (e->x == list[j].x && e->y == list[j].y && !strcmp(e->loader, list[j].loader) &&
                    !strcmp(e->mod, list[j].mod) && !strcmp(e->kind, list[j].kind))
                    ok = false;
            total += size;
        }
        if (ok)
            ok = fgetc(f) == EOF && !ferror(f);
        fclose(f);
    }
    if (ok) {
        dispose(entries, count);
        entries = list;
        count = n;
        bytes = total;
        free(current_path);
        current_path = identity;
        identity = NULL;
        writable = true;
        tef_world_entity_runtime_bind(true);
    } else {
        dispose(list,
                length); /* Preserve previous data but do not let a corrupt world overwrite it. */
        writable = false;
        tef_world_entity_runtime_bind(false);
    }
    pthread_mutex_unlock(&mutex);
    free(identity);
    free(path);
    return ok;
}
