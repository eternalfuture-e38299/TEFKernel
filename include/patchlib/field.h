/*******************************************************************************
 * File: field
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

#ifndef TEFKERNEL_FIELD_H
#define TEFKERNEL_FIELD_H

#include "type.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 字段基本信息获取 ====================
/**
 * @brief 获取字段名称
 * @param field 字段句柄
 * @return 字段名称
 */
DEFINE_FUNCTION(const char*, patchlib_field_get_name, patch_handle_t field)

// ==================== 字段特征检查 ====================
/**
 * @brief 检查字段是否为静态
 * @param field 字段句柄(必须为有效句柄)
 * @return 为静态字段时返回true，否则返回 false
 */
DEFINE_FUNCTION(bool, patchlib_field_is_static, patch_handle_t field)

/**
 * @brief 判断字段是否为实例字段
 * @param field 字段句柄(必须有效)
 * @return 为实例字段时返回true，否则返回 false
 */
DEFINE_FUNCTION(bool, patchlib_field_is_instance, patch_handle_t field)

/**
 * @brief 检查字段是否为常量(const)
 * @param field 字段句柄（必须为有效句柄）
 * @return true表示为常量字段，false表示非常量字段
 */
DEFINE_FUNCTION(bool, patchlib_field_is_const, patch_handle_t field)

/**
 * @brief 检查字段是否为线程静态(thread static)
 * @param field 字段句柄（必须为有效句柄）
 * @return true表示为线程静态字段，false表示非线程静态字段
 */
DEFINE_FUNCTION(bool, patchlib_field_is_thread_static, patch_handle_t field)

// ==================== 字段值操作 ====================
/**
 * @brief 获取字段值
 * @param field 字段句柄(必须为有效句柄)
 * @param instance 实例(静态时为NULL)
 * @param value [out] 输出值缓冲区
 */
DEFINE_FUNCTION(void, patchlib_field_get_value, patch_handle_t field, patch_handle_t instance, void *value)

/**
 * @brief 设置字段值
 * @param field 字段句柄(必须为有效句柄)
 * @param instance 实例(静态时为NULL)
 * @param value 要设置的值
 */
DEFINE_FUNCTION(void, patchlib_field_set_value, patch_handle_t field, patch_handle_t instance, void *value)

#if __ANDROID__
// ==================== Android平台特定功能 ====================
/**
 * @brief 获取字段真实指针
 * @param field 字段句柄
 * @param instance 实例指针(非实例时为NULL)
 * @return 成功则返回指针，错误返回NULL
 */
DEFINE_FUNCTION(void *, patchlib_field_get_pointer, patch_handle_t field, void *instance)
#endif

/**
 * @brief 获取字段数据大小
 * @param field 字段句柄
 * @return 成功返回1~8，否则返回0
 */
DEFINE_FUNCTION(size_t, patchlib_field_get_size, patch_handle_t field)

/**
 * @brief 获取字段类型
 * @param field 字段句柄
 * @return 类型
 */
DEFINE_FUNCTION(patch_type_t, patchlib_field_get_type, patch_handle_t field)

#if __ANDROID__

#    define patchlib_field_free(handle) ((void)0)

#endif

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_FIELD_H