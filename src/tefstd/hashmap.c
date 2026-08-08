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
#include <stdio.h>

// 默认字符串哈希
uint64_t tefstd_hash_str(const char* str) {
    uint64_t h = 14695981039346656037ULL;
    if (!str) return 0;
    while (*str) h = (h ^ (uint8_t)(*str++)) * 1099511628211ULL;
    return h;
}

// 内存哈希
uint64_t tefstd_hash_mem(const void* data, size_t len) {
    uint64_t h = 14695981039346656037ULL;
    if (!data || len == 0) return 0;
    const uint8_t* p = data;
    while (len--) h = (h ^ *p++) * 1099511628211ULL;
    return h;
}

// 比较两个字符串
static int compare_strings(const char* a, const char* b) {
    if (a == b) return 0;
    if (!a || !b) return 1;
    return strcmp(a, b);
}

// 自动选择哈希函数 - 统一按内存块哈希
static uint64_t auto_hash(const void* key, const size_t key_size) {
    if (!key || key_size == 0) return 0;
    return tefstd_hash_mem(key, key_size);
}

// 自动比较键 - 统一按内存块比较
static int auto_compare(const void* a, const void* b, const size_t key_size) {
    if (a == b) return 0;
    if (!a || !b) return 1;
    return memcmp(a, b, key_size);
}

bool tefstd_hashmap_init(tefstd_hashmap_t* map, const size_t key_size, const size_t value_size) {
    if (!map || key_size == 0 || value_size == 0) {
        return false;
    }

    const size_t initial_capacity = 8;

    map->size = 0;
    map->capacity = initial_capacity;
    map->key_size = key_size;
    map->value_size = value_size;

    map->keys = calloc(initial_capacity, key_size);
    map->values = calloc(initial_capacity, value_size);
    map->states = calloc(initial_capacity, 1);

    if (!map->keys || !map->values || !map->states) {
        free(map->keys);
        free(map->values);
        free(map->states);
        memset(map, 0, sizeof(*map));
        return false;
    }

    return true;
}

void tefstd_hashmap_free(tefstd_hashmap_t* map) {
    if (!map) return;

    free(map->keys);
    free(map->values);
    free(map->states);
    memset(map, 0, sizeof(*map));
}

// 查找桶位置
static size_t find_slot(const tefstd_hashmap_t* map, const void* key, bool* found) {
    if (!map || !key || !found) {
        return SIZE_MAX;
    }

    if (map->capacity == 0) {
        *found = false;
        return 0;
    }

    const uint64_t h = auto_hash(key, map->key_size);
    if (h == 0) {
        *found = false;
        return 0;
    }

    const size_t idx = h & (map->capacity - 1);
    size_t first_tombstone = SIZE_MAX;

    for (size_t i = 0; i < map->capacity; i++) {
        const size_t pos = (idx + i) & (map->capacity - 1);
        const uint8_t state = map->states[pos];

        if (state == 0) { // 空桶
            *found = false;
            return (first_tombstone != SIZE_MAX) ? first_tombstone : pos;
        }

        if (state == 2) { // 墓碑
            if (first_tombstone == SIZE_MAX) {
                first_tombstone = pos;
            }
            continue;
        }

        // 占用桶，比较键
        const void* stored_key = (char*)map->keys + pos * map->key_size;
        if (auto_compare(stored_key, key, map->key_size) == 0) {
            *found = true;
            return pos;
        }
    }

    *found = false;
    return SIZE_MAX;
}

// 扩容
static bool resize(tefstd_hashmap_t* map, const size_t new_cap) {
    if (!map || new_cap == 0) {
        return false;
    }

    // 创建新的哈希表
    tefstd_hashmap_t new_map = {
        .keys = calloc(new_cap, map->key_size),
        .values = calloc(new_cap, map->value_size),
        .states = calloc(new_cap, 1),
        .size = 0,
        .capacity = new_cap,
        .key_size = map->key_size,
        .value_size = map->value_size
    };

    if (!new_map.keys || !new_map.values || !new_map.states) {
        free(new_map.keys);
        free(new_map.values);
        free(new_map.states);
        return false;
    }

    // 重新插入所有元素
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->states[i] != 1) continue; // 跳过非占用桶

        const void* key = (char*)map->keys + i * map->key_size;
        const void* value = (char*)map->values + i * map->value_size;

        // 直接在新表中查找位置并插入
        bool found = false;
        const size_t new_pos = find_slot(&new_map, key, &found);

        if (new_pos != SIZE_MAX && !found) {
            void* new_key_dst = (char*)new_map.keys + new_pos * map->key_size;
            void* new_val_dst = (char*)new_map.values + new_pos * map->value_size;

            memcpy(new_key_dst, key, map->key_size);
            memcpy(new_val_dst, value, map->value_size);
            new_map.states[new_pos] = 1;
            new_map.size++;
        }
    }

    // 释放旧内存
    free(map->keys);
    free(map->values);
    free(map->states);

    // 复制新表到原表
    *map = new_map;
    return true;
}

bool tefstd_hashmap_put(tefstd_hashmap_t* map, const void* key, const void* value) {
    if (!map || !key || !value) {
        return false;
    }

    // 检查负载因子，超过75%时扩容
    if (map->size * 4 >= map->capacity * 3) {
        if (!resize(map, map->capacity * 2)) {
            return false;
        }
    }

    bool found = false;
    const size_t pos = find_slot(map, key, &found);
    if (pos == SIZE_MAX) {
        return false;
    }

    void* key_dst = (char*)map->keys + pos * map->key_size;
    void* val_dst = (char*)map->values + pos * map->value_size;

    if (!found) {
        memcpy(key_dst, key, map->key_size);
        map->states[pos] = 1;
        map->size++;
    }

    memcpy(val_dst, value, map->value_size);
    return true;
}

void* tefstd_hashmap_get(tefstd_hashmap_t* map, const void* key) {
    if (!map || !key) {
        return NULL;
    }

    bool found = false;
    const size_t pos = find_slot(map, key, &found);
    if (!found) {
        return NULL;
    }

    return (char*)map->values + pos * map->value_size;
}

bool tefstd_hashmap_del(tefstd_hashmap_t* map, const void* key) {
    if (!map || !key) {
        return false;
    }

    bool found = false;
    const size_t pos = find_slot(map, key, &found);
    if (!found) {
        return false;
    }

    map->states[pos] = 2; // 标记为墓碑
    map->size--;
    return true;
}

bool tefstd_hashmap_has(const tefstd_hashmap_t* map, const void* key) {
    if (!map || !key) {
        return false;
    }

    bool found = false;
    find_slot(map, key, &found);
    return found;
}

size_t tefstd_hashmap_len(const tefstd_hashmap_t* map) {
    return map ? map->size : 0;
}

void tefstd_hashmap_clear(tefstd_hashmap_t* map) {
    if (!map) return;
    memset(map->states, 0, map->capacity);
    map->size = 0;
}

tefstd_hashmap_iter_t tefstd_hashmap_iter(const tefstd_hashmap_t* map) {
    tefstd_hashmap_iter_t iter = {.map = map, .index = 0};
    if (map) {
        // 找到第一个有效元素
        while (iter.index < map->capacity && map->states[iter.index] != 1) {
            iter.index++;
        }
    }
    return iter;
}

bool tefstd_hashmap_next(tefstd_hashmap_iter_t* iter, void* key_out, void* value_out) {
    if (!iter || !iter->map || iter->index >= iter->map->capacity) {
        return false;
    }

    const tefstd_hashmap_t* map = iter->map;
    const size_t pos = iter->index;

    // 当前元素必须是占用的
    if (map->states[pos] != 1) {
        return false;
    }

    // 复制键值对到输出缓冲区
    if (key_out) {
        memcpy(key_out, (char*)map->keys + pos * map->key_size, map->key_size);
    }
    if (value_out) {
        memcpy(value_out, (char*)map->values + pos * map->value_size, map->value_size);
    }

    // 移动到下一个有效元素
    iter->index++;
    while (iter->index < map->capacity && map->states[iter->index] != 1) {
        iter->index++;
    }

    return true;
}
