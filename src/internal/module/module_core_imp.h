/*******************************************************************************
 * tefkernel - module_core_imp
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
 * Created: 2026/3/8
 *******************************************************************************/

#ifndef TEFKERNEL_MODULE_CORE_IMP_H
#define TEFKERNEL_MODULE_CORE_IMP_H

#include "module/module_core.h"
#include "tefstd/vector.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct module_handle_t {
    void *handle; ///< 动态库句柄
    module_entry_t *module_entry; ///< 条目指针
    size_t index; ///< 引索
} module_handle_t;

extern tefstd_vector_t g_module_list; ///< 句柄(module_handle_t*)
extern bool g_module_list_initialized; ///< 初始化状态

/**
 * @brief 加载模块
 * @param handle 动态库句柄
 * @param pkg_handle 包句柄(可空)
 * @param out_module[out] 输出的句柄指针
 * @return 执行结果
 */
bool tefkernel_load_module(void *handle, tefpkg_t *pkg_handle, module_handle_t **out_module);

/**
 * @brief 卸载模块
 * @param module_handle 模块句柄
 */
void tefkernel_cleanup_module(module_handle_t *module_handle);

/**
 * @brief 清理所有模块
 */
void tefkernel_cleanup_all_modules(void);

/**
 * @brief 通过引索获取模块句柄
 * @param index
 * @return 返回结果
 */
module_handle_t *tefkernel_get_module_by_index(size_t index);

/**
 * @brief 获取模块数量
 * @return 模块数量
 */
size_t tefkernel_get_module_count(void);

/**
 * @brief 获取模块信息
 * @param module_handle 模块句柄
 * @return 模块信息结构体指针
 */
const module_info_t *tefkernel_get_module_info(module_handle_t *module_handle);

/**
 * @brief 初始化所有已加载的模块
 * @return 成功返回true，失败返回false
 */
bool tefkernel_initialize_all_modules(void);

/**
 * @brief 检查插件是否还有其他引用
 * @param pkg_id 插件包ID
 * @return 如果还有引用返回true，否则返回false
 */
bool module_check_plugin_references(const char *pkg_id);

/**
 * @brief 调用所有模块的热重载函数
 */
void tefkernel_hot_reload_all_modules(void);

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_MODULE_CORE_IMP_H