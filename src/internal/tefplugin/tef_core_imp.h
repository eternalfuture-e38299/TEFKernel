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
    tef_vector_t sym_names; // const char*

    // 符号地址
    tef_vector_t sym_addrs; // void*

    // 插件引索
    size_t index;
} plugin_handle_t;


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
 * @brief 多线程注册插件符号到所有共享库（阻塞版本）
 * @param plugin 包含要注册符号的插件句柄
 * @return true-所有符号注册成功 false-部分或全部失败
 *
 * @note 函数会阻塞直到所有线程完成符号注册
 * @note 每个共享库由一个独立线程处理
 */
bool tpf_register_plugin_symbols(plugin_handle_t *plugin);

/**
 * @brief 初始化内核依赖
 */
void tpf_init_libtefkernel();

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_TEF_CORE_IMP_H