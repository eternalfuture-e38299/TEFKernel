/*******************************************************************************
 * tefkernel - log
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
 * Created: 2026/10/30
 *******************************************************************************/

#ifndef TEFKERNEL_STD_LOG_H
#define TEFKERNEL_STD_LOG_H

#include <stdbool.h>
#include <stdint.h>

#include "../tef_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 日志级别 ====================
 * 数值与 tefkernel_log_level_t 对齐，便于跨系统统一过滤。
 */
typedef enum tefstd_log_level_t {
    TEFSTDLOG_TRACE    = 0,
    TEFSTDLOG_DEBUG    = 1,
    TEFSTDLOG_INFO     = 2,
    TEFSTDLOG_WARN     = 3,
    TEFSTDLOG_ERROR    = 4,
    TEFSTDLOG_CRITICAL = 5,
    TEFSTDLOG_LEVEL_COUNT = 6
} tefstd_log_level_t;

/* ==================== 配置 ====================
 * 所有字段都有默认值；传 NULL 或字段为 0 表示使用默认。
 *
 * 内存占用估算：
 *   queue_capacity * (msg_max + 32) 字节
 * 默认 4096 * (256 + 32) ≈ 1.1 MB / logger
 */
typedef struct tefstd_log_config_t {
    const char *path;             /* 文件路径；NULL=仅控制台 */
    uint32_t    queue_capacity;   /* 槽数，2 的幂，默认 4096 */
    uint32_t    msg_max;          /* 单条最大字节，默认 256，范围 [64,4096] */
    uint32_t    batch_size;       /* 每批落盘条数上限，默认 256 */
    uint32_t    flush_interval_ms;/* 空闲刷盘间隔，默认 200 */
    uint32_t    rotate_max_bytes; /* >0 启用按大小轮转，默认 0 */
    uint32_t    rotate_max_files; /* 轮转保留份数，默认 5 */
    bool        sync_on_critical; /* CRITICAL 立即 fsync，默认 true */
    bool        also_console;     /* 同时输出到控制台(stderr)，默认 false */
} tefstd_log_config_t;

typedef struct tefstd_logger_t tefstd_logger_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief 打开一个 logger（一个 logger = 一个文件 + 一条无锁队列 + 一个写线程）
 * @param cfg 配置，NULL 使用全部默认
 * @return logger 句柄，失败返回 NULL
 */
DEFINE_FUNCTION(tefstd_logger_t *, tefstd_log_open,
                const tefstd_log_config_t *cfg)

/**
 * @brief 关闭 logger，阻塞直到队列排干并落盘
 * @param lg logger 句柄，NULL 安全
 */
DEFINE_FUNCTION(void, tefstd_log_close,
                tefstd_logger_t *lg)

/* ==================== 写入（线程安全，无锁生产者） ==================== */

/**
 * @brief 写入一条日志
 * @note 非阻塞；队列满时丢弃并计数，不影响调用方
 */
DEFINE_FUNCTION(void, tefstd_log_write,
                tefstd_logger_t *lg, tefstd_log_level_t level,
                const char *fmt, ...)

/**
 * @brief 写入一条带源码位置的日志
 */
DEFINE_FUNCTION(void, tefstd_log_write_ex,
                tefstd_logger_t *lg, tefstd_log_level_t level,
                const char *file, int line, const char *func,
                const char *fmt, ...)


/* ==================== 运行时控制 ==================== */

/**
 * @brief 设置最低输出级别（低于此级别直接丢弃，开销 = 一次比较）
 */
DEFINE_FUNCTION(void, tefstd_log_set_level,
                tefstd_logger_t *lg, tefstd_log_level_t min_level)

/**
 * @brief 立即排干队列并刷盘（阻塞当前线程直到消费者处理完）
 */
DEFINE_FUNCTION(void, tefstd_log_flush,
                tefstd_logger_t *lg)

/**
 * @brief 获取累计丢弃条数
 */
DEFINE_FUNCTION(uint64_t, tefstd_log_dropped,
                const tefstd_logger_t *lg)

/* ==================== 便捷宏 ==================== */

#define TEFSTDLOG(lg, lvl, ...)  tefstd_log_write((lg), (lvl), __VA_ARGS__)
#define TEFSTDLOGX(lg, lvl, ...) \
    tefstd_log_write_ex((lg), (lvl), __FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif //TEFKERNEL_STD_LOG_H