/*******************************************************************************
 * tefkernel - skipmap
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2025/12/12
 *******************************************************************************/

#include "tefstd/skipmap.h"
#include <stdlib.h>
#include <string.h>

#define MAX_LEVEL 16
#define P 0.25

#define CMP(a, b, sz) memcmp((a), (b), (sz))

// 沿第 i 层前进到最后一个 key < 目标 的节点
#define DESCEND(x, i, key, sz) \
    while ((x)->forward[i] && CMP((x)->forward[i]->key, (key), (sz)) < 0) \
        (x) = (x)->forward[i];

static int random_level(void) {
    int level = 1;
    const int threshold = (long long) RAND_MAX / 4;
    while (level < MAX_LEVEL && rand() < threshold) // NOLINT(cert-msc50-cpp)
        level++;
    return level;
}

// 找到第一个 key >= target 的节点路径，update[i] 存各级前驱
static tefstd_skipnode_t *find_path(const tefstd_skipmap_t *map, const void *key,
                                    tefstd_skipnode_t **update) {
    tefstd_skipnode_t *x = map->header;
    for (int i = map->level - 1; i >= 0; i--) {
        DESCEND(x, i, key, map->key_size);
        if (update) update[i] = x;
    }
    return x->forward[0];
}

bool tefstd_skipmap_init(tefstd_skipmap_t *map, const size_t key_size, const size_t value_size) {
    if (!map || !key_size || !value_size) return false;

    map->header = malloc(sizeof(tefstd_skipnode_t));
    if (!map->header) return false;

    map->header->forward = calloc(MAX_LEVEL, sizeof(tefstd_skipnode_t *));
    if (!map->header->forward) {
        free(map->header);
        return false;
    }

    map->header->key = NULL;
    map->header->value = NULL;
    map->size = 0;
    map->key_size = key_size;
    map->value_size = value_size;
    map->level = 1;
    return true;
}

void tefstd_skipmap_free(tefstd_skipmap_t *map) {
    if (!map || !map->header) return;
    tefstd_skipnode_t *node = map->header;
    while (node) {
        tefstd_skipnode_t *next = node->forward[0];
        free(node->key);
        free(node->value);
        free(node->forward);
        free(node);
        node = next;
    }
    memset(map, 0, sizeof(*map));
}

bool tefstd_skipmap_put(tefstd_skipmap_t *map, const void *key, const void *value) {
    if (!map || !key || !value) return false;

    tefstd_skipnode_t *update[MAX_LEVEL];
    const tefstd_skipnode_t *x = find_path(map, key, update);

    if (x && CMP(x->key, key, map->key_size) == 0) {
        memcpy(x->value, value, map->value_size);
        return true;
    }

    const int level = random_level();
    tefstd_skipnode_t *node = malloc(sizeof(tefstd_skipnode_t));
    if (!node) return false;
    node->forward = calloc(level, sizeof(tefstd_skipnode_t *));
    node->key = malloc(map->key_size);
    node->value = malloc(map->value_size);
    if (!node->key || !node->value || !node->forward) {
        free(node->key);
        free(node->value);
        free(node->forward);
        free(node);
        return false;
    }

    memcpy(node->key, key, map->key_size);
    memcpy(node->value, value, map->value_size);

    if (level > map->level) {
        for (int i = map->level; i < level; i++) update[i] = map->header;
        map->level = level;
    }

    for (int i = 0; i < level; i++) {
        node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = node;
    }
    map->size++;
    return true;
}

void *tefstd_skipmap_get(tefstd_skipmap_t *map, const void *key) {
    if (!map || !key) return NULL;
    const tefstd_skipnode_t *x = find_path(map, key, NULL);
    return (x && CMP(x->key, key, map->key_size) == 0) ? x->value : NULL;
}

bool tefstd_skipmap_del(tefstd_skipmap_t *map, const void *key) {
    if (!map || !key) return false;

    tefstd_skipnode_t *update[MAX_LEVEL];
    tefstd_skipnode_t *x = find_path(map, key, update);
    if (!x || CMP(x->key, key, map->key_size) != 0) return false;

    for (int i = 0; i < map->level; i++) {
        if (update[i]->forward[i] != x) break;
        update[i]->forward[i] = x->forward[i];
    }
    while (map->level > 1 && !map->header->forward[map->level - 1]) map->level--;

    free(x->key);
    free(x->value);
    free(x->forward);
    free(x);
    map->size--;
    return true;
}

void *tefstd_skipmap_min(tefstd_skipmap_t *map) {
    if (!map || !map->header) return NULL;
    const tefstd_skipnode_t *n = map->header->forward[0];
    return n ? n->value : NULL;
}

void *tefstd_skipmap_max(tefstd_skipmap_t *map) {
    if (!map || !map->header) return NULL;
    const tefstd_skipnode_t *x = map->header;
    while (x->forward[0]) x = x->forward[0]; // 沿第 0 层走到尾
    return x != map->header ? x->value : NULL;
}

// 内部：判断当前节点是否已超出 [start, end] 范围
static bool out_of_range(const tefstd_skipmap_t *map, const tefstd_skipnode_t *n,
                         const void *end, const bool inclusive) {
    if (!n || !end) return false;
    const int r = CMP(n->key, end, map->key_size);
    return r > 0 || (!inclusive && r == 0);
}

skipmap_iter_t tefstd_skipmap_range(tefstd_skipmap_t *map, const void *start,
                                    const void *end, const bool inclusive) {
    skipmap_iter_t iter = {0};
    if (!map || !map->header) return iter;

    iter.map = map;
    iter.inclusive = inclusive;
    if (end) {
        iter.end_key = malloc(map->key_size);
        if (iter.end_key) memcpy(iter.end_key, end, map->key_size);
    }

    iter.current = start ? find_path(map, start, NULL) : map->header->forward[0];
    if (out_of_range(map, iter.current, iter.end_key, inclusive)) iter.current = NULL;
    return iter;
}

bool tefstd_skipmap_next(skipmap_iter_t *iter, void *key_out, void *value_out) {
    if (!iter || !iter->map || !iter->current) return false;

    if (key_out) memcpy(key_out, iter->current->key, iter->map->key_size);
    if (value_out) memcpy(value_out, iter->current->value, iter->map->value_size);

    iter->current = iter->current->forward[0];
    if (out_of_range(iter->map, iter->current, iter->end_key, iter->inclusive))
        iter->current = NULL;

    if (!iter->current && iter->end_key) {
        free(iter->end_key);
        iter->end_key = NULL;
    }
    return iter->current != NULL;
}
