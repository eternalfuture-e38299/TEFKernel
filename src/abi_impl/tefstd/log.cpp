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

#include "log_internal.hpp"
#include "tefstd/log.h"

#include <cstdarg>
#include <cstring>
#include <ctime>

#if defined(__ANDROID__)
#   include <android/log.h>
#   include <unistd.h>
#   define TL_ANDROID 1
#else
#   define TL_ANDROID 0
#endif

#if defined(_WIN32)
#   include <io.h>
#   include <process.h>
#   define TL_FSYNC(fp) _commit(_fileno(fp))
#else
#   include <unistd.h>
#   define TL_FSYNC(fp) fsync(fileno(fp))
#endif

namespace tefstd::detail {
    /* ============================================================================
     * 工具
     * ============================================================================ */

    uint32_t round_pow2(uint32_t v) noexcept {
        if (v < 2) return 2;
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        return v + 1;
    }

    /* ============================================================================
     * Ring
     * ============================================================================ */

    ring::ring(const uint32_t cap, const uint32_t slot_stride)
        : buf_(static_cast<size_t>(cap) * slot_stride, 0),
          cap_(cap),
          mask_(cap - 1),
          slot_stride_(slot_stride) {
        for (uint32_t i = 0; i < cap; ++i)
            slot_at(i)->seq.store(i, std::memory_order_relaxed);
    }

    bool ring::reserve(uint32_t *out_pos, slot_hdr **out_slot) {
        uint32_t pos = head_.load(std::memory_order_relaxed);
        for (;;) {
            slot_hdr *s = slot_at(pos & mask_);
            const uint32_t seq = s->seq.load(std::memory_order_acquire);

            if (const auto diff = static_cast<int32_t>(seq - pos); diff == 0) {
                if (head_.compare_exchange_weak(pos, pos + 1,
                                                std::memory_order_acq_rel,
                                                std::memory_order_relaxed)) {
                    *out_pos = pos;
                    *out_slot = s;
                    return true;
                }
                /* CAS 失败：pos 已被更新为当前值，继续 */
            } else if (diff < 0) {
                return false; /* 队列满 */
            } else {
                pos = head_.load(std::memory_order_relaxed);
            }
        }
    }

    void ring::publish(const uint32_t pos, slot_hdr *s) {
        s->seq.store(pos + 1, std::memory_order_release);
    }

    bool ring::peek(uint32_t *out_pos, slot_hdr **out_slot) {
        const uint32_t pos = tail_.load(std::memory_order_relaxed);
        slot_hdr *s = slot_at(pos & mask_);
        if (const uint32_t seq = s->seq.load(std::memory_order_acquire); static_cast<int32_t>(seq - (pos + 1)) == 0) {
            *out_pos = pos;
            *out_slot = s;
            return true;
        }
        return false;
    }

    void ring::consume(const uint32_t pos, slot_hdr *s) {
        s->seq.store(pos + cap_, std::memory_order_release);
        tail_.store(pos + 1, std::memory_order_release);
    }

    /* ============================================================================
     * Logger：时间戳 / 控制台 / 轮转
     * ============================================================================ */

    void logger::refresh_ts() {
        const std::time_t now = std::time(nullptr);
        if (now == tsc.last_sec && tsc.ts[0]) return;
        tsc.last_sec = now;
        std::tm tmv{};
#if defined(_WIN32)
        localtime_s(&tmv, &now);
#else
        localtime_r(&now, &tmv);
#endif
        std::strftime(tsc.ts, sizeof(tsc.ts), "%Y-%m-%d %H:%M:%S", &tmv);
    }

    void logger::console_emit(const int level, const char *ts, const char *msg, const size_t len) {
        const char *level_str = k_level_str[level & 7];
#if TL_ANDROID
        android_LogPriority prio;
        switch (level & 7) {
            case tefstdlog_trace: prio = ANDROID_LOG_VERBOSE;
                break;
            case tefstdlog_debug: prio = ANDROID_LOG_DEBUG;
                break;
            case tefstdlog_info: prio = ANDROID_LOG_INFO;
                break;
            case tefstdlog_warn: prio = ANDROID_LOG_WARN;
                break;
            case tefstdlog_error: prio = ANDROID_LOG_ERROR;
                break;
            case tefstdlog_critical: prio = ANDROID_LOG_FATAL;
                break;
            default: prio = ANDROID_LOG_INFO;
                break;
        }
        __android_log_print(prio, "TEFKernel",
                            "[%s] [%s] %.*s", ts, level_str, static_cast<int>(len), msg);
#else
        std::FILE *out = stderr;
        const bool is_term = isatty(fileno(out));
        auto color = "";
        auto reset = "";
        if (is_term) {
            reset = "\033[0m";
            switch (level & 7) {
                case tefstdlog_error:
                case tefstdlog_critical: color = "\033[1;31m";
                    break;
                case tefstdlog_warn: color = "\033[1;33m";
                    break;
                case tefstdlog_info: color = "\033[1;32m";
                    break;
                case tefstdlog_debug: color = "\033[1;36m";
                    break;
                default: color = "\033[1;37m";
                    break;
            }
        }
        std::fprintf(out, "%s[%s] [%s] %.*s%s\n",
                     color, ts, level_str, static_cast<int>(len), msg, reset);
        std::fflush(out);
#endif
    }

    void logger::rotate() {
        if (rotate_max_bytes == 0 || !fp) return;
        if (const long sz = std::ftell(fp.get()); sz < 0 || static_cast<uint32_t>(sz) < rotate_max_bytes) return;

        fp.reset();

        char oldp[600], newp[600];
        for (uint32_t i = rotate_max_files; i > 0; --i) {
            std::snprintf(oldp, sizeof(oldp), "%s.%u", path.c_str(), i - 1);
            std::snprintf(newp, sizeof(newp), "%s.%u", path.c_str(), i);
            if (i == rotate_max_files) std::remove(newp);
            std::rename(oldp, newp);
        }
        std::snprintf(newp, sizeof(newp), "%s.0", path.c_str());
        std::rename(path.c_str(), newp);

        if (std::FILE *f = std::fopen(path.c_str(), "a")) {
            std::setvbuf(f, nullptr, _IOFBF, 32 * 1024);
            fp.reset(f);
        }
    }

    /* ============================================================================
     * Logger：创建 / 写入 / 消费 / 关闭
     * ============================================================================ */

    std::unique_ptr<logger> logger::create(const tefstd_log_config_t *cfg) {
        auto lg = std::make_unique<logger>();

        const uint32_t cap_req = cfg && cfg->queue_capacity ? cfg->queue_capacity : 4096;
        const uint32_t msg_req = cfg && cfg->msg_max ? cfg->msg_max : 256;
        uint32_t msg_max = msg_req < 64 ? 64 : msg_req > 4096 ? 4096 : msg_req;
        msg_max = msg_max + 7u & ~7u;

        lg->batch_size = cfg && cfg->batch_size ? cfg->batch_size : 256;
        lg->flush_interval_ms = cfg && cfg->flush_interval_ms ? cfg->flush_interval_ms : 200;
        lg->rotate_max_bytes = cfg ? cfg->rotate_max_bytes : 0;
        lg->rotate_max_files = cfg && cfg->rotate_max_files ? cfg->rotate_max_files : 5;
        lg->sync_on_critical = cfg ? cfg->sync_on_critical : true;
        lg->also_console = cfg ? cfg->also_console : false;
        lg->min_level.store(tefstdlog_trace, std::memory_order_relaxed);

        const uint32_t cap = round_pow2(cap_req);
        const auto stride = static_cast<uint32_t>(
            (sizeof(slot_hdr) + msg_max + 7u) & ~7u);

        lg->_ring = std::make_unique<detail::ring>(cap, stride);

        if (cfg && cfg->path && cfg->path[0]) {
            lg->path = cfg->path;
            if (std::FILE *f = std::fopen(lg->path.c_str(), "a")) {
                std::setvbuf(f, nullptr, _IOFBF, 32 * 1024);
                lg->fp.reset(f);
            } else {
                return nullptr;
            }
        }

        return lg;
    }

    void logger::enqueue(const int level, const char *msg, size_t len) {
        uint32_t pos = 0;
        slot_hdr *s = nullptr;
        if (!_ring->reserve(&pos, &s)) {
            dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        if (const uint32_t cap = _ring->slot_msg_cap(); len >= cap) len = cap - 1;
        char *dst = ring::slot_msg(s);
        std::memcpy(dst, msg, len);
        dst[len] = '\0';
        s->len = static_cast<uint16_t>(len);
        s->level = static_cast<uint8_t>(level);
        ring::publish(pos, s);

        /* 半满以上提示扩 worker */
        const uint32_t h = _ring->head();
        if (const uint32_t t = _ring->tail(); (h - t) > (_ring->cap() >> 1)) {
            pool::instance().request_grow();
        }
    }

    int logger::drain(char *batch, const size_t batch_cap, size_t *used_io, const bool final) {
        int drained = 0;
        refresh_ts();

        const uint32_t max_batch = batch_size ? batch_size : 256;
        uint32_t pos = 0;
        slot_hdr *s = nullptr;

        while (drained < static_cast<int>(max_batch) && _ring->peek(&pos, &s)) {
            const char *msg = ring::slot_msg(s);
            const uint16_t len = s->len;
            const uint8_t lvl = s->level;

            char hdr[40];
            int hlen = std::snprintf(hdr, sizeof(hdr), "%s [%s] ",
                                     tsc.ts, k_level_str[lvl & 7]);
            if (hlen < 0) hlen = 0;

            if (const auto need = static_cast<size_t>(hlen) + len + 1; *used_io + need > batch_cap) {
                if (*used_io && fp) {
                    std::fwrite(batch, 1, *used_io, fp.get());
                    *used_io = 0;
                }
                if (fp) {
                    std::fwrite(hdr, 1, static_cast<size_t>(hlen), fp.get());
                    std::fwrite(msg, 1, len, fp.get());
                    std::fputc('\n', fp.get());
                }
                if (also_console) console_emit(lvl, tsc.ts, msg, len);
            } else {
                std::memcpy(batch + *used_io, hdr, static_cast<size_t>(hlen));
                *used_io += static_cast<size_t>(hlen);
                std::memcpy(batch + *used_io, msg, len);
                *used_io += len;
                batch[(*used_io)++] = '\n';
                if (also_console) console_emit(lvl, tsc.ts, msg, len);
            }

            _ring->consume(pos, s);

            if (lvl == tefstdlog_critical && sync_on_critical) {
                if (*used_io && fp) {
                    std::fwrite(batch, 1, *used_io, fp.get());
                    *used_io = 0;
                }
                if (fp) {
                    std::fflush(fp.get());
                    TL_FSYNC(fp.get());
                }
            }
            ++drained;
        }

        if (*used_io && fp) {
            std::fwrite(batch, 1, *used_io, fp.get());
            *used_io = 0;
        }

        if (final && fp) {
            std::fflush(fp.get());
            TL_FSYNC(fp.get());
        }
        return drained;
    }

    void logger::shutdown() {
        active.store(false, std::memory_order_release);

        /* 等 worker 释放 busy */
        while (busy.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        char batch[64 * 1024];
        size_t used = 0;
        drain(batch, sizeof(batch), &used, true);
    }

    /* ============================================================================
     * Pool
     * ============================================================================ */

    pool &pool::instance() {
        static pool p;
        return p;
    }

    void pool::ensure_started() {
        if (initialized.load(std::memory_order_acquire)) return;

        std::lock_guard g(lock);
        if (initialized.load(std::memory_order_relaxed)) return;

        running.store(true, std::memory_order_release);

        /* 至少 1 个 worker */
        if (worker_count.load(std::memory_order_relaxed) < k_min_workers)
            spawn_one_locked();

        initialized.store(true, std::memory_order_release);
    }

    void pool::Register(logger *lg) {
        std::lock_guard g(lock);
        lg->pool_next = head;
        head = lg;
        lg->active.store(true, std::memory_order_release);
    }

    void pool::unregister(logger *lg) {
        std::lock_guard g(lock);
        logger **pp = &head;
        while (*pp && *pp != lg) pp = &(*pp)->pool_next;
        if (*pp == lg) *pp = lg->pool_next;
        lg->pool_next = nullptr;
        lg->active.store(false, std::memory_order_release);
    }

    void pool::request_grow() {
        if (!running.load(std::memory_order_relaxed)) return;
        if (worker_count.load(std::memory_order_relaxed) >= k_max_workers) return;
        pending_spawn.store(true, std::memory_order_release);
    }

    bool pool::spawn_one_locked() {
        if (const uint32_t wc = worker_count.load(std::memory_order_relaxed); wc >= k_max_workers) return false;

        try {
            std::thread t([this] { worker_main(); });
            t.detach();
        } catch (...) {
            return false;
        }

        worker_count.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    void pool::worker_main() {
        constexpr logger::ts_cache tsc{};
        (void) tsc; /* 时间戳由每个 logger 自己维护 */

        char batch[64 * 1024];
        size_t used_io = 0;
        uint32_t backoff_ms = 1;

        /* Logger 内部的 ts cache 独立，这里只做批次缓冲 */

        while (running.load(std::memory_order_acquire)) {
            bool did_work = false;

            /* 遍历所有 logger（持锁拷贝链，逐个尝试独占） */
            std::vector<logger *> snapshot;
            {
                std::lock_guard<std::mutex> g(lock);
                for (logger *lg = head; lg; lg = lg->pool_next)
                    snapshot.push_back(lg);
            }

            for (logger *lg: snapshot) {
                if (!lg->active.load(std::memory_order_acquire)) continue;
                if (!lg->try_claim()) continue;

                if (const int n = lg->drain(batch, sizeof(batch), &used_io, false); n > 0) {
                    did_work = true;
                    if (lg->fp) std::fflush(lg->fp.get());
                    if (lg->rotate_max_bytes) lg->rotate();
                }
                lg->release();
            }

            if (did_work) {
                backoff_ms = 1;
            } else {
                /* 检查是否有积压 */
                bool backlog = false;
                for (const logger *lg: snapshot) {
                    if (lg->active.load(std::memory_order_acquire) && lg->_ring->has_backlog()) {
                        backlog = true;
                        break;
                    }
                }
                if (backlog && worker_count.load(std::memory_order_relaxed) < k_max_workers)
                    pending_spawn.store(true, std::memory_order_release);

                std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
                if (backoff_ms < 200) backoff_ms *= 2;
            }

            /* 按需扩 worker */
            if (pending_spawn.exchange(false, std::memory_order_acq_rel)) {
                std::lock_guard<std::mutex> g(lock);
                spawn_one_locked();
            }
        }

        /* 退出：排干所有活跃 logger */
        std::vector<logger *> snapshot;
        {
            std::lock_guard<std::mutex> g(lock);
            for (logger *lg = head; lg; lg = lg->pool_next)
                snapshot.push_back(lg);
        }
        for (logger *lg: snapshot) {
            if (lg->active.load(std::memory_order_acquire) && lg->try_claim()) {
                lg->drain(batch, sizeof(batch), &used_io, true);
                lg->release();
            }
        }
    }

    void pool::shutdown() {
        running.store(false, std::memory_order_release);
        pending_spawn.store(false, std::memory_order_release);
    }
} // namespace tefstd::detail

/* ============================================================================
 * 公开 C ABI（tefstd_log_*）
 * ============================================================================ */

using tefstd::detail::logger;
using tefstd::detail::pool;

static logger *l(tefstd_logger_t *p) { return reinterpret_cast<logger *>(p); }
static const logger *l(const tefstd_logger_t *p) { return reinterpret_cast<const logger *>(p); }
static tefstd_logger_t *t(logger *p) { return reinterpret_cast<tefstd_logger_t *>(p); }

extern "C" {
tefstd_logger_t *tefstd_log_open(const tefstd_log_config_t *cfg) {
    try {
        auto lg = logger::create(cfg);
        if (!lg) return nullptr;

        auto &pool = pool::instance();
        pool.ensure_started();
        pool.Register(lg.get());
        pool.request_grow();

        return t(lg.release());
    } catch (...) {
        return nullptr;
    }
}

void tefstd_log_close(tefstd_logger_t *lg) {
    if (!lg) return;
    auto *p = l(lg);

    /* 从池中摘除，禁止 worker 再领 */
    pool::instance().unregister(p);

    /* 排干 + 关闭 */
    p->shutdown();

    delete p;
}

void tefstd_log_write(tefstd_logger_t *lg, const tefstd_log_level_t level,
                      const char *fmt, ...) {
    if (!lg || !fmt) return;
    auto *p = l(lg);

    if (static_cast<int>(level) <
        p->min_level.load(std::memory_order_relaxed))
        return;

    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (static_cast<size_t>(n) >= sizeof(buf)) n = static_cast<int>(sizeof(buf)) - 1;

    p->enqueue(static_cast<int>(level), buf, static_cast<size_t>(n));
}

void tefstd_log_write_ex(tefstd_logger_t *lg, const tefstd_log_level_t level,
                         const char *file, const int line, const char *func,
                         const char *fmt, ...) {
    if (!lg || !fmt) return;
    auto *p = l(lg);

    if (static_cast<int>(level) <
        p->min_level.load(std::memory_order_relaxed))
        return;

    const char *base = std::strrchr(file ? file : "", '/');
    if (!base) base = std::strrchr(file ? file : "", '\\');
    base = base ? base + 1 : (file ? file : "?");

    char body[448];
    va_list ap;
    va_start(ap, fmt);
    const int bn = std::vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);
    if (bn < 0) return;
    /* bn 不再使用；vsnprintf 保证 body 有 '\0' 结尾 */

    char full[512];
    int fn = std::snprintf(full, sizeof(full), "[%s:%d][%s] %s",
                           base, line, func ? func : "?", body);
    if (fn < 0) return;
    if (static_cast<size_t>(fn) >= sizeof(full))
        fn = static_cast<int>(sizeof(full)) - 1;

    p->enqueue(static_cast<int>(level), full, static_cast<size_t>(fn));
}

void tefstd_log_set_level(tefstd_logger_t *lg, const tefstd_log_level_t min_level) {
    if (!lg) return;
    l(lg)->min_level.store(static_cast<int32_t>(min_level),
                           std::memory_order_relaxed);
}

void tefstd_log_flush(tefstd_logger_t *lg) {
    if (!lg) return;
    const auto *p = l(lg);

    /* 等队列排干 */
    while (p->_ring->has_backlog()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (p->fp) {
        std::fflush(p->fp.get());
        TL_FSYNC(p->fp.get());
    }
}

uint64_t tefstd_log_dropped(const tefstd_logger_t *lg) {
    if (!lg) return 0;
    return l(lg)->dropped.load(std::memory_order_relaxed);
}
}
