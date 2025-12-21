/*******************************************************************************
 * tefkernel - log
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

#ifndef TEFKERNEL_LOG_H
#define TEFKERNEL_LOG_H
#ifdef __cplusplus
extern "C" {



#endif

typedef enum tefkernel_log_level_t {
    TEFKERNEL_LOG_LEVEL_TRACE, // 最详细的日志信息
    TEFKERNEL_LOG_LEVEL_DEBUG, // 调试信息
    TEFKERNEL_LOG_LEVEL_INFO, // 一般信息
    TEFKERNEL_LOG_LEVEL_WARN, // 警告信息
    TEFKERNEL_LOG_LEVEL_ERROR, // 错误信息
    TEFKERNEL_LOG_LEVEL_CRITICAL // 严重错误信息
} tefkernel_log_level_t;

/**
 * @brief 打印并写出日志
 * @param level 日志级别
 * @param fmt 格式化内容
 * @param ... 格式化参数
 */
void tefkernel_log_write(tefkernel_log_level_t level, const char *fmt, ...)
__attribute__((__format__(printf, 2, 3)));

/**
 * @brief 打印并写出日志（带源码位置）
 * @param level 日志级别
 * @param file 源码文件名
 * @param line 源码行号
 * @param func 函数名
 * @param fmt 格式化内容
 * @param ... 格式化参数
 */
void tefkernel_log_write_ex(tefkernel_log_level_t level,
                            const char *file, int line, const char *func,
                            const char *fmt, ...)
__attribute__((__format__(printf, 5, 6)));


/**
 * @brief 初始化日志系统
 * @param filename 输出文件名称
 */
void tefkernel_log_init(const char *filename);

/**
 * @brief 清理日志系统资源
 */
void tefkernel_log_cleanup(void);

// 宏

#if !defined(NDEBUG)
#define TEKLOG(level, ...) tefkernel_log_write_ex((level), __FILE__, __LINE__, __func__, __VA_ARGS__);
#else
#define TEKLOG(level, ...) tefkernel_log_write((level), __VA_ARGS__);
#endif

// ==================== 便捷日志宏 ====================

#if !defined(NDEBUG)
/** @brief TRACE级别日志 */
#define TEKLOG_TRACE(...) TEKLOG(TEFKERNEL_LOG_LEVEL_TRACE, __VA_ARGS__)

/** @brief DEBUG级别日志 */
#define TEKLOG_DEBUG(...) TEKLOG(TEFKERNEL_LOG_LEVEL_DEBUG, __VA_ARGS__)
#else
#define TEKLOG_TRACE(...) ((void)0)
#define TEKLOG_DEBUG(...) ((void)0)
#endif

/** @brief INFO级别日志 */
#define TEKLOG_INFO(...)  TEKLOG(TEFKERNEL_LOG_LEVEL_INFO, __VA_ARGS__)

/** @brief WARN级别日志 */
#define TEKLOG_WARN(...)  TEKLOG(TEFKERNEL_LOG_LEVEL_WARN, __VA_ARGS__)

/** @brief ERROR级别日志 */
#define TEKLOG_ERROR(...) TEKLOG(TEFKERNEL_LOG_LEVEL_ERROR, __VA_ARGS__)

/** @brief CRITICAL级别日志 */
#define TEKLOG_CRITICAL(...) TEKLOG(TEFKERNEL_LOG_LEVEL_CRITICAL, __VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_LOG_H
