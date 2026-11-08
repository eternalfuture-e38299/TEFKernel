/*******************************************************************************
 * tefkernel - vector
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
 * Created: 2025/12/6
 *******************************************************************************/

#include "tefstd/vector.h"

#include <stdlib.h>
#include <string.h>

#define TEFSTD_VECTOR_MIN_CAPACITY 32

/* 元素地址：唯一一处做偏移计算，其余全部复用 */
#define TEFSTD_VECTOR_ELEM(vec, idx) \
    ((void *)((char *)(vec)->data + (idx) * (vec)->elem_size))

/* 初始化公共部分（语句宏） */
#define TEFSTD_VECTOR_RESET(vec, esz) do { \
    (vec)->data      = NULL; \
    (vec)->size      = 0; \
    (vec)->capacity  = 0; \
    (vec)->elem_size = (esz); \
} while (0)

/* ==================== 公共 API ==================== */

bool tefstd_vector_init(tefstd_vector_t *vec, const size_t elem_size) {
    if (!vec || elem_size == 0)
        return false;
    TEFSTD_VECTOR_RESET(vec, elem_size);
    return true;
}

void tefstd_vector_destroy(tefstd_vector_t *vec) {
    if (!vec)
        return;
    free(vec->data);
    TEFSTD_VECTOR_RESET(vec, 0);
}

bool tefstd_vector_reserve(tefstd_vector_t *vec, const size_t new_cap) {
    if (!vec || new_cap == 0)
        return false;
    if (new_cap <= vec->capacity)
        return true;

    void *p = realloc(vec->data, new_cap * vec->elem_size);
    if (!p)
        return false;
    vec->data = p;
    vec->capacity = new_cap;
    return true;
}

bool tefstd_vector_push_back(tefstd_vector_t *vec, const void *elem) {
    if (!vec || !elem || vec->elem_size == 0)
        return false;

    if (vec->size >= vec->capacity) {
        const size_t cap = vec->capacity ? vec->capacity * 2 : TEFSTD_VECTOR_MIN_CAPACITY;
        if (!tefstd_vector_reserve(vec, cap))
            return false;
    }

    memcpy(TEFSTD_VECTOR_ELEM(vec, vec->size), elem, vec->elem_size);
    vec->size++;
    return true;
}

bool tefstd_vector_pop_back(tefstd_vector_t *vec, void *out_elem) {
    if (!vec || vec->size == 0)
        return false;

    vec->size--;
    if (out_elem)
        memcpy(out_elem, TEFSTD_VECTOR_ELEM(vec, vec->size), vec->elem_size);
    return true;
}

void *tefstd_vector_at(const tefstd_vector_t *vec, const size_t index) {
    if (!vec || index >= vec->size)
        return NULL;
    return TEFSTD_VECTOR_ELEM(vec, index);
}

size_t tefstd_vector_size(const tefstd_vector_t *vec) {
    return vec ? vec->size : 0;
}

size_t tefstd_vector_capacity(const tefstd_vector_t *vec) {
    return vec ? vec->capacity : 0;
}

void tefstd_vector_clear(tefstd_vector_t *vec) {
    if (vec)
        vec->size = 0;
}

bool tefstd_vector_erase(tefstd_vector_t *vec, const size_t index, void *out_elem) {
    if (!vec || index >= vec->size)
        return false;

    void *slot = TEFSTD_VECTOR_ELEM(vec, index);
    if (out_elem)
        memcpy(out_elem, slot, vec->elem_size);

    if (index + 1 < vec->size) {
        const size_t rest = vec->size - index - 1;
        memmove(slot, (char *) slot + vec->elem_size, rest * vec->elem_size);
    }
    vec->size--;
    return true;
}

bool tefstd_vector_remove_value(tefstd_vector_t *vec, const void *value) {
    if (!vec || !value || vec->elem_size == 0)
        return false;

    bool removed = false;
    size_t w = 0;
    for (size_t r = 0; r < vec->size; ++r) {
        const void *cur = TEFSTD_VECTOR_ELEM(vec, r);
        if (memcmp(cur, value, vec->elem_size) == 0) {
            removed = true;
            continue;
        }
        if (r != w)
            memcpy(TEFSTD_VECTOR_ELEM(vec, w), cur, vec->elem_size);
        w++;
    }
    vec->size = w;
    return removed;
}

bool tefstd_vector_init_from_array(tefstd_vector_t *vec, const size_t elem_size,
                                   void *array, const size_t array_length) {
    if (!vec || elem_size == 0)
        return false;
    if (array_length > 0 && !array)
        return false;

    TEFSTD_VECTOR_RESET(vec, elem_size);
    if (array_length == 0)
        return true;

    void *data = malloc(array_length * elem_size);
    if (!data)
        return false;

    if (array)
        memcpy(data, array, array_length * elem_size);
    vec->data = data;
    vec->size = array_length;
    vec->capacity = array_length;
    return true;
}
