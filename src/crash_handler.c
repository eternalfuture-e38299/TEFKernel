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
#endif

#define MAX_BACKTRACE_DEPTH 64
static volatile sig_atomic_t g_crash_in_progress = 0;
static pthread_mutex_t g_crash_mutex = PTHREAD_MUTEX_INITIALIZER;

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
            if (dladdr(buffer[i], &info) != 0 && info.dli_sname != NULL) {
                TEKLOG_CRITICAL("  #%02d: %s (%s+0x%tx)",
                               i, info.dli_fname, info.dli_sname,
                               (char*)buffer[i] - (char*)info.dli_saddr);
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
    if (g_crash_in_progress) return EXCEPTION_CONTINUE_SEARCH;
    g_crash_in_progress = 1;

    pthread_mutex_lock(&g_crash_mutex);

    DWORD code = exception_info->ExceptionRecord->ExceptionCode;
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
    (void)context; // 抑制未使用参数警告

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

    #elif defined(PLATFORM_ANDROID)
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = posix_signal_handler;
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&sa.sa_mask);

        // 阻塞所有信号在信号处理期间
        sigfillset(&sa.sa_mask);

        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGABRT, &sa, NULL);
        sigaction(SIGFPE, &sa, NULL);
        sigaction(SIGILL, &sa, NULL);
        sigaction(SIGBUS, &sa, NULL);
        sigaction(SIGTRAP, &sa, NULL);

        TEKLOG_INFO("Android signal handlers installed");

    #else // Linux/macOS
        struct sigaction sa = {0};
        sa.sa_sigaction = posix_signal_handler;
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
        sigemptyset(&sa.sa_mask);

        // 添加额外的安全信号
        sigaddset(&sa.sa_mask, SIGSEGV);
        sigaddset(&sa.sa_mask, SIGABRT);
        sigaddset(&sa.sa_mask, SIGFPE);
        sigaddset(&sa.sa_mask, SIGILL);
        sigaddset(&sa.sa_mask, SIGBUS);
        sigaddset(&sa.sa_mask, SIGTRAP);

        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGABRT, &sa, NULL);
        sigaction(SIGFPE, &sa, NULL);
        sigaction(SIGILL, &sa, NULL);
        sigaction(SIGBUS, &sa, NULL);
        sigaction(SIGTRAP, &sa, NULL);

        TEKLOG_INFO("POSIX signal handlers installed");
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