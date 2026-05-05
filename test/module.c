/*******************************************************************************
 * tefkernel - module
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

#include "module/module_core.h"

#include <android/log.h>  // Android 日志头文件

/* 定义日志标签 */
#define LOG_TAG "TestModule"

/* Android 日志宏定义 */
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, __VA_ARGS__)

/* 模块信息 - 存储在静态内存中 */
static const module_info_t g_module_info = {
    .pkg_id = "com.example.testmodule",  /* 唯一包名 */
    .name = "Test Module",                /* 模块名称 */
    .author = "Developer",                /* 作者 */
    .version = "1.0.0",                   /* 版本号 */
    .version_code = 1,                    /* 版本代码 */
    .api_version = 1,                     /* API版本 */
    .plugin_dependencies_sizes = 0,       /* 无依赖插件 */
    .plugin_dependencies = NULL           /* 依赖列表为空 */
};

/* 模块实例 */
static module_entry_t *g_module_entry = NULL;

/* 模块操作函数实现 */

/* 初始化模块 */
static bool test_module_init(module_entry_t *entry)
{
    LOGI("Initializing module: %s", entry->info->name);
    LOGI("Version: %s (code: %d)",
         entry->info->version, entry->info->version_code);

    g_module_entry = entry;

    /* 这里可以进行模块初始化操作 */
    LOGI("Module initialized successfully");
    LOGI("Private directory: %s", entry->private_dir);
    LOGI("Logs directory: %s", entry->logs_dir);

    /* 添加一些调试信息 */
    LOGD("Module pointer: %p", (void*)entry);
    LOGD("Module info pointer: %p", (void*)entry->info);
    LOGD("API version: %d", entry->info->api_version);

    return true;  /* 返回true表示初始化成功 */
}

/* 清理模块 */
static bool test_module_cleanup(module_entry_t *entry)
{
    LOGI("Cleaning up module: %s", entry->info->name);

    /* 这里进行资源释放等清理操作 */
    g_module_entry = NULL;

    LOGI("Module cleanup completed");
    return true;  /* 返回true表示清理成功 */
}

/* 热重载 */
static void test_module_hot_reload(module_entry_t *entry)
{
    LOGI("Hot reload triggered for module: %s", entry->info->name);

    /* 这里可以实现热重载逻辑，比如重新加载配置等 */
    LOGI("Hot reload completed");
}

/* 获取模块信息 */
static const module_info_t *test_module_get_info(void)
{
    LOGD("Getting module info");
    /* 直接返回静态模块信息 */
    return &g_module_info;
}

/* 模块操作函数表 */
static const module_ops_t g_module_ops = {
    .init_module = test_module_init,
    .cleanup_module = test_module_cleanup,
    .hot_reload = test_module_hot_reload,
    .get_info = test_module_get_info
};

/* 模块必须导出的入口函数 */
API_EXPORT const module_ops_t * API_CALL module_create(void)
{
    LOGI("module_create() called");
    LOGD("Module info: name=%s, version=%s, author=%s",
         g_module_info.name, g_module_info.version, g_module_info.author);

    /* 必须返回静态内存中的操作函数表 */
    return &g_module_ops;
}