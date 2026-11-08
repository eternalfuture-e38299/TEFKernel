/*******************************************************************************
 * File: memdl
 * Project: tefkernel
 * Created: 2025/11/23
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

#ifndef TEFKERNEL_MEMDL_H
#define TEFKERNEL_MEMDL_H

#include <stddef.h>

#include "../tef_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// 平台定义
#define MEMDL_PLATFORM_UNKNOWN   0
#define MEMDL_PLATFORM_LINUX     1
#define MEMDL_PLATFORM_ANDROID   2
#define MEMDL_PLATFORM_MACOS     3
#define MEMDL_PLATFORM_IOS       4
#define MEMDL_PLATFORM_WINDOWS   5

// 架构定义
#define MEMDL_ARCH_UNKNOWN       0
#define MEMDL_ARCH_X86           1
#define MEMDL_ARCH_X86_64        2
#define MEMDL_ARCH_ARM           3
#define MEMDL_ARCH_ARM64         4

// 标志定义
#define MEMDL_NOW    0x1     // 立即解析符号
#define MEMDL_LAZY   0x2     // 延迟解析符号
#define MEMDL_LOCAL  0x4     // 局部符号
#define MEMDL_GLOBAL 0x8     // 全局符号

typedef void *memdl_handle_t;

// 核心API
DEFINE_FUNCTION(memdl_handle_t, memdl_open_file, const char *filename, int flags)

DEFINE_FUNCTION(memdl_handle_t, memdl_open, const void *so_data, size_t so_size, int flags)

DEFINE_FUNCTION(void *, memdl_sym, memdl_handle_t handle, const char *symbol)

DEFINE_FUNCTION(int, memdl_close, memdl_handle_t handle);

DEFINE_FUNCTION(const char *, memdl_error, void);

// 高级功能
DEFINE_FUNCTION(int, memdl_get_arch, const void *so_data, size_t so_size)

DEFINE_FUNCTION(int, memdl_validate, const void *so_data, size_t so_size)

DEFINE_FUNCTION(int, memdl_get_platform, void)

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_MEMDL_H
