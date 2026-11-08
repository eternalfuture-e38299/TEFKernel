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
 * Created: 2026/11/7
 *******************************************************************************/

#include "crash_handler.hpp"
#include "kernel_state.hpp"

#include <array>
#include <atomic>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "tef/core/log.hpp"

/* ============================================================================
 * 平台分支（#if 只能用宏）
 * ============================================================================ */

#if defined(TEF_PLATFORM_ANDROID)
#   define TEF_CRASH_POSIX   1
#   define TEF_CRASH_ANDROID 1
#   include <unistd.h>
#   include <dlfcn.h>
#   include <unwind.h>
#   include <ucontext.h>
#elif defined(TEF_PLATFORM_LINUX)
#   define TEF_CRASH_POSIX   1
#   define TEF_CRASH_ANDROID 0
#   include <unistd.h>
#   include <dlfcn.h>
#   include <execinfo.h>
#   include <ucontext.h>
#elif defined(TEF_PLATFORM_IOS) || defined(TEF_PLATFORM_MACOS)
#   define TEF_CRASH_POSIX   1
#   define TEF_CRASH_ANDROID 0
#   include <csignal>
#   include <unistd.h>
#   include <dlfcn.h>
#   include <execinfo.h>
#elif defined(TEF_PLATFORM_WINDOWS)
#   define TEF_CRASH_POSIX   0
#   define TEF_CRASH_ANDROID 0
#   include <windows.h>
#   include <dbghelp.h>
#else
#   error "Unsupported platform"
#endif

namespace tef::crash {
    /* ============================================================================
     * 常量
     * ============================================================================ */

    namespace {
        constexpr int k_max_backtrace_depth = 64;
        constexpr int k_crumb_slots = 24;
        constexpr int k_crumb_len = 112;
        constexpr int k_max_signal = 64;
    }

    /* ============================================================================
     * 全局状态（信号路径要用，必须非匿名 namespace 才能给 detail 访问）
     * ============================================================================ */

    namespace detail {
        static std::atomic g_crash_in_progress{0};
        static std::atomic<unsigned> g_crumb_seq{0};

        static std::array<std::array<char, k_crumb_len>, k_crumb_slots> g_crumbs{};

#if TEF_CRASH_POSIX
        static std::array<struct sigaction, k_max_signal + 1> g_prev_action{};
        static std::array<bool, k_max_signal + 1> g_prev_saved{};
        static std::atomic g_handler_installed{false};
#endif

        static void crumb_vl(const char *fmt, va_list ap);
    } // namespace detail

    /* ============================================================================
     * 崩溃报告输出（全部走 log::）
     * ============================================================================ */

    namespace {
        void dump_crumbs() {
            const unsigned seq = detail::g_crumb_seq.load(std::memory_order_relaxed);
            if (seq == 0) return;

            const unsigned shown = seq < k_crumb_slots ? seq : k_crumb_slots;
            log::critical("Recent events (last {}):", shown);

            const unsigned first = seq > k_crumb_slots ? seq - k_crumb_slots : 0;
            for (unsigned i = first; i < seq; ++i) {
                if (const char *s = detail::g_crumbs[i % k_crumb_slots].data(); s[0])
                    log::critical("  [{}] {}", i, s);
            }
        }

        /* ---- 调用栈 ---- */

#if TEF_CRASH_ANDROID
        struct android_backtrace_state {
            void **current;
            void **end;
        };

        _Unwind_Reason_Code android_unwind_callback(_Unwind_Context *ctx, void *arg) {
            auto *st = static_cast<android_backtrace_state *>(arg);
            const uintptr_t pc = _Unwind_GetIP(ctx);
            if (pc && st->current < st->end)
                *st->current++ = reinterpret_cast<void *>(pc);
            return _URC_NO_REASON;
        }
#endif

        void print_backtrace() {
            log::critical("Call stack:");

#if TEF_CRASH_ANDROID
            void *buffer[k_max_backtrace_depth];
            android_backtrace_state st{.current = buffer, .end = buffer + k_max_backtrace_depth};
            _Unwind_Backtrace(android_unwind_callback, &st);
            const int count = static_cast<int>(st.current - buffer);

            for (int i = 0; i < count; ++i) {
                Dl_info info{};
                if (::dladdr(buffer[i], &info) != 0 && info.dli_fname) {
                    const auto off = reinterpret_cast<uintptr_t>(buffer[i]) -
                                     reinterpret_cast<uintptr_t>(info.dli_fbase);
                    if (info.dli_sname) {
                        const auto sym_off = reinterpret_cast<uintptr_t>(buffer[i]) -
                                             reinterpret_cast<uintptr_t>(info.dli_saddr);
                        log::critical("  #{:02}: {}+{:#x} [{}]", i, info.dli_sname,
                                      sym_off, info.dli_fname);
                    } else {
                        log::critical("  #{:02}: {}+{:#x}", i, info.dli_fname, off);
                    }
                } else {
                    log::critical("  #{:02}: {:#x}", i,
                                  reinterpret_cast<uintptr_t>(buffer[i]));
                }
            }

#elif TEF_CRASH_POSIX
            void *buffer[k_max_backtrace_depth];
            const int frames = ::backtrace(buffer, k_max_backtrace_depth);
            if (char **symbols = ::backtrace_symbols(buffer, frames)) {
                for (int i = 0; i < frames; ++i)
                    log::critical("  #{:02}: {}", i, symbols[i]);
                std::free(symbols);
            }
#endif
        }

        /* ---- POSIX 信号 ---- */

#if TEF_CRASH_POSIX

        void posix_signal_handler(int sig, siginfo_t *info, void *ctx);

        /*
         * 注意：Android 的 libsigchain 会拦截"信号 handler 里调 sigaction 改 SIGSEGV"
         * 的行为（打 Setting SIGSEGV to SIG_DFL 并撤销链）。因此这里 **不调 sigaction**，
         * 直接调用保存的旧 handler。
         */
        bool forward_to_previous(const int sig, siginfo_t *info, void *ctx) noexcept {
            if (sig <= 0 || sig > k_max_signal || !detail::g_prev_saved[sig])
                return false;

            const struct sigaction *prev = &detail::g_prev_action[sig];

            if (prev->sa_flags & SA_SIGINFO) {
                if (!prev->sa_sigaction ||
                    prev->sa_sigaction ==
                        reinterpret_cast<void (*)(int, siginfo_t *, void *)>(SIG_DFL) ||
                    prev->sa_sigaction ==
                        reinterpret_cast<void (*)(int, siginfo_t *, void *)>(SIG_IGN))
                    return false;
                prev->sa_sigaction(sig, info, ctx);
                return true;
            }

            if (prev->sa_handler == SIG_DFL || prev->sa_handler == SIG_IGN)
                return false;
            prev->sa_handler(sig);
            return true;
        }

        bool install_signal(const int sig) noexcept {
            struct sigaction sa{};
            sa.sa_sigaction = posix_signal_handler;
            sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
            sigfillset(&sa.sa_mask);

            struct sigaction old{};
            if (sigaction(sig, &sa, &old) != 0)
                return false;

            if (sig > 0 && sig <= k_max_signal) {
                detail::g_prev_action[sig] = old;
                detail::g_prev_saved[sig] = true;
            }
            return true;
        }

        void posix_signal_handler(const int sig, siginfo_t *info, void *ctx) {
            if (detail::g_crash_in_progress.exchange(1, std::memory_order_acq_rel))
                return;

            auto sig_name = "UNKNOWN";
            switch (sig) {
                case SIGSEGV: sig_name = "SIGSEGV (Segmentation Fault)"; break;
                case SIGABRT: sig_name = "SIGABRT (Abort)"; break;
                case SIGFPE:  sig_name = "SIGFPE (Floating Point Exception)"; break;
                case SIGILL:  sig_name = "SIGILL (Illegal Instruction)"; break;
                case SIGBUS:  sig_name = "SIGBUS (Bus Error)"; break;
                case SIGTRAP: sig_name = "SIGTRAP (Trace/breakpoint trap)"; break;
                default: break;
            }

            log::critical("========================================");
            log::critical("CRASH DETECTED - Signal: {} ({})", sig_name,
                          static_cast<std::uint64_t>(sig));
            log::critical("Fault address: {:#x}",
                          reinterpret_cast<uintptr_t>(info ? info->si_addr : nullptr));
            log::critical("PID: {}", static_cast<std::uint64_t>(::getpid()));

#if defined(TEF_PLATFORM_ANDROID) || defined(TEF_PLATFORM_LINUX)
            if (ctx) {
                auto *uc = static_cast<const ucontext_t *>(ctx);
#if defined(__x86_64__)
                auto pc = reinterpret_cast<const void *>(uc->uc_mcontext.gregs[REG_RIP]);
                auto sp = reinterpret_cast<const void *>(uc->uc_mcontext.gregs[REG_RSP]);
                auto bp = reinterpret_cast<const void *>(uc->uc_mcontext.gregs[REG_RBP]);
#elif defined(__aarch64__)
                auto *pc = reinterpret_cast<const void *>(uc->uc_mcontext.pc);
                auto *sp = reinterpret_cast<const void *>(uc->uc_mcontext.sp);
                auto *bp = reinterpret_cast<const void *>(uc->uc_mcontext.regs[29]);
#else
                const void *pc = nullptr;
                const void *sp = nullptr;
                const void *bp = nullptr;
#endif
                log::critical("Fault PC: {:#x}  SP: {:#x}  BP: {:#x}",
                              reinterpret_cast<uintptr_t>(pc),
                              reinterpret_cast<uintptr_t>(sp),
                              reinterpret_cast<uintptr_t>(bp));

                if (pc) {
                    Dl_info pi{};
                    if (::dladdr(const_cast<void *>(pc), &pi) != 0 && pi.dli_fname) {
                        const auto off = reinterpret_cast<uintptr_t>(pc) -
                                         reinterpret_cast<uintptr_t>(pi.dli_fbase);
                        log::critical("Fault PC module: {}+{:#x}", pi.dli_fname, off);
                    }
                }
            }
#endif

#if TEF_CRASH_ANDROID
            {
                Dl_info si{};
                if (::dladdr(reinterpret_cast<void *>(&posix_signal_handler), &si) != 0 &&
                    si.dli_fbase) {
                    log::critical("Kernel base: {:#x} ({})",
                                  reinterpret_cast<uintptr_t>(si.dli_fbase),
                                  si.dli_fname ? si.dli_fname : "?");
                }
            }
#endif

            dump_crumbs();
            print_backtrace();
            log::critical("========================================");

            /*
             * 不调用 sigaction(SIG_DFL) / raise(sig)：
             * Android libsigchain 会拦截该行为并撤销链。直接返回，让信号链继续，
             * 最终由链末尾的 SIG_DFL 终止进程（tombstone 仍由系统写）。
             */

            // 恢复默认并 re-raise
            struct sigaction sa{};
            sa.sa_handler = SIG_DFL;
            sigemptyset(&sa.sa_mask);
            sigaction(sig, &sa, nullptr);   // ← libsigchain 在这里拦截
            raise(sig);
        }

#endif // TEF_CRASH_POSIX

        /* ---- Windows SEH ---- */

#if defined(TEF_PLATFORM_WINDOWS)

        LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS *info) {
            const DWORD code = info->ExceptionRecord->ExceptionCode;

            /* CLR / C++ EH 交回运行时 */
            if (code == 0xE0434352 || code == 0xE06D7363)
                return EXCEPTION_CONTINUE_SEARCH;

            if (detail::g_crash_in_progress.exchange(1, std::memory_order_acq_rel))
                return EXCEPTION_CONTINUE_SEARCH;

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

            log::critical("========================================");
            log::critical("CRASH DETECTED - Windows Exception");
            log::critical("Exception: {} (0x{:08X})", code_name, code);
            log::critical("Address: {}", fmt::ptr(info->ExceptionRecord->ExceptionAddress));

            if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR) {
                const ULONG_PTR type = info->ExceptionRecord->ExceptionInformation[0];
                const ULONG_PTR addr = info->ExceptionRecord->ExceptionInformation[1];
                const char *at = "UNKNOWN";
                if (type == 0) at = "READ";
                else if (type == 1) at = "WRITE";
                else if (type == 8) at = "EXECUTE";
                log::critical("Access violation: {} at {}", at,
                              fmt::ptr(reinterpret_cast<void *>(addr)));
            }

            void *stack[k_max_backtrace_depth];
            HANDLE process = ::GetCurrentProcess();
            ::SymInitialize(process, nullptr, TRUE);
            const USHORT frames = ::CaptureStackBackTrace(
                0, k_max_backtrace_depth, stack, nullptr);

            char symbol_buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
            auto *symbol = reinterpret_cast<PSYMBOL_INFO>(symbol_buf);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;

            for (USHORT i = 0; i < frames; ++i) {
                DWORD64 disp = 0;
                if (::SymFromAddr(process, reinterpret_cast<DWORD64>(stack[i]), &disp, symbol)) {
                    log::critical("  #{:02}: {}+0x{:X} [{}]",
                                  i, symbol->Name,
                                  static_cast<unsigned long long>(disp),
                                  fmt::ptr(stack[i]));
                } else {
                    log::critical("  #{:02}: {}", i, fmt::ptr(stack[i]));
                }
            }

            ::SymCleanup(process);
            log::critical("========================================");
            log::shutdown();
            std::_Exit(EXIT_FAILURE);
        }

#endif // TEF_PLATFORM_WINDOWS
    } // namespace (anonymous)

    /* ============================================================================
     * 面包屑（detail + 公开）
     * ============================================================================ */

    namespace detail {
        void crumb_vl(const char *fmt, va_list ap) {
            if (!fmt) return;
            const unsigned idx =
                    g_crumb_seq.fetch_add(1, std::memory_order_relaxed) % k_crumb_slots;
            std::vsnprintf(g_crumbs[idx].data(), k_crumb_len, fmt, ap);
            g_crumbs[idx][k_crumb_len - 1] = '\0';
        }
    } // namespace detail

    void crumb(const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        detail::crumb_vl(fmt, ap);
        va_end(ap);
    }

    /* ============================================================================
     * Init / Cleanup
     * ============================================================================ */

    void init() {
        log::info("Initializing crash handler...");

#if defined(TEF_PLATFORM_WINDOWS)
        ::SetUnhandledExceptionFilter(windows_exception_handler);
        ::signal(SIGABRT, SIG_DFL);
        ::SymInitialize(::GetCurrentProcess(), nullptr, TRUE);
        log::info("Windows exception handler installed");
#elif TEF_CRASH_POSIX
        if (!detail::g_handler_installed.exchange(true, std::memory_order_acq_rel)) {
            install_signal(SIGSEGV);
            install_signal(SIGABRT);
            install_signal(SIGFPE);
            install_signal(SIGILL);
            install_signal(SIGBUS);
            install_signal(SIGTRAP);
        }
        log::info("POSIX signal handlers installed (chained)");
#endif

        log::info("Crash handler initialized");
    }

    void cleanup() {
        log::info("Cleaning up crash handler...");

#if defined(TEF_PLATFORM_WINDOWS)
        ::SymCleanup(::GetCurrentProcess());
        ::SetUnhandledExceptionFilter(nullptr);
#elif TEF_CRASH_POSIX
        struct sigaction sa{};
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGSEGV, &sa, nullptr);
        sigaction(SIGABRT, &sa, nullptr);
        sigaction(SIGFPE, &sa, nullptr);
        sigaction(SIGILL, &sa, nullptr);
        sigaction(SIGBUS, &sa, nullptr);
        sigaction(SIGTRAP, &sa, nullptr);
#endif

        log::info("Crash handler cleaned up");
    }
}