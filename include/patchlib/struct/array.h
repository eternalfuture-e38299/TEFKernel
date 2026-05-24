/*******************************************************************************
 * File: array
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

#ifndef TEFKERNEL_ARRAY_H
#define TEFKERNEL_ARRAY_H

#include "../type.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建数组实例
 * @param size 数组大小
 * @param type 元素类型句柄
 * @return 返回的数组实例
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_array_create, size_t size, patch_handle_t type);

/**
 * @brief 获取数组元素
 * @param array 数组实例
 * @param index 索引
 * @param out_value[out] 输出的值（自动根据元素类型复制）
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_array_at, patch_handle_t array, size_t index, void* out_value);

/**
 * @brief 修改数组元素
 * @param array 数组实例
 * @param index 索引
 * @param new_value[in] 新值（自动根据元素类型读取）
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_array_set, patch_handle_t array, size_t index, void* new_value);

/**
 * @brief 填充数组元素
 * @param array 数组实例
 * @param value[in] 填充值（自动根据元素类型读取）
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_array_fill, patch_handle_t array, void* value);

/**
 * @brief 判断数组是否为空
 * @param array 数组实例
 * @return 判断结果
 */
DEFINE_FUNCTION(bool, patchlib_array_empty, patch_handle_t array);

/**
 * @brief 获取数组长度
 * @param array 数组实例
 * @return 数组长度
 */
DEFINE_FUNCTION(size_t, patchlib_array_length, patch_handle_t array);

/**
 * @brief 从 C 数组复制到 IL2CPP 数组
 * @param dest IL2CPP 目标数组
 * @param src C 源数组指针
 * @param count 要复制的元素个数
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_array_copy_from_c, patch_handle_t dest, const void* src, size_t count);

/**
 * @brief 从 IL2CPP 数组复制到 C 数组
 * @param dest C 目标数组指针
 * @param src IL2CPP 源数组
 * @param count 要复制的元素个数
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_array_copy_to_c, void* dest, patch_handle_t src, size_t count);

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_ARRAY_H