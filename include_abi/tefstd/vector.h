/*******************************************************************************
 * File: vector
 * Project: tefkernel
 * Created: 2025/12/6
 * Author: eternalfuture-e38299
 * Github: https://github.com/eternalfuture-e38299
 *
 * MIT License
 *
 * Copyright (c) 2025 eternalfuture-e38299
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ******************************************************************************/

#ifndef TEFKERNEL_VECTOR_H
#define TEFKERNEL_VECTOR_H

#include <stdbool.h>
#include <stddef.h>

#include "../tef_api.h"

#ifdef __cplusplus
extern "C" {


#endif

/**
 * @brief 动态数组结构体（类似 C++ std::vector）
 *
 * 此结构体用于管理一个可自动扩容的连续内存块，支持任意类型元素。
 * 用户不应直接操作内部字段，而应通过 tefstd_vector_* 系列函数进行操作。
 */
typedef struct {
    void *data; ///< 指向元素数组的起始地址（连续内存）
    size_t size; ///< 当前存储的有效元素个数
    size_t capacity; ///< 当前已分配的容量（以元素个数计，非字节数）
    size_t elem_size; ///< 单个元素的大小（字节），由初始化时指定
} tefstd_vector_t;

/**
 * @brief 初始化一个 vector 实例
 *
 * @param vec 指向待初始化的 vector_t 结构体（不能为 NULL）
 * @param elem_size 单个元素的大小（字节），例如 sizeof(int) 或 sizeof(MyStruct)
 * @return 成功返回 true，失败（如参数无效）返回 false
 *
 * @note 调用后 vector 为空（size=0），但内部可能预分配少量内存
 */
DEFINE_FUNCTION(bool, tefstd_vector_init, tefstd_vector_t *vec, size_t elem_size)

/**
 * @brief 销毁 vector 并释放其占用的内存
 *
 * @param vec 指向 vector_t 的指针（可以为 NULL，此时无操作）
 *
 * @note 调用后 vec->data 将被置为 NULL，其他字段清零。
 *       不会释放 vec 本身的内存（即 &vec 是栈变量时无需 free）。
 */
DEFINE_FUNCTION(void, tefstd_vector_destroy, tefstd_vector_t *vec)

/**
 * @brief 向 vector 末尾追加一个新元素
 *
 * @param vec 已初始化的 vector_t 指针（不能为 NULL）
 * @param elem 指向要添加的元素的地址（不能为 NULL）
 * @return 成功返回 true；若内存分配失败则返回 false，vector 保持原状
 *
 * @note 元素内容会被 memcpy 到内部缓冲区，因此传入的是值的副本。
 */
DEFINE_FUNCTION(bool, tefstd_vector_push_back, tefstd_vector_t *vec, const void *elem)

/**
 * @brief 弹出 vector 的最后一个元素
 *
 * @param vec 已初始化的 vector_t 指针（不能为 NULL）
 * @param out_elem 可选，若不为 NULL，则将弹出的元素内容复制到该地址
 * @return 若 vector 非空则返回 true 并弹出元素；否则返回 false
 *
 * @note 如果 out_elem 为 NULL，则仅减少 size，不复制数据。
 */
DEFINE_FUNCTION(bool, tefstd_vector_pop_back, tefstd_vector_t *vec, void *out_elem)

/**
 * @brief 获取指定索引处元素的地址
 *
 * @param vec 已初始化的 vector_t 指针（不能为 NULL）
 * @param index 要访问的元素索引（从 0 开始）
 * @return 若 index < size，返回指向该元素的指针；否则返回 NULL
 *
 * @note 返回的指针可用于读取或修改元素（类似 &vec[index]）。
 *       不进行边界检查以外的任何操作，性能高。
 */
DEFINE_FUNCTION(void*, tefstd_vector_at, const tefstd_vector_t *vec, size_t index)

/**
 * @brief 获取 vector 中当前元素的数量
 *
 * @param vec 已初始化的 vector_t 指针（不能为 NULL）
 * @return 当前元素个数（>= 0）
 */
DEFINE_FUNCTION(size_t, tefstd_vector_size, const tefstd_vector_t *vec)

/**
 * @brief 获取 vector 当前分配的容量（元素个数）
 *
 * @param vec 已初始化的 vector_t 指针（不能为 NULL）
 * @return 当前可容纳的元素总数（无需 realloc 的最大 size）
 */
DEFINE_FUNCTION(size_t, tefstd_vector_capacity, const tefstd_vector_t *vec)

/**
 * @brief 清空 vector 中的所有元素（不释放内存）
 *
 * @param vec 已初始化的 vector_t 指针（不能为 NULL）
 *
 * @note size 被设为 0，但 capacity 和 data 保持不变，后续 push_back 可复用内存。
 */
DEFINE_FUNCTION(void, tefstd_vector_clear, tefstd_vector_t *vec)

/**
 * @brief 预分配至少 new_cap 个元素的容量
 *
 * @param vec 已初始化的 vector_t 指针（不能为 NULL）
 * @param new_cap 期望的最小容量（元素个数）
 * @return 若成功分配内存（或当前 capacity >= new_cap）返回 true；否则返回 false
 *
 * @note 可用于避免多次 push_back 触发频繁 realloc，提升性能。
 *       若 new_cap <= 当前 capacity，则无操作且返回 true。
 */
DEFINE_FUNCTION(bool, tefstd_vector_reserve, tefstd_vector_t *vec, size_t new_cap)

/**
 * @brief 删除指定索引处的元素
 *
 * @param vec 已初始化的 vector_t 指针（不能为 NULL）
 * @param index 要删除的元素索引（从 0 开始）
 * @param out_elem 可选，若不为 NULL，则将删除的元素内容复制到该地址
 * @return 若索引有效且删除成功返回 true；否则返回 false
 *
 * @note 删除后，后面的元素会向前移动，保持连续性。
 *       时间复杂度为 O(n)，因为需要移动元素。
 */
DEFINE_FUNCTION(bool, tefstd_vector_erase, tefstd_vector_t *vec, size_t index, void *out_elem)


/**
 * @brief 从vector中移除所有匹配指定值的元素（按字节比较）
 *
 * @param vec 已初始化的vector_t指针（不能为NULL）
 * @param value 指向要移除的值的指针（不能为NULL）
 * @return 成功移除至少一个元素返回true，未找到匹配元素返回false
 *
 * @note 使用memcmp进行字节级别的精确比较
 */
DEFINE_FUNCTION(bool, tefstd_vector_remove_value, tefstd_vector_t *vec, const void *value)

/**
 * @brief 从现有数组初始化 vector
 *
 * @param vec 指向待初始化的 vector_t 结构体（不能为 NULL）
 * @param elem_size 单个元素的大小（字节），例如 sizeof(int) 或 sizeof(MyStruct)
 * @param array 源数组指针（不能为 NULL，除非 array_length 为 0）
 * @param array_length 源数组中的元素个数
 * @return 成功返回 true，失败返回 false
 *
 * @note 此函数会：
 *       1. 初始化 vector 结构体
 *       2. 分配足够容纳所有元素的内存
 *       3. 将数组内容复制到 vector 中
 *       4. 设置 size 和 capacity 为 array_length
 *
 *       如果 array_length 为 0：
 *       - array 参数可以为 NULL
 *       - vector 将被初始化为空
 *       - 可能预分配少量内存
 *
 *       内存管理：
 *       - vector 内部会复制数组内容，不会引用原始数组
 *       - 原始数组的内存由调用者管理
 *       - 使用后必须调用 tefstd_vector_destroy 释放内存
 *
 *       错误处理：
 *       - 如果 array 为 NULL 但 array_length > 0，返回 false
 *       - 如果内存分配失败，返回 false，vec 保持未初始化状态
 */
DEFINE_FUNCTION(bool, tefstd_vector_init_from_array, tefstd_vector_t *vec, size_t elem_size, void* array,
                size_t array_length)


#ifdef __cplusplus
}
#endif

#endif // TEFKERNEL_VECTOR_H
