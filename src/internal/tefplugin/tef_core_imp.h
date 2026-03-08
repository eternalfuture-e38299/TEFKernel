/*******************************************************************************
 * tefkernel - tef_core_imp
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
 * Created: 2025/12/12
 *******************************************************************************/

#ifndef TEFKERNEL_TEF_CORE_IMP_H
#define TEFKERNEL_TEF_CORE_IMP_H

#include "tefstd/vector.h"
#include "tefplugin/tpf_core.h"
#include "tefstd/hashmap.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef struct plugin_handle_t {
    // 插件句柄
    void *handle;

    // 插件函数表
    tpf_plugin_ops_t *ops;

    // 符号名称
    tefstd_vector_t sym_names; // const char*

    // 符号地址
    tefstd_vector_t sym_addrs; // void*

    // 插件引索
    size_t index;
} plugin_handle_t;

// 插件引用计数结构
typedef struct {
    const char* pkg_id;
    int ref_count;  // 引用计数
} plugin_ref_entry_t;

// 全局插件引用计数表
extern tefstd_hashmap_t g_plugin_refs; ///< 插件引用计数表
extern bool g_plugin_refs_initialized; ///< 初始化状态

/**
 * @brief 初始化插件引用计数系统
 */
void tpf_init_plugin_refs(void);

/**
 * @brief 增加插件引用计数
 */
void tpf_add_plugin_ref(const char* pkg_id);

/**
 * @brief 减少插件引用计数
 */
void tpf_remove_plugin_ref(const char* pkg_id);

/**
 * @brief 获取插件引用计数
 * @param pkg_id 插件包ID
 * @return 引用计数
 */
int tpf_get_plugin_ref_count(const char* pkg_id);

/**
 * @brief 通过pkg_id获取插件句柄
 * @param pkg_id 插件包ID
 * @return 插件句柄，如果不存在返回NULL
 */
plugin_handle_t *tpf_get_plugin_by_id(const char *pkg_id);

/**
 * @brief 检查插件是否存在
 * @param pkg_id 插件包ID
 * @return 如果插件存在返回true，否则返回false
 */
bool tpf_plugin_exists(const char *pkg_id);

/**
 * @brief 加载一个插件
 * @param handle 插件句柄
 * @param out_plugin[out] 输出的插件句柄
 * @return 执行结果
 */
bool tpf_load_plugin(void *handle, plugin_handle_t **out_plugin);

/**
 * @brief 卸载插件
 * @param plugin 插件句柄
 * @return 执行结果
 */
bool tpf_cleanup_plugin(plugin_handle_t *plugin);

/**
 * @brief 检查插件是否还有其他引用
 * @param pkg_id 插件包ID
 * @return 如果还有引用返回true，否则返回false
 */
bool tpf_check_plugin_references(const char *pkg_id);

/**
 * @brief 多线程注册插件符号到所有共享库（阻塞版本）
 * @param plugin 包含要注册符号的插件句柄
 * @return true-所有符号注册成功 false-部分或全部失败
 *
 * @note 函数会阻塞直到所有线程完成符号注册
 * @note 每个共享库由一个独立线程处理
 */
bool tpf_register_plugin_symbols(plugin_handle_t *plugin);

/**
 * @brief 初始化所有已加载的插件并将其注册到所有共享库
 *
 * 这个函数会遍历所有已加载的插件，调用它们的初始化函数，并将所有插件的符号
 * 注册到已注册的共享库中。这样可以确保所有插件都能互相访问彼此的API。
 *
 * @return true 所有插件都成功初始化和注册
 * @return false 有一个或多个插件初始化或注册失败
 */
bool tpf_initialize_all_plugins();

/**
 * @brief 初始化内核依赖
 */
void tpf_init_libtefkernel();

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_TEF_CORE_IMP_H