/*******************************************************************************
 * File: thread
 * Project: tefkernel
 * Created: 2026/7/27
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

#ifndef TEFKERNEL_THREAD_H
#define TEFKERNEL_THREAD_H

#include "type.h"

#ifdef __cplusplus
extern "C" {
#endif

#if __ANDROID__
/**
 * @brief 获取当前线程
 * @return 线程句柄
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_thread_current)

/**
 * @brief 附加当前线程到运行时
 * @return 线程句柄
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_thread_attach)

/**
 * @brief 分离当前线程
 * @param thread 线程句柄
 */
DEFINE_FUNCTION(void, patchlib_thread_detach, patch_handle_t thread)

#else
#define patchlib_thread_current() ((patch_handle_t)1)
#define patchlib_thread_attach() ((void)0)
#define patchlib_thread_detach(thread) ((void)0)
#endif

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_THREAD_H