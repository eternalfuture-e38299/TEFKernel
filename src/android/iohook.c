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
 * Created: 2026/11/14
 *******************************************************************************/

/*******************************************************************************
 * tefkernel - iohook
 * Copyright (C) 2026 eternalfuture-e38299
 * ...（许可证头不变）
 *******************************************************************************/

#include "cpcall.h"

#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <android/log.h>

#include "shadowhook.h"
#include "xdl.h"
#include "internal/kernel_state.h"

typedef int (*open_func_t)(const char *, int, ...);
typedef int (*openat_func_t)(int, const char *, int, ...);
typedef int (*unlink_func_t)(const char *);
typedef int (*unlinkat_func_t)(int, const char *, int);
typedef int (*rmdir_func_t)(const char *);
typedef int (*mkdir_func_t)(const char *, mode_t);
typedef int (*mkdirat_func_t)(int, const char *, mode_t);
typedef int (*rename_func_t)(const char *, const char *);
typedef int (*renameat_func_t)(int, const char *, int, const char *);
typedef int (*stat_func_t)(const char *, struct stat *);
typedef int (*fstatat_func_t)(int, const char *, struct stat *, int);
typedef int (*access_func_t)(const char *, int);
typedef int (*faccessat_func_t)(int, const char *, int, int);
typedef DIR *(*opendir_func_t)(const char *);
typedef char *(*realpath_func_t)(const char *, char *);
typedef int (*truncate_func_t)(const char *, off_t);

static open_func_t orig_open = NULL;
static openat_func_t orig_openat = NULL;
static unlink_func_t orig_unlink = NULL;
static unlinkat_func_t orig_unlinkat = NULL;
static rmdir_func_t orig_rmdir = NULL;
static mkdir_func_t orig_mkdir = NULL;
static mkdirat_func_t orig_mkdirat = NULL;
static rename_func_t orig_rename = NULL;
static renameat_func_t orig_renameat = NULL;
static stat_func_t orig_stat = NULL;
static fstatat_func_t orig_fstatat = NULL;
static access_func_t orig_access = NULL;
static faccessat_func_t orig_faccessat = NULL;
static opendir_func_t orig_opendir = NULL;
static realpath_func_t orig_realpath = NULL;
static truncate_func_t orig_truncate = NULL;

#define TAG "CPCall"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static bool cp_should_intercept(const char *path) {
    if (!path || !*path) return false;
    if (!tefkernel_working_dir) return false;
    return strstr(path, tefkernel_working_dir) != NULL;
}

static int my_open(const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    if (cp_should_intercept(path)) {
        LOGI("Hook open: %s (flags: 0x%x, mode: 0%o)", path, flags, mode);
        return cp_call_open(path, flags, mode);
    }
    if (orig_open) {
        return (flags & (O_CREAT | O_TMPFILE))
            ? orig_open(path, flags, mode)
            : orig_open(path, flags);
    }
    errno = ENOSYS;
    return -1;
}

static int my_openat(int dirfd, const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    if (cp_should_intercept(path)) {
        LOGI("Hook openat: %s (dirfd: %d, flags: 0x%x)", path, dirfd, flags);
        return cp_call_open(path, flags, mode);
    }
    if (orig_openat) {
        return (flags & (O_CREAT | O_TMPFILE))
            ? orig_openat(dirfd, path, flags, mode)
            : orig_openat(dirfd, path, flags, 0);
    }
    errno = ENOSYS;
    return -1;
}

static int my_unlink(const char *path) {
    if (cp_should_intercept(path)) {
        LOGI("Hook unlink: %s", path);
        return cp_call_unlink(path);
    }
    return orig_unlink ? orig_unlink(path) : -1;
}

static int my_unlinkat(int dirfd, const char *path, int flags) {
    if (cp_should_intercept(path)) {
        LOGI("Hook unlinkat: %s (dirfd: %d)", path, dirfd);
        return cp_call_unlink(path);
    }
    return orig_unlinkat ? orig_unlinkat(dirfd, path, flags) : -1;
}

static int my_rmdir(const char *path) {
    if (cp_should_intercept(path)) {
        LOGI("Hook rmdir: %s", path);
        return cp_call_rmdir(path);
    }
    return orig_rmdir ? orig_rmdir(path) : -1;
}

static int my_mkdir(const char *path, mode_t mode) {
    if (cp_should_intercept(path)) {
        LOGI("Hook mkdir: %s (mode: 0%o)", path, mode);
        return cp_call_mkdir(path, mode);
    }
    return orig_mkdir ? orig_mkdir(path, mode) : -1;
}

static int my_mkdirat(int dirfd, const char *path, mode_t mode) {
    if (cp_should_intercept(path)) {
        LOGI("Hook mkdirat: %s (dirfd: %d)", path, dirfd);
        return cp_call_mkdir(path, mode);
    }
    return orig_mkdirat ? orig_mkdirat(dirfd, path, mode) : -1;
}

static int my_rename(const char *oldpath, const char *newpath) {
    if (cp_should_intercept(oldpath) || cp_should_intercept(newpath)) {
        LOGI("Hook rename: %s -> %s", oldpath, newpath);
        return cp_call_rename(oldpath, newpath);
    }
    return orig_rename ? orig_rename(oldpath, newpath) : -1;
}

static int my_renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath) {
    if (cp_should_intercept(oldpath) || cp_should_intercept(newpath)) {
        LOGI("Hook renameat: %s -> %s", oldpath, newpath);
        return cp_call_rename(oldpath, newpath);
    }
    return orig_renameat ? orig_renameat(olddirfd, oldpath, newdirfd, newpath) : -1;
}

static int my_stat(const char *path, struct stat *buf) {
    if (cp_should_intercept(path)) {
        LOGI("Hook stat: %s", path);
        return cp_call_stat(path, buf);
    }
    return orig_stat ? orig_stat(path, buf) : -1;
}

static int my_fstatat(int dirfd, const char *path, struct stat *buf, int flags) {
    if (cp_should_intercept(path)) {
        LOGI("Hook fstatat: %s (dirfd: %d)", path, dirfd);
        return cp_call_stat(path, buf);
    }
    return orig_fstatat ? orig_fstatat(dirfd, path, buf, flags) : -1;
}

static int my_access(const char *path, int mode) {
    if (cp_should_intercept(path)) {
        LOGI("Hook access: %s (mode: 0x%x)", path, mode);
        return cp_call_access(path, mode);
    }
    return orig_access ? orig_access(path, mode) : -1;
}

static int my_faccessat(int dirfd, const char *path, int mode, int flags) {
    if (cp_should_intercept(path)) {
        LOGI("Hook faccessat: %s (dirfd: %d)", path, dirfd);
        return cp_call_access(path, mode);
    }
    return orig_faccessat ? orig_faccessat(dirfd, path, mode, flags) : -1;
}

static DIR *my_opendir(const char *name) {
    if (cp_should_intercept(name)) {
        LOGI("Hook opendir: %s", name);
        return cp_call_opendir(name);
    }
    return orig_opendir ? orig_opendir(name) : NULL;
}

static char *my_realpath(const char *path, char *resolved_path) {
    if (cp_should_intercept(path)) {
        LOGI("Hook realpath: %s", path);
        return cp_call_realpath(path, resolved_path);
    }
    return orig_realpath ? orig_realpath(path, resolved_path) : NULL;
}

static int my_truncate(const char *path, off_t length) {
    if (cp_should_intercept(path)) {
        LOGI("Hook truncate: %s (length: %lld)", path, (long long)length);
        return cp_call_truncate(path, length);
    }
    return orig_truncate ? orig_truncate(path, length) : -1;
}

/* ---------- Hook 辅助 ---------- */
static bool hook_symbol(const char *symbol, void *replace_func, void **original_func) {
    void *lib = xdl_open("libc.so", XDL_DEFAULT);
    if (!lib) {
        LOGE("Failed to open libc.so");
        return false;
    }

    void *target = xdl_sym(lib, symbol, NULL);
    if (!target) {
        if (strcmp(symbol, "open") == 0)        target = xdl_sym(lib, "__open", NULL);
        else if (strcmp(symbol, "stat") == 0)   target = xdl_sym(lib, "__stat", NULL);
        else if (strcmp(symbol, "access") == 0) target = xdl_sym(lib, "__access", NULL);
    }
    xdl_close(lib);

    if (!target) {
        LOGE("Failed to resolve symbol: %s", symbol);
        return false;
    }

    void *stub = shadowhook_hook_func_addr(target, replace_func, original_func);
    if (!stub) {
        LOGE("Failed to hook %s: %s", symbol, shadowhook_to_errmsg(shadowhook_get_errno()));
        return false;
    }

    LOGD("Successfully hooked %s at %p (stub=%p)", symbol, target, stub);
    return true;
}

static void install_hooks(void) {
    LOGI("Installing hooks...");
    int hook_count = 0;

    if (hook_symbol("open",     (void *)my_open,     (void **)&orig_open))     hook_count++;
    if (hook_symbol("unlink",   (void *)my_unlink,   (void **)&orig_unlink))   hook_count++;
    if (hook_symbol("rmdir",    (void *)my_rmdir,    (void **)&orig_rmdir))    hook_count++;
    if (hook_symbol("mkdir",    (void *)my_mkdir,    (void **)&orig_mkdir))    hook_count++;
    if (hook_symbol("rename",   (void *)my_rename,   (void **)&orig_rename))   hook_count++;
    if (hook_symbol("stat",     (void *)my_stat,     (void **)&orig_stat))     hook_count++;
    if (hook_symbol("access",   (void *)my_access,   (void **)&orig_access))   hook_count++;
    if (hook_symbol("opendir",  (void *)my_opendir,  (void **)&orig_opendir))  hook_count++;
    if (hook_symbol("realpath", (void *)my_realpath, (void **)&orig_realpath)) hook_count++;
    if (hook_symbol("truncate", (void *)my_truncate, (void **)&orig_truncate)) hook_count++;

    if (hook_symbol("openat",     (void *)my_openat,     (void **)&orig_openat))     hook_count++;
    if (hook_symbol("unlinkat",   (void *)my_unlinkat,   (void **)&orig_unlinkat))   hook_count++;
    if (hook_symbol("mkdirat",    (void *)my_mkdirat,    (void **)&orig_mkdirat))    hook_count++;
    if (hook_symbol("renameat",   (void *)my_renameat,   (void **)&orig_renameat))   hook_count++;
    if (hook_symbol("fstatat",    (void *)my_fstatat,    (void **)&orig_fstatat))    hook_count++;
    if (hook_symbol("faccessat",  (void *)my_faccessat,  (void **)&orig_faccessat))  hook_count++;

    LOGI("Installed %d hooks successfully", hook_count);
}

void init_iohook(JavaVM *vm) {
    cp_call_init(vm);
    tefkernel_working_dir = cp_call_get_external_dir();
    install_hooks();
}