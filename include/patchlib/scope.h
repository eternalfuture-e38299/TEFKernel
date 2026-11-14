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

#ifndef TEFKERNEL_PATCHLIB_SCOPE_H
#define TEFKERNEL_PATCHLIB_SCOPE_H

#include "../tef_api.h"
#include "../tefstd/scope.h"
#include "type.h"   /* patch_handle_t, patchlib_free, patchlib_object_root/unroot */

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 释放策略 ==================== */

/* 桌面：patchlib_free 真释放；Android：patchlib_free 是空宏，无操作。
 * 两端调用点一致，行为由 patchlib_free 的平台定义决定。 */
static inline void patch_scope_release_handle(void *p) {
    if (p) patchlib_free((patch_handle_t)p);
}

#if defined(__ANDROID__)
/* Android：句柄是裸托管指针，若建立了 GC 强引用，用 unroot 解除；
 * 桌面：patchlib_object_root 不存在，退化为 patchlib_free。 */
static inline void patch_scope_release_root(void *p) {
    if (p) patchlib_object_unroot(p);
}
#else
static inline void patch_scope_release_root(void *p) {
    if (p) patchlib_free((patch_handle_t)p);
}
#endif

/* ==================== 特化作用域 ==================== */

/**
 * patch_scope { ... }
 * 与 tefstd_scope 同构，块尾逆序清理。
 * 块内禁止 return / goto / longjmp 跳出。
 */
#define patch_scope tefstd_scope

/**
 * patch_own(name, init)
 * 声明并登记一个 patch 句柄，块尾用 patch_scope_release_handle 清理。
 * 例：patch_own(patch_handle_t, cls, patchlib_type_get_type("Terraria", "Main"));
 */
#define patch_own(name, init) \
    tefstd_own(patch_handle_t, name, init, patch_scope_release_handle)

/**
 * patch_own_root(name, init)
 * 声明并登记一个 GC 根句柄，块尾用 patch_scope_release_root 清理。
 * Android 上调 patchlib_object_unroot；桌面退化为 patchlib_free。
 */
#define patch_own_root(name, init) \
    tefstd_own(patch_handle_t, name, init, patch_scope_release_root)

/**
 * patch_hold(name)
 * 登记一个已声明的 patch 句柄。
 */
#define patch_hold(name) \
    tefstd_hold(name, patch_scope_release_handle)

/**
 * patch_hold_root(name)
 * 登记一个已声明的 GC 根句柄。
 */
#define patch_hold_root(name) \
    tefstd_hold(name, patch_scope_release_root)

/**
 * patch_drop(name)
 * 提前释放并摘除登记。
 */
#define patch_drop(name) \
    tefstd_drop(name, patch_scope_release_handle)

#ifdef __cplusplus
}
#endif
#endif // TEFKERNEL_PATCHLIB_SCOPE_H