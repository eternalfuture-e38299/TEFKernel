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

typedef void tefpkg_handle_t;

#include "../tefstd/vector.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ML_SUCCESS = 0,
    ML_ERROR = -1,
    ML_ERROR_INVALID_PARAM = -2,
    ML_ERROR_NOT_FOUND = -3
} ml_result_t;


typedef struct ml_entry_t ml_entry_t;

typedef struct {
    const char *pkg_id; ///< 唯一包名
    int version_code; ///< 版本代码
    const char *version; ///< 版本
    int api_version; ///< api版本
    const char **plugin_dependencies; ///< 依赖插件(pkg_id)
    size_t plugin_dependencies_sizes; ///< 依赖插件数组大小
} ml_info_t;

typedef struct {
    // 初始化/关闭
    ml_result_t (*initialize)(ml_entry_t *loader);

    void (*shutdown)(ml_entry_t *loader);

    // Mod管理
    ml_result_t (*load_mods)(ml_entry_t *loader);

    void (*unload_mods)(ml_entry_t *loader);

    // Mod生命周期
    ml_result_t (*initialize_mods)(ml_entry_t *loader);

    // 信息查询
    ml_info_t* (*get_ml_info)();
} ml_ops_t;

typedef struct ml_entry_t {
    ml_info_t *info;
    ml_ops_t *ops;
    tefpkg_handle_t* pkg_handle;
    const char *private_dir;
#if __ANDROID__
    void *jni_env; // only android
#endif
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


/**
 * @def ML_DECLARE(pkg_id_, version_code_, version_, api_version_, plugin_deps_, plugin_deps_size_)
 * 声明ModLoader信息的便捷宏
 *
 * @param pkg_id_ 唯一包名
 * @param version_code_ 版本代码
 * @param version_ 版本字符串
 * @param api_version_ API版本
 * @param plugin_deps_ 插件依赖数组
 * @param plugin_deps_size_ 依赖数组大小
 *
 * @note 必须在全局作用域使用
 */
#define ML_DECLARE(pkg_id_, version_code_, version_, api_version_, plugin_deps_, plugin_deps_size_) \
    static const ml_info_t ml_info = { \
        .pkg_id = pkg_id_, \
        .version_code = version_code_, \
        .version = version_, \
        .api_version = api_version_, \
        .plugin_dependencies = plugin_deps_, \
        .plugin_dependencies_sizes = plugin_deps_size_ \
    }; \
    static ml_info_t* ml_get_info(void) { \
        return (ml_info_t*)&ml_info; \
    }

/**
 * @def ML_DEFINE(init_func_, shutdown_func_, load_mods_func_, unload_mods_func_, init_mods_func_)
 * 定义ModLoader操作函数的便捷宏
 *
 * @param init_func_ 初始化函数指针
 * @param shutdown_func_ 关闭函数指针
 * @param load_mods_func_ 加载模组函数指针
 * @param unload_mods_func_ 卸载模组函数指针
 * @param init_mods_func_ 初始化模组函数指针
 *
 * @note 必须在全局作用域使用
 */
#define ML_DEFINE(init_func_, shutdown_func_, load_mods_func_, unload_mods_func_, init_mods_func_) \
    static const ml_ops_t ml_ops = { \
        .initialize = init_func_, \
        .shutdown = shutdown_func_, \
        .load_mods = load_mods_func_, \
        .unload_mods = unload_mods_func_, \
        .initialize_mods = init_mods_func_, \
        .get_ml_info = ml_get_info \
    }; \
    const ml_ops_t *ml_create(void) { \
        return &ml_ops; \
    }

/**
 * @def ML_CREATE(pkg_id_, version_code_, version_, api_version_, plugin_deps_, plugin_deps_size_, \
 *                init_func_, shutdown_func_, load_mods_func_, unload_mods_func_, init_mods_func_)
 * 创建完整ModLoader的便捷宏（组合宏）
 *
 * 这个宏组合了ML_DECLARE和ML_DEFINE，提供一站式ModLoader创建。
 *
 * @example
 *     ML_CREATE(
 *         "com.example.mymodloader",  // pkg_id
 *         1,                          // version_code
 *         "1.0.0",                    // version
 *         1,                          // api_version
 *         NULL,                       // plugin_deps
 *         0,                          // plugin_deps_size
 *         my_initialize,              // init_func
 *         my_shutdown,                // shutdown_func
 *         my_load_mods,               // load_mods_func
 *         my_unload_mods,             // unload_mods_func
 *         my_init_mods                // init_mods_func
 *     )
 */
#define ML_CREATE(pkg_id_, version_code_, version_, api_version_, plugin_deps_, plugin_deps_size_, \
                  init_func_, shutdown_func_, load_mods_func_, unload_mods_func_, init_mods_func_) \
    ML_DECLARE(pkg_id_, version_code_, version_, api_version_, plugin_deps_, plugin_deps_size_) \
    ML_DEFINE(init_func_, shutdown_func_, load_mods_func_, unload_mods_func_, init_mods_func_)

/**
 * @def ML_EMPTY_DEPENDENCIES
 * 定义空依赖数组的便捷宏
 */
#define ML_EMPTY_DEPENDENCIES NULL, 0

/**
 * @def ML_DEPENDENCIES(deps_array)
 * 定义依赖数组的便捷宏，自动计算数组大小
 *
 * @param deps_array 依赖数组（const char*类型）
 *
 * @example
 *     static const char* my_deps[] = {"plugin1", "plugin2", NULL};
 *     ML_DECLARE("my_loader", 1, "1.0", 1, my_deps, ML_DEPENDENCIES(my_deps));
 */
#define ML_DEPENDENCIES(deps_array) \
    deps_array, sizeof(deps_array) / sizeof((deps_array)[0])

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_MODLOADER_CORE_H
