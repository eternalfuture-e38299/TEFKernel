/*******************************************************************************
 * tefkernel - il2cpp_api
 * Copyright (C) 2026 eternalfuture-e38299
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
 * Created: 2026/5/23
 *******************************************************************************/

#ifndef TEFKERNEL_IL2CPP_API_H
#define TEFKERNEL_IL2CPP_API_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {

#endif

#if defined(__ANDROID__)
void il2cpp_api_init(void *handle);
#endif

typedef enum il2cpp_type_enum_t {
    IL2CPP_TYPE_END = 0x00,
    IL2CPP_TYPE_VOID = 0x01,
    IL2CPP_TYPE_BOOLEAN = 0x02,
    IL2CPP_TYPE_CHAR = 0x03,
    IL2CPP_TYPE_I1 = 0x04,
    IL2CPP_TYPE_U1 = 0x05,
    IL2CPP_TYPE_I2 = 0x06,
    IL2CPP_TYPE_U2 = 0x07,
    IL2CPP_TYPE_I4 = 0x08,
    IL2CPP_TYPE_U4 = 0x09,
    IL2CPP_TYPE_I8 = 0x0a,
    IL2CPP_TYPE_U8 = 0x0b,
    IL2CPP_TYPE_R4 = 0x0c,
    IL2CPP_TYPE_R8 = 0x0d,
    IL2CPP_TYPE_STRING = 0x0e,
    IL2CPP_TYPE_PTR = 0x0f,
    IL2CPP_TYPE_BYREF = 0x10,
    IL2CPP_TYPE_VALUETYPE = 0x11,
    IL2CPP_TYPE_CLASS = 0x12,
    IL2CPP_TYPE_VAR = 0x13,
    IL2CPP_TYPE_ARRAY = 0x14,
    IL2CPP_TYPE_GENERICINST = 0x15,
    IL2CPP_TYPE_TYPEDBYREF = 0x16,
    IL2CPP_TYPE_I = 0x18,
    IL2CPP_TYPE_U = 0x19,
    IL2CPP_TYPE_FNPTR = 0x1b,
    IL2CPP_TYPE_OBJECT = 0x1c,
    IL2CPP_TYPE_SZARRAY = 0x1d,
    IL2CPP_TYPE_MVAR = 0x1e,
    IL2CPP_TYPE_CMOD_REQD = 0x1f,
    IL2CPP_TYPE_CMOD_OPT = 0x20,
    IL2CPP_TYPE_INTERNAL = 0x21,
    IL2CPP_TYPE_MODIFIER = 0x40,
    IL2CPP_TYPE_SENTINEL = 0x41,
    IL2CPP_TYPE_PINNED = 0x45,
    IL2CPP_TYPE_ENUM = 0x55
} il2cpp_type_enum_t;

#if defined(__ANDROID__)
/**
 * @brief 从对象获取类
 * @param object 对象
 * @return 返回结果
 */
void *il2cpp_object_get_class(void *object);
#else
#define il2cpp_object_get_class(object) object
#endif

#ifndef IL2CPP_API_IMPL
#define IL2CPP_API_IMPL 0
#endif

// 方便识别的标记宏 (可选)
#define IL2CPP_API_DECLARE

// 优化后的宏定义
#if IL2CPP_API_IMPL
// 定义模式：直接定义变量
#define DEFINE_IL2CPP_API(ret, name, ...) \
ret (*name)(__VA_ARGS__) = NULL;
#else
// 声明模式：使用 extern 声明
#define DEFINE_IL2CPP_API(ret, name, ...) \
extern ret (*name)(__VA_ARGS__);
#endif

#if defined(__ANDROID__)
/**
 * @brief 初始化IL2CPP运行时
 * @param domain_name 域名
 * @return 初始化状态
 */
DEFINE_IL2CPP_API(int, il2cpp_init, const char* domain_name)
#endif

/**
 * @brief 获取当前应用域
 * @return 应用域指针(原始类型: Il2CppDomain*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_domain_get)

/**
 * @brief 获取域中所有程序集
 * @param domain 应用域指针(原始类型: const Il2CppDomain*)
 * @param size [out] 程序集数量
 * @return 程序集指针数组(原始类型: const Il2CppAssembly**)
 */
DEFINE_IL2CPP_API(void**, il2cpp_domain_get_assemblies,
                  const void* domain, size_t* size)

#if defined(__ANDROID__)
/**
 * @brief 获取程序集对应的镜像
 * @param assembly 程序集指针(原始类型: const Il2CppAssembly*)
 * @return 镜像指针(原始类型: const Il2CppImage*)
 */
DEFINE_IL2CPP_API(const void*, il2cpp_assembly_get_image, const void* assembly)
#else
#define il2cpp_assembly_get_image(assembly) assembly
#endif

#if  defined(__ANDROID__)
#define il2cpp_free(obj) (void)0
#else
DEFINE_IL2CPP_API(void, il2cpp_free, void* obj)
#endif

#if  !defined(__ANDROID)
DEFINE_IL2CPP_API(void*, il2cpp_object_copy, void* obj)
#endif


/**
 * @brief 通过名称获取类
 * @param image 程序集镜像(原始类型: const Il2CppImage*)
 * @param namespaze 命名空间
 * @param name 类名
 * @return 类指针(原始类型: Il2CppClass*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_from_name,
                  const void* image, const char* namespaze, const char *name)

#if defined(__ANDROID__)
/**
 * @brief 获取嵌套类型(迭代器方式)
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @param iter 迭代器指针(必须初始化为nullptr)
 * @return 嵌套类指针(原始类型: Il2CppClass*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_get_nested_types,
                  void* klass, void** iter)

/**
 * @brief 获取类的方法(迭代器方式)
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @param iter 迭代器指针(必须初始化为nullptr)
 * @return 方法信息(原始类型: const MethodInfo*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_get_methods,
                  void* klass, void** iter)

/**
 * @brief 获取类的属性(迭代器方式)
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @param iter 迭代器指针(必须初始化为nullptr)
 * @return 属性信息(原始类型: const PropertyInfo*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_get_properties,
                  void* klass, void** iter)

/**
 * @brief 获取类的字段(迭代器方式)
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @param iter 迭代器指针(必须初始化为nullptr)
 * @return 字段信息(原始类型: FieldInfo*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_get_fields,
                  void* klass, void** iter)

#else
/**
 * @brief 获取嵌套类型(非迭代器方式)
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @param size[out] 输出嵌套类型数量
 * @return 嵌套类数组(原始类型: Il2CppClass**)
 */
DEFINE_IL2CPP_API(void**, il2cpp_class_get_nested_types,
                  void* klass, int* size)

/**
 * @brief 获取类的方法(非迭代器方式)
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @param size[out] 输出方法数量
 * @return 方法数组(原始类型: const MethodInfo**)
 */
DEFINE_IL2CPP_API(void**, il2cpp_class_get_methods,
                  void* klass, int* size)

/**
 * @brief 获取类的属性(非迭代器方式)
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @param size[out] 输出属性数量
 * @return 属性数组(原始类型: const PropertyInfo**)
 */
DEFINE_IL2CPP_API(void**, il2cpp_class_get_properties,
                  void* klass, int* size)

/**
 * @brief 获取类的字段(非迭代器方式)
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @param size[out] 输出字段数量
 * @return 字段数组(原始类型: FieldInfo**)
 */
DEFINE_IL2CPP_API(void**, il2cpp_class_get_fields,
                  void* klass, int* size)

#endif

/**
 * @brief 获取类名
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @return 类名字符串
 */
DEFINE_IL2CPP_API(const char*, il2cpp_class_get_name, void* klass)


/**
 * @brief 获取父类
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @return 父类指针(原始类型: Il2CppClass*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_get_parent, void* klass)


/**
 * @brief 创建新对象
 * @param klass 类指针(原始类型: const Il2CppClass*)
 * @return 对象指针(原始类型: Il2CppObject*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_object_new, const void* klass)

/**
 * @brief 获取字段名称
 * @param field 字段信息指针(原始类型: FieldInfo*)
 * @return 字段名称字符串
 */
DEFINE_IL2CPP_API(const char*, il2cpp_field_get_name, void* field)

#if  defined(__ANDROID__)
/**
 * @brief 获取静态字段数据
 * @param klass 类指针(原始类型: const Il2CppClass*)
 * @return 静态数据指针
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_get_static_field_data, const void* klass)
#endif

/**
 * @brief 获取字段所属类
 * @param field 字段信息指针(原始类型: FieldInfo*)
 * @return 所属类指针(原始类型: Il2CppClass*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_field_get_parent, void* field)

#if  defined(__ANDROID__)
/**
 * @brief 获取字段偏移量
 * @param field 字段信息指针(原始类型: FieldInfo*)
 * @return 字段偏移量(字节)
 */
DEFINE_IL2CPP_API(size_t, il2cpp_field_get_offset, void* field)
#endif

/**
 * @brief 获取类型种类
 * @param type 类型指针(原始类型: const Il2CppType*)
 * @return 类型枚举值
 */
DEFINE_IL2CPP_API(int, il2cpp_type_get_type, const void* type)

/**
 * @brief 获取命名空间
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @return 命名空间字符串
 */
DEFINE_IL2CPP_API(const char*, il2cpp_class_get_namespace, void* klass)

/**
 * @brief 获取字段类型
 * @param field 字段信息指针(原始类型: FieldInfo*)
 * @return 类型指针(原始类型: const Il2CppType*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_field_get_type, void* field)

/**
 * @brief 获取属性名称
 * @param prop 属性信息指针(原始类型: PropertyInfo*)
 * @return 属性名称字符串
 */
DEFINE_IL2CPP_API(const char*, il2cpp_property_get_name, void* prop)

/**
 * @brief 获取方法名称
 * @param method 方法信息指针(原始类型: const MethodInfo*)
 * @return 方法名称字符串
 */
DEFINE_IL2CPP_API(const char*, il2cpp_method_get_name, const void* method)

/**
 * @brief 获取方法参数数量
 * @param method 方法信息指针(原始类型: const MethodInfo*)
 * @return 参数数量
 */
DEFINE_IL2CPP_API(uint32_t, il2cpp_method_get_param_count, const void* method)


/**
 * @brief 获取方法参数名称
 * @param method 方法信息指针(原始类型: const MethodInfo*)
 * @param index 参数索引
 * @return 参数名称字符串
 */
DEFINE_IL2CPP_API(const char*, il2cpp_method_get_param_name, const void* method, uint32_t index)

/**
 * @brief 获取方法参数类型
 * @param method 方法信息指针(原始类型: const MethodInfo*)
 * @param index 参数索引
 * @return 类型指针(原始类型: const Il2CppType*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_method_get_param, const void* method, uint32_t index)

#if defined(__ANDROID__)
/**
 * @brief 获取类的类型
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @return 类型指针(原始类型: const Il2CppType*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_get_type, void* klass)
#else
#define il2cpp_class_get_type(klass) klass
#endif

#if defined(__ANDROID__)
/**
 * @brief 从类型获取类
 * @param type 类型指针(原始类型: const Il2CppType*)
 * @return 类指针(原始类型: Il2CppClass*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_from_type, const void* type)
#else
#define il2cpp_class_from_type(type) type
#endif

#if !defined(__ANDROID__)
DEFINE_IL2CPP_API(void*, il2cpp_class_make_generic, void* type, void* types, int types_count)
#endif

#if !defined(__ANDROID__)
DEFINE_IL2CPP_API(bool, il2cpp_class_is_same, void* c1, void* c2)
#endif

/**
 * @brief 检查是否是抽象类
 * @param klass 类指针(原始类型: const Il2CppClass*)
 * @return 是否为抽象类
 */
DEFINE_IL2CPP_API(bool, il2cpp_class_is_abstract, const void* klass)

/**
 * @brief 检查是否是接口
 * @param klass 类指针(原始类型: const Il2CppClass*)
 * @return 是否为接口
 */
DEFINE_IL2CPP_API(bool, il2cpp_class_is_interface, const void* klass)

/**
 * @brief 检查是否是枚举
 * @param klass 类指针(原始类型: const Il2CppClass*)
 * @return 是否为枚举
 */
DEFINE_IL2CPP_API(bool, il2cpp_class_is_enum, const void* klass)

/**
 * @brief 检查类是否是泛型类型
 * @param klass 类指针(原始类型: const Il2CppClass*)
 * @return 是否为泛型类型
 */
DEFINE_IL2CPP_API(bool, il2cpp_class_is_generic, const void* klass)

/**
 * @brief 获取类型的反射对象
 * @param type 类型指针(原始类型: const Il2CppType*)
 * @return 反射对象(原始类型: Il2CppObject*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_type_get_object, const void* type)


/**
 * @brief 获取属性的get方法
 * @param prop 属性信息指针(原始类型: PropertyInfo*)
 * @return 方法信息指针(原始类型: const MethodInfo*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_property_get_get_method, void* prop)

/**
 * @brief 获取属性的set方法
 * @param prop 属性信息指针(原始类型: PropertyInfo*)
 * @return 方法信息指针(原始类型: const MethodInfo*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_property_get_set_method, void* prop)

/**
 * @brief 检查方法是否为实例方法
 * @param method 方法信息指针(原始类型: const MethodInfo*)
 * @return 是否为实例方法
 */
DEFINE_IL2CPP_API(bool, il2cpp_method_is_instance, const void* method)


/**
 * @brief 检查方法是否为泛型方法
 * @param method 方法信息指针(原始类型: const MethodInfo*)
 * @return 是否为泛型方法
 */
DEFINE_IL2CPP_API(bool, il2cpp_method_is_generic, const void* method)

/**
 * @brief 获取核心库镜像
 * @return 核心库镜像指针(原始类型: const Il2CppImage*)
 */
DEFINE_IL2CPP_API(const void*, il2cpp_get_corlib)


/**
 * @brief 创建新数组
 * @param elementTypeInfo 元素类型信息(原始类型: Il2CppClass*)
 * @param length 数组长度
 * @return 数组对象(原始类型: Il2CppArray*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_array_new, void* elementTypeInfo, uintptr_t length)

#if !defined(__ANDROID__)
/**
 * @brief 获取数组元素（通过索引）
 * @param array 数组对象
 * @param index 索引
 * @param out_value 输出值指针
 * @return 执行结果
 */
DEFINE_IL2CPP_API(bool, il2cpp_array_at, void* array, size_t index, void* out_value)

/**
 * @brief 设置数组元素（通过索引）
 * @param array 数组对象
 * @param index 索引
 * @param new_value 新值指针
 * @return 执行结果
 */
DEFINE_IL2CPP_API(bool, il2cpp_array_set, void* array, size_t index, void* new_value)

/**
 * @brief 填充数组所有元素
 * @param array 数组对象
 * @param value 填充值指针
 * @return 执行结果
 */
DEFINE_IL2CPP_API(bool, il2cpp_array_fill, void* array, void* value)

/**
 * @brief 从 C 数组复制到 IL2CPP 数组
 * @param dest IL2CPP 目标数组
 * @param src C 源数组指针
 * @param count 要复制的元素个数
 * @return 执行结果
 */
DEFINE_IL2CPP_API(bool, il2cpp_array_copy_from_c, void* dest, const void* src, size_t count)

/**
 * @brief 从 IL2CPP 数组复制到 C 数组
 * @param dest C 目标数组指针
 * @param src IL2CPP 源数组
 * @param count 要复制的元素个数
 * @return 执行结果
 */
DEFINE_IL2CPP_API(bool, il2cpp_array_copy_to_c, void* dest, void* src, size_t count)
#endif

/**
 * @brief 获取数组元素大小
 * @param array_class 数组类(原始类型: const Il2CppClass*)
 * @return 单个元素大小(字节)
 */
DEFINE_IL2CPP_API(int, il2cpp_array_element_size, const void* array_class)

/**
 * @brief 获取数组字节长度
 * @param array 数组对象(原始类型: Il2CppArray*)
 * @return 字节长度
 */
DEFINE_IL2CPP_API(uint32_t, il2cpp_array_get_byte_length, void* array)

/**
 * @brief 获取数组长度
 * @param array 数组对象(原始类型: Il2CppArray*)
 * @return 数组元素数量
 */
DEFINE_IL2CPP_API(uint32_t, il2cpp_array_length, void* array)

#if defined(__ANDROID__)
/**
 * @brief 从System.Type获取类
 * @param type 反射类型对象(原始类型: Il2CppReflectionType*)
 * @return 类指针(原始类型: Il2CppClass*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_from_system_type, void* type)
#else
#define il2cpp_class_from_system_type(type) type
#endif


/**
 * @brief 通过名称获取字段
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @param name 字段名
 * @return 字段信息(原始类型: FieldInfo*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_get_field_from_name,
                  void* klass, const char *name)

#if !defined(__ANDROID__)
DEFINE_IL2CPP_API(bool, il2cpp_field_is_static,
                  void* field)

DEFINE_IL2CPP_API(bool, il2cpp_field_is_thread_static,
                  void* field)

DEFINE_IL2CPP_API(bool, il2cpp_field_is_literal,
                  void* field)

DEFINE_IL2CPP_API(void, il2cpp_field_set_value, void* field, void* instance, void* value)

DEFINE_IL2CPP_API(void, il2cpp_field_get_value, void* field, void* instance, void* out_value)
#endif

/**
 * @brief 通过名称获取属性
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @param name 属性名
 * @return 属性信息(原始类型: const PropertyInfo*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_get_property_from_name,
                  void* klass, const char *name)

/**
 * @brief 通过名称获取方法
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @param name 方法名
 * @param argsCount 参数数量
 * @return 方法信息(原始类型: const MethodInfo*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_get_method_from_name,
                  void* klass, const char* name, int argsCount)

#if defined(__ANDROID__)
/**
 * @brief 从类型获取类
 * @param type 类型指针(原始类型: const Il2CppType*)
 * @return 类指针(原始类型: Il2CppClass*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_from_il2cpp_type, const void* type)
#else
#define il2cpp_class_from_il2cpp_type(type) type
#endif

/**
 * @brief 获取方法返回类型
 * @param method 方法信息指针(原始类型: const MethodInfo*)
 * @return 类型指针(原始类型: const Il2CppType*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_method_get_return_type, const void* method)

#if !defined(__ANDROID__)
/**
 * @brief 创建泛型方法实例
 * @param method 泛型方法定义指针(原始类型: const MethodInfo*)
 * @param types 泛型参数类型数组(原始类型: const Il2CppType**)
 * @param types_count 泛型参数数量
 * @return 实例化的泛型方法指针(原始类型: const MethodInfo*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_method_make_generic, void* method, void** types, int types_count)

/**
 * @brief 运行时方法调用
 * @param method 方法信息指针(原始类型: const MethodInfo*)
 * @param obj 对象实例
 * @param params 参数数组
 * @param returnValue 返回值输出指针
 * @return 调用成功返回 true，失败返回 false
 */
DEFINE_IL2CPP_API(bool, il2cpp_method_invoke, void* method, void* obj, void** params, void* returnValue)
#endif

/**
 * @brief 获取方法声明类
 * @param method 方法信息指针(原始类型: const MethodInfo*)
 * @return 类指针(原始类型: Il2CppClass*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_method_get_declaring_type, const void* method)

#if  defined(__ANDROID__)
/**
 * @brief 获取方法的反射对象
 * @param method 方法信息指针(原始类型: const MethodInfo*)
 * @param refclass 反射类(原始类型: Il2CppClass*)
 * @return 反射方法对象(原始类型: Il2CppReflectionMethod*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_method_get_object, const void* method, void* refclass)

/**
 * @brief 从反射方法获取方法信息
 * @param method 反射方法对象(原始类型: const Il2CppReflectionMethod*)
 * @return 方法信息指针(原始类型: const MethodInfo*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_method_get_from_reflection, const void* method)
#endif

/**
 * @brief 获取字符串长度
 * @param str 字符串对象(原始类型: Il2CppString*)
 * @return 字符串长度(字符数)
 */
DEFINE_IL2CPP_API(int32_t, il2cpp_string_length, void* str)

#if !defined(__ANDROID__)
/**
 * @brief 获取C风格字符串(非托管分配)
 * @param str 字符串对象
 * @return UTF-8编码的C字符串指针，需要手动释放
 */
DEFINE_IL2CPP_API(char*, il2cpp_string_cstr, void* str)
#endif

#if  defined(__ANDROID__)
/**
 * @brief 获取字符串字符数组
 * @param str 字符串对象(原始类型: Il2CppString*)
 * @return 字符数组指针(原始类型: Il2CppChar*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_string_chars, void* str)
#endif

/**
 * @brief 创建新字符串(UTF-8)
 * @param str UTF-8字符串
 * @return 字符串对象(原始类型: Il2CppString*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_string_new, const char* str)

/**
 * @brief 获取方法所属类
 * @param method 方法信息指针(原始类型: const MethodInfo*)
 * @return 类指针(原始类型: Il2CppClass*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_method_get_class, const void* method)

#if  defined(__ANDROID__)
/**
 * @brief 运行时方法调用
 * @param method 方法信息指针(原始类型: const MethodInfo*)
 * @param obj 对象实例
 * @param params 参数数组
 * @param exc [out] 异常对象(原始类型: Il2CppException**)
 * @return 调用结果(原始类型: Il2CppObject*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_runtime_invoke,
                  void* method, void* obj, void** params, void** exc)

/**
 * @brief 拆箱对象
 * @param obj 对象指针(原始类型: Il2CppObject*)
 * @return 值类型数据指针
 */
DEFINE_IL2CPP_API(void*, il2cpp_object_unbox, void* obj)
#endif

/**
 * @brief 获取方法Token
 * @param method 方法信息指针(原始类型: const MethodInfo*)
 * @return 方法Token
 */
DEFINE_IL2CPP_API(uint32_t, il2cpp_method_get_token, const void* method)

#if  defined(__ANDROID__)
/**
 * @brief 获取静态字段值
 * @param field 字段信息指针(原始类型: FieldInfo*)
 * @param value [out] 输出值缓冲区
 */
DEFINE_IL2CPP_API(void, il2cpp_field_static_get_value,
                  void* field, void* value)
#endif


// ==================== Hook API (仅非 Android 平台) ====================
#if !defined(__ANDROID__)

/**
 * @brief 安装 Hook
 * @param method 方法句柄
 * @param methodSignature 方法签名指针
 * @param prefix 前置回调
 * @param postfix 后置回调
 * @return Hook ID，失败返回 0
 */
DEFINE_IL2CPP_API(short, il2cpp_hook_method,
                  void* method, void* methodSignature,
                  void* prefix,
                  void* postfix)

/**
 * @brief 卸载 Hook
 * @param hookId Hook ID
 * @return 成功返回 true
 */
DEFINE_IL2CPP_API(bool, il2cpp_unhook_method, short hookId)

/**
 * @brief 检查方法是否已被 Hook
 * @param method 方法句柄
 * @return 已 Hook 返回 true
 */
DEFINE_IL2CPP_API(bool, il2cpp_is_method_hooked, void* method)

/**
 * @brief 检查方法是否只有一个 Hook 节点
 * @param method 方法句柄
 * @return 只有一个节点返回 true
 */
DEFINE_IL2CPP_API(bool, il2cpp_has_single_hook_node, void* method)

/**
 * @brief 获取被 Hook 方法的签名
 * @param method 方法句柄
 * @return 方法签名指针
 */
DEFINE_IL2CPP_API(void*, il2cpp_get_hooked_method_sig, void* method)

/**
 * @brief 通过 Hook 节点获取方法句柄
 * @param hookId Hook ID
 * @return 方法句柄，失败返回 -1
 */
DEFINE_IL2CPP_API(void*, il2cpp_get_method_by_hook_node, short hookId)

#endif // !defined(__ANDROID__)

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_IL2CPP_API_H
