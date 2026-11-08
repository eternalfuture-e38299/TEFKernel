/*******************************************************************************
 * File: skipmap
 * Project: tefkernel
 * Created: 2025/12/12
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
 *******************************************************************************/

#ifndef TEFKERNEL_SKIPMAP_H
#define TEFKERNEL_SKIPMAP_H

#include <stdbool.h>
#include <stddef.h>

#include "../tef_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 跳表节点结构体
 *
 * 跳表是一种基于多层有序链表的数据结构，提供平均 O(log n) 复杂度的查找、插入和删除操作。
 * 每个节点包含一个键值对，以及多层的前向指针数组。
 */
typedef struct skipnode {
    void *key;                 ///< 指向键数据的指针
    void *value;               ///< 指向值数据的指针
    struct skipnode **forward; ///< 多层前向指针数组
} tefstd_skipnode_t;

/**
 * @brief 跳表映射结构体（有序映射）
 *
 * 基于跳表实现的有序键值映射。所有元素按键排序，支持范围查询和有序遍历。
 * 用户不应直接操作内部字段，而应通过 tefstd_skipmap_* 系列函数进行操作。
 */
typedef struct {
    tefstd_skipnode_t *header; ///< 头节点（哨兵节点，不存储实际数据）
    size_t size;               ///< 当前存储的有效键值对个数
    size_t key_size;           ///< 键的大小（字节），由初始化时指定
    size_t value_size;         ///< 值的大小（字节），由初始化时指定
    int level;                 ///< 当前跳表的最大层数
} tefstd_skipmap_t;

/**
 * @brief 范围查询迭代器结构体
 */
typedef struct {
    tefstd_skipmap_t *map;     ///< 指向要遍历的跳表
    tefstd_skipnode_t *current; ///< 当前遍历到的节点
    void *end_key;             ///< 范围结束键（可为 NULL 表示无上限）
    bool inclusive;            ///< 是否包含结束键
} skipmap_iter_t;

/**
 * @brief 初始化跳表映射
 *
 * @param map 指向待初始化的 tefstd_skipmap_t 结构体（不能为 NULL）
 * @param key_size 单个键的大小（字节），例如 sizeof(int)
 * @param value_size 单个值的大小（字节），例如 sizeof(MyStruct)
 * @return 成功返回 true，内存分配失败返回 false
 *
 * @note 调用后跳表为空（size=0），会创建头节点
 */
DEFINE_FUNCTION(bool, tefstd_skipmap_init,
                tefstd_skipmap_t *map, size_t key_size, size_t value_size)

/**
 * @brief 销毁跳表映射并释放其占用的内存
 *
 * @param map 指向跳表映射的指针（可以为 NULL，此时无操作）
 *
 * @note 会释放所有节点和键值对的内存
 */
DEFINE_FUNCTION(void, tefstd_skipmap_free, tefstd_skipmap_t *map)

/**
 * @brief 插入或更新键值对
 *
 * @param map 已初始化的跳表映射指针（不能为 NULL）
 * @param key 指向要插入的键的地址（不能为 NULL）
 * @param value 指向要插入的值的地址（不能为 NULL）
 * @return 成功返回 true；若内存分配失败返回 false
 *
 * @note 如果键已存在，则更新对应的值
 * @note 元素内容会被复制到内部存储
 */
DEFINE_FUNCTION(bool, tefstd_skipmap_put,
                tefstd_skipmap_t *map, const void *key, const void *value)

/**
 * @brief 查找指定键对应的值
 *
 * @param map 已初始化的跳表映射指针（不能为 NULL）
 * @param key 指向要查找的键的地址（不能为 NULL）
 * @return 若找到则返回指向该值的指针；否则返回 NULL
 *
 * @note 返回的指针可用于读取或修改值
 */
DEFINE_FUNCTION(void *, tefstd_skipmap_get,
                tefstd_skipmap_t *map, const void *key)

/**
 * @brief 删除指定键值对
 *
 * @param map 已初始化的跳表映射指针（不能为 NULL）
 * @param key 指向要删除的键的地址（不能为 NULL）
 * @return 若找到并删除返回 true；否则返回 false
 */
DEFINE_FUNCTION(bool, tefstd_skipmap_del,
                tefstd_skipmap_t *map, const void *key)

/**
 * @brief 获取最小键对应的值
 *
 * @param map 已初始化的跳表映射指针（不能为 NULL）
 * @return 若跳表非空则返回最小键的值指针；否则返回 NULL
 */
DEFINE_FUNCTION(void *, tefstd_skipmap_min, tefstd_skipmap_t *map)

/**
 * @brief 获取最大键对应的值
 *
 * @param map 已初始化的跳表映射指针（不能为 NULL）
 * @return 若跳表非空则返回最大键的值指针；否则返回 NULL
 */
DEFINE_FUNCTION(void *, tefstd_skipmap_max, tefstd_skipmap_t *map)

/**
 * @brief 创建范围查询迭代器
 *
 * @param map 已初始化的跳表映射指针（不能为 NULL）
 * @param start 范围起始键（可为 NULL 表示从头开始）
 * @param end 范围结束键（可为 NULL 表示直到末尾）
 * @param inclusive 是否包含结束键
 * @return 初始化好的迭代器
 *
 * @note 迭代范围：[start, end) 或 [start, end]（根据 inclusive 参数）
 */
DEFINE_FUNCTION(skipmap_iter_t, tefstd_skipmap_range,
                tefstd_skipmap_t *map, const void *start, const void *end, bool inclusive)

/**
 * @brief 获取迭代器的下一个键值对
 *
 * @param iter 迭代器指针（不能为 NULL）
 * @param key_out 可选，若不为 NULL 则将键复制到该地址
 * @param value_out 可选，若不为 NULL 则将值复制到该地址
 * @return 若还有下一个元素返回 true；否则返回 false
 *
 * @note 如果 key_out 或 value_out 为 NULL，则不复制对应数据
 */
DEFINE_FUNCTION(bool, tefstd_skipmap_next,
                skipmap_iter_t *iter, void *key_out, void *value_out)

#ifdef __cplusplus
}
#endif

#endif // TEFKERNEL_SKIPMAP_H