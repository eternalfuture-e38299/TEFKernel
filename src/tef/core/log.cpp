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
 * Created: 2026/11/6
 *******************************************************************************/

#include "tef/core/log.hpp"

#include <spdlog/async.h>
#include <spdlog/spdlog.h>
#include <spdlog/details/thread_pool.h>
#include <spdlog/sinks/rotating_file_sink.h>

#if defined(__ANDROID__)
#   include <spdlog/sinks/android_sink.h>
#   include <android/log.h>
#else
#   include <spdlog/sinks/stdout_color_sinks.h>
#endif

#include <atomic>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tef::log {
    namespace {
        constexpr auto k_logger_name = "tefkernel";
        constexpr size_t k_queue_size = 8192;
        constexpr size_t k_thread_count = 1;

        /* 普通 shared_ptr，用 std::atomic_load / std::atomic_store 自由函数做
         * 原子读写。不用 std::atomic<std::shared_ptr<...>>：NDK r28 的 libc++
         * 要求 T trivially copyable，shared_ptr 不是，编译期 static_assert 失败。 */
        std::shared_ptr<spdlog::logger> g_logger;

        std::atomic<bool> g_initialized{false};

        /* ============================================================================
         * 级别映射
         * ============================================================================ */

        spdlog::level::level_enum to_spdlog(const level l) noexcept {
            switch (l) {
                case level::trace: return spdlog::level::trace;
                case level::debug: return spdlog::level::debug;
                case level::info: return spdlog::level::info;
                case level::warn: return spdlog::level::warn;
                case level::error: return spdlog::level::err;
                case level::critical: return spdlog::level::critical;
            }
            return spdlog::level::info;
        }

#if defined(__ANDROID__)
        android_LogPriority to_android(const level l) noexcept {
            switch (l) {
                case level::trace: return ANDROID_LOG_VERBOSE;
                case level::debug: return ANDROID_LOG_DEBUG;
                case level::info: return ANDROID_LOG_INFO;
                case level::warn: return ANDROID_LOG_WARN;
                case level::error: return ANDROID_LOG_ERROR;
                case level::critical: return ANDROID_LOG_FATAL;
            }
            return ANDROID_LOG_INFO;
        }
#endif

        /* ============================================================================
         * 时间戳文件名
         * ============================================================================ */

        std::string build_filename(const char *base) {
            char time_buf[32];
            const std::time_t now = std::time(nullptr);
            const std::tm *tm_info = std::localtime(&now);
            std::strftime(time_buf, sizeof(time_buf), "%Y%m%d_%H%M%S", tm_info);

            std::string out{base};
            out += '_';
            out += time_buf;
            out += ".log";
            return out;
        }

        /* ============================================================================
         * 未初始化时的控制台降级（无 spdlog 依赖）
         * ============================================================================ */

        void fallback_console(level level, const std::string_view msg) noexcept {
            static const char *k_names[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"};
            const int idx = static_cast<int>(level);
            const char *level_str = (idx >= 0 && idx <= 5) ? k_names[idx] : "UNKNOWN";

#if defined(__ANDROID__)
            __android_log_print(to_android(level), "TEFKernel",
                                "[%s] %.*s", level_str,
                                static_cast<int>(msg.size()), msg.data());
#else
            std::fprintf(stderr, "[TEFKernel] [%s] %.*s\n",
                         level_str, static_cast<int>(msg.size()), msg.data());
            std::fflush(stderr);
#endif
        }
    } // namespace

    /* ============================================================================
     * 公开 API
     * ============================================================================ */

    void init(const char *filename) {
        if (g_initialized.load(std::memory_order_acquire)) {
            shutdown();
        }

        std::vector<spdlog::sink_ptr> sinks;

#if defined(__ANDROID__)
        sinks.push_back(std::make_shared<spdlog::sinks::android_sink_mt>("TEFKernel"));
#else
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
#endif

        std::string log_filename;
        if (filename && filename[0] != '\0') {
            log_filename = build_filename(filename);
            try {
                /* 轮转：单文件上限 256 KB，只保留 1 个文件。
                 * 写满后从文件头覆盖，崩溃前最后一段日志始终在文件里。
                 * max_files=1 时 spdlog 不再生成 .1 / .2 备份。 */
                constexpr size_t k_max_bytes = 256 * 1024;
                constexpr size_t k_max_files = 1;
                sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    log_filename, k_max_bytes, k_max_files));
            } catch (const std::exception &e) {
                std::fprintf(stderr, "Failed to open log file '%s': %s\n",
                             log_filename.c_str(), e.what());
                log_filename.clear();
            }
        }

        if (!spdlog::thread_pool()) {
            spdlog::init_thread_pool(k_queue_size, k_thread_count);
        }

        auto logger = std::make_shared<spdlog::async_logger>(
            k_logger_name,
            sinks.begin(), sinks.end(),
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::overrun_oldest);

#if defined(NDEBUG)
        logger->set_level(spdlog::level::info);
#else
        logger->set_level(spdlog::level::trace);
#endif
        logger->flush_on(spdlog::level::err);

#if !defined(__ANDROID__)
        logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
#endif

        spdlog::register_logger(logger);

        /* 先转成 shared_ptr<spdlog::logger>，让 atomic_store 的模板 T 一致。
         * 直接传 std::move(logger)（shared_ptr<async_logger>）会推导出两个 T，
         * 编译失败。 */
        std::shared_ptr<spdlog::logger> base_logger = std::move(logger);
        std::atomic_store_explicit(&g_logger, std::move(base_logger),
                                   std::memory_order_release);
        g_initialized.store(true, std::memory_order_release);

        if (const auto current = std::atomic_load_explicit(&g_logger, std::memory_order_acquire)) {
            current->info("Log system initialized{}",
                          log_filename.empty() ? " (console only)"
                                               : " | file: " + log_filename);
        }
    }

    void shutdown() {
        if (!g_initialized.exchange(false, std::memory_order_acq_rel))
            return;

        /* 原子取出并清空；旧 logger 的引用由本地 logger 持有 */
        const auto logger = std::atomic_exchange_explicit(
            &g_logger, std::shared_ptr<spdlog::logger>{},
            std::memory_order_acq_rel);

        if (logger) {
            logger->info("Log system shutting down");
            logger->flush();
        }

        spdlog::drop(k_logger_name);
        /* logger 析构后，spdlog 线程池留到进程退出 */
    }

    bool is_initialized() noexcept {
        return g_initialized.load(std::memory_order_acquire);
    }

    void write(const level level, const char *file, const int line, const char *func,
               const std::string_view msg) {
        /* 一次 acquire load；拿不到 logger 就降级 */
        const auto logger = std::atomic_load_explicit(&g_logger, std::memory_order_acquire);
        if (!logger) {
            fallback_console(level, msg);
            return;
        }

        const spdlog::source_loc loc{file ? file : "", line, func ? func : ""};
        logger->log(loc, to_spdlog(level), "{}", msg);
    }
}