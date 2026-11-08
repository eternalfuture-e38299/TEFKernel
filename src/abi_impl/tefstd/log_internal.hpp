/*******************************************************************************
 * tefkernel - log_internal
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

#pragma once

#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "tefstd/log.h"

namespace tefstd::detail {
    /* ============================================================================
     * 槽位头：与旧 C 版二进制布局一致
     * ============================================================================ */

    struct slot_hdr {
        std::atomic<uint32_t> seq;
        uint16_t len;
        uint8_t level;
        uint8_t pad;
    };

    static_assert(sizeof(slot_hdr) == 8, "SlotHdr layout must stay 8 bytes");

    /* ============================================================================
     * 无锁 SPSC 环
     * ============================================================================ */

    class ring {
    public:
        ring(uint32_t cap, uint32_t slot_stride);

        /* 生产者 */
        bool reserve(uint32_t *out_pos, slot_hdr **out_slot);

        static void publish(uint32_t pos, slot_hdr *s);

        /* 消费者 */
        bool peek(uint32_t *out_pos, slot_hdr **out_slot);

        void consume(uint32_t pos, slot_hdr *s);

        [[nodiscard]] uint32_t cap() const noexcept { return cap_; }
        [[nodiscard]] uint32_t mask() const noexcept { return mask_; }

        [[nodiscard]] uint32_t slot_msg_cap() const noexcept {
            return slot_stride_ - static_cast<uint32_t>(sizeof(slot_hdr));
        }

        [[nodiscard]] static char *slot_msg(slot_hdr *s) noexcept {
            return reinterpret_cast<char *>(s + 1);
        }

        /* 供 worker 判断是否有积压 */
        [[nodiscard]] bool has_backlog() const noexcept {
            return head_.load(std::memory_order_acquire) !=
                   tail_.load(std::memory_order_acquire);
        }

        [[nodiscard]] uint32_t head() const noexcept {
            return head_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] uint32_t tail() const noexcept {
            return tail_.load(std::memory_order_relaxed);
        }

    private:
        slot_hdr *slot_at(const uint32_t i) noexcept {
            return reinterpret_cast<slot_hdr *>(buf_.data() + static_cast<size_t>(i) * slot_stride_);
        }

        std::vector<uint8_t> buf_;
        uint32_t cap_;
        uint32_t mask_;
        uint32_t slot_stride_;

        alignas(64) std::atomic<uint32_t> head_{0}; /* 生产者写 */
        alignas(64) std::atomic<uint32_t> tail_{0}; /* 消费者写 */
    };

    /* ============================================================================
     * Logger
     * ============================================================================ */

    struct logger {
        std::string path;
        std::unique_ptr<std::FILE, decltype(&std::fclose)> fp{nullptr, &std::fclose};
        bool also_console = false;

        uint32_t batch_size = 256;
        uint32_t flush_interval_ms = 200;
        uint32_t rotate_max_bytes = 0;
        uint32_t rotate_max_files = 5;
        bool sync_on_critical = true;

        std::atomic<bool> active{false}; /* 在池中可消费 */
        std::atomic<bool> busy{false}; /* 被某 worker 独占中 */
        std::atomic<uint64_t> dropped{0};
        std::atomic<int32_t> min_level{0};

        std::unique_ptr<ring> _ring;

        logger *pool_next = nullptr;

        /* 时间戳缓存 */
        struct ts_cache {
            std::time_t last_sec = 0;
            char ts[20] = {0};
        } tsc;

        /* ---- 操作 ---- */

        static std::unique_ptr<logger> create(const tefstd_log_config_t *cfg);

        /* 生产者路径：入队一条消息 */
        void enqueue(int level, const char *msg, size_t len);

        /* 消费者路径：由 worker 独占调用；返回处理条数 */
        int drain(char *batch, size_t batch_cap, size_t *used_io, bool final);

        /* 独占/释放（worker 协调用） */
        bool try_claim() noexcept {
            bool expected = false;
            return busy.compare_exchange_strong(expected, true,
                                                std::memory_order_acq_rel,
                                                std::memory_order_relaxed);
        }

        void release() noexcept { busy.store(false, std::memory_order_release); }

        /* 排干 + 关闭（tefstd_log_close 用） */
        void shutdown();

        void rotate();

    private:
        void refresh_ts();

        static void console_emit(int level, const char *ts, const char *msg, size_t len);
    };

    /* ============================================================================
     * 全局 worker 池
     * ============================================================================ */

    class pool {
    public:
        static pool &instance();

        void ensure_started();

        void Register(logger *lg);

        void unregister(logger *lg);

        void request_grow();

        void shutdown();

        std::mutex lock; /* 保护 head / 链 */
        logger *head = nullptr;

        std::atomic<uint32_t> worker_count{0};
        std::atomic<uint32_t> pending_spawn{0};
        std::atomic<bool> running{false};
        std::atomic<bool> initialized{false};

        static constexpr uint32_t k_max_workers = 8;
        static constexpr uint32_t k_min_workers = 1;

    private:
        pool() = default;

        void worker_main();

        bool spawn_one_locked();
    };

    /* ============================================================================
     * 工具
     * ============================================================================ */

    inline constexpr const char *k_level_str[] = {
        "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"
    };

    uint32_t round_pow2(uint32_t v) noexcept;
}
