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

#include "internal/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

// 平台检测
#if defined(__ANDROID__)
#include <android/log.h>
#include <unistd.h>
#define ANDROID_PLATFORM 1
#else
#define ANDROID_PLATFORM 0
#endif

// 通用POSIX线程支持
#if !defined(_WIN32) && !defined(_WIN64)
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#define POSIX_THREADS 1
#else
#define POSIX_THREADS 0
#include <windows.h>
#include <process.h>
#endif

// 日志队列节点
typedef struct log_node_t {
    tefkernel_log_level_t level;
    char message[2048];
    struct log_node_t* next;
} log_node_t;

// 日志队列
typedef struct {
    log_node_t* head;
    log_node_t* tail;
#if POSIX_THREADS
    pthread_mutex_t mutex;
#else
    CRITICAL_SECTION mutex;
#endif
    int count;
    int capacity;
} log_queue_t;

// 全局日志上下文
typedef struct {
    FILE* log_file;
    log_queue_t queue;
#if POSIX_THREADS
    pthread_t thread_id;
#else
    HANDLE thread_id;
#endif
    int running;
    char filename[256];  // 存储生成的文件名
} log_context_t;

static log_context_t g_log_ctx = {0};

#if ANDROID_PLATFORM
// Android日志级别映射
static android_LogPriority get_android_log_level(const tefkernel_log_level_t level) {
    switch (level) {
        case TEFKERNEL_LOG_LEVEL_TRACE: return ANDROID_LOG_VERBOSE;
        case TEFKERNEL_LOG_LEVEL_DEBUG: return ANDROID_LOG_DEBUG;
        case TEFKERNEL_LOG_LEVEL_INFO: return ANDROID_LOG_INFO;
        case TEFKERNEL_LOG_LEVEL_WARN: return ANDROID_LOG_WARN;
        case TEFKERNEL_LOG_LEVEL_ERROR: return ANDROID_LOG_ERROR;
        case TEFKERNEL_LOG_LEVEL_CRITICAL: return ANDROID_LOG_FATAL;
        default: return ANDROID_LOG_INFO;
    }
}

static const char* ANDROID_LOG_TAG = "TEFKernel";
#endif

// 获取当前时间字符串（用于文件名）
static void get_current_time_for_filename(char* time_buf, const size_t buf_size) {
    const time_t now = time(NULL);
    const struct tm* tm_info = localtime(&now);
    strftime(time_buf, buf_size, "%Y%m%d_%H%M%S", tm_info);
}

// 获取当前时间字符串（用于日志内容）
static void get_current_time_for_log(char* time_buf, const size_t buf_size) {
    const time_t now = time(NULL);
    const struct tm* tm_info = localtime(&now);
    strftime(time_buf, buf_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// 获取日志级别字符串
static const char* get_level_string(const tefkernel_log_level_t level) {
    switch (level) {
        case TEFKERNEL_LOG_LEVEL_TRACE: return "TRACE";
        case TEFKERNEL_LOG_LEVEL_DEBUG: return "DEBUG";
        case TEFKERNEL_LOG_LEVEL_INFO: return "INFO";
        case TEFKERNEL_LOG_LEVEL_WARN: return "WARN";
        case TEFKERNEL_LOG_LEVEL_ERROR: return "ERROR";
        case TEFKERNEL_LOG_LEVEL_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// 生成带时间戳的文件名
static int generate_filename(char* buffer, size_t buffer_size, const char* base_name) {
    char time_buf[64];
    get_current_time_for_filename(time_buf, sizeof(time_buf));

    return snprintf(buffer, buffer_size, "%s_%s.log", base_name, time_buf);
}

// 初始化日志队列
static void log_queue_init(log_queue_t* queue) {
    queue->head = queue->tail = NULL;
    queue->count = 0;
    queue->capacity = 100000;  // 增大队列容量

#if POSIX_THREADS
    pthread_mutex_init(&queue->mutex, NULL);
#else
    InitializeCriticalSection(&queue->mutex);
#endif
}

// 销毁日志队列
static void log_queue_destroy(log_queue_t* queue) {
#if POSIX_THREADS
    pthread_mutex_destroy(&queue->mutex);
#else
    DeleteCriticalSection(&queue->mutex);
#endif
}

// 向队列添加日志节点（无阻塞）
static int log_queue_push(log_queue_t* queue, const tefkernel_log_level_t level, const char* message) {
#if POSIX_THREADS
    pthread_mutex_lock(&queue->mutex);
#else
    EnterCriticalSection(&queue->mutex);
#endif

    // 检查队列容量
    if (queue->count >= queue->capacity) {
#if POSIX_THREADS
        pthread_mutex_unlock(&queue->mutex);
#else
        LeaveCriticalSection(&queue->mutex);
#endif
        return -1;
    }

    log_node_t* node = (log_node_t*)malloc(sizeof(log_node_t));
    if (!node) {
#if POSIX_THREADS
        pthread_mutex_unlock(&queue->mutex);
#else
        LeaveCriticalSection(&queue->mutex);
#endif
        return -1;
    }

    node->level = level;
    strncpy(node->message, message, sizeof(node->message) - 1);
    node->message[sizeof(node->message) - 1] = '\0';
    node->next = NULL;

    if (queue->tail) {
        queue->tail->next = node;
    } else {
        queue->head = node;
    }
    queue->tail = node;
    queue->count++;

#if POSIX_THREADS
    pthread_mutex_unlock(&queue->mutex);
#else
    LeaveCriticalSection(&queue->mutex);
#endif

    return 0;
}

// 从队列获取所有节点（批量处理提高性能）
static log_node_t* log_queue_pop_all(log_queue_t* queue) {
#if POSIX_THREADS
    pthread_mutex_lock(&queue->mutex);
#else
    EnterCriticalSection(&queue->mutex);
#endif

    log_node_t* head = queue->head;
    if (head == NULL) {
#if POSIX_THREADS
        pthread_mutex_unlock(&queue->mutex);
#else
        LeaveCriticalSection(&queue->mutex);
#endif
        return NULL;
    }

    queue->head = queue->tail = NULL;
    queue->count = 0;

#if POSIX_THREADS
    pthread_mutex_unlock(&queue->mutex);
#else
    LeaveCriticalSection(&queue->mutex);
#endif

    return head;
}

// 清空队列
static void log_queue_clear(log_queue_t* queue) {
#if POSIX_THREADS
    pthread_mutex_lock(&queue->mutex);
#else
    EnterCriticalSection(&queue->mutex);
#endif

    log_node_t* node = queue->head;
    while (node) {
        log_node_t* next = node->next;
        free(node);
        node = next;
    }

    queue->head = queue->tail = NULL;
    queue->count = 0;

#if POSIX_THREADS
    pthread_mutex_unlock(&queue->mutex);
#else
    LeaveCriticalSection(&queue->mutex);
#endif
}

// 控制台输出（带颜色）
static void console_output(const tefkernel_log_level_t level, const char* time_buf,
                          const char* level_str, const char* message) {
#if !ANDROID_PLATFORM
    const char* color_code = "";
    const char* reset_code = "";

    // 检查是否在终端中
    int is_terminal = 0;
#if !defined(_WIN32) && !defined(_WIN64)
    is_terminal = isatty(fileno(stdout));
#else
    is_terminal = _isatty(_fileno(stdout));
#endif

    if (is_terminal) {
        reset_code = "\033[0m";
        switch (level) {
            case TEFKERNEL_LOG_LEVEL_ERROR:
            case TEFKERNEL_LOG_LEVEL_CRITICAL:
                color_code = "\033[1;31m";
                break;
            case TEFKERNEL_LOG_LEVEL_WARN:
                color_code = "\033[1;33m";
                break;
            case TEFKERNEL_LOG_LEVEL_INFO:
                color_code = "\033[1;32m";
                break;
            case TEFKERNEL_LOG_LEVEL_DEBUG:
                color_code = "\033[1;36m";
                break;
            default:
                color_code = "\033[1;37m";
                break;
        }
    }

    printf("[TEFKernel] %s[%s] [%s] %s%s\n", color_code, time_buf, level_str, message, reset_code);
    fflush(stdout);
#endif
}

#if POSIX_THREADS
static void* log_thread(void* arg)
#else
static unsigned __stdcall log_thread(void* arg)
#endif
{
    log_context_t* ctx = arg;

    // 批量处理缓冲区
    char batch_buffer[8192];  // 8KB缓冲区
    size_t batch_size = 0;

    while (ctx->running) {
        // 批量获取所有日志节点
        log_node_t* node_list = log_queue_pop_all(&ctx->queue);

        if (node_list == NULL) {
            // 没有日志，短暂休眠避免CPU占用过高
#if POSIX_THREADS
            usleep(1000);  // 1ms
#else
            Sleep(1);
#endif
            continue;
        }

        // 处理批量日志
        log_node_t* current = node_list;
        while (current != NULL) {
            // 获取当前时间
            char time_buf[64];
            get_current_time_for_log(time_buf, sizeof(time_buf));
            const char* level_str = get_level_string(current->level);

#if ANDROID_PLATFORM
            const android_LogPriority android_level = get_android_log_level(current->level);
            __android_log_print(android_level, ANDROID_LOG_TAG, "[%s] [%s] %s",
                               time_buf, level_str, current->message);
#else
            console_output(current->level, time_buf, level_str, current->message);
#endif

            if (ctx->log_file) {
                // 格式化日志行
                const int line_len = snprintf(batch_buffer + batch_size,
                                      sizeof(batch_buffer) - batch_size,
                                      "[%s] [%s] %s\n",
                                      time_buf, level_str, current->message);

                if (line_len > 0 && (batch_size + line_len) < sizeof(batch_buffer)) {
                    batch_size += line_len;
                } else {
                    // 缓冲区满，立即写入
                    if (batch_size > 0) {
                        fwrite(batch_buffer, 1, batch_size, ctx->log_file);
                        fflush(ctx->log_file);  // 立即刷新到磁盘
                        batch_size = 0;
                    }
                    // 直接写入当前行
                    fprintf(ctx->log_file, "[%s] [%s] %s\n", time_buf, level_str, current->message);
                    fflush(ctx->log_file);
                }
            }

            log_node_t* next = current->next;
            free(current);
            current = next;
        }

        // 写入剩余的缓冲区内容
        if (batch_size > 0 && ctx->log_file) {
            fwrite(batch_buffer, 1, batch_size, ctx->log_file);
            fflush(ctx->log_file);
            batch_size = 0;
        }
    }

    // 清理阶段：处理队列中剩余的消息
    log_node_t* final_list = log_queue_pop_all(&ctx->queue);
    log_node_t* current = final_list;
    while (current != NULL) {
        char time_buf[64];
        get_current_time_for_log(time_buf, sizeof(time_buf));
        const char* level_str = get_level_string(current->level);

#if ANDROID_PLATFORM
        const android_LogPriority android_level = get_android_log_level(current->level);
        __android_log_print(android_level, ANDROID_LOG_TAG, "[%s] [%s] %s",
                           time_buf, level_str, current->message);
#else
        console_output(current->level, time_buf, level_str, current->message);
#endif

        if (ctx->log_file) {
            fprintf(ctx->log_file, "[%s] [%s] %s\n", time_buf, level_str, current->message);
        }

        log_node_t* next = current->next;
        free(current);
        current = next;
    }

    // 最后刷新文件
    if (ctx->log_file) {
        fflush(ctx->log_file);
    }

#if POSIX_THREADS
    return NULL;
#else
    return 0;
#endif
}

void tefkernel_log_init(const char* filename) {
    // 如果已经初始化，先清理
    if (g_log_ctx.running) {
        tefkernel_log_cleanup();
    }

    // 初始化队列
    log_queue_init(&g_log_ctx.queue);

    // 生成带时间戳的文件名
    if (filename && generate_filename(g_log_ctx.filename, sizeof(g_log_ctx.filename), filename) > 0) {
        // 创建新文件
        g_log_ctx.log_file = fopen(g_log_ctx.filename, "w");

        if (g_log_ctx.log_file) {
            // 设置文件缓冲区为无缓冲，实现实时写入
            setvbuf(g_log_ctx.log_file, NULL, _IONBF, 0);

            // 写入文件头
            char time_buf[64];
            get_current_time_for_log(time_buf, sizeof(time_buf));
            fflush(g_log_ctx.log_file);

#if ANDROID_PLATFORM
            __android_log_print(ANDROID_LOG_INFO, ANDROID_LOG_TAG,
                              "Log system initialized. File: %s", g_log_ctx.filename);
#else
            printf("Log system initialized. File: %s\n", g_log_ctx.filename);
#endif
        } else {
            // 文件打开失败
#if ANDROID_PLATFORM
            __android_log_print(ANDROID_LOG_ERROR, ANDROID_LOG_TAG,
                              "Failed to create log file: %s", g_log_ctx.filename);
#else
            fprintf(stderr, "Failed to create log file: %s\n", g_log_ctx.filename);
#endif
            g_log_ctx.filename[0] = '\0';
            g_log_ctx.log_file = NULL;
        }
    } else {
        g_log_ctx.filename[0] = '\0';
        g_log_ctx.log_file = NULL;
#if ANDROID_PLATFORM
        __android_log_print(ANDROID_LOG_INFO, ANDROID_LOG_TAG,
                          "Log system initialized (console only)");
#else
        printf("Log system initialized (console only)\n");
#endif
    }

    // 启动日志线程
    g_log_ctx.running = 1;

#if POSIX_THREADS
    if (pthread_create(&g_log_ctx.thread_id, NULL, log_thread, &g_log_ctx) != 0) {
#else
    g_log_ctx.thread_id = (HANDLE)_beginthreadex(NULL, 0, log_thread, &g_log_ctx, 0, NULL);
    if (g_log_ctx.thread_id == NULL) {
#endif
#if ANDROID_PLATFORM
        __android_log_print(ANDROID_LOG_ERROR, ANDROID_LOG_TAG, "Failed to create log thread");
#else
        fprintf(stderr, "Failed to create log thread\n");
#endif
        g_log_ctx.running = 0;
        if (g_log_ctx.log_file) {
            fclose(g_log_ctx.log_file);
            g_log_ctx.log_file = NULL;
        }
        log_queue_destroy(&g_log_ctx.queue);
        return;
    }

    // 写入初始化成功日志
    tefkernel_log_write(TEFKERNEL_LOG_LEVEL_INFO,
                       "Log system initialized successfully. File: %s",
                       g_log_ctx.filename[0] != '\0' ? g_log_ctx.filename : "console only");
}

void tefkernel_log_write(const tefkernel_log_level_t level, const char* fmt, ...) {
    // 格式化消息
    char message[2048];
    va_list args;
    va_start(args, fmt);
    const int len = vsnprintf(message, sizeof(message) - 1, fmt, args);
    message[len] = '\0';
    va_end(args);

    if (!g_log_ctx.running) {
        // 如果日志系统未初始化，直接输出到控制台
        char time_buf[64];
        get_current_time_for_log(time_buf, sizeof(time_buf));
        const char* level_str = get_level_string(level);

#if ANDROID_PLATFORM
        const android_LogPriority android_level = get_android_log_level(level);
        __android_log_print(android_level, ANDROID_LOG_TAG, "[%s] [%s] %s", time_buf, level_str, message);
#else
        console_output(level, time_buf, level_str, message);
#endif
        return;
    }

    // 添加到队列（非阻塞）
    if (log_queue_push(&g_log_ctx.queue, level, message) != 0) {
        // 队列已满，直接输出到控制台
        char time_buf[64];
        get_current_time_for_log(time_buf, sizeof(time_buf));
        const char* level_str = get_level_string(level);

#if ANDROID_PLATFORM
        __android_log_print(ANDROID_LOG_ERROR, ANDROID_LOG_TAG,
                          "[%s] [%s] LOG_QUEUE_FULL: %s", time_buf, level_str, message);
#else
        fprintf(stderr, "[%s] [%s] LOG_QUEUE_FULL: %s\n", time_buf, level_str, message);
#endif
    }
}

void tefkernel_log_write_ex(const tefkernel_log_level_t level, const char *file, const int line, const char *func, const char *fmt, ...) {
    char message[2048];
    va_list args;
    va_start(args, fmt);

    // 提取基础文件名
    const char* base_file = strrchr(file, '/');
    if (!base_file) base_file = strrchr(file, '\\');
    base_file = base_file ? base_file + 1 : file;

    // 格式化消息内容
    char formatted_message[2048];
    const int len = vsnprintf(formatted_message, sizeof(formatted_message) - 1, fmt, args);
    formatted_message[len] = '\0';
    va_end(args);

    // 组合完整消息
    snprintf(message, sizeof(message), "[%s:%d][%s] %s", base_file, line, func, formatted_message);

    // 复用基础日志函数
    tefkernel_log_write(level, "%s", message);
}

void tefkernel_log_write_net(const tefkernel_log_level_t level, const char *file, const int line, const char *func,
    const char *msg) {

#if !defined(NDEBUG)
    tefkernel_log_write_ex(level, file, line, func, "%s", msg);
#else
    // 发布版本：过滤掉 TRACE 和 DEBUG 级别的日志
    if (level >= TEFKERNEL_LOG_LEVEL_INFO) {
        tefkernel_log_write(level, "%s", msg);
    }
#endif
}


void tefkernel_log_cleanup(void) {
    if (g_log_ctx.running) {
        // 写入关闭日志
        tefkernel_log_write(TEFKERNEL_LOG_LEVEL_INFO, "Log system shutting down");

        // 等待日志写入完成
#if POSIX_THREADS
        usleep(50000);  // 50ms
#else
        Sleep(50);
#endif

        g_log_ctx.running = 0;

        // 等待日志线程退出
#if POSIX_THREADS
        pthread_join(g_log_ctx.thread_id, NULL);
#else
        WaitForSingleObject(g_log_ctx.thread_id, INFINITE);
        CloseHandle(g_log_ctx.thread_id);
#endif

        if (g_log_ctx.log_file) {
            // 写入关闭信息
            fclose(g_log_ctx.log_file);
            g_log_ctx.log_file = NULL;
        }

        log_queue_clear(&g_log_ctx.queue);
        log_queue_destroy(&g_log_ctx.queue);

#if ANDROID_PLATFORM
        __android_log_print(ANDROID_LOG_INFO, ANDROID_LOG_TAG, "Log system shutdown complete");
#else
        printf("Log system shutdown complete\n");
#endif
    }
}

// 获取当前日志文件名
const char* tefkernel_log_get_filename(void) {
    return g_log_ctx.filename[0] != '\0' ? g_log_ctx.filename : NULL;
}
