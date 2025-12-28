/*******************************************************************************
 * File: dictionary
 * Project: tefkernel
 * Created: 2025/12/27
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

#ifndef TEFKERNEL_DICTIONARY_H
#define TEFKERNEL_DICTIONARY_H

#include "../type.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建一个dictionary
 * @param key_type 键类型
 * @param value_type 值类型
 * @param capacity 初始容量
 * @return dictionary实例
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_dictionary_create, patch_handle_t key_type, patch_handle_t value_type, size_t capacity);

/**
 * @brief 尝试在dictionary中添加一个键对
 * @param dictionary dictionary实例
 * @param key[in] 键指针
 * @param value[in] 值指针
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_dictionary_add, patch_handle_t dictionary, void* key, void* value);

/**
 * @brief 通过键获取值
 * @param dictionary dictionary实例
 * @param key 键值指针
 * @param out_value[out] 输出值
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_dictionary_get_value, patch_handle_t dictionary, void* key, void* out_value);

/**
 * @brief 修改键对
 * @param dictionary dictionary实例
 * @param key 键值指针
 * @param value 值指针
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_dictionary_set_value, patch_handle_t dictionary, void* key, void* value);

/**
 * @brief 清空dictionary
 * @param dictionary dictionary实例
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_dictionary_clear, patch_handle_t dictionary);

/**
 * @brief 获取dictionary长度
 * @param dictionary dictionary实例
 * @return dictionary长度
 */
DEFINE_FUNCTION(size_t, patchlib_dictionary_length, patch_handle_t dictionary);

/**
 * @brief 移除指定键对
 * @param dictionary dictionary实例
 * @param key 键值指针
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_dictionary_remove, patch_handle_t dictionary, void* key);

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_DICTIONARY_H