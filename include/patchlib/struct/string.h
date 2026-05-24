/*******************************************************************************
 * File: string
 * Project: tefkernel
 * Created: 2025/12/26
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

#ifndef TEFKERNEL_STRING_H
#define TEFKERNEL_STRING_H

#include "../type.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建一个字符串实例
 * @param str 字符串
 * @return 返回实例
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_string_create, const char* str);

/**
 * @brief 转换为字符串
 * @param str 字符串实例
 * @return c字符串(malloc分配)
 * @warning 别忘了free(
 */
DEFINE_FUNCTION(char*, patchlib_string_cstr, patch_handle_t str);

/**
 * @brief 判断字符串是否为空
 * @param str 字符串实例
 * @return 判断结果
 */
DEFINE_FUNCTION(bool, patchlib_string_empty, patch_handle_t str);

/**
 * @brief 获取字符串长度
 * @param str 字符串实例
 * @return 字符串长度
 */
DEFINE_FUNCTION(size_t, patchlib_string_length, patch_handle_t str);

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_STRING_H