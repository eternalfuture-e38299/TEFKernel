/*******************************************************************************
 * File: modloader_core
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

#ifndef TEFKERNEL_MODLOADER_CORE_H
#define TEFKERNEL_MODLOADER_CORE_H

#include "../tefstd/vector.h"
#include "../tefpackage/tefpkg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ML_SUCCESS = 0,
    ML_ERROR = -1,
    ML_ERROR_INVALID_PARAM = -2,
    ML_ERROR_NOT_FOUND = -3
} ml_result_t;

typedef struct {
    const char *path;
    const char *mod_id;
    const char *private_dir;
    const char *logs_dir;
} mod_manifest_t;

typedef struct {
    const char *mod_id; ///< Mod唯一标识符
    int is_multiplayer_safe; ///< 是否可联机（必须为1）
    int version_code; ///< 版本代码
    const char *version; ///< 版本字符串
} multiplayer_mod_info_t;

typedef struct ml_entry_t ml_entry_t;

typedef struct {
    const char *pkg_id; ///< 唯一包名
    int version_code; ///< 版本代码
    const char *version; ///< 版本
    int api_version; ///< api版本
    int plugin_dependencies_sizes; ///< 依赖插件数组大小
    const char **plugin_dependencies; ///< 依赖插件(pkg_id)
} ml_info_t;

typedef struct {
    /**
     * @brief 加载单个Mod
     *
     * @param mod_manifest Mod描述信息
     * @return ml_result_t 结果码
     */
    ml_result_t (*load_mod)(mod_manifest_t *mod_manifest);

    /**
     * @brief 卸载单个Mod
     * @param mod_manifest Mod描述信息
     * @return ml_result_t 结果码
     */
    ml_result_t (*unload_mod)(mod_manifest_t *mod_manifest);

    /**
     * @brief 重新加载Mod（热重载）
     *
     * @param mod_manifest Mod描述信息
     * @return ml_result_t 结果码
     */
    ml_result_t (*reload_mod)(mod_manifest_t *mod_manifest);

    /**
     * @brief 初始化单个Mod
     *
     * @param mod_manifest Mod描述信息
     * @return ml_result_t 结果码
     */
    ml_result_t (*init_mod)(mod_manifest_t *mod_manifest);

    /**
     * @brief 获取Mod的联机检测信息
     * @warning 内核不会卸载其信息，请注意内存管理
     *
     * @param mod_manifest Mod描述信息
     * @return const multiplayer_mod_info_t* Mod联机信息
     */
    const multiplayer_mod_info_t * (*get_multiplayer_info)(mod_manifest_t *mod_manifest);

    /**
     * @brief 初始化modloader
     * @return ml_result_t 结果码
     */
    ml_result_t (*init_ml)(ml_entry_t* ml_entry);

    /**
     * @brief 清理并关闭modloader
     * @return ml_result_t 结果码
     */
    ml_result_t (*cleanup_ml)(ml_entry_t* ml_entry);

    /**
     * @brief 获取Mod信息（加载后调用）
     * @return 包含版本、依赖等信息的结构体指针
     * @note 必须在静态内存中
     */
    const ml_info_t *(*get_info)(void);
} ml_ops_t;

typedef struct ml_entry_t {
    ml_info_t *info;
    ml_ops_t *ops;
    tefpkg_t *pkg_handle;
    const char *private_dir;
    const char *logs_dir;
} ml_entry_t;

/**
 * @brief 创建ModLoader
 *
 * ModLoader必须导出的唯一函数，返回ModLoader的操作函数表。
 *
 * @return ModLoader操作函数表指针
 *
 * @note 必须返回静态内存，不要动态分配
 * @note 此函数在ModLoader加载时调用一次
 */
API_EXPORT const ml_ops_t * API_CALL ml_create(void);

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_MODLOADER_CORE_H
