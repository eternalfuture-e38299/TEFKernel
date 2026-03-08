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

// 内部常量：最小初始容量
#define TEFSTD_VECTOR_MIN_CAPACITY 32

// 内部辅助函数：扩容
static bool tefstd_vector_grow(tefstd_vector_t *vec) {
    const size_t new_cap = vec->capacity > 0 ? vec->capacity * 2 : TEFSTD_VECTOR_MIN_CAPACITY;
    void *new_data = realloc(vec->data, new_cap * vec->elem_size);
    if (!new_data)
        return false; // 内存分配失败

    vec->data = new_data;
    vec->capacity = new_cap;
    return true;
}

// ==================== 公共 API 实现 ====================

bool tefstd_vector_init(tefstd_vector_t *vec, const size_t elem_size) {
    if (!vec || elem_size == 0)
        return false;
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
    vec->elem_size = elem_size;
    return true;
}

void tefstd_vector_destroy(tefstd_vector_t *vec) {
    if (vec) {
        free(vec->data);
        vec->data = NULL;
        vec->size = 0;
        vec->capacity = 0;
        vec->elem_size = 0;
    }
}

bool tefstd_vector_push_back(tefstd_vector_t *vec, const void *elem) {
    if (!vec || !elem || vec->elem_size == 0)
        return false;

    if (vec->size >= vec->capacity)
        if (!tefstd_vector_grow(vec))
            return false;

    // 复制元素到末尾
    memcpy((char*)vec->data + vec->size * vec->elem_size, elem, vec->elem_size);
    vec->size++;
    return true;
}

bool tefstd_vector_pop_back(tefstd_vector_t *vec, void *out_elem) {
    if (!vec || vec->size == 0) {
        return false;
    }

    vec->size--;

    if (out_elem) {
        // 复制被弹出的元素
        memcpy(out_elem, (char*)vec->data + vec->size * vec->elem_size, vec->elem_size);
    }

    return true;
}

void* tefstd_vector_at(const tefstd_vector_t *vec, const size_t index) {
    if (!vec || index >= vec->size)
        return NULL;
    return (char*)vec->data + index * vec->elem_size;
}

size_t tefstd_vector_size(const tefstd_vector_t *vec) {
    return vec ? vec->size : 0;
}

size_t tefstd_vector_capacity(const tefstd_vector_t *vec) {
    return vec ? vec->capacity : 0;
}

void tefstd_vector_clear(tefstd_vector_t *vec) {
    if (vec) {
        vec->size = 0;
    }
}

bool tefstd_vector_reserve(tefstd_vector_t *vec, const size_t new_cap) {
    if (!vec || new_cap == 0)
        return false;

    if (new_cap <= vec->capacity)
        return true; // 已满足要求

    void *new_data = realloc(vec->data, new_cap * vec->elem_size);
    if (!new_data)
        return false;

    vec->data = new_data;
    vec->capacity = new_cap;
    return true;
}

bool tefstd_vector_erase(tefstd_vector_t *vec, const size_t index, void *out_elem) {
    if (!vec || index >= vec->size) {
        return false;
    }

    // 如果需要输出被删除的元素
    if (out_elem) {
        memcpy(out_elem, (char*)vec->data + index * vec->elem_size, vec->elem_size);
    }

    // 如果删除的不是最后一个元素，需要移动后面的元素
    if (index < vec->size - 1) {
        void *dest = (char*)vec->data + index * vec->elem_size;
        const void *src = (char*)dest + vec->elem_size;
        const size_t bytes_to_move = (vec->size - index - 1) * vec->elem_size;
        memmove(dest, src, bytes_to_move);
    }

    vec->size--;
    return true;
}

bool tefstd_vector_remove_value(tefstd_vector_t *vec, const void *value) {
    if (!vec || !value || vec->elem_size == 0) {
        return false;
    }

    bool removed = false;
    size_t write_idx = 0;

    // 使用双指针法原地移除元素
    for (size_t read_idx = 0; read_idx < vec->size; ++read_idx) {
        const void *current_elem = (char*)vec->data + read_idx * vec->elem_size;

        if (memcmp(current_elem, value, vec->elem_size) == 0) {
            // 匹配到要移除的元素，跳过不复制
            removed = true;
        } else {
            // 不匹配的元素，复制到write_idx位置
            if (read_idx != write_idx) {
                void *dest = (char*)vec->data + write_idx * vec->elem_size;
                memcpy(dest, current_elem, vec->elem_size);
            }
            write_idx++;
        }
    }

    // 更新vector的大小
    if (removed) {
        vec->size = write_idx;
    }

    return removed;
}

bool tefstd_vector_init_from_array(tefstd_vector_t *vec, const size_t elem_size, void* array, const size_t array_length) {
    // 参数验证
    if (!vec || elem_size == 0) {
        return false;  // 无效参数
    }

    if (array_length > 0 && !array) {
        return false;  // 有元素但数组为 NULL
    }

    // 清零 vector 结构体
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
    vec->elem_size = elem_size;

    // 如果数组长度为0，直接初始化空 vector
    if (array_length == 0) {
        // 可以选择预分配少量内存，或者保持为0
        // 这里我们保持为空，第一次 push_back 时会自动分配
        return true;
    }

    // 分配足够的内存
    const size_t total_bytes = array_length * elem_size;
    void *data = malloc(total_bytes);
    if (!data) {
        return false;  // 内存分配失败
    }

    // 复制数组内容
    if (array) {
        memcpy(data, array, total_bytes);
    }

    // 设置 vector 状态
    vec->data = data;
    vec->size = array_length;
    vec->capacity = array_length;  // 容量设置为刚好容纳所有元素

    return true;
}