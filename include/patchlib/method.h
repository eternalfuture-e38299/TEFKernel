/*******************************************************************************
 * File: method
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

#ifndef TEFKERNEL_METHOD_H
#define TEFKERNEL_METHOD_H

#include <stdint.h>

#include "type.h"

#ifdef __cplusplus
extern "C" {



#endif

// ==================== 方法基本信息获取 ====================
/**
 * @brief 获取方法名称
 * @param method 方法句柄(必须有效)
 * @return 函数名称
 */
DEFINE_FUNCTION(const char*, patchlib_method_get_name, patch_handle_t method)

/**
 * @brief 获取方法的返回类型
 * @param method 方法句柄(必须有效)
 * @return 返回类型句柄
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_method_get_return_type, patch_handle_t method)

/**
 * @brief 获取方法的参数数量
 * @param method 方法句柄(必须有效)
 * @return 参数数量
 */
DEFINE_FUNCTION(int, patchlib_method_get_param_count, patch_handle_t method)

/**
 * @brief 获取方法的参数类型
 * @param method 方法句柄(必须有效)
 * @param index 参数索引(0-based)
 * @return 参数类型句柄
 */
DEFINE_FUNCTION(patch_handle_t, patchlib_method_get_param_type, patch_handle_t method, int index)

// ==================== 方法特征检查 ====================
/**
 * @brief 检查方法是否为实例方法
 * @param method 方法句柄（必须为有效句柄）
 * @return true表示为实例方法，false表示非实例方法
 */
DEFINE_FUNCTION(bool, patchlib_method_is_instance, patch_handle_t method)

/**
 * @brief 检查方法是否为静态方法
 * @param method 方法句柄（必须为有效句柄）
 * @return true表示为静态方法，false表示实例方法
 */
DEFINE_FUNCTION(bool, patchlib_method_is_static, patch_handle_t method)

// ==================== 泛型方法操作 ====================
/**
* @brief 获取方法的泛型实例化方法
* @param method 基方法句柄（必须有效且为泛型类型定义）
* @param template_types 模板参数类型列表（元素必须为有效类型句柄，且为MonoType）
* @return 成功返回实例化的泛型方法句柄，失败返回PATCH_NULL
* @note 如果在移动端中使用则一定要传入MonoType
*/
DEFINE_FUNCTION(patch_handle_t, patchlib_method_make_generic_instance, patch_handle_t method,
                const tef_vector_t *template_types)

// ==================== 方法调用操作 ====================
#if __ANDROID__
/**
 * @brief 获取函数指针(仅Android，IOS)
 * @param method 函数句柄(必须有效)
 * @return 成功返回函数指针，否则返回NULL
 */
DEFINE_FUNCTION(void *, patchlib_method_get_pointer, patch_handle_t method)

#else
/**
 * @brief 调用函数(桌面端)
 * @param method 函数句柄(可为无效)
 * @param instance 实例对象(静态函数为PATCH_NULL)
 * @param return_value [out] 输出值缓冲区
 * @param args_types 参数类型
 * @param args_count 参数数量
 * @param ... 实际参数
 */
DEFINE_FUNCTION(void, patchlib_method_invoke, patch_handle_t method, patch_handle_t instance,
                void *return_value, const patch_type_t* args_types, int args_count, ...)
#endif

// ==================== 高级 ====================

typedef uint16_t patch_hook_id_t;
#define PATCH_HOOK_INVALID_ID 0 // 无效 ID 的定义

typedef struct patch_method_signature_t {
    bool is_instance;               ///< 是否为实例函数
    patch_type_t return_type;       ///< 返回类型
    tef_vector_t arg_types;         ///< patch_type_t，参数类型
} patch_method_signature_t;

/**
 * @brief 获取函数签名
 * @param method 函数句柄
 * @param signature[out] 函数输出签名
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_method_get_signature, patch_handle_t method, patch_method_signature_t* signature)


typedef void (*prefix_callback_t)(patch_handle_t orig_func, patch_handle_t instance, void** args, const patch_method_signature_t* sig_info);
typedef void (*postfix_callback_t)(patch_handle_t orig_func, patch_handle_t instance, void** args, void* result, const patch_method_signature_t* sig_info);

/**
 * @brief 安装前缀和后缀 Hook (Prefix/Postfix Hook)
 *
 * 此函数允许你在目标函数 `method` 执行前后插入自定义逻辑。
 * - Prefix Hook (前缀 Hook) 在目标函数执行*之前*运行。
 * - Postfix Hook (后缀 Hook) 在目标函数执行*之后*运行，并可以访问函数的返回值。
 * 可以为同一个 `method` 多次调用此函数来安装多组不同的 Pre/Post Hook。
 *
 * @param method        目标函数的句柄 (patch_handle_t)，用于标识要被 Hook 的函数。不可为空。
 * @param prefix        指向 Prefix Hook 函数的指针。该函数将在目标函数执行前被调用。
 *                      函数签名应为: void prefix(patch_handle_t orig_func, patch_handle_t instance, void** args, const patch_method_signature_t* sig_info)
 *                      - orig_func: 被 Hook 的原始函数句柄。
 *                      - instance: 对象实例指针（如果是成员函数），可能为空。
 *                      - args: 指向函数参数数组的指针（可能为空）。
 *                      - sig_info: 指向描述被 Hook 函数签名的 `patch_method_signature_t` 结构的指针。
 *                                  Hook 函数可以据此了解参数和返回值的类型信息。
 *                      如果不需要 Prefix Hook，可以传入 NULL。
 * @param postfix       指向 Postfix Hook 函数的指针。该函数将在目标函数执行后被调用。
 *                      函数签名应为: void postfix(patch_handle_t orig_func, patch_handle_t instance, void** args, void* result, const patch_method_signature_t* sig_info)
 *                      - orig_func: 被 Hook 的原始函数句柄。
 *                      - instance: 对象实例指针（如果是成员函数），可能为空。
 *                      - args: 指向函数参数数组的指针（可能为空）。
 *                      - result: 目标函数的返回值指针。如果目标函数返回 void 或 Hook 不关心返回值，则可能为空。
 *                      - sig_info: 指向描述被 Hook 函数签名的 `patch_method_signature_t` 结构的指针。
 *                                  Hook 函数可以据此了解参数和返回值的类型信息。
 *                      如果不需要 Postfix Hook，可以传入 NULL。
 * @return              如果 Hook 安装成功，则返回一个唯一的 `patch_hook_id_t` 用于后续卸载。
 *                      如果安装失败（例如 method 无效，或 prefix/postfix 都为 NULL），则返回 `PATCH_HOOK_INVALID_ID`。
 *
 * @note                同一个 `method` 可以被多次 Hook。每次调用都会返回不同的 ID。
 *                      至少 `prefix` 或 `postfix` 其中一个必须非空。
 *                      `sig_info` 指针指向的数据由 Hook 库管理，Hook 函数只需读取，不应尝试修改或释放它。
 * @warning             Prefix 和 Postfix Hook 函数的实现必须非常小心，避免引入不稳定性或死循环。
 *                      它们的执行环境与被 Hook 的函数紧密相关。
 *                      多个 Hook 的执行顺序（特别是同一类型的多个 Hook）需要明确定义（例如，按安装顺序）。
 *                      Hook 函数必须能正确处理 `sig_info` 中描述的各种类型。
 *                      没有线程安全，请不要并行调用。
 */
DEFINE_FUNCTION(patch_hook_id_t, patchlib_install_prepost_hook, patch_handle_t method, void* prefix, void* postfix)

/**
 * @brief 卸载指定的 Hook
 *
 * 根据提供的 `hook_id` 卸载先前安装的 Hook（无论是传统 Hook 还是 Pre/Post Hook）。
 * 卸载后，该 `hook_id` 将变为无效。
 *
 * @param hook_id       通过 `patchlib_install_traditional_hook` 或 `patchlib_install_prepost_hook`
 *                      返回的 Hook 标识符。不可为 `PATCH_HOOK_INVALID_ID`。
 * @return              如果 Hook 成功卸载，则返回 true。
 *                      如果 `hook_id` 无效或卸载过程中发生错误，则返回 false。
 *
 * @note                此操作仅影响由 `hook_id` 标识的那一次 Hook 注册。
 *                      如果目标函数上还有其他 Hook，它们将继续保持活动状态。
 *                      调用者应确保在不再需要该 Hook 时调用此函数，以避免资源泄漏。
 * @warning             卸载 Hook 后，不应再使用与之相关的任何资源（例如，通过传统 Hook 获取的 `orig_func` 句柄，
 *                      或传递给 Pre/Post Hook 的 `orig_func`）。
 *                      没有线程安全，请不要并行调用。
 */
DEFINE_FUNCTION(bool, patchlib_uninstall_hook, patch_hook_id_t hook_id)


// ==================== 资源管理 ====================
/**
 * @brief 释放方法相关资源
 * @param method 要释放的方法句柄(可以为无效句柄)
 * @return 执行结果
 */
DEFINE_FUNCTION(bool, patchlib_method_free, patch_handle_t method)

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_METHOD_H
