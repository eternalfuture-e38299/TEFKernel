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


#include "tefstd/log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#include "internal/platform_threads.h"

/* ==================== 平台检测 ==================== */
#if defined(__ANDROID__)
#  include <android/log.h>
#  include <unistd.h>
#  define TL_ANDROID 1
#else
#  define TL_ANDROID 0
#endif

#if defined(_WIN32)
#  include <io.h>
#  define TL_ISATTY(fd)  _isatty(fd)
#  define TL_FILENO(fp)  _fileno(fp)
#else
#  include <unistd.h>
#  define TL_ISATTY(fd)  isatty(fd)
#  define TL_FILENO(fp)  fileno(fp)
#endif

/* ==================== 原子 / 文件同步 ==================== */
#if defined(_WIN32)
#  define TL_ATOMIC_U32_LOAD(p)      ((uint32_t)InterlockedCompareExchange((volatile LONG*)(p),0,0))
#  define TL_ATOMIC_U32_STORE(p,v)   ((void)InterlockedExchange((volatile LONG*)(p),(LONG)(v)))
#  define TL_ATOMIC_U32_CAS(p,exp,des) \
        (InterlockedCompareExchange((volatile LONG*)(p),(LONG)(des),(LONG)(*(exp))) == (LONG)(*(exp)) ? ((void)0, true) : (*(exp) = (uint32_t)InterlockedCompareExchange((volatile LONG*)(p),0,0), false))
#  define TL_ATOMIC_U64_INC(p)       ((uint64_t)InterlockedIncrement64((volatile LONG64*)(p)))
#  define TL_ATOMIC_U64_LOAD(p)      ((uint64_t)InterlockedCompareExchange64((volatile LONG64*)(p),0,0))
#  define TL_ALIGN(n)                __declspec(align(n))
#  define TL_LOCALTIME(dst,src)      localtime_s((dst),(src))
#  define TL_FSYNC_FILE(fp)          _commit(_fileno(fp))
#else
#  define TL_ATOMIC_U32_LOAD(p)      __atomic_load_n((p), __ATOMIC_ACQUIRE)
#  define TL_ATOMIC_U32_STORE(p,v)   __atomic_store_n((p), (v), __ATOMIC_RELEASE)
#  define TL_ATOMIC_U32_CAS(p,exp,des) \
        __atomic_compare_exchange_n((p),(exp),(des),false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)
#  define TL_ATOMIC_U64_INC(p)       __atomic_add_fetch((p),1,__ATOMIC_RELAXED)
#  define TL_ATOMIC_U64_LOAD(p)      __atomic_load_n((p),__ATOMIC_RELAXED)
#  define TL_ALIGN(n)                __attribute__((aligned(n)))
#  define TL_LOCALTIME(dst,src)      localtime_r((src),(dst))
#  define TL_FSYNC_FILE(fp)          fsync(fileno(fp))
#endif

/* busy 标志用 32 位原子即可（避免部分平台无 8 位 CAS） */
#define TL_ATOMIC_FLAG_TEST_SET(p)   (TL_ATOMIC_U32_CAS((p), &(uint32_t){0}, 1))

#define TL_CACHE_LINE 64
#define TL_MSG_HDR    40

/* 内部池上限（不公开） */
#define TL_POOL_MAX_WORKERS 8
#define TL_POOL_MIN_WORKERS 1

/* ==================== 无锁 SPSC 环 ==================== */
typedef struct {
    uint32_t seq;
    uint16_t len;
    uint8_t  level;
    uint8_t  _pad;
} tl_slot_hdr_t;

typedef struct {
    uint8_t *buf;
    uint32_t cap;
    uint32_t mask;
    uint32_t slot_stride;
    TL_ALIGN(TL_CACHE_LINE) uint32_t head;
    TL_ALIGN(TL_CACHE_LINE) uint32_t tail;
} tl_ring_t;

static tl_slot_hdr_t *tl_slot_at(const tl_ring_t *r, const uint32_t idx) {
    return (tl_slot_hdr_t *)(r->buf + (size_t)idx * r->slot_stride);
}
static char *tl_slot_msg(tl_slot_hdr_t *s) { return (char *)(s + 1); }
static uint32_t tl_slot_msg_cap(const tl_ring_t *r) {
    return r->slot_stride - (uint32_t)sizeof(tl_slot_hdr_t);
}

static bool tl_ring_reserve(tl_ring_t *r, uint32_t *out_pos, tl_slot_hdr_t **out_slot) {
    uint32_t pos = TL_ATOMIC_U32_LOAD(&r->head);
    for (;;) {
        tl_slot_hdr_t *s = tl_slot_at(r, pos & r->mask);
        const uint32_t seq = TL_ATOMIC_U32_LOAD(&s->seq);
        const int32_t diff = (int32_t)(seq - pos);
        if (diff == 0) {
            uint32_t expected = pos;
            if (TL_ATOMIC_U32_CAS(&r->head, &expected, pos + 1)) {
                *out_pos = pos; *out_slot = s; return true;
            }
            pos = expected;
        } else if (diff < 0) {
            return false;
        } else {
            pos = TL_ATOMIC_U32_LOAD(&r->head);
        }
    }
}
static void tl_ring_publish(const tl_ring_t *r, const uint32_t pos, tl_slot_hdr_t *s) {
    (void)r; TL_ATOMIC_U32_STORE(&s->seq, pos + 1);
}
static bool tl_ring_peek(const tl_ring_t *r, uint32_t *out_pos, tl_slot_hdr_t **out_slot) {
    const uint32_t pos = TL_ATOMIC_U32_LOAD(&r->tail);
    tl_slot_hdr_t *s = tl_slot_at(r, pos & r->mask);
    const uint32_t seq = TL_ATOMIC_U32_LOAD(&s->seq);
    if ((int32_t)(seq - (pos + 1)) == 0) { *out_pos = pos; *out_slot = s; return true; }
    return false;
}
static void tl_ring_consume(tl_ring_t *r, const uint32_t pos, tl_slot_hdr_t *s) {
    TL_ATOMIC_U32_STORE(&s->seq, pos + r->cap);
    TL_ATOMIC_U32_STORE(&r->tail, pos + 1);
}

/* ==================== logger ==================== */
typedef struct tefstd_logger_t {
    char      path[512];
    FILE     *fp;
    bool      also_console;

    uint32_t  batch_size;
    uint32_t  flush_interval_ms;
    uint32_t  rotate_max_bytes;
    uint32_t  rotate_max_files;
    bool      sync_on_critical;

    volatile uint32_t active;   /* 在池中且可消费（0/1） */
    volatile uint32_t busy;     /* 被某 worker 独占消费中（0/1） */
    uint64_t          dropped;
    volatile int32_t  min_level;

    tl_ring_t ring;

    struct tefstd_logger_t *pool_next;
} tefstd_logger_t;

/* ==================== 内部池 ==================== */
typedef struct {
    mtx_t    lock;                  /* 保护 head / worker_count */
    tefstd_logger_t *head;

    volatile uint32_t worker_count;
    volatile uint32_t pending_spawn;
    thrd_t   workers[TL_POOL_MAX_WORKERS];
    volatile uint32_t running;
    volatile uint32_t initialized;

    mtx_t    boot_lock;             /* 启停/扩缩保护 */
    volatile uint32_t boot_ready;
} tl_pool_t;

static tl_pool_t g_pool = {0};

static const char *const k_level_str[TEFSTDLOG_LEVEL_COUNT] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"
};

/* ==================== 控制台 ==================== */
#if TL_ANDROID
static android_LogPriority tl_android_priority(const uint8_t lvl) {
    switch (lvl & 7) {
        case TEFSTDLOG_TRACE:    return ANDROID_LOG_VERBOSE;
        case TEFSTDLOG_DEBUG:    return ANDROID_LOG_DEBUG;
        case TEFSTDLOG_INFO:     return ANDROID_LOG_INFO;
        case TEFSTDLOG_WARN:     return ANDROID_LOG_WARN;
        case TEFSTDLOG_ERROR:    return ANDROID_LOG_ERROR;
        case TEFSTDLOG_CRITICAL: return ANDROID_LOG_FATAL;
        default:                 return ANDROID_LOG_INFO;
    }
}
#  define TL_ANDROID_TAG "TEFKernel"
#endif

static const char *tl_color_for_level(const uint8_t lvl) {
    switch (lvl & 7) {
        case TEFSTDLOG_ERROR:
        case TEFSTDLOG_CRITICAL: return "\033[1;31m";
        case TEFSTDLOG_WARN:     return "\033[1;33m";
        case TEFSTDLOG_INFO:     return "\033[1;32m";
        case TEFSTDLOG_DEBUG:    return "\033[1;36m";
        default:                 return "\033[1;37m";
    }
}

static void tl_console_emit(const uint8_t lvl, const char *ts,
                            const char *msg, const size_t len) {
    const char *level_str = k_level_str[lvl & 7];
#if TL_ANDROID
    __android_log_print(tl_android_priority(lvl), TL_ANDROID_TAG,
                        "[%s] [%s] %.*s", ts, level_str, (int)len, msg);
#else
    FILE *out = stderr;
    const int is_term = TL_ISATTY(TL_FILENO(out));
    const char *color = is_term ? tl_color_for_level(lvl) : "";
    const char *reset = is_term ? "\033[0m" : "";
    fprintf(out, "%s[%s] [%s] %.*s%s\n", color, ts, level_str, (int)len, msg, reset);
    fflush(out);
#endif
}

/* ==================== 时间戳缓存 ==================== */
typedef struct { time_t last_sec; char ts[20]; } tl_ts_cache_t;

static void tl_ts_refresh(tl_ts_cache_t *c) {
    const time_t now = time(NULL);
    if (now == c->last_sec && c->ts[0]) return;
    c->last_sec = now;
    struct tm tmv;
    TL_LOCALTIME(&tmv, &now);
    strftime(c->ts, sizeof(c->ts), "%Y-%m-%d %H:%M:%S", &tmv);
}

/* ==================== 轮转 ==================== */
static void tl_rotate(tefstd_logger_t *lg) {
    if (lg->rotate_max_bytes == 0 || !lg->fp) return;
    const long sz = ftell(lg->fp);
    if (sz < 0 || (uint32_t)sz < lg->rotate_max_bytes) return;

    fclose(lg->fp);
    lg->fp = NULL;

    char oldp[600], newp[600];
    for (uint32_t i = lg->rotate_max_files; i > 0; --i) {
        snprintf(oldp, sizeof(oldp), "%s.%u", lg->path, i - 1);
        snprintf(newp, sizeof(newp), "%s.%u", lg->path, i);
        if (i == lg->rotate_max_files) remove(newp);
        (void)rename(oldp, newp);
    }
    snprintf(newp, sizeof(newp), "%s.0", lg->path);
    (void)rename(lg->path, newp);

    lg->fp = fopen(lg->path, "a");
    if (lg->fp) setvbuf(lg->fp, NULL, _IOFBF, 32 * 1024);
}

/* ==================== 消费单个 logger（worker 独占） ==================== */
static int tl_drain_logger(tefstd_logger_t *lg, tl_ts_cache_t *tsc,
                           char *batch, const size_t batch_cap, size_t *used_io,
                           const int final) {
    int drained = 0;
    uint32_t pos;
    tl_slot_hdr_t *s;

    tl_ts_refresh(tsc);
    const uint32_t max_batch = lg->batch_size ? lg->batch_size : 256;

    while (drained < (int)max_batch && tl_ring_peek(&lg->ring, &pos, &s)) {
        const char *msg = tl_slot_msg(s);
        const uint16_t len = s->len;
        const uint8_t  lvl = s->level;

        char hdr[TL_MSG_HDR];
        int hlen = snprintf(hdr, sizeof(hdr), "%s [%s] ",
                            tsc->ts, k_level_str[lvl & 7]);
        if (hlen < 0) hlen = 0;

        const size_t need = (size_t)hlen + len + 1;
        if (*used_io + need > batch_cap) {
            if (*used_io && lg->fp) { fwrite(batch, 1, *used_io, lg->fp); *used_io = 0; }
            if (lg->fp) {
                fwrite(hdr, 1, (size_t)hlen, lg->fp);
                fwrite(msg, 1, len, lg->fp);
                fputc('\n', lg->fp);
            }
            if (lg->also_console) tl_console_emit(lvl, tsc->ts, msg, len);
        } else {
            memcpy(batch + *used_io, hdr, (size_t)hlen); *used_io += (size_t)hlen;
            memcpy(batch + *used_io, msg, len);          *used_io += len;
            batch[(*used_io)++] = '\n';
            if (lg->also_console) tl_console_emit(lvl, tsc->ts, msg, len);
        }

        tl_ring_consume(&lg->ring, pos, s);

        if (lvl == TEFSTDLOG_CRITICAL && lg->sync_on_critical) {
            if (*used_io && lg->fp) { fwrite(batch, 1, *used_io, lg->fp); *used_io = 0; }
            if (lg->fp) { fflush(lg->fp); TL_FSYNC_FILE(lg->fp); }
        }
        drained++;
    }

    if (*used_io && lg->fp) { fwrite(batch, 1, *used_io, lg->fp); *used_io = 0; }

    if (final && lg->fp) { fflush(lg->fp); TL_FSYNC_FILE(lg->fp); }
    return drained;
}

/* 尝试独占一个 logger */
static bool tl_try_claim(tefstd_logger_t *lg) {
    uint32_t expected = 0;
    return TL_ATOMIC_U32_CAS(&lg->busy, &expected, 1);
}
static void tl_release(tefstd_logger_t *lg) {
    TL_ATOMIC_U32_STORE(&lg->busy, 0);
}

/* ==================== worker ==================== */
static int tl_worker_main(void *arg) {
    (void)arg;
    tl_ts_cache_t tsc = {0};
    char  batch[64 * 1024];
    size_t used_io = 0;
    uint32_t backoff_ms = 1;

    while (TL_ATOMIC_U32_LOAD(&g_pool.running)) {
        int did_work = 0;

        mtx_lock(&g_pool.lock);
        tefstd_logger_t *lg = g_pool.head;
        mtx_unlock(&g_pool.lock);

        while (lg) {
            if (TL_ATOMIC_U32_LOAD(&lg->active) && tl_try_claim(lg)) {
                const int n = tl_drain_logger(lg, &tsc, batch, sizeof(batch), &used_io, 0);
                if (n) {
                    did_work = 1;
                    if (lg->fp) fflush(lg->fp);
                    if (lg->rotate_max_bytes) tl_rotate(lg);
                }
                tl_release(lg);
            }
            mtx_lock(&g_pool.lock);
            tefstd_logger_t *next = lg->pool_next;
            mtx_unlock(&g_pool.lock);
            lg = next;
        }

        if (did_work) {
            backoff_ms = 1;
        } else {
            /* 检查是否有积压但没被处理（用于建议扩 worker） */
            int backlog = 0;
            mtx_lock(&g_pool.lock);
            for (const tefstd_logger_t *it = g_pool.head; it; it = it->pool_next) {
                if (TL_ATOMIC_U32_LOAD(&it->active) &&
                    TL_ATOMIC_U32_LOAD(&it->ring.head) != TL_ATOMIC_U32_LOAD(&it->ring.tail)) {
                    backlog = 1; break;
                }
            }
            const uint32_t wc = g_pool.worker_count;
            mtx_unlock(&g_pool.lock);
            if (backlog && wc < TL_POOL_MAX_WORKERS) {
                TL_ATOMIC_U32_STORE(&g_pool.pending_spawn, 1);
            }

            struct timespec ts;
            ts.tv_sec  = 0;
            ts.tv_nsec = (long)backoff_ms * 1000000L;
            thrd_sleep(&ts, NULL);
            if (backoff_ms < 200) backoff_ms *= 2;
        }
    }

    /* 退出：排干所有活跃 logger */
    mtx_lock(&g_pool.lock);
    tefstd_logger_t *lg = g_pool.head;
    mtx_unlock(&g_pool.lock);
    while (lg) {
        if (TL_ATOMIC_U32_LOAD(&lg->active) && tl_try_claim(lg)) {
            tl_drain_logger(lg, &tsc, batch, sizeof(batch), &used_io, 1);
            tl_release(lg);
        }
        mtx_lock(&g_pool.lock);
        tefstd_logger_t *next = lg->pool_next;
        mtx_unlock(&g_pool.lock);
        lg = next;
    }
    return 0;
}

/* 内部：启动一个 worker（持 boot_lock 调用） */
static bool tl_spawn_one_locked(void) {
    const uint32_t wc = g_pool.worker_count;
    if (wc >= TL_POOL_MAX_WORKERS) return false;
    thrd_t t;
    if (thrd_create(&t, tl_worker_main, NULL) != thrd_success) return false;
    g_pool.workers[wc] = t;
    TL_ATOMIC_U32_STORE(&g_pool.worker_count, wc + 1);
    return true;
}

/* 内部：确保池已启动（至少 1 worker） */
static bool tl_pool_ensure_started(void) {
    if (TL_ATOMIC_U32_LOAD(&g_pool.initialized)) return true;

    if (!TL_ATOMIC_U32_LOAD(&g_pool.boot_ready)) {
        static mtx_t once_lock;
        static bool once_inited = false;
        if (!once_inited) { mtx_init(&once_lock, mtx_plain); once_inited = true; }
        mtx_lock(&once_lock);
        if (!TL_ATOMIC_U32_LOAD(&g_pool.boot_ready)) {
            mtx_init(&g_pool.lock, mtx_plain);
            mtx_init(&g_pool.boot_lock, mtx_plain);
            g_pool.running = 1;
            TL_ATOMIC_U32_STORE(&g_pool.boot_ready, 1);
        }
        mtx_unlock(&once_lock);
    }

    mtx_lock(&g_pool.boot_lock);
    bool ok = true;
    if (g_pool.worker_count < TL_POOL_MIN_WORKERS) {
        ok = tl_spawn_one_locked();
    }
    TL_ATOMIC_U32_STORE(&g_pool.initialized, ok ? 1 : 0);
    mtx_unlock(&g_pool.boot_lock);
    return ok;
}

/* 内部：按需补 worker */
static void tl_pool_maybe_grow(void) {
    if (!TL_ATOMIC_U32_LOAD(&g_pool.pending_spawn)) return;
    TL_ATOMIC_U32_STORE(&g_pool.pending_spawn, 0);
    mtx_lock(&g_pool.boot_lock);
    tl_spawn_one_locked();
    mtx_unlock(&g_pool.boot_lock);
}

/* ==================== 写入路径 ==================== */
static void tl_enqueue(tefstd_logger_t *lg, const tefstd_log_level_t level,
                       const char *msg, size_t len) {
    uint32_t pos;
    tl_slot_hdr_t *s;
    if (!tl_ring_reserve(&lg->ring, &pos, &s)) {
        TL_ATOMIC_U64_INC(&lg->dropped);
        return;
    }
    const uint32_t cap = tl_slot_msg_cap(&lg->ring);
    if (len >= cap) len = cap - 1;
    char *dst = tl_slot_msg(s);
    memcpy(dst, msg, len);
    dst[len] = '\0';
    s->len   = (uint16_t)len;
    s->level = (uint8_t)level;
    tl_ring_publish(&lg->ring, pos, s);

    /* 半满以上提示补 worker */
    const uint32_t h = TL_ATOMIC_U32_LOAD(&lg->ring.head);
    const uint32_t t = TL_ATOMIC_U32_LOAD(&lg->ring.tail);
    if ((h - t) > (lg->ring.cap >> 1)) {
        TL_ATOMIC_U32_STORE(&g_pool.pending_spawn, 1);
    }
}

void tefstd_log_write(tefstd_logger_t *lg,
                                          const tefstd_log_level_t level,
                                          const char *fmt, ...) {
    if (!lg || (int)level < TL_ATOMIC_U32_LOAD((volatile uint32_t *)&lg->min_level)) return;
    char buf[512];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= sizeof(buf)) n = (int)sizeof(buf) - 1;
    tl_enqueue(lg, level, buf, (size_t)n);
}

void tefstd_log_write_ex(tefstd_logger_t *lg,
                                             const tefstd_log_level_t level,
                                             const char *file, const int line,
                                             const char *func,
                                             const char *fmt, ...) {
    if (!lg || (int)level < TL_ATOMIC_U32_LOAD((volatile uint32_t *)&lg->min_level)) return;
    const char *base = strrchr(file, '/');
    if (!base) base = strrchr(file, '\\');
    base = base ? base + 1 : file;

    char body[448];
    va_list ap; va_start(ap, fmt);
    int bn = vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);
    if (bn < 0) return;
    if ((size_t)bn >= sizeof(body)) bn = (int)sizeof(body) - 1;

    char full[512];
    int fn = snprintf(full, sizeof(full), "[%s:%d][%s] %s",
                      base, line, func ? func : "?", body);
    if (fn < 0) return;
    if ((size_t)fn >= sizeof(full)) fn = (int)sizeof(full) - 1;
    tl_enqueue(lg, level, full, (size_t)fn);
}

/* ==================== 生命周期 ==================== */
static uint32_t tl_round_pow2(uint32_t v) {
    if (v < 2) return 2;
    v--; v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
    return v + 1;
}

tefstd_logger_t *tefstd_log_open(const tefstd_log_config_t *cfg) {
    tefstd_logger_t *lg = calloc(1, sizeof(*lg));
    if (!lg) return NULL;

    const uint32_t cap_req = cfg && cfg->queue_capacity ? cfg->queue_capacity : 4096;
    const uint32_t msg_req = cfg && cfg->msg_max ? cfg->msg_max : 256;
    uint32_t msg_max = msg_req < 64 ? 64 : msg_req > 4096 ? 4096 : msg_req;
    msg_max = msg_max + 7u & ~7u;

    lg->batch_size        = cfg && cfg->batch_size ? cfg->batch_size : 256;
    lg->flush_interval_ms = cfg && cfg->flush_interval_ms ? cfg->flush_interval_ms : 200;
    lg->rotate_max_bytes  = cfg ? cfg->rotate_max_bytes : 0;
    lg->rotate_max_files  = cfg && cfg->rotate_max_files ? cfg->rotate_max_files : 5;
    lg->sync_on_critical  = cfg ? cfg->sync_on_critical : true;
    lg->also_console      = cfg ? cfg->also_console : false;
    lg->min_level         = TEFSTDLOG_TRACE;

    const uint32_t cap = tl_round_pow2(cap_req);
    const uint32_t stride = (uint32_t)((sizeof(tl_slot_hdr_t) + msg_max + 7u) & ~7u);
    lg->ring.cap = cap; lg->ring.mask = cap - 1; lg->ring.slot_stride = stride;
    lg->ring.buf = (uint8_t *)calloc((size_t)cap, stride);
    if (!lg->ring.buf) { free(lg); return NULL; }
    for (uint32_t i = 0; i < cap; ++i) tl_slot_at(&lg->ring, i)->seq = i;
    lg->ring.head = 0; lg->ring.tail = 0;

    if (cfg && cfg->path && cfg->path[0]) {
        strncpy(lg->path, cfg->path, sizeof(lg->path) - 1);
        lg->fp = fopen(lg->path, "a");
        if (!lg->fp) { free(lg->ring.buf); free(lg); return NULL; }
        setvbuf(lg->fp, NULL, _IOFBF, 32 * 1024);
    }

    if (!tl_pool_ensure_started()) {
        if (lg->fp) fclose(lg->fp);
        free(lg->ring.buf); free(lg);
        return NULL;
    }

    TL_ATOMIC_U32_STORE((volatile uint32_t *)&lg->active, 1);
    mtx_lock(&g_pool.lock);
    lg->pool_next = g_pool.head;
    g_pool.head = lg;
    mtx_unlock(&g_pool.lock);

    tl_pool_maybe_grow();
    return lg;
}

void tefstd_log_close(tefstd_logger_t *lg) {
    if (!lg) return;

    /* 先从链摘除，再置 inactive，保证 worker 不再新领 */
    mtx_lock(&g_pool.lock);
    tefstd_logger_t **pp = &g_pool.head;
    while (*pp && *pp != lg) pp = &(*pp)->pool_next;
    if (*pp == lg) *pp = lg->pool_next;
    mtx_unlock(&g_pool.lock);

    TL_ATOMIC_U32_STORE(&lg->active, 0);

    /* 等 busy 释放（可能有 worker 正在消费该 logger） */
    while (TL_ATOMIC_U32_LOAD(&lg->busy) != 0) {
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000L};
        thrd_sleep(&ts, NULL);
    }

    /* 自己排干（此时无 worker 会碰它） */
    tl_ts_cache_t tsc = {0};
    char batch[64 * 1024];
    size_t used = 0;
    tl_drain_logger(lg, &tsc, batch, sizeof(batch), &used, 1);

    if (lg->fp) fclose(lg->fp);
    free(lg->ring.buf);
    free(lg);
}

void tefstd_log_set_level(tefstd_logger_t *lg,
                                              const tefstd_log_level_t min_level) {
    if (!lg) return;
    TL_ATOMIC_U32_STORE((volatile uint32_t *)&lg->min_level, min_level);
}

void tefstd_log_flush(tefstd_logger_t *lg) {
    if (!lg) return;
    for (;;) {
        const uint32_t tail = TL_ATOMIC_U32_LOAD(&lg->ring.tail);
        const uint32_t head = TL_ATOMIC_U32_LOAD(&lg->ring.head);
        if (tail == head) break;
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000L};
        thrd_sleep(&ts, NULL);
    }
    if (lg->fp) { fflush(lg->fp); TL_FSYNC_FILE(lg->fp); }
}

uint64_t tefstd_log_dropped(const tefstd_logger_t *lg) {
    return lg ? TL_ATOMIC_U64_LOAD((uint64_t *)&lg->dropped) : 0;
}