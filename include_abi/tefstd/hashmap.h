/*******************************************************************************
 * File: hashmap
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

#ifndef TEFKERNEL_HASHMAP_H
#define TEFKERNEL_HASHMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../tef_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 基于开放寻址法的哈希表实现，使用连续数组存储键值对，提供平均 O(1) 复杂度的查找、插入和删除。
 * 支持自定义键值类型，自动选择适当的哈希和比较函数。
 */
typedef struct {
    void *keys;       ///< 键数组（连续内存）
    void *values;     ///< 值数组（连续内存，与键数组一一对应）
    uint8_t *states;  ///< 状态数组：0=空, 1=占用, 2=删除（墓碑标记）
    size_t size;      ///< 当前存储的有效元素个数
    size_t capacity;  ///< 当前已分配的容量（总是2的幂，用于快速取模）
    size_t key_size;  ///< 单个键的大小（字节），由初始化时指定
    size_t value_size; ///< 单个值的大小（字节），由初始化时指定
} tefstd_hashmap_t;

/**
 * @brief 哈希表迭代器结构体
 *
 * 用于顺序遍历哈希表中的所有有效元素。遍历顺序与插入顺序无关，由哈希函数决定。
 */
typedef struct {
    const tefstd_hashmap_t *map; ///< 指向要遍历的哈希表
    size_t index;                ///< 当前遍历到的数组索引
} tefstd_hashmap_iter_t;

/**
 * @brief 计算字符串的哈希值
 *
 * @param str 要计算哈希的C字符串
 * @return 64位哈希值
 *
 * @note 使用优化的哈希算法，适用于字符串键
 */
DEFINE_FUNCTION(uint64_t, tefstd_hash_str, const char* str)

/**
 * @brief 计算内存块的哈希值
 *
 * @param data 指向内存块的指针
 * @param len 内存块长度（字节）
 * @return 64位哈希值
 *
 * @note 通用哈希函数，适用于任意类型的数据
 */
DEFINE_FUNCTION(uint64_t, tefstd_hash_mem, const void* data, size_t len)

/**
 * @brief 初始化哈希表
 *
 * @param map 指向待初始化的 tefstd_hashmap_t 结构体（不能为 NULL）
 * @param key_size 单个键的大小（字节），例如 sizeof(int) 或 sizeof(MyKey)
 * @param value_size 单个值的大小（字节），例如 sizeof(void*) 或 sizeof(MyValue)
 * @return 成功返回 true，内存分配失败返回 false
 *
 * @note 根据键类型自动选择哈希和比较函数：
 *       - 字符串（char*）使用字符串特化版本
 *       - 其他类型使用通用的内存哈希和比较
 */
DEFINE_FUNCTION(bool, tefstd_hashmap_init, tefstd_hashmap_t* map,
                size_t key_size, size_t value_size)

/**
 * @brief 销毁哈希表并释放其占用的内存
 *
 * @param map 指向哈希表的指针（可以为 NULL，此时无操作）
 *
 * @note 会释放键、值和状态数组的内存
 */
DEFINE_FUNCTION(void, tefstd_hashmap_free, tefstd_hashmap_t* map)

/**
 * @brief 插入或更新键值对
 *
 * @param map 已初始化的哈希表指针（不能为 NULL）
 * @param key 指向要插入的键的地址（不能为 NULL）
 * @param value 指向要插入的值的地址（不能为 NULL）
 * @return 成功返回 true；若内存分配失败返回 false
 *
 * @note 如果键已存在，则更新对应的值
 * @note 元素内容会被复制到内部存储
 * @note 当负载因子超过阈值时会自动扩容
 */
DEFINE_FUNCTION(bool, tefstd_hashmap_put, tefstd_hashmap_t* map,
                const void* key, const void* value)

/**
 * @brief 查找指定键对应的值
 *
 * @param map 已初始化的哈希表指针（不能为 NULL）
 * @param key 指向要查找的键的地址（不能为 NULL）
 * @return 若找到则返回指向该值的指针；否则返回 NULL
 *
 * @note 返回的指针可用于读取或修改值
 */
DEFINE_FUNCTION(void*, tefstd_hashmap_get, tefstd_hashmap_t* map, const void* key)

/**
 * @brief 删除指定键值对
 *
 * @param map 已初始化的哈希表指针（不能为 NULL）
 * @param key 指向要删除的键的地址（不能为 NULL）
 * @return 若找到并删除返回 true；否则返回 false
 *
 * @note 使用墓碑标记删除，不会立即收缩表
 */
DEFINE_FUNCTION(bool, tefstd_hashmap_del, tefstd_hashmap_t* map, const void* key)

/**
 * @brief 检查键是否存在
 *
 * @param map 已初始化的哈希表指针（不能为 NULL）
 * @param key 指向要检查的键的地址（不能为 NULL）
 * @return 键存在返回 true；否则返回 false
 *
 * @note 比 tefstd_hashmap_get 更快，因为不需要返回值
 */
DEFINE_FUNCTION(bool, tefstd_hashmap_has, const tefstd_hashmap_t* map, const void* key)

/**
 * @brief 获取哈希表中元素的数量
 *
 * @param map 已初始化的哈希表指针（不能为 NULL）
 * @return 当前元素个数
 */
DEFINE_FUNCTION(size_t, tefstd_hashmap_len, const tefstd_hashmap_t* map)

/**
 * @brief 清空哈希表中的所有元素
 *
 * @param map 已初始化的哈希表指针（不能为 NULL）
 *
 * @note 会标记所有元素为删除状态，但不释放底层内存
 */
DEFINE_FUNCTION(void, tefstd_hashmap_clear, tefstd_hashmap_t* map)

/**
 * @brief 创建哈希表迭代器
 *
 * @param map 已初始化的哈希表指针（不能为 NULL）
 * @return 指向第一个有效元素的迭代器
 *
 * @note 如果哈希表为空，返回的迭代器 index 等于 capacity
 */
DEFINE_FUNCTION(tefstd_hashmap_iter_t, tefstd_hashmap_iter, const tefstd_hashmap_t* map)

/**
 * @brief 将迭代器移动到下一个有效元素
 *
 * @param iter 迭代器指针（不能为 NULL）
 * @param key_out 可选，若不为 NULL 则将键复制到该地址
 * @param value_out 可选，若不为 NULL 则将值复制到该地址
 * @return 若还有下一个元素返回 true；否则返回 false
 *
 * @note 如果 key_out 或 value_out 为 NULL，则不复制对应数据
 * @note 会自动跳过空桶和墓碑桶
 */
DEFINE_FUNCTION(bool, tefstd_hashmap_next,
                tefstd_hashmap_iter_t* iter,
                void* key_out,
                void* value_out)

#ifdef __cplusplus
}
#endif

#endif // TEFKERNEL_HASHMAP_H