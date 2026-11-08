/*******************************************************************************
 * tefkernel - hashmap
 * Copyright (C) 2025 eternalfuture-e38299
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2025/12/12
 *******************************************************************************/

#include "tefstd/hashmap.h"
#include <stdlib.h>
#include <string.h>

#define EMPTY 0
#define SLOT  1
#define TOMB  2

#define OFF(base, i, sz) ((char*)(base) + (i) * (sz))

uint64_t tefstd_hash_str(const char *str) {
    uint64_t h = 14695981039346656037ULL;
    if (!str) return 0;
    while (*str) h = (h ^ (uint8_t) (*str++)) * 1099511628211ULL;
    return h;
}

uint64_t tefstd_hash_mem(const void *data, size_t len) {
    uint64_t h = 14695981039346656037ULL;
    if (!data || !len) return 0;
    const uint8_t *p = data;
    while (len--) h = (h ^ *p++) * 1099511628211ULL;
    return h;
}

#define AUTO_HASH(key, key_size) \
    ((key) && (key_size) ? tefstd_hash_mem((key), (key_size)) : 0U)

#define AUTO_COMPARE(a, b, key_size) \
    ((a) == (b) ? 0 : memcmp((a), (b), (key_size)))

// 查找桶位置。found 指示是否命中；返回值是可用位置（命中或空/墓碑），SIZE_MAX 表示失败
static size_t find_slot(const tefstd_hashmap_t *map, const void *key, bool *found) {
    *found = false;
    if (!map || !key || !map->capacity) return SIZE_MAX;

    const uint64_t h = AUTO_HASH(key, map->key_size);
    if (!h) return SIZE_MAX; // 哈希 0 视为无效键

    const size_t mask = map->capacity - 1;
    const size_t idx = h & mask;
    size_t tomb = SIZE_MAX;

    for (size_t i = 0; i < map->capacity; i++) {
        const size_t pos = (idx + i) & mask;
        const uint8_t st = map->states[pos];

        if (st == EMPTY) return (tomb != SIZE_MAX) ? tomb : pos;
        if (st == TOMB) {
            if (tomb == SIZE_MAX) tomb = pos;
            continue;
        }
        if (!AUTO_COMPARE(OFF(map->keys, pos, map->key_size), key, map->key_size)) {
            *found = true;
            return pos;
        }
    }
    return (tomb != SIZE_MAX) ? tomb : SIZE_MAX;
}

bool tefstd_hashmap_init(tefstd_hashmap_t *map, const size_t key_size, const size_t value_size) {
    if (!map || !key_size || !value_size) return false;

    const size_t cap = 8;
    *map = (tefstd_hashmap_t){
        .keys = calloc(cap, key_size),
        .values = calloc(cap, value_size),
        .states = calloc(cap, 1),
        .size = 0,
        .capacity = cap,
        .key_size = key_size,
        .value_size = value_size,
    };
    if (map->keys && map->values && map->states) return true;

    free(map->keys);
    free(map->values);
    free(map->states);
    memset(map, 0, sizeof(*map));
    return false;
}

void tefstd_hashmap_free(tefstd_hashmap_t *map) {
    if (!map) return;
    free(map->keys);
    free(map->values);
    free(map->states);
    memset(map, 0, sizeof(*map));
}

// 在 map 中就地插入（不检查重复，调用方保证 key 未存在）
static bool insert_raw(tefstd_hashmap_t *map, const void *key, const void *value) {
    bool found;
    const size_t pos = find_slot(map, key, &found);
    if (pos == SIZE_MAX || found) return false;

    memcpy(OFF(map->keys, pos, map->key_size), key, map->key_size);
    memcpy(OFF(map->values, pos, map->value_size), value, map->value_size);
    map->states[pos] = SLOT;
    map->size++;
    return true;
}

static bool resize(tefstd_hashmap_t *map, const size_t new_cap) {
    if (!map || !new_cap) return false;

    tefstd_hashmap_t nm = {
        .keys = calloc(new_cap, map->key_size),
        .values = calloc(new_cap, map->value_size),
        .states = calloc(new_cap, 1),
        .capacity = new_cap,
        .key_size = map->key_size,
        .value_size = map->value_size,
    };
    if (!nm.keys || !nm.values || !nm.states) {
        free(nm.keys);
        free(nm.values);
        free(nm.states);
        return false;
    }

    for (size_t i = 0; i < map->capacity; i++) {
        if (map->states[i] != SLOT) continue;
        insert_raw(&nm, OFF(map->keys, i, map->key_size),
                   OFF(map->values, i, map->value_size));
    }

    free(map->keys);
    free(map->values);
    free(map->states);
    *map = nm;
    return true;
}

bool tefstd_hashmap_put(tefstd_hashmap_t *map, const void *key, const void *value) {
    if (!map || !key || !value) return false;

    if (map->size * 4 >= map->capacity * 3 && !resize(map, map->capacity * 2))
        return false;

    bool found;
    const size_t pos = find_slot(map, key, &found);
    if (pos == SIZE_MAX) return false;

    if (!found) {
        memcpy(OFF(map->keys, pos, map->key_size), key, map->key_size);
        map->states[pos] = SLOT;
        map->size++;
    }
    memcpy(OFF(map->values, pos, map->value_size), value, map->value_size);
    return true;
}

void *tefstd_hashmap_get(tefstd_hashmap_t *map, const void *key) {
    if (!map || !key) return NULL;
    bool found;
    const size_t pos = find_slot(map, key, &found);
    return found ? OFF(map->values, pos, map->value_size) : NULL;
}

bool tefstd_hashmap_has(const tefstd_hashmap_t *map, const void *key) {
    if (!map || !key) return false;
    bool found;
    find_slot(map, key, &found);
    return found;
}

bool tefstd_hashmap_del(tefstd_hashmap_t *map, const void *key) {
    if (!map || !key) return false;
    bool found;
    const size_t pos = find_slot(map, key, &found);
    if (!found) return false;
    map->states[pos] = TOMB;
    map->size--;
    return true;
}

size_t tefstd_hashmap_len(const tefstd_hashmap_t *map) {
    return map ? map->size : 0;
}

void tefstd_hashmap_clear(tefstd_hashmap_t *map) {
    if (!map) return;
    memset(map->states, 0, map->capacity);
    map->size = 0;
}

// 从 index 起找下一个占用槽位
static size_t next_slot(const tefstd_hashmap_t *map, size_t i) {
    while (i < map->capacity && map->states[i] != SLOT) i++;
    return i;
}

tefstd_hashmap_iter_t tefstd_hashmap_iter(const tefstd_hashmap_t *map) {
    tefstd_hashmap_iter_t iter = {.map = map, .index = 0};
    if (map) iter.index = next_slot(map, 0);
    return iter;
}

bool tefstd_hashmap_next(tefstd_hashmap_iter_t *iter, void *key_out, void *value_out) {
    if (!iter || !iter->map || iter->index >= iter->map->capacity) return false;

    const tefstd_hashmap_t *map = iter->map;
    const size_t pos = iter->index;
    if (map->states[pos] != SLOT) return false;

    if (key_out) memcpy(key_out, OFF(map->keys, pos, map->key_size), map->key_size);
    if (value_out) memcpy(value_out, OFF(map->values, pos, map->value_size), map->value_size);

    iter->index = next_slot(map, pos + 1);
    return true;
}
