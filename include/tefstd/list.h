/*******************************************************************************
 * File: list
 * Project: tefkernel
 * Created: 2026/11/14
 * Author: eternalfuture-e38299
 * Github: https://github.com/eternalfuture-e38299
 *
 * MIT License
 *
 * Copyright (c) 2026 eternalfuture-e38299
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

#ifndef TEFKERNEL_STD_LIST_H
#define TEFKERNEL_STD_LIST_H

#include <stdbool.h>
#include <stddef.h>

#include "../tef_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 双向链表节点
 *
 * 用户不应直接操作节点字段，应通过 tefstd_list_* 系列函数操作。
 */
typedef struct tefstd_list_node_t {
    struct tefstd_list_node_t *prev; ///< 前驱节点
    struct tefstd_list_node_t *next; ///< 后继节点
    void *data;                      ///< 指向元素数据的指针（堆分配，大小为 elem_size）
} tefstd_list_node_t;

/**
 * @brief 双向链表结构体（类似 C++ std::list）
 *
 * 使用哨兵头节点：header.next 指向第一个元素，header.prev 指向最后一个元素。
 * 空链表时 header.next == header.prev == &header。
 */
typedef struct {
    tefstd_list_node_t header; ///< 哨兵头节点，不存储实际数据
    size_t size;               ///< 当前元素个数
    size_t elem_size;          ///< 单个元素的大小（字节），由初始化时指定
} tefstd_list_t;

/**
 * @brief 链表迭代器
 *
 * 通过 tefstd_list_iter 创建，用 tefstd_list_next 推进。
 */
typedef struct {
    tefstd_list_node_t *node; ///< 当前节点；等于 &map->header 表示遍历结束
    const tefstd_list_t *list;///< 所属链表
} tefstd_list_iter_t;

/**
 * @brief 初始化链表
 *
 * @param list 指向待初始化的 tefstd_list_t 结构体（不能为 NULL）
 * @param elem_size 单个元素的大小（字节），例如 sizeof(int)
 * @return 成功返回 true；elem_size 为 0 或 list 为 NULL 返回 false
 *
 * @note 调用后链表为空（size=0），哨兵节点自环。
 */
DEFINE_FUNCTION(bool, tefstd_list_init, tefstd_list_t *list, size_t elem_size)

/**
 * @brief 销毁链表并释放所有节点和元素内存
 *
 * @param list 指向链表的指针（可以为 NULL，此时无操作）
 *
 * @note 调用后 list 字段清零。
 */
DEFINE_FUNCTION(void, tefstd_list_free, tefstd_list_t *list)

/**
 * @brief 在链表头部插入元素
 *
 * @param list 已初始化的链表指针（不能为 NULL）
 * @param elem 指向要插入元素的地址（不能为 NULL）
 * @return 成功返回 true；内存分配失败返回 false，链表保持原状
 *
 * @note 元素内容被复制到内部节点。
 */
DEFINE_FUNCTION(bool, tefstd_list_push_front, tefstd_list_t *list, const void *elem)

/**
 * @brief 在链表尾部插入元素
 *
 * @param list 已初始化的链表指针（不能为 NULL）
 * @param elem 指向要插入元素的地址（不能为 NULL）
 * @return 成功返回 true；内存分配失败返回 false，链表保持原状
 */
DEFINE_FUNCTION(bool, tefstd_list_push_back, tefstd_list_t *list, const void *elem)

/**
 * @brief 弹出链表头部元素
 *
 * @param list 已初始化的链表指针（不能为 NULL）
 * @param out_elem 可选，若不为 NULL 则将弹出的元素内容复制到该地址
 * @return 链表非空返回 true；否则返回 false
 */
DEFINE_FUNCTION(bool, tefstd_list_pop_front, tefstd_list_t *list, void *out_elem)

/**
 * @brief 弹出链表尾部元素
 *
 * @param list 已初始化的链表指针（不能为 NULL）
 * @param out_elem 可选，若不为 NULL 则将弹出的元素内容复制到该地址
 * @return 链表非空返回 true；否则返回 false
 */
DEFINE_FUNCTION(bool, tefstd_list_pop_back, tefstd_list_t *list, void *out_elem)

/**
 * @brief 获取链表头部元素的地址
 *
 * @param list 已初始化的链表指针（不能为 NULL）
 * @return 链表非空返回指向第一个元素的指针；否则返回 NULL
 */
DEFINE_FUNCTION(void *, tefstd_list_front, const tefstd_list_t *list)

/**
 * @brief 获取链表尾部元素的地址
 *
 * @param list 已初始化的链表指针（不能为 NULL）
 * @return 链表非空返回指向最后一个元素的指针；否则返回 NULL
 */
DEFINE_FUNCTION(void *, tefstd_list_back, const tefstd_list_t *list)

/**
 * @brief 获取链表元素个数
 */
DEFINE_FUNCTION(size_t, tefstd_list_size, const tefstd_list_t *list)

/**
 * @brief 判断链表是否为空
 */
DEFINE_FUNCTION(bool, tefstd_list_empty, const tefstd_list_t *list)

/**
 * @brief 清空链表中的所有元素（保留哨兵结构）
 *
 * @param list 已初始化的链表指针（不能为 NULL）
 *
 * @note 释放所有节点，size 置 0。
 */
DEFINE_FUNCTION(void, tefstd_list_clear, tefstd_list_t *list)

/**
 * @brief 删除迭代器指向的节点
 *
 * @param iter 指向迭代器的指针（不能为 NULL）
 * @param out_elem 可选，若不为 NULL 则将删除的元素内容复制到该地址
 * @return 删除成功返回 true；迭代器已结束或无效返回 false
 *
 * @note 删除后 iter->node 被更新为被删节点的下一个节点，可继续遍历。
 */
DEFINE_FUNCTION(bool, tefstd_list_erase, tefstd_list_iter_t *iter, void *out_elem)

/**
 * @brief 创建链表迭代器，指向第一个元素
 *
 * @param list 已初始化的链表指针（不能为 NULL）
 * @return 迭代器；空链表时 iter.node == &list->header
 */
DEFINE_FUNCTION(tefstd_list_iter_t, tefstd_list_iter, const tefstd_list_t *list)

/**
 * @brief 将迭代器移动到下一个元素并取出数据
 *
 * @param iter 迭代器指针（不能为 NULL）
 * @param out_elem 可选，若不为 NULL 则将当前元素复制到该地址
 * @return 还有元素返回 true；遍历结束返回 false
 *
 * @note 语义为“取出当前并前进”，与 tefstd_hashmap_next 一致。
 */
DEFINE_FUNCTION(bool, tefstd_list_next, tefstd_list_iter_t *iter, void *out_elem)

/**
 * @brief 在堆上创建并初始化链表
 * @param elem_size 单个元素大小
 * @return 成功返回已初始化的链表指针；失败返回 NULL
 */
DEFINE_FUNCTION(tefstd_list_t *, tefstd_list_new, size_t elem_size)

#ifdef __cplusplus
}
#endif
#endif // TEFKERNEL_STD_LIST_H