/*******************************************************************************
 * File: type
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

#ifndef TEFKERNEL_TYPE_H
#define TEFKERNEL_TYPE_H

#include <stdint.h>

#include "../tef_api.h"
#include "../tefstd/vector.h"

#ifdef __cplusplus
extern "C" {


#endif

/* 平台相关类型定义 */
#if defined(__ANDROID__)
typedef void *patch_handle_t;
#define PATCH_NULL NULL
#else
typedef int32_t patch_handle_t;
#define PATCH_NULL (-1)
#endif

// 基本类型枚举
typedef enum {
    PATCH_VOID,
    PATCH_INT8,
    PATCH_INT16,
    PATCH_INT32,
    PATCH_INT64,
    PATCH_UINT8,
    PATCH_UINT16,
    PATCH_UINT32,
    PATCH_UINT64,
    PATCH_BOOL,
    PATCH_FLOAT,
    PATCH_DOUBLE,
    PATCH_POINTER,
    PATCH_OBJECT,
    PATCH_CHAR
} patch_type_t;

#define PATCH_TYPE_TO_NATIVE(type) \
_Generic((type), \
patch_type_t: _Generic((int)(type), \
PATCH_UINT8:   uint8_t, \
PATCH_UINT16:  uint16_t, \
PATCH_UINT32:  uint32_t, \
PATCH_UINT64:  uint64_t, \
PATCH_INT8:    int8_t, \
PATCH_INT16:   int16_t, \
PATCH_INT32:   int32_t, \
PATCH_INT64:   int64_t, \
PATCH_FLOAT:   float, \
PATCH_DOUBLE:  double, \
PATCH_BOOL:    bool, \
PATCH_POINTER: void*, \
PATCH_OBJECT:  void*)

#if defined(__ANDROID__) || defined(ANDROID)
#define PATCH_HANDLE_FMT "%p"
#else
#define PATCH_HANDLE_FMT "%d"
#endif

DEFINE_FUNCTION(size_t, get_size_from_patch_type, patch_type_t type);

// ==================== 基础工具函数 ====================
/**
 * @brief 检查句柄是否有效
 * @param h 要检查的句柄
 * @return 有效返回true，无效返回false
 */
DEFINE_FUNCTION(bool, patchlib_is_valid, patch_handle_t h)

// ==================== 类型获取和创建 ====================
/**
 * @brief 根据命名空间和名称获取类型
 * @param ns 点分隔的命名空间字符串(如"System.Collections")
 * @param name 类型名称(如"List")
 * @return 成功返回有效句柄，失败返回PATCH_NULL
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_type_get_type, const char *ns, const char *name)

/**
 * @brief 获取基础类型句柄（避免字符串查找的开销）
 * @param basic_type 基础类型枚举
 * @return 类型句柄
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_get_basic_type, patch_type_t basic_type)

/**
 * @brief 创建类型的新实例
 * @param type 类型句柄(必须有效)
 * @return 成功返回实例句柄，失败返回PATCH_NULL
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_type_new_instance, patch_handle_t type)

/**
 * @brief 实例化泛型类型
 * @param generic_type_def 泛型类型定义句柄
 * @param type_args 类型参数数组(MonoType)
 * @return 实例化的泛型类型句柄
 * @note 如果在移动端中使用则一定要传入MonoType
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_type_make_generic_type, patch_handle_t generic_type_def,
                const tefstd_vector_t* type_args)

/**
 * @brief 获取类型的MonoType
 * @param type 类型句柄
 * @return MonoType句柄
 * @note 该函数用于移动端，桌面端返回原句柄
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_type_get_mono_type, patch_handle_t type)

// ==================== 类型信息查询 ====================
/**
 * @brief 获取类型的名称
 * @param type 类型句柄（必须有效）
 * @return 返回字符串
 */
DEFINE_FUNCTION(const char*, patchlib_type_get_name, patch_handle_t type)

/**
 * @brief 获取类型的命名空间
 * @param[in] type 类型句柄（必须有效）
 * @return 命名空间
 */
DEFINE_FUNCTION(const char*, patchlib_type_get_namespace, patch_handle_t type)

/**
 * @brief 获取类型的完整名称（命名空间 + 类名）
 * @param[in] type 类型句柄（必须有效）
 * @return 完整`名称
 * @warning 使用malloc分配
 */
DEFINE_FUNCTION(char*, patchlib_type_get_full_name, patch_handle_t type)

/**
 * @brief 获取指定类型的父类型
 * @param type 类型句柄，必须是通过 patchlib_type_get_type() 获取的有效句柄
 * @return 成功返回父类型句柄，可通过 patchlib_is_valid() 检查有效性；
 *         如果没有父类型（如System.Object）或发生错误时返回 PATCH_NULL
 * @note 此函数用于获取类型的直接基类，对于接口类型将返回NULL
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_type_get_parent, patch_handle_t type)


// ==================== 成员获取（单个） ====================

/**
 * @brief 获取内嵌类型(嵌套类型)
 * @param parent 父类型句柄(必须有效)
 * @param name 内嵌类型名称
 * @return 成功返回有效句柄，失败返回PATCH_NULL
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_type_get_inner_type, patch_handle_t parent, const char *name)

/**
 * @brief 根据名称获取字段
 * @param type 类型句柄(必须有效)
 * @param name 字段名称
 * @return 成功返回字段句柄，失败返回PATCH_NULL
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_type_get_field, patch_handle_t type, const char *name)

/**
 * @brief 根据名称获取属性
 * @param type 类型句柄(必须有效)
 * @param name 属性名称
 * @return 成功返回属性句柄，失败返回PATCH_NULL
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_type_get_property, patch_handle_t type, const char *name)

/**
* @brief 根据名称获取函数
* @param type 类型句柄(必须有效)
* @param name 函数名称
* @return 成功返回函数句柄，失败返回PATCH_NULL
*/
DEFINE_FUNCTION(patch_handle_t, patchlib_type_get_method, patch_handle_t type, const char *name)

/**
* @brief 根据名称和参数数量获取函数
* @param type 类型句柄(必须有效)
* @param name 函数名称
* @param args_count 参数数量
* @return 成功返回函数句柄，失败返回PATCH_NULL
*/
DEFINE_FUNCTION(patch_handle_t, patchlib_type_get_method_by_param_count, patch_handle_t type, const char *name,
                int args_count)

/**
* @brief 根据名称和参数名称获取函数
* @param type 类型句柄(必须有效)
* @param name 函数名称
* @param args_count 参数数量
* @param args_names 参数名称
* @return 成功返回函数句柄，失败返回PATCH_NULL
*/
DEFINE_FUNCTION(patch_handle_t, patchlib_type_get_method_by_param_names, patch_handle_t type, const char *name,
                int args_count, const char **args_names)

/**
* @brief 根据名称和参数类型获取函数
* @param type 类型句柄(必须有效)
* @param name 函数名称
* @param args_count 参数数量
* @param args_types 参数类型(通过patchlib_type_get_type获取)
* @return 成功返回函数句柄，失败返回PATCH_NULL
*/
DEFINE_FUNCTION(patch_handle_t, patchlib_type_get_method_by_param_types, patch_handle_t type, const char *name,
                int args_count, const patch_handle_t* args_types)

/**
* @brief 根据函数签名获取函数
* @param type 类型句柄(必须有效)
* @param name 函数名称
* @param args_count 参数数量
* @param args_types 参数类型(通过patchlib_type_get_type获取)
* @param args_names 参数名称
* @return 成功返回函数句柄，失败返回PATCH_NULL
*/
DEFINE_FUNCTION(patch_handle_t, patchlib_type_get_method_by_signature, patch_handle_t type, const char *name,
                int args_count, const patch_handle_t *args_types, const char **args_names)

// ==================== 成员数组（批量获取） ====================
/**
 * @brief 获取类型的所有嵌套类型（数组版本）
 * @param type 类型句柄
 * @param including_parent 包括父类中的类型
 * @param[out] array 返回的数组
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_type_get_inner_types, patch_handle_t type, bool including_parent, tefstd_vector_t* array)

/**
 * @brief 获取类型的所有方法（数组版本）
 * @param type 类型句柄
 * @param including_parent 包括父类中的类型
 * @param[out] array 返回的数组
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_type_get_methods, patch_handle_t type, bool including_parent, tefstd_vector_t* array)

/**
 * @brief 获取类型的所有字段（数组版本）
 * @param type 类型句柄
 * @param including_parent 包括父类中的类型
 * @param[out] array 返回的数组
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_type_get_fields, patch_handle_t type, bool including_parent, tefstd_vector_t* array)

/**
 * @brief 获取类型的所有属性（数组版本）
 * @param type 类型句柄
 * @param including_parent 包括父类中的类型
 * @param[out] array 返回的数组
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_type_get_properties, patch_handle_t type, bool including_parent, tefstd_vector_t* array)

// ==================== 资源释放 ====================
/**
 * @brief 释放类型相关资源
 * @param type 要释放的类型句柄(可以为无效句柄)
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_type_free, patch_handle_t type)

/**
 * @brief 释放对象
 * @param object 对象句柄
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_object_free, patch_handle_t object)

/**
 * @brief 持久化保存对象
 * @param object 临时对象句柄
 * @return 持久化后的对象句柄
 * @note 用于将hook等场景中的临时对象持久化保存，
 *       避免函数结束后对象被自动卸载
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_object_persist, patch_handle_t object);

#if __ANDROID__

#    define patchlib_type_free(handle) ((void)0)
#    define patchlib_object_free(handle) ((void)0)
#    define patchlib_object_persist(handle) handle

#endif

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_TYPE_H
