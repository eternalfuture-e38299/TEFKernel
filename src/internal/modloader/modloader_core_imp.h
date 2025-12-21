/*******************************************************************************
 * tefkernel - modloader_core_imp
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
 * Created: 2025/12/20
 *******************************************************************************/

#ifndef TEFKERNEL_MODLOADER_CORE_IMP_H
#define TEFKERNEL_MODLOADER_CORE_IMP_H

#include "modloader/modloader_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ml_handle_t {
    void *handle;           ///< 动态库句柄
    ml_entry_t* ml_entry;   ///< 条目指针
    size_t index;           ///< 引索
} ml_handle_t;


/**
 * @brief 加载ModLoader
 * @param handle 动态库句柄
 * @param pkg_handle 包句柄(可空)
 * @param private_dir 私有目录
 * @param out_ml[out] 输出的句柄指针
 * @return 执行结果
 */
bool tefkernel_load_ml(void* handle, tefpkg_handle_t* pkg_handle, const char* private_dir, ml_handle_t** out_ml);


/**
 * @brief 卸载Mod加载器
 * @param ml_handle Mod加载器句柄
 */
void tefkernel_cleanup_ml(ml_handle_t* ml_handle);

/**
 * @brief 清理所有modloader
 */
void tefkernel_cleanup_all_ml(void);

/**
 * @brief 通过引索获取modloader句柄
 * @param index
 * @return 返回结果
 */
ml_handle_t* tefkernel_get_ml_by_index(size_t index);

/**
 * @brief 获取modloader数量
 * @return modloader数量
 */
size_t tefkernel_get_ml_count(void);


#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_MODLOADER_CORE_IMP_H