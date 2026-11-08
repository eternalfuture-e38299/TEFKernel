/*******************************************************************************
 * File: property
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

#ifndef TEFKERNEL_PROPERTY_H
#define TEFKERNEL_PROPERTY_H

#include <stdint.h>

#include "type.h"

#ifdef __cplusplus
extern "C" {

#endif

// ==================== 属性基本信息获取 ====================
/**
 * @brief 获取属性名称
 * @param property 属性句柄(必须有效)
 * @return 属性名称
 */
DEFINE_FUNCTION(const char*, patchlib_property_get_name, patch_handle_t property)

// ==================== 属性方法获取 ====================
/**
 * @brief 获取属性的Get方法
 * @param property 属性句柄(必须有效)
 * @return 成功返回函数句柄，否则返回PATCH_NULL
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_property_get_get_method, patch_handle_t property)

/**
 * @brief 获取属性的Set方法
 * @param property 属性句柄(必须有效)
 * @return 成功返回函数句柄，否则返回PATCH_NULL
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_property_get_set_method, patch_handle_t property)

#if __ANDROID__

#    define patchlib_property_free(handle) ((void)0)

#endif

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_PROPERTY_H