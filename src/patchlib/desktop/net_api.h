/*******************************************************************************
 * tefkernel - net_api
 * Copyright (C) 2025 eternalfuture-e38299
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2025/11/23
 *******************************************************************************/

#ifndef TEFKERNEL_NET_API_H
#define TEFKERNEL_NET_API_H

#include <stddef.h>
#include <stdint.h>

#include "patchlib/type.h"

#ifdef _WIN32
#define SYM_EXPORT __declspec(dllexport)
#else
#define SYM_EXPORT __attribute__((visibility("default")))
#endif

#ifdef _WIN32
#define NETAPI_CALL __cdecl
#else
#define NETAPI_CALL
#endif

// 修改宏，只在没有定义时定义
#ifndef DEFINE_NETAPI_FUNCTIONS
#define DEFINE_NETAPI_FUNCTION(ret, name, ...) \
extern SYM_EXPORT ret (NETAPI_CALL *name)(__VA_ARGS__);
#else
#define DEFINE_NETAPI_FUNCTION(ret, name, ...) \
SYM_EXPORT ret (NETAPI_CALL *name)(__VA_ARGS__) = NULL;
#endif

#ifdef __cplusplus
extern "C" {



#endif

/**
 * @brief 获取类型
 * @param ns 命名空间
 * @param name 名称
 * @return 返回句柄
 */
DEFINE_NETAPI_FUNCTION(patch_handle_t, net_get_type, const char* ns, const char* name);

/**
 * @brief 创建实例
 * @param type 实例
 * @return 返回实例
 */
DEFINE_NETAPI_FUNCTION(patch_handle_t, net_new_instance, patch_handle_t type);

/**
 * @brief 构造类型泛型
 * @param type 类型句柄
 * @param generic_types 泛型
 * @param types_size 泛型大小
 * @return 泛型类型句柄
 */
DEFINE_NETAPI_FUNCTION(patch_handle_t, net_type_make_generic, patch_handle_t type, patch_handle_t generic_types[],
                       size_t types_size);

/**
 * @brief 获取类型名称
 * @param type 类型句柄
 * @return 类型名称
 */
DEFINE_NETAPI_FUNCTION(const char*, net_type_get_name, patch_handle_t type)


/**
 * @brief 获取类型命名空间
 * @param type 类型句柄
 * @return 类型命名空间
 */
DEFINE_NETAPI_FUNCTION(const char*, net_type_get_namespace, patch_handle_t type)

/**
 * @brief 获取父类型
 * @param type 类型句柄
 * @return 类型句柄
 */
DEFINE_NETAPI_FUNCTION(patch_handle_t, net_type_get_parent, patch_handle_t type)

/**
 * @brief 获取字段
 * @param type 类型句柄
 * @param name 字段名称
 * @return 字段句柄
 */
DEFINE_NETAPI_FUNCTION(patch_handle_t, net_type_get_field, patch_handle_t type, const char* name)


/**
 * @brief 获取属性
 * @param type 类型句柄
 * @param name 属性名称
 * @return 属性句柄
 */
DEFINE_NETAPI_FUNCTION(patch_handle_t, net_type_get_property, patch_handle_t type, const char* name)

/**
 * @brief 获取函数(通过名称与参数数量)
 * @param type 类型句柄
 * @param name 函数名称
 * @param args_count 参数数量
 * @return 函数句柄
 */
DEFINE_NETAPI_FUNCTION(patch_handle_t, net_type_get_method_from_args_count, patch_handle_t type, const char* name,
                       size_t args_count)

/**
 * @brief 将类型转换为patch_type_t
 * @param type 类型句柄
 * @return 转换结果
 */
DEFINE_NETAPI_FUNCTION(patch_type_t, net_type_to_patchlib_type, patch_handle_t type);

/**
 * @brief 获取类型中的所有内部类型
 * @param type 类型句柄
 * @param including_parent 是否包含父类型
 * @param out_array[out] 获取的数组
 * @param count[out] 数组大小
 * @return 执行结果
 * @warning 使用malloc分配
 */
DEFINE_NETAPI_FUNCTION(bool, net_type_get_inner_types, patch_handle_t type, bool including_parent,
                       patch_handle_t** out_array,
                       int* count)

/**
 * @brief 获取类型中的所有函数
 * @param type 类型句柄
 * @param including_parent 是否包含父类型
 * @param out_array[out] 获取的数组
 * @param count[out] 数组大小
 * @return 执行结果
 * @warning 使用malloc分配
 */
DEFINE_NETAPI_FUNCTION(bool, net_type_get_methods, patch_handle_t type, bool including_parent,
                       patch_handle_t** out_array,
                       int* count)

/**
 * @brief 获取类型中的所有字段
 * @param type 类型句柄
 * @param including_parent 是否包含父类型
 * @param out_array[out] 获取的数组
 * @param count[out] 数组大小
 * @return 执行结果
 * @warning 使用malloc分配
 */
DEFINE_NETAPI_FUNCTION(bool, net_type_get_fields, patch_handle_t type, bool including_parent,
                       patch_handle_t** out_array,
                       int* count)

/**
 * @brief 获取类型中的所有函数
 * @param type 类型句柄
 * @param including_parent 是否包含父类型
 * @param out_array[out] 获取的数组
 * @param count[out] 数组大小
 * @return 执行结果
 * @warning 使用malloc分配
 */
DEFINE_NETAPI_FUNCTION(bool, net_type_get_properties, patch_handle_t type, bool including_parent,
                       patch_handle_t** out_array,
                       int* count)

/**
 * @brief 卸载类型句柄
 * @param type 类型句柄
 * @return 执行结果
 */
DEFINE_NETAPI_FUNCTION(bool, net_type_free, patch_handle_t type);

/**
 * @brief 获取字段名称
 * @param field 字段句柄
 * @return 字段名称
 */
DEFINE_NETAPI_FUNCTION(const char*, net_field_get_name, patch_handle_t field);

/**
 * @brief 判断字段是否静态
 * @param field 字段句柄
 * @return 判断结果
 */
DEFINE_NETAPI_FUNCTION(bool, net_field_is_static, patch_handle_t field);

/**
 * @brief 判断字段是否静态
 * @param field 字段句柄
 * @return 判断结果
 */
DEFINE_NETAPI_FUNCTION(bool, net_field_is_const, patch_handle_t field);

/**
 * @brief 判断字段是否静态
 * @param field 字段句柄
 * @return 判断结果
 */
DEFINE_NETAPI_FUNCTION(bool, net_field_is_thread_static, patch_handle_t field);

/**
 * @brief 获取字段值
 * @param field 字段句柄
 * @param instance 实例句柄
 * @param value[out] 输出值
 * @return 执行结果
 */
DEFINE_NETAPI_FUNCTION(bool, net_field_get_value, patch_handle_t field, patch_handle_t instance, void *value);

/**
 * @brief 设置字段值
 * @param field 字段句柄
 * @param instance 实例句柄
 * @param value[in] 输入值
 * @return 执行结果
 */
DEFINE_NETAPI_FUNCTION(bool, net_field_set_value, patch_handle_t field, patch_handle_t instance, void *value);


/**
 * @brief 卸载字段句柄
 * @param field 字段句柄
 */
DEFINE_NETAPI_FUNCTION(void, net_field_free, patch_handle_t field);

/**
 * @brief 获取属性名称
 * @param property 属性句柄
 * @return 属性名称
 */
DEFINE_NETAPI_FUNCTION(const char*, net_property_get_name, patch_handle_t property);

/**
 * @brief 获取属性的Get函数
 * @param property 属性句柄
 * @return 函数句柄
 */
DEFINE_NETAPI_FUNCTION(patch_handle_t, net_property_get_get_method, patch_handle_t property);

/**
 * @brief 获取属性Set函数
 * @param property 属性句柄
 * @return 函数句柄
 */
DEFINE_NETAPI_FUNCTION(patch_handle_t, net_property_get_set_method, patch_handle_t property);

/**
 * @brief 卸载属性
 * @param property 属性句柄
 */
DEFINE_NETAPI_FUNCTION(void, net_property_free, patch_handle_t property);

/**
 * @brief 获取函数名称
 * @param method 函数句柄
 * @return 函数名称
 */
DEFINE_NETAPI_FUNCTION(const char*, net_method_get_name, patch_handle_t method);

/**
 * @brief 获取函数参数数量
 * @param method 函数句柄
 * @return 参数数量
 */
DEFINE_NETAPI_FUNCTION(int, net_method_get_param_count, patch_handle_t method);

/**
 * @brief 判断函数是否为实例函数
 * @param method 函数句柄
 * @return 判断结果
 */
DEFINE_NETAPI_FUNCTION(bool, net_method_is_instance, patch_handle_t method);

/**
 * @brief 构造泛型函数
 * @param method 函数句柄
 * @param template_types 类型句柄数组
 * @param types_size 类型句柄数组大小
 * @return 泛型函数句柄
 */
DEFINE_NETAPI_FUNCTION(patch_handle_t, net_method_make_generic_method, patch_handle_t method,
                       patch_handle_t template_types[], int types_size);

/**
 * @brief 获取函数返回类型
 * @param method 函数句柄
 * @return 类型句柄
 */
DEFINE_NETAPI_FUNCTION(patch_handle_t, net_method_get_return_type, patch_handle_t method);

/**
 * @brief 获取函数参数类型
 * @param method 函数句柄
 * @param index 引索
 * @return 类型句柄
 */
DEFINE_NETAPI_FUNCTION(patch_handle_t, net_method_get_param, patch_handle_t method, int index);

/**
 * @brief 获取函数参数名称
 * @param method 函数句柄
 * @param index 引索
 * @return 参数名称
 */
DEFINE_NETAPI_FUNCTION(const char*, net_method_get_param_name, patch_handle_t method, int index);

/**
 * @brief 调用函数
 * @param method 函数句柄
 * @param instance 实例句柄
 * @param arg_count 参数数量
 * @param return_value[out] 返回值
 * @param args 参数指针
 * @param types 参数类型
 * @return 执行结果
 */
DEFINE_NETAPI_FUNCTION(bool, net_method_invoke, patch_handle_t method, patch_handle_t instance, int arg_count,
                       void* return_value, void* args[], patch_type_t types[]);

/**
 * @brief Hook一个方法
 * @param method 方法句柄
 * @param method_signature 方法签名信息
 * @param prefix_hook 前缀Hook回调
 * @param postfix_hook 后缀Hook回调
 * @return Hook节点ID，0表示失败
 */
DEFINE_NETAPI_FUNCTION(int, net_hook_method, patch_handle_t method, void* method_signature,
                       void* prefix_hook,
                       void* postfix_hook);

/**
 * @brief 卸载Hook
 * @param node_index Hook节点ID
 * @return 是否成功
 */
DEFINE_NETAPI_FUNCTION(bool, net_unhook_method, uint16_t node_index);

/**
 * @brief 检查方法是否只有一个Hook节点
 * @param method 方法句柄
 * @return 是否只有一个Hook节点
 */
DEFINE_NETAPI_FUNCTION(bool, net_has_single_hook_node, patch_handle_t method);

/**
 * @brief 通过hook节点获取函数句柄
 * @param id hook节点
 * @return 函数句柄
 */
DEFINE_NETAPI_FUNCTION(patch_handle_t, net_get_method_by_hook_node, uint16_t id);

/**
 * @brief 检查方法是否已被Hook
 * @param method 方法句柄
 * @return 是否已被Hook
 */
DEFINE_NETAPI_FUNCTION(bool, net_is_method_hooked, patch_handle_t method);

/**
 * @brief 获取已hook函数的签名指针
 * @param method 函数句柄
 * @return 签名指针
 */
DEFINE_NETAPI_FUNCTION(void*, net_get_hooked_method_sig, patch_handle_t method);

/**
 * @brief 卸载函数句柄
 * @param method 函数句柄
 */
DEFINE_NETAPI_FUNCTION(void, net_method_free, patch_handle_t method);

#ifdef __cplusplus
}
#endif

#endif //TEFKERNEL_NET_API_H
