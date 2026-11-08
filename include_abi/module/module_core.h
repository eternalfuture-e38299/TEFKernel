/*******************************************************************************
 * File: module_core
 * Project: tefkernel
 * Created: 2026/3/8
 * Author: eternalfuture-e38299
 * Github: https://github.com/eternalfuture-e38299
 * 
 * MIT License
 * 
 * Copyright (c) 2026 eternalfuture-e38299
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

#ifndef TEFKERNEL_MODULE_CORE_H
#define TEFKERNEL_MODULE_CORE_H
#include <stdbool.h>

#include "../tefpackage/tefpkg.h"

#ifdef __cplusplus
extern "C" {


#endif

typedef struct module_entry_t module_entry_t;

typedef struct {
    const char *pkg_id; ///< 唯一包名
    const char *name; ///< 插件名称
    const char *author; ///< 作者
    const char *version; ///< 版本
    int version_code; ///< 版本代码
    int api_version; ///< api版本
    int plugin_dependencies_sizes; ///< 依赖插件数组大小
    const char **plugin_dependencies; ///< 依赖插件(pkg_id)
} module_info_t;

typedef struct {
    /**
     * @brief 初始化模块
     * @param entry 模块条目
     */
    bool (*init_module)(module_entry_t *entry);

    /**
     * @brief 清理并关闭
     * @param entry 模块条目
     */
    bool (*cleanup_module)(module_entry_t *entry);

    /**
     * @brief 热重载操作
     * @param entry 模块条目
     */
    void (*hot_reload)(module_entry_t *entry);

    /**
     * @brief 获取模块信息（加载后调用）
     * @return 包含版本、依赖等信息的结构体指针
     * @note 必须在静态内存中
     */
    const module_info_t *(*get_info)(void);
} module_ops_t;

typedef struct module_entry_t {
    module_info_t *info;
    module_ops_t *ops;
    tefpkg_t *pkg_handle;
    const char *private_dir;
    const char *logs_dir;
} module_entry_t;

/**
 * @brief 创建模块
 *
 * 模块必须导出的唯一函数，返回模块的操作函数表。
 *
 * @return 模块操作函数表指针
 *
 * @note 必须返回静态内存，不要动态分配
 * @note 此函数在模块加载时调用一次
 */
API_EXPORT const module_ops_t * API_CALL module_create(void);

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_MODULE_CORE_H
