/*******************************************************************************
 * tefkernel - crash_handler
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
 * Created: 2026/5/30
 *******************************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "internal/crash_handler.h"
#include "internal/log.h"

// 平台检测
#if defined(__ANDROID__)
    #define PLATFORM_ANDROID 1
#elif defined(_WIN32)
    #define PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
    #define PLATFORM_APPLE 1
#elif defined(__linux__)
    #define PLATFORM_LINUX 1
#endif

// 跨平台基础头文件
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>

// 平台特定头文件
#if defined(PLATFORM_WINDOWS)
    #include <windows.h>
    #include <dbghelp.h>
#elif defined(PLATFORM_ANDROID)
    #include <unistd.h>
    #include <dlfcn.h>
    #include <unwind.h>
#elif defined(PLATFORM_APPLE) || defined(PLATFORM_LINUX)
    #include <unistd.h>
    #include <execinfo.h>
    #include <dlfcn.h>
#endif

#if defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX)
    #include <ucontext.h>
#endif

#define MAX_BACKTRACE_DEPTH 64
static volatile sig_atomic_t g_crash_in_progress = 0;
static pthread_mutex_t g_crash_mutex = PTHREAD_MUTEX_INITIALIZER;

#if defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_APPLE)
/* Mono / IL2CPP 会用 SIGSEGV（以及 SIGFPE 等）实现空引用/除零检查，
 * 并依靠自己的信号处理器把它们转换成可被 C# catch 的异常。
 *
 * 我们不能再像以前那样抢占这些信号后直接 exit：那样任何本来会被游戏
 * catch 掉的 NullReferenceException 都会变成闪退。这里保存原来的
 * handler，低地址故障（空引用）原样转交回去，运行时照常抛异常。 */
#define CRASH_MAX_SIGNAL 64
static struct sigaction g_prev_action[CRASH_MAX_SIGNAL + 1];
static bool g_prev_saved[CRASH_MAX_SIGNAL + 1];
static bool g_handler_installed = false;

static void posix_signal_handler(int signal_code, siginfo_t *info, void *context);

static bool crash_forward_to_previous(int sig, siginfo_t *info, void *ctx) {
    if (sig <= 0 || sig > CRASH_MAX_SIGNAL || !g_prev_saved[sig])
        return false;

    struct sigaction *prev = &g_prev_action[sig];
    if (prev->sa_flags & SA_SIGINFO) {
        if (prev->sa_sigaction == NULL ||
            prev->sa_sigaction == (void (*)(int, siginfo_t *, void *)) SIG_DFL ||
            prev->sa_sigaction == (void (*)(int, siginfo_t *, void *)) SIG_IGN)
            return false;
        sigaction(sig, prev, NULL); /* 恢复原 handler，避免递归 */
        prev->sa_sigaction(sig, info, ctx);
        return true;
    }

    if (prev->sa_handler == SIG_DFL || prev->sa_handler == SIG_IGN)
        return false;
    sigaction(sig, prev, NULL);
    prev->sa_handler(sig);
    return true;
}

static bool crash_install_signal(int sig) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = posix_signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigfillset(&sa.sa_mask);

    struct sigaction old;
    if (sigaction(sig, &sa, &old) != 0)
        return false;
    if (sig > 0 && sig <= CRASH_MAX_SIGNAL) {
        g_prev_action[sig] = old;
        g_prev_saved[sig] = true;
    }
    return true;
}
#endif

/* ---- 崩溃面包屑 ----
 * 正常运行时记录最近发生的事件（收发包、物品操作等），崩溃时一起写日志。
 * 写入路径不加锁：用原子序号占槽，单条 vsnprintf 基本不会被打断。 */
#define CRUMB_SLOTS 24
#define CRUMB_LEN   112
static char g_crumbs[CRUMB_SLOTS][CRUMB_LEN];
static unsigned g_crumb_seq = 0;

void tefkernel_crumb(const char *fmt, ...) {
    if (!fmt) return;
    const unsigned idx = __atomic_fetch_add(&g_crumb_seq, 1u, __ATOMIC_RELAXED)
                         % CRUMB_SLOTS;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_crumbs[idx], CRUMB_LEN, fmt, ap);
    va_end(ap);
    g_crumbs[idx][CRUMB_LEN - 1] = '\0';
}

static void dump_crumbs(void) {
    const unsigned seq = __atomic_load_n(&g_crumb_seq, __ATOMIC_RELAXED);
    if (seq == 0) return;
    TEKLOG_CRITICAL("Recent events (last %u):",
                    seq < CRUMB_SLOTS ? seq : CRUMB_SLOTS);
    const unsigned first = seq > CRUMB_SLOTS ? seq - CRUMB_SLOTS : 0;
    for (unsigned i = first; i < seq; ++i) {
        const char *s = g_crumbs[i % CRUMB_SLOTS];
        if (s[0]) TEKLOG_CRITICAL("  [%u] %s", i, s);
    }
}

// Android 回溯状态结构体
#if defined(PLATFORM_ANDROID)
struct android_backtrace_state {
    void **current;
    void **end;
};

// Android 回溯回调函数
static _Unwind_Reason_Code android_unwind_callback(struct _Unwind_Context *context, void *arg) {
    struct android_backtrace_state *state = (struct android_backtrace_state *)arg;
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc && state->current < state->end) {
        *state->current++ = (void*)pc;
    }
    return _URC_NO_REASON;
}
#endif

// 获取调用栈
static void print_backtrace(void) {
    TEKLOG_CRITICAL("Call stack:");

    #if defined(PLATFORM_WINDOWS)
        void* stack[MAX_BACKTRACE_DEPTH];
        HANDLE process = GetCurrentProcess();
        SymInitialize(process, NULL, TRUE);

        USHORT frames = CaptureStackBackTrace(0, MAX_BACKTRACE_DEPTH, stack, NULL);

        for (int i = 0; i < frames; i++) {
            char symbol_buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
            PSYMBOL_INFO symbol = (PSYMBOL_INFO)symbol_buffer;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;

            DWORD64 displacement = 0;
            if (SymFromAddr(process, (DWORD64)stack[i], &displacement, symbol)) {
                TEKLOG_CRITICAL("  #%02d: %s+0x%llX [0x%p]",
                               i, symbol->Name, displacement, stack[i]);
            } else {
                TEKLOG_CRITICAL("  #%02d: 0x%p", i, stack[i]);
            }
        }

        SymCleanup(process);

    #elif defined(PLATFORM_ANDROID)
        void *buffer[MAX_BACKTRACE_DEPTH];
        struct android_backtrace_state state = {buffer, buffer + MAX_BACKTRACE_DEPTH};
        _Unwind_Backtrace(android_unwind_callback, &state);
        int count = (int)(state.current - buffer);

        for (int i = 0; i < count; i++) {
            Dl_info info;
            if (dladdr(buffer[i], &info) != 0 && info.dli_fname != NULL) {
                /* 只要 dladdr 认出了模块，就必须打印「模块+偏移」。
                 * 以前只在 dli_sname 非空时打印，导致没有导出符号的帧
                 * （libil2cpp 局部函数、内核 static 函数）退化成裸地址，离线无法符号化。 */
                const uintptr_t offset = (uintptr_t) buffer[i] - (uintptr_t) info.dli_fbase;
                if (info.dli_sname != NULL) {
                    TEKLOG_CRITICAL("  #%02d: %s (%s+0x%tx) [%s+0x%tx]",
                                   i, info.dli_fname, info.dli_sname,
                                   (char*)buffer[i] - (char*)info.dli_saddr,
                                   info.dli_fname, offset);
                } else {
                    TEKLOG_CRITICAL("  #%02d: %s+0x%tx", i, info.dli_fname, offset);
                }
            } else {
                TEKLOG_CRITICAL("  #%02d: 0x%p", i, buffer[i]);
            }
        }

    #elif defined(PLATFORM_APPLE) || defined(PLATFORM_LINUX)
        void *buffer[MAX_BACKTRACE_DEPTH];
        const int frames = backtrace(buffer, MAX_BACKTRACE_DEPTH);
        char **symbols = backtrace_symbols(buffer, frames);

        if (symbols != NULL) {
            for (int i = 0; i < frames; i++) {
                TEKLOG_CRITICAL("  #%02d: %s", i, symbols[i]);
            }
            free(symbols);
        }

    #endif
}

#if defined(PLATFORM_WINDOWS)
// Windows 异常处理
static LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS *exception_info) {
    DWORD code = exception_info->ExceptionRecord->ExceptionCode;

    /* CLR (.NET) 自身的异常：0xE0434352 = 托管异常，0xE06D7363 = C++ EH。
     * 这些不是原生崩溃，必须交回 CLR/运行时处理；过去这里直接退出，
     * 会把本来能被游戏捕获的异常（如线程 Abort、音频初始化失败）当成闪退。 */
    if (code == 0xE0434352 || code == 0xE06D7363)
        return EXCEPTION_CONTINUE_SEARCH;

    if (g_crash_in_progress) return EXCEPTION_CONTINUE_SEARCH;
    g_crash_in_progress = 1;

    pthread_mutex_lock(&g_crash_mutex);

    const char *code_name = "UNKNOWN";

    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: code_name = "ACCESS_VIOLATION"; break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO: code_name = "INT_DIVIDE_BY_ZERO"; break;
        case EXCEPTION_STACK_OVERFLOW: code_name = "STACK_OVERFLOW"; break;
        case EXCEPTION_ILLEGAL_INSTRUCTION: code_name = "ILLEGAL_INSTRUCTION"; break;
        case EXCEPTION_IN_PAGE_ERROR: code_name = "IN_PAGE_ERROR"; break;
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: code_name = "ARRAY_BOUNDS_EXCEEDED"; break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: code_name = "FLT_DIVIDE_BY_ZERO"; break;
        case EXCEPTION_FLT_OVERFLOW: code_name = "FLT_OVERFLOW"; break;
        case EXCEPTION_FLT_UNDERFLOW: code_name = "FLT_UNDERFLOW"; break;
        case EXCEPTION_INT_OVERFLOW: code_name = "INT_OVERFLOW"; break;
        default: break;
    }

    TEKLOG_CRITICAL("========================================");
    TEKLOG_CRITICAL("CRASH DETECTED - Windows Exception");
    TEKLOG_CRITICAL("Exception: %s (0x%08X)", code_name, code);
    TEKLOG_CRITICAL("Address: 0x%p", exception_info->ExceptionRecord->ExceptionAddress);

    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR) {
        ULONG_PTR type = exception_info->ExceptionRecord->ExceptionInformation[0];
        ULONG_PTR addr = exception_info->ExceptionRecord->ExceptionInformation[1];
        const char* access_type = "UNKNOWN";

        if (type == 0) access_type = "READ";
        else if (type == 1) access_type = "WRITE";
        else if (type == 8) access_type = "EXECUTE";

        TEKLOG_CRITICAL("Access violation: %s at 0x%p",
                       access_type, (void*)addr);
    }

    print_backtrace();
    TEKLOG_CRITICAL("========================================");

    tefkernel_log_cleanup();
    pthread_mutex_unlock(&g_crash_mutex);

    exit(EXIT_FAILURE);
    return EXCEPTION_EXECUTE_HANDLER;
}

#else // POSIX 系统 (Linux/macOS/Android)

static void posix_signal_handler(int signal_code, siginfo_t *info, void *context) {
    /* 运行时自己的空引用/除零检查：低地址故障直接还给原 handler。
     * 不做日志，避免每次 NullReferenceException 刷屏。 */
    if (signal_code != SIGABRT && info &&
        (uintptr_t) info->si_addr < 0x10000 &&
        crash_forward_to_previous(signal_code, info, context))
        return;

    if (g_crash_in_progress) return;
    g_crash_in_progress = 1;

    pthread_mutex_lock(&g_crash_mutex);

    const char *signal_name = "UNKNOWN";
    switch (signal_code) {
        case SIGSEGV: signal_name = "SIGSEGV (Segmentation Fault)"; break;
        case SIGABRT: signal_name = "SIGABRT (Abort)"; break;
        case SIGFPE:  signal_name = "SIGFPE (Floating Point Exception)"; break;
        case SIGILL:  signal_name = "SIGILL (Illegal Instruction)"; break;
        case SIGBUS:  signal_name = "SIGBUS (Bus Error)"; break;
        case SIGTRAP: signal_name = "SIGTRAP (Trace/breakpoint trap)"; break;
        default: break;
    }

    TEKLOG_CRITICAL("========================================");
    TEKLOG_CRITICAL("CRASH DETECTED - Signal: %s (%d)", signal_name, signal_code);
    TEKLOG_CRITICAL("Fault address: %p", info->si_addr);
    TEKLOG_CRITICAL("PID: %d", getpid());

#if defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX)
    /* 打印故障现场的 PC / SP，backtrace 在信号帧上常常不完整，
     * 有了 PC 才能知道到底是哪条指令炸的（PC=0 即为空函数指针调用）。 */
    if (context) {
        const ucontext_t *uc = (const ucontext_t *) context;
#if defined(__x86_64__)
        const void *pc = (const void *) uc->uc_mcontext.gregs[REG_RIP];
        const void *sp = (const void *) uc->uc_mcontext.gregs[REG_RSP];
        const void *bp = (const void *) uc->uc_mcontext.gregs[REG_RBP];
#elif defined(__aarch64__)
        const void *pc = (const void *) uc->uc_mcontext.pc;
        const void *sp = (const void *) uc->uc_mcontext.sp;
        const void *bp = (const void *) uc->uc_mcontext.regs[29];
#else
        const void *pc = NULL, *sp = NULL, *bp = NULL;
#endif
        TEKLOG_CRITICAL("Fault PC: %p  SP: %p  BP: %p", pc, sp, bp);
        if (pc) {
            Dl_info pc_info;
            if (dladdr(pc, &pc_info) != 0 && pc_info.dli_fname) {
                const uintptr_t off = (uintptr_t) pc - (uintptr_t) pc_info.dli_fbase;
                TEKLOG_CRITICAL("Fault PC module: %s+0x%tx", pc_info.dli_fname, off);
            }
        }
    }
#endif

#if defined(PLATFORM_ANDROID)
    {
        Dl_info self_info;
        if (dladdr((void *) &posix_signal_handler, &self_info) != 0 &&
            self_info.dli_fbase != NULL) {
            TEKLOG_CRITICAL("Kernel base: %p (%s)", self_info.dli_fbase,
                            self_info.dli_fname ? self_info.dli_fname : "?");
        }
    }
#endif

    dump_crumbs();
    print_backtrace();
    TEKLOG_CRITICAL("========================================");

    tefkernel_log_cleanup();

    pthread_mutex_unlock(&g_crash_mutex);

    // 恢复默认处理器并重新触发信号
    struct sigaction default_action = {0};
    default_action.sa_handler = SIG_DFL;
    sigemptyset(&default_action.sa_mask);
    sigaction(signal_code, &default_action, NULL);

    raise(signal_code);
}
#endif

void tefkernel_crash_handler_init(void) {
    TEKLOG_INFO("Initializing crash handler...");

    #if defined(PLATFORM_WINDOWS)
        SetUnhandledExceptionFilter(windows_exception_handler);
        signal(SIGABRT, SIG_DFL);
        // 初始化符号处理
        SymInitialize(GetCurrentProcess(), NULL, TRUE);
        TEKLOG_INFO("Windows exception handler installed");

    #elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_APPLE)
        /* 保存原 handler 再安装，低地址故障会转交给运行时（见文件顶部） */
        if (!g_handler_installed) {
            crash_install_signal(SIGSEGV);
            crash_install_signal(SIGABRT);
            crash_install_signal(SIGFPE);
            crash_install_signal(SIGILL);
            crash_install_signal(SIGBUS);
            crash_install_signal(SIGTRAP);
            g_handler_installed = true;
        }
        TEKLOG_INFO("POSIX signal handlers installed (chained)");
    #endif

    TEKLOG_INFO("Crash handler initialized successfully");
}

// 可选的清理函数
void tefkernel_crash_handler_cleanup(void) {
    TEKLOG_INFO("Cleaning up crash handler...");

    #if defined(PLATFORM_WINDOWS)
        SymCleanup(GetCurrentProcess());
        SetUnhandledExceptionFilter(NULL);
    #elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_APPLE)
        struct sigaction default_action = {0};
        default_action.sa_handler = SIG_DFL;
        sigemptyset(&default_action.sa_mask);

        sigaction(SIGSEGV, &default_action, NULL);
        sigaction(SIGABRT, &default_action, NULL);
        sigaction(SIGFPE, &default_action, NULL);
        sigaction(SIGILL, &default_action, NULL);
        sigaction(SIGBUS, &default_action, NULL);
        sigaction(SIGTRAP, &default_action, NULL);
    #endif

    TEKLOG_INFO("Crash handler cleaned up");
}