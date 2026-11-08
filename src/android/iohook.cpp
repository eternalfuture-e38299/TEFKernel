/*******************************************************************************
 * tefkernel - iohook
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
 * Created: 2026/4/4
 *******************************************************************************/

#include "cpcall.h"

#include <dobby.h>
#include <android/log.h>

#include <string>
#include <string_view>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <cerrno>
#include <vector>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kernel_state.hpp"

// ============================================================================
// Android 原生日志
// ============================================================================
namespace {
    constexpr const char *kTag = "TEFKernel/IOHook";

#if defined(__GNUC__) || defined(__clang__)
#  define TEF_LOG_ATTR __attribute__((format(printf, 1, 2)))
#else
#  define TEF_LOG_ATTR
#endif

#define log_d(fmt, ...) \
    __android_log_print(ANDROID_LOG_DEBUG, kTag, fmt, ##__VA_ARGS__)

#define log_i(fmt, ...) \
    __android_log_print(ANDROID_LOG_INFO, kTag, fmt, ##__VA_ARGS__)

#define log_e(fmt, ...) \
    __android_log_print(ANDROID_LOG_ERROR, kTag, fmt, ##__VA_ARGS__)
}

// ============================================================================
// 函数类型
// ============================================================================
using open_func_t         = int (*)(const char *, int, ...);
using openat_func_t       = int (*)(int, const char *, int, ...);
using unlink_func_t       = int (*)(const char *);
using unlinkat_func_t     = int (*)(int, const char *, int);
using rmdir_func_t        = int (*)(const char *);
using mkdir_func_t        = int (*)(const char *, mode_t);
using mkdirat_func_t      = int (*)(int, const char *, mode_t);
using rename_func_t       = int (*)(const char *, const char *);
using renameat_func_t     = int (*)(int, const char *, int, const char *);
using renameat2_func_t    = int (*)(int, const char *, int, const char *, unsigned int);
using stat_func_t         = int (*)(const char *, struct stat *);
using fstatat_func_t      = int (*)(int, const char *, struct stat *, int);
using access_func_t       = int (*)(const char *, int);
using faccessat_func_t    = int (*)(int, const char *, int, int);
using faccessat2_func_t   = int (*)(int, const char *, int, int);
using opendir_func_t      = DIR *(*)(const char *);
using realpath_func_t     = char *(*)(const char *, char *);
using truncate_func_t     = int (*)(const char *, off_t);
using truncate64_func_t   = int (*)(const char *, off64_t);

// ============================================================================
// 原始函数指针
// ============================================================================
static open_func_t        orig_open        = nullptr;
static openat_func_t      orig_openat      = nullptr;
static unlink_func_t      orig_unlink      = nullptr;
static unlinkat_func_t    orig_unlinkat    = nullptr;
static rmdir_func_t       orig_rmdir       = nullptr;
static mkdir_func_t       orig_mkdir       = nullptr;
static mkdirat_func_t     orig_mkdirat     = nullptr;
static rename_func_t      orig_rename      = nullptr;
static renameat_func_t    orig_renameat    = nullptr;
static stat_func_t        orig_stat        = nullptr;
static fstatat_func_t     orig_fstatat     = nullptr;
static access_func_t      orig_access      = nullptr;
static faccessat_func_t   orig_faccessat   = nullptr;
static opendir_func_t     orig_opendir     = nullptr;
static realpath_func_t    orig_realpath    = nullptr;
static truncate_func_t    orig_truncate    = nullptr;
static truncate64_func_t  orig_truncate64  = nullptr;

// ============================================================================
// 路径拦截判断
// ============================================================================
bool cp_should_intercept(const char *path) {
    if (!path || !*path) return false;

    const char *wd = tef::state::working_dir();
    if (!wd || !*wd) return false;

    return std::strstr(path, wd) != nullptr;
}

// ============================================================================
// 可变参数辅助
// ============================================================================
static mode_t fetch_mode(const int flags, va_list args) {
    if (flags & (O_CREAT | O_TMPFILE)) return va_arg(args, mode_t);
    return 0;
}

static mode_t read_mode(const int flags, ...) {
    va_list args;
    va_start(args, flags);
    const mode_t m = fetch_mode(flags, args);
    va_end(args);
    return m;
}

// ============================================================================
// 各系统调用 hook
// ============================================================================
static int my_open(const char *path, int flags, ...) {
    const mode_t mode = read_mode(flags);

    if (cp_should_intercept(path)) {
        log_i("Hook open: %s (flags: 0x%x, mode: 0%o)", path, flags, mode);
        return cp_call_open(path, flags, mode);
    }

    if (!orig_open) { errno = ENOSYS; return -1; }
    return (flags & (O_CREAT | O_TMPFILE))
        ? orig_open(path, flags, mode)
        : orig_open(path, flags);
}

static int my_openat(int dirfd, const char *path, int flags, ...) {
    const mode_t mode = read_mode(flags);

    if (cp_should_intercept(path)) {
        log_i("Hook openat: %s (dirfd: %d, flags: 0x%x)", path, dirfd, flags);
        return cp_call_open(path, flags, mode);
    }

    if (!orig_openat) { errno = ENOSYS; return -1; }
    return (flags & (O_CREAT | O_TMPFILE))
        ? orig_openat(dirfd, path, flags, mode)
        : orig_openat(dirfd, path, flags, 0);
}

static int my_unlink(const char *path) {
    if (cp_should_intercept(path)) {
        log_i("Hook unlink: %s", path);
        return cp_call_unlink(path);
    }
    return orig_unlink ? orig_unlink(path) : -1;
}

static int my_unlinkat(int dirfd, const char *path, int flags) {
    if (cp_should_intercept(path)) {
        log_i("Hook unlinkat: %s (dirfd: %d)", path, dirfd);
        return cp_call_unlink(path);
    }
    return orig_unlinkat ? orig_unlinkat(dirfd, path, flags) : -1;
}

static int my_rmdir(const char *path) {
    if (cp_should_intercept(path)) {
        log_i("Hook rmdir: %s", path);
        return cp_call_rmdir(path);
    }
    return orig_rmdir ? orig_rmdir(path) : -1;
}

static int my_mkdir(const char *path, mode_t mode) {
    if (cp_should_intercept(path)) {
        log_i("Hook mkdir: %s (mode: 0%o)", path, mode);
        return cp_call_mkdir(path, mode);
    }
    return orig_mkdir ? orig_mkdir(path, mode) : -1;
}

static int my_mkdirat(int dirfd, const char *path, mode_t mode) {
    if (cp_should_intercept(path)) {
        log_i("Hook mkdirat: %s (dirfd: %d)", path, dirfd);
        return cp_call_mkdir(path, mode);
    }
    return orig_mkdirat ? orig_mkdirat(dirfd, path, mode) : -1;
}

static int my_rename(const char *oldpath, const char *newpath) {
    if (cp_should_intercept(oldpath) || cp_should_intercept(newpath)) {
        log_i("Hook rename: %s -> %s", oldpath, newpath);
        return cp_call_rename(oldpath, newpath);
    }
    return orig_rename ? orig_rename(oldpath, newpath) : -1;
}

static int my_renameat(int olddirfd, const char *oldpath,
                       int newdirfd, const char *newpath) {
    if (cp_should_intercept(oldpath) || cp_should_intercept(newpath)) {
        log_i("Hook renameat: %s -> %s", oldpath, newpath);
        return cp_call_rename(oldpath, newpath);
    }
    return orig_renameat ? orig_renameat(olddirfd, oldpath, newdirfd, newpath) : -1;
}

static int my_stat(const char *path, struct stat *buf) {
    if (cp_should_intercept(path)) {
        log_i("Hook stat: %s", path);
        return cp_call_stat(path, buf);
    }
    return orig_stat ? orig_stat(path, buf) : -1;
}

static int my_fstatat(int dirfd, const char *path, struct stat *buf, int flags) {
    if (cp_should_intercept(path)) {
        log_i("Hook fstatat: %s (dirfd: %d)", path, dirfd);
        return cp_call_stat(path, buf);
    }
    return orig_fstatat ? orig_fstatat(dirfd, path, buf, flags) : -1;
}

static int my_access(const char *path, int mode) {
    if (cp_should_intercept(path)) {
        log_i("Hook access: %s (mode: 0x%x)", path, mode);
        return cp_call_access(path, mode);
    }
    return orig_access ? orig_access(path, mode) : -1;
}

static int my_faccessat(int dirfd, const char *path, int mode, int flags) {
    if (cp_should_intercept(path)) {
        log_i("Hook faccessat: %s (dirfd: %d)", path, dirfd);
        return cp_call_access(path, mode);
    }
    return orig_faccessat ? orig_faccessat(dirfd, path, mode, flags) : -1;
}

static DIR *my_opendir(const char *name) {
    if (cp_should_intercept(name)) {
        log_i("Hook opendir: %s", name);
        return cp_call_opendir(name);
    }
    return orig_opendir ? orig_opendir(name) : nullptr;
}

static char *my_realpath(const char *path, char *resolved_path) {
    if (cp_should_intercept(path)) {
        log_i("Hook realpath: %s", path);
        return cp_call_realpath(path, resolved_path);
    }
    return orig_realpath ? orig_realpath(path, resolved_path) : nullptr;
}

static int my_truncate(const char *path, off_t length) {
    if (cp_should_intercept(path)) {
        log_i("Hook truncate: %s (length: %lld)", path, static_cast<long long>(length));
        return cp_call_truncate(path, length);
    }
    return orig_truncate ? orig_truncate(path, length) : -1;
}

// ============================================================================
// Hook 辅助
// ============================================================================
static bool hook_symbol(std::string_view symbol, void *replace_fn, void **orig_fn) {
    void *target = DobbySymbolResolver("libc.so", std::string(symbol).c_str());
    if (!target) {
        if (symbol == "open")        target = DobbySymbolResolver("libc.so", "__open");
        else if (symbol == "stat")   target = DobbySymbolResolver("libc.so", "__stat");
        else if (symbol == "access") target = DobbySymbolResolver("libc.so", "__access");

        if (!target) {
            log_e("Failed to resolve symbol: %s", std::string(symbol).c_str());
            return false;
        }
    }

    if (DobbyHook(target, replace_fn, orig_fn) != 0) {
        log_e("Failed to hook %s", std::string(symbol).c_str());
        return false;
    }

    log_d("Hooked %s at %p", std::string(symbol).c_str(), target);
    return true;
}

static void install_hooks() {
    log_i("Installing IO hooks...");
    int n = 0;

    if (hook_symbol("open",        reinterpret_cast<void *>(&my_open),        reinterpret_cast<void **>(&orig_open)))        ++n;
    if (hook_symbol("unlink",      reinterpret_cast<void *>(&my_unlink),      reinterpret_cast<void **>(&orig_unlink)))      ++n;
    if (hook_symbol("rmdir",       reinterpret_cast<void *>(&my_rmdir),       reinterpret_cast<void **>(&orig_rmdir)))       ++n;
    if (hook_symbol("mkdir",       reinterpret_cast<void *>(&my_mkdir),       reinterpret_cast<void **>(&orig_mkdir)))       ++n;
    if (hook_symbol("rename",      reinterpret_cast<void *>(&my_rename),      reinterpret_cast<void **>(&orig_rename)))      ++n;
    if (hook_symbol("stat",        reinterpret_cast<void *>(&my_stat),        reinterpret_cast<void **>(&orig_stat)))        ++n;
    if (hook_symbol("access",      reinterpret_cast<void *>(&my_access),      reinterpret_cast<void **>(&orig_access)))      ++n;
    if (hook_symbol("opendir",     reinterpret_cast<void *>(&my_opendir),     reinterpret_cast<void **>(&orig_opendir)))     ++n;
    if (hook_symbol("realpath",    reinterpret_cast<void *>(&my_realpath),    reinterpret_cast<void **>(&orig_realpath)))    ++n;
    if (hook_symbol("truncate",    reinterpret_cast<void *>(&my_truncate),    reinterpret_cast<void **>(&orig_truncate)))    ++n;

    if (hook_symbol("openat",      reinterpret_cast<void *>(&my_openat),      reinterpret_cast<void **>(&orig_openat)))      ++n;
    if (hook_symbol("unlinkat",    reinterpret_cast<void *>(&my_unlinkat),    reinterpret_cast<void **>(&orig_unlinkat)))    ++n;
    if (hook_symbol("mkdirat",     reinterpret_cast<void *>(&my_mkdirat),     reinterpret_cast<void **>(&orig_mkdirat)))     ++n;
    if (hook_symbol("renameat",    reinterpret_cast<void *>(&my_renameat),    reinterpret_cast<void **>(&orig_renameat)))    ++n;
    if (hook_symbol("fstatat",     reinterpret_cast<void *>(&my_fstatat),     reinterpret_cast<void **>(&orig_fstatat)))     ++n;
    if (hook_symbol("faccessat",   reinterpret_cast<void *>(&my_faccessat),   reinterpret_cast<void **>(&orig_faccessat)))   ++n;

    log_i("Installed %d IO hooks", n);
}

// ============================================================================
// 公开入口
// ============================================================================
void init_iohook(JavaVM *vm) {
    cp_call_init(vm);

    if (char *dir = cp_call_get_external_dir(); dir) {
        tef::state::set_working_dir(dir);
    } else {
        log_e("cp_call_get_external_dir returned null");
    }

    install_hooks();
}