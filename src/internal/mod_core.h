/*******************************************************************************
 * tefkernel - mod_core
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
 * Created: 2026/3/14
 *******************************************************************************/

#ifndef TEFKERNEL_MOD_CORE_H
#define TEFKERNEL_MOD_CORE_H

#include <time.h>

#include "modloader/modloader_core.h"
#include "modloader/modloader_core_imp.h"
#include "tefstd/vector.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mod加载状态
 */
typedef enum {
    MOD_STATE_UNLOADED = 0,  ///< 未加载
    MOD_STATE_LOADED = 1,    ///< 已加载
    MOD_STATE_INITIALIZED = 2, ///< 已初始化
    MOD_STATE_ERROR = 3      ///< 加载错误
} mod_state_t;

/**
 * @brief Mod句柄
 */
typedef struct {
    const char* mod_id;             ///< Mod唯一标识符
    const char* mod_path;           ///< Mod文件路径
    mod_manifest_t* manifest;       ///< Mod清单
    ml_handle_t* owner_ml;          ///< 所属的ModLoader
    mod_state_t state;              ///< 加载状态
    time_t load_time;               ///< 加载时间
} mod_handle_t;

// 全局Mod列表
extern tefstd_vector_t g_mod_list;  ///< mod_handle_t*
extern bool g_mod_list_initialized; ///< 初始化状态

/**
 * @brief 加载所有Mod
 *
 * 遍历所有已加载的ModLoader，为每个ModLoader加载启用的Mod
 *
 * @return 成功加载的Mod数量
 */
int tefkernel_load_all_mods(void);

/**
 * @brief 初始化所有已加载的Mod
 *
 * 调用每个Mod所属ModLoader的init_mod函数
 *
 * @return 成功初始化的Mod数量
 */
int tefkernel_init_all_mods(void);

/**
 * @brief 卸载所有Mod
 *
 * 调用每个Mod所属ModLoader的unload_mod函数
 *
 * @return 成功卸载的Mod数量
 */
int tefkernel_cleanup_all_mods(void);

/**
 * @brief 通过ModLoader包名获取启用的Mod列表
 *
 * @param ml_pkg_id ModLoader的包名
 * @param enabled_mods 输出的启用Mod列表
 * @return 启用的Mod数量，-1表示失败
 */
int tefkernel_get_enabled_mods_for_ml(const char* ml_pkg_id, tefstd_vector_t* enabled_mods);

/**
 * @brief 获取Mod数量
 *
 * @return 已加载的Mod数量
 */
size_t tefkernel_get_mod_count(void);

/**
 * @brief 通过索引获取Mod句柄
 *
 * @param index Mod索引
 * @return Mod句柄，失败返回NULL
 */
mod_handle_t* tefkernel_get_mod_by_index(size_t index);

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_MOD_CORE_H