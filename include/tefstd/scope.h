/*******************************************************************************
 * File: scope
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

#ifndef TEFKERNEL_STD_SCOPE_H
#define TEFKERNEL_STD_SCOPE_H

#include <stdbool.h>

#include "../tef_api.h"
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#error "tefstd_scope requires ISO C11 or later (C17 recommended)"
#endif

/* ==================== 释放函数类型 ==================== */

typedef void (*tefstd_scope_release_fn)(void *ptr);

/* ==================== 清理链表 ==================== */

typedef struct tefstd_scope_node_t {
    void *ptr;
    tefstd_scope_release_fn fn;
    struct tefstd_scope_node_t *next;
} tefstd_scope_node_t;

typedef struct tefstd_scope_list_t {
    tefstd_scope_node_t *head;
} tefstd_scope_list_t;

DEFINE_FUNCTION(void, tefstd_scope_push,
                tefstd_scope_list_t *list, tefstd_scope_node_t *node,
                void *ptr, tefstd_scope_release_fn fn)

DEFINE_FUNCTION(void, tefstd_scope_run,
                tefstd_scope_list_t *list)

DEFINE_FUNCTION(bool, tefstd_scope_detach,
                tefstd_scope_list_t *list, const void *ptr)

/* ==================== 作用域宏 ==================== */

/**
 * tefstd_scope { ... }
 * 打开一个资源作用域，块尾逆序清理。
 * 块内禁止 return / goto / longjmp 跳出（见规范 S1）。
 */
#define tefstd_scope                                                   \
    for (tefstd_scope_list_t _tef_scope_l = {0};                       \
         _tef_scope_l.head != (tefstd_scope_node_t *)1;                \
         tefstd_scope_run(&_tef_scope_l),                              \
         _tef_scope_l.head = (tefstd_scope_node_t *)1)                 \
        for (int _tef_scope_once = 1; _tef_scope_once;                 \
             _tef_scope_once = 0)

/**
 * tefstd_own(type, name, init, release_fn)
 * 声明并登记一个变量，块尾按登记逆序用 release_fn 释放。
 *
 * 例：tefstd_own(FILE *, fp, fopen("a.txt","w"), tefstd_scope_release_file);
 */
#define tefstd_own(type, name, init, release_fn)                       \
    type name = (init);                                                \
    tefstd_scope_node_t _tef_scope_n_##name = {                        \
        (void *)(name), (release_fn), NULL                             \
    };                                                                 \
    tefstd_scope_push(&_tef_scope_l, &_tef_scope_n_##name,             \
                      (void *)(name), (release_fn))

/**
 * tefstd_hold(name, release_fn)
 * 登记一个已声明的变量。
 *
 * 例：tefstd_hold(fp, tefstd_scope_release_file);
 */
#define tefstd_hold(name, release_fn) do {                             \
    tefstd_scope_node_t _tef_scope_n_##name = {                        \
        (void *)(name), (release_fn), NULL                             \
    };                                                                 \
    tefstd_scope_push(&_tef_scope_l, &_tef_scope_n_##name,             \
                      (void *)(name), (release_fn));                   \
} while (0)

/**
 * tefstd_drop(name, release_fn)
 * 提前释放并摘除登记，变量置 NULL。
 * release_fn 必须与登记时一致（或等价），否则行为未定义。
 */
#define tefstd_drop(name, release_fn) do {                             \
    if (tefstd_scope_detach(&_tef_scope_l, (void *)(name))) {          \
        (release_fn)((void *)(name));                                  \
    }                                                                  \
    (name) = NULL;                                                     \
} while (0)

#ifdef __cplusplus
}
#endif
#endif // TEFKERNEL_STD_SCOPE_H