/*******************************************************************************
 * tefkernel - platform_threads
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
 * Created: 2026/5/5
 *******************************************************************************/

#ifndef TEFKERNEL_PLATFORM_THREADS_H
#define TEFKERNEL_PLATFORM_THREADS_H

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <time.h>
#include <errno.h>

// Windows 版本的 C11 线程类型
typedef HANDLE thrd_t;
typedef CRITICAL_SECTION mtx_t;

// 互斥锁类型
#define mtx_plain 0
#define mtx_recursive 1
#define mtx_timed 2

// 线程返回值
#define thrd_success 0
#define thrd_error 1
#define thrd_timedout 2
#define thrd_busy 3

// 线程函数类型
typedef int (*thrd_start_t)(void*);

// 包装函数
static DWORD WINAPI thrd_wrapper(LPVOID arg) {
    thrd_start_t func = (thrd_start_t)arg;
    return (DWORD)func(NULL);
}

// 创建线程
static inline int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
    *thr = CreateThread(NULL, 0, thrd_wrapper, (LPVOID)func, 0, NULL);
    return (*thr != NULL) ? thrd_success : thrd_error;
}

// 等待线程
static inline int thrd_join(thrd_t thr, int *res) {
    if (WaitForSingleObject(thr, INFINITE) == WAIT_FAILED) {
        return thrd_error;
    }
    CloseHandle(thr);
    if (res) *res = 0;
    return thrd_success;
}

// 线程睡眠
static inline int thrd_sleep(const struct timespec *duration, struct timespec *remaining) {
    DWORD ms = (DWORD)(duration->tv_sec * 1000 + duration->tv_nsec / 1000000);
    Sleep(ms);
    if (remaining) {
        remaining->tv_sec = 0;
        remaining->tv_nsec = 0;
    }
    return 0;
}

// 初始化互斥锁
static inline int mtx_init(mtx_t *mtx, int type) {
    (void)type;
    InitializeCriticalSection(mtx);
    return thrd_success;
}

// 锁定互斥锁
static inline int mtx_lock(mtx_t *mtx) {
    EnterCriticalSection(mtx);
    return thrd_success;
}

// 尝试锁定互斥锁
static inline int mtx_trylock(mtx_t *mtx) {
    return TryEnterCriticalSection(mtx) ? thrd_success : thrd_busy;
}

// 解锁互斥锁
static inline int mtx_unlock(mtx_t *mtx) {
    LeaveCriticalSection(mtx);
    return thrd_success;
}

// 销毁互斥锁
static inline void mtx_destroy(mtx_t *mtx) {
    DeleteCriticalSection(mtx);
}

#elif defined(__APPLE__)
// macOS 使用 POSIX pthread
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

// 定义 C11 线程类型映射到 pthread
typedef pthread_t thrd_t;
typedef pthread_mutex_t mtx_t;

// 互斥锁类型常量
#define mtx_plain 0
#define mtx_recursive 1
#define mtx_timed 2

// 线程返回值常量
#define thrd_success 0
#define thrd_error 1
#define thrd_timedout 2
#define thrd_busy 3

// 线程函数类型
typedef int (*thrd_start_t)(void*);

// 包装函数：将返回 int 的线程函数适配为返回 void*
static void* thrd_wrapper(void* arg) {
    thrd_start_t func = (thrd_start_t)arg;
    int result = func(NULL);
    return (void*)(intptr_t)result;
}

// 创建线程
static inline int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
    int ret = pthread_create(thr, NULL, thrd_wrapper, (void*)func);
    return (ret == 0) ? thrd_success : thrd_error;
}

// 等待线程结束并获取返回值
static inline int thrd_join(thrd_t thr, int *res) {
    void *retval;
    int ret = pthread_join(thr, &retval);
    if (ret != 0) return thrd_error;
    if (res) *res = (int)(intptr_t)retval;
    return thrd_success;
}

// 线程睡眠（纳秒级精度）
static inline int thrd_sleep(const struct timespec *duration, struct timespec *remaining) {
    return nanosleep(duration, remaining);
}

// 初始化互斥锁
static inline int mtx_init(mtx_t *mtx, int type) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);

    if (type == mtx_recursive) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    } else {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    }

    int ret = pthread_mutex_init(mtx, &attr);
    pthread_mutexattr_destroy(&attr);
    return (ret == 0) ? thrd_success : thrd_error;
}

// 锁定互斥锁
static inline int mtx_lock(mtx_t *mtx) {
    return (pthread_mutex_lock(mtx) == 0) ? thrd_success : thrd_error;
}

// 尝试锁定互斥锁（非阻塞）
static inline int mtx_trylock(mtx_t *mtx) {
    int ret = pthread_mutex_trylock(mtx);
    if (ret == 0) return thrd_success;
    if (ret == EBUSY) return thrd_busy;
    return thrd_error;
}

// 解锁互斥锁
static inline int mtx_unlock(mtx_t *mtx) {
    return (pthread_mutex_unlock(mtx) == 0) ? thrd_success : thrd_error;
}

// 销毁互斥锁
static inline void mtx_destroy(mtx_t *mtx) {
    pthread_mutex_destroy(mtx);
}

#else
// Linux 和其他 POSIX 系统使用标准 C11 线程
#include <threads.h>
#include <time.h>
#endif

#endif // TEFKERNEL_PLATFORM_THREADS_H
