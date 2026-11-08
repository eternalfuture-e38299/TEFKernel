/*******************************************************************************
 * File: tpf_core
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

#ifndef TEFKERNEL_TPF_CORE_H
#define TEFKERNEL_TPF_CORE_H

#include "../tef_api.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {

#endif

/* ================ 基础结构 ================ */

/**
 * @brief 插件信息结构
 */
typedef struct {
    const char *pkg_id; ///< 插件包名
    const char *name; ///< 插件名称
    const char *author; ///< 作者
    const char *version; ///< 版本
    int version_code;   ///< 版本代码
} tpf_plugin_info_t;

#if IS_TEFKERNEL_BUILD
typedef struct plugin_handle_t plugin_handle_t;
#else
typedef void plugin_handle_t;
#endif

/**
 * @brief 插件操作函数
 */
typedef struct {
    /**
     * @brief 初始化插件
     * @return 成功返回true，失败返回false
     */
    bool (*initialize)(plugin_handle_t *this_handle);

    /**
     * @brief 清理插件
     */
    void (*cleanup)(plugin_handle_t *this_handle);

    /**
     * @brief 获取插件信息
     * @return 插件信息指针
     */
    const tpf_plugin_info_t *(*get_info)(void);
} tpf_plugin_ops_t;

/**
 * @brief 创建插件
 *
 * 插件必须导出的唯一函数，返回插件的操作函数表。
 *
 * @return 插件操作函数表指针
 *
 * @note 必须返回静态内存，不要动态分配
 * @note 此函数在插件加载时调用一次
 */
API_EXPORT const tpf_plugin_ops_t * API_CALL tpf_create_plugin(void);

/**
 * @brief 注册符号
 *
 * 在插件初始化时调用，注册插件提供的符号。
 * 系统会保存符号的地址，稍后批量处理。
 *
 * @param name 符号名称
 * @param addr 符号地址
 * @param this_handle 当前句柄
 * @return 成功返回true，失败返回false
 *
 * @example
 *     // 在插件initialize函数中：
 *     tpf_register_symbol(this_handle, "my_function", &my_function);
 *     tpf_register_symbol(this_handle, "my_data", &my_data);
 */
DEFINE_FUNCTION(bool, tpf_register_symbol,
                plugin_handle_t* this_handle,
                const char *name,
                const void *addr)

/**
 * @brief 注册可享用插件的动态库
 * @param handle 动态库句柄
 * @return 执行结果
 * @warning 无线程安全
 */
DEFINE_FUNCTION(bool, tpf_register_shared_plugin_library, void* handle)

/**
 * @def TPF_SYMBOL(func)
 * 注册符号的便捷宏
 *
 * @note 必须在插件初始化阶段调用
 * @warning 不要在多个线程中同时使用此宏
 */
#define TPF_SYMBOL(func) \
    tpf_register_symbol(this_handle, #func, (const void *)(func))


/* ================ 线程安全说明 ================ */

/**
 * 线程安全规则：
 * 1. 所有插件相关操作都应在主线程中完成
 * 2. 插件加载、初始化、符号注册是串行的
 * 3. 符号遍历时不能进行符号注册
 * 4. 插件清理应在插件不再使用时进行
 *
 * 典型流程：
 * 主线程: 加载插件 -> 初始化插件 -> 注册符号 -> 遍历符号 -> 使用插件 -> 清理插件
 *
 * 注意：本系统设计为单线程使用，不支持并发操作
 */

#ifdef __cplusplus
}
#endif

#endif // TEFKERNEL_TPF_CORE_H
