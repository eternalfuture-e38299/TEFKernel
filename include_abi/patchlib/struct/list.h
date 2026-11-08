/*******************************************************************************
 * File: list
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

#ifndef TEFKERNEL_LIST_H
#define TEFKERNEL_LIST_H

#include "../type.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建列表实例
 * @param capacity 初始容量
 * @param type 列表类型
 * @return 返回的列表实例
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_list_create, size_t capacity, patch_handle_t type);

/**
 * @brief 复制其他数组的内容到列表
 * @param list 列表实例
 * @param array 数组实例
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_list_copy_from, patch_handle_t list, patch_handle_t array);

/**
 * @brief 在列表中添加一个值
 * @param list 列表实例
 * @param value[int] 新值指针
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_list_add, patch_handle_t list, void* value);

/**
 * @brief 从列表中移除一个值
 * @param list 列表实例
 * @param value[in] 删除值
 */
DEFINE_FUNCTION(bool, patchlib_list_remove, patch_handle_t list, void* value);

/**
 * @brief 移除列表中某个引索的值
 * @param list 列表实例
 * @param index 引索
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_list_remove_at, patch_handle_t list, size_t index);

/**
 * @brief 清空列表
 * @param list 列表实例
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_list_clear, patch_handle_t list);

/**
 * @brief 获取内部array
 * @param list 列表实例
 * @return 执行结果
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_list_get_array, patch_handle_t list);

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_LIST_H