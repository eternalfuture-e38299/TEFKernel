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
#define P 0.25  // 晋升概率

// 随机生成层数
static int random_level() {
    int level = 1;
    while (rand() < RAND_MAX * P && level < MAX_LEVEL) level++;
    return level;
}

// 比较函数
static int cmp(const void* a, const void* b, const size_t size) {
    return memcmp(a, b, size);
}

bool tefstd_skipmap_init(tefstd_skipmap_t* map, const size_t key_size, const size_t value_size) {
    if (!map || !key_size || !value_size) return false;

    map->header = malloc(sizeof(tefstd_skipnode_t));
    if (!map->header) return false;

    map->header->forward = calloc(MAX_LEVEL, sizeof(tefstd_skipnode_t*));
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

void tefstd_skipmap_free(tefstd_skipmap_t* map) {
    if (!map) return;

    tefstd_skipnode_t* node = map->header;
    while (node) {
        tefstd_skipnode_t* next = node->forward[0];
        free(node->key);
        free(node->value);
        free(node->forward);
        free(node);
        node = next;
    }
    memset(map, 0, sizeof(*map));
}

bool tefstd_skipmap_put(tefstd_skipmap_t* map, const void* key, const void* value) {
    if (!map || !key) return false;

    tefstd_skipnode_t* update[MAX_LEVEL];
    tefstd_skipnode_t* x = map->header;

    // 查找插入位置
    for (int i = map->level - 1; i >= 0; i--) {
        while (x->forward[i] &&
               cmp(x->forward[i]->key, key, map->key_size) < 0) {
            x = x->forward[i];
        }
        update[i] = x;
    }

    x = x->forward[0];

    // 如果键已存在，更新值
    if (x && cmp(x->key, key, map->key_size) == 0) {
        memcpy(x->value, value, map->value_size);
        return true;
    }

    // 创建新节点
    const int level = random_level();
    tefstd_skipnode_t* new_node = malloc(sizeof(tefstd_skipnode_t));
    if (!new_node) return false;

    new_node->key = malloc(map->key_size);
    new_node->value = malloc(map->value_size);
    new_node->forward = calloc(level, sizeof(tefstd_skipnode_t*));

    if (!new_node->key || !new_node->value || !new_node->forward) {
        free(new_node->key);
        free(new_node->value);
        free(new_node->forward);
        free(new_node);
        return false;
    }

    memcpy(new_node->key, key, map->key_size);
    memcpy(new_node->value, value, map->value_size);

    // 调整层级
    if (level > map->level) {
        for (int i = map->level; i < level; i++) {
            update[i] = map->header;
        }
        map->level = level;
    }

    // 插入节点
    for (int i = 0; i < level; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }

    map->size++;
    return true;
}

void* tefstd_skipmap_get(tefstd_skipmap_t* map, const void* key) {
    if (!map || !key) return NULL;

    const tefstd_skipnode_t* x = map->header;
    for (int i = map->level - 1; i >= 0; i--) {
        while (x->forward[i] &&
               cmp(x->forward[i]->key, key, map->key_size) < 0) {
            x = x->forward[i];
        }
    }

    x = x->forward[0];
    if (x && cmp(x->key, key, map->key_size) == 0) {
        return x->value;
    }
    return NULL;
}

bool tefstd_skipmap_del(tefstd_skipmap_t* map, const void* key) {
    if (!map || !key) return false;

    tefstd_skipnode_t* update[MAX_LEVEL];
    tefstd_skipnode_t* x = map->header;

    // 查找节点
    for (int i = map->level - 1; i >= 0; i--) {
        while (x->forward[i] &&
               cmp(x->forward[i]->key, key, map->key_size) < 0) {
            x = x->forward[i];
        }
        update[i] = x;
    }

    x = x->forward[0];
    if (!x || cmp(x->key, key, map->key_size) != 0) {
        return false;  // 未找到
    }

    // 删除节点
    for (int i = 0; i < map->level; i++) {
        if (update[i]->forward[i] != x) break;
        update[i]->forward[i] = x->forward[i];
    }

    // 清理空层级
    while (map->level > 1 && !map->header->forward[map->level - 1]) {
        map->level--;
    }

    free(x->key);
    free(x->value);
    free(x->forward);
    free(x);
    map->size--;
    return true;
}

void* tefstd_skipmap_min(tefstd_skipmap_t* map) {
    if (!map || !map->header) return NULL;
    const tefstd_skipnode_t* node = map->header->forward[0];
    return node ? node->value : NULL;
}

void* tefstd_skipmap_max(tefstd_skipmap_t* map) {
    if (!map || !map->header) return NULL;

    const tefstd_skipnode_t* x = map->header;
    for (int i = map->level - 1; i >= 0; i--) {
        while (x->forward[i]) {
            x = x->forward[i];
        }
    }
    return x != map->header ? x->value : NULL;
}

skipmap_iter_t tefstd_skipmap_range(tefstd_skipmap_t* map, const void* start, const void* end, const bool inclusive) {
    skipmap_iter_t iter = {0};

    if (!map || !map->header) {
        return iter;
    }

    iter.map = map;
    iter.end_key = end ? malloc(map->key_size) : NULL;
    iter.inclusive = inclusive;

    // 复制结束键（如果提供了）
    if (end && iter.end_key) {
        memcpy(iter.end_key, end, map->key_size);
    }

    // 查找起始位置
    if (start) {
        // 查找第一个大于等于start的节点
        const tefstd_skipnode_t* x = map->header;
        for (int i = map->level - 1; i >= 0; i--) {
            while (x->forward[i] &&
                   cmp(x->forward[i]->key, start, map->key_size) < 0) {
                x = x->forward[i];
            }
        }
        iter.current = x->forward[0];
    } else {
        // 如果没有指定起始位置，从头开始
        iter.current = map->header->forward[0];
    }

    // 如果当前节点不存在，或者已经超过结束键，则设为NULL
    if (iter.current && end) {
        const int cmp_result = cmp(iter.current->key, end, map->key_size);
        if (cmp_result > 0 || (!inclusive && cmp_result == 0)) {
            iter.current = NULL;
        }
    }

    return iter;
}

bool tefstd_skipmap_next(skipmap_iter_t* iter, void* key_out, void* value_out) {
    if (!iter || !iter->map || !iter->current) {
        return false;
    }

    // 复制当前节点的键值（如果提供了输出参数）
    if (key_out) {
        memcpy(key_out, iter->current->key, iter->map->key_size);
    }
    if (value_out) {
        memcpy(value_out, iter->current->value, iter->map->value_size);
    }

    // 移动到下一个节点
    iter->current = iter->current->forward[0];

    // 检查是否到达结束条件
    if (iter->current && iter->end_key) {
        const int cmp_result = cmp(iter->current->key, iter->end_key, iter->map->key_size);
        if (cmp_result > 0 || (!iter->inclusive && cmp_result == 0)) {
            iter->current = NULL;  // 超出范围
        }
    }

    // 清理资源（如果迭代结束）
    if (!iter->current) {
        if (iter->end_key) {
            free(iter->end_key);
            iter->end_key = NULL;
        }
    }

    return iter->current != NULL;
}