/*******************************************************************************
 * File: memdl.c
 * Project: memdl
 * Created: 2025/11/7
 * Author: eternalfuture-e38299
 * Github: https://github.com/eternalfuture-e38299
 * 
 * MIT License
 * 
 * Copyright (c) 2025 EternalFuture
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *******************************************************************************/

#include "memdl/memdl.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

// 平台特定头文件
#if defined(_WIN32) || defined(_WIN64)
#define MEMDL_WINDOWS
#include <windows.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define MEMDL_IOS
#else
#define MEMDL_MACOS
#endif
#include <dlfcn.h>
#include <stdlib.h>
#elif defined(__ANDROID__)
#define MEMDL_ANDROID
#include <android/dlext.h>
#include <dlfcn.h>
#include <sys/system_properties.h>
#include <asm-generic/unistd.h>
#include <sys/syscall.h>
#elif defined(__linux__)
#define MEMDL_LINUX
#include <dlfcn.h>
#else
#error "Unsupported platform"
#endif

// 通用Unix头文件
#if !defined(MEMDL_WINDOWS)
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

// 内存文件描述符相关头文件
#if defined(MEMDL_LINUX) || defined(MEMDL_ANDROID)
#include <sys/syscall.h>
#endif

// 临时文件相关
#if !defined(MEMDL_WINDOWS)
#include <stdlib.h>  // 为了mkstemp
#endif

// 内部错误信息
static char memdl_error_msg[256] = {0};

// 设置错误信息
static void memdl_set_error(const char *msg) {
    strncpy(memdl_error_msg, msg, sizeof(memdl_error_msg) - 1);
    memdl_error_msg[sizeof(memdl_error_msg) - 1] = '\0';
}

static void memdl_set_error_format(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(memdl_error_msg, sizeof(memdl_error_msg), format, args);
    va_end(args);
}

// 平台检测
int memdl_get_platform(void) {
#if defined(MEMDL_WINDOWS)
    return MEMDL_PLATFORM_WINDOWS;
#elif defined(MEMDL_IOS)
    return MEMDL_PLATFORM_IOS;
#elif defined(MEMDL_MACOS)
    return MEMDL_PLATFORM_MACOS;
#elif defined(MEMDL_ANDROID)
    return MEMDL_PLATFORM_ANDROID;
#elif defined(MEMDL_LINUX)
    return MEMDL_PLATFORM_LINUX;
#else
    return MEMDL_PLATFORM_UNKNOWN;
#endif
}

// 验证ELF/Mach-O/PE头
int memdl_validate(const void *so_data, const size_t so_size) {
    if (!so_data || so_size < 4) {
        memdl_set_error("Invalid data or size");
        return -1;
    }

    const unsigned char *data = so_data;

    // 检查ELF头 (Linux/Android)
    if (data[0] == 0x7F && data[1] == 'E' &&
        data[2] == 'L' && data[3] == 'F') {
        return 0;
    }

    // 检查Mach-O头 (macOS/iOS)
    const uint32_t magic = *(const uint32_t *) data;
    if (magic == 0xFEEDFACE || magic == 0xFEEDFACF || // 32位
        magic == 0xCEFAEDFE || magic == 0xCFFAEDFE) {
        // 64位
        return 0;
    }

    // 检查PE头 (Windows)
    if (data[0] == 'M' && data[1] == 'Z') {
        return 0;
    }

    memdl_set_error("Not a valid executable format");
    return -1;
}

// 获取架构信息
int memdl_get_arch(const void *so_data, const size_t so_size) {
    if (memdl_validate(so_data, so_size) != 0) {
        return MEMDL_ARCH_UNKNOWN;
    }

    const unsigned char *data = (const unsigned char *) so_data;

    // ELF格式
    if (data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') {
        return data[4] == 1
                   ? MEMDL_ARCH_X86
                   : // 32位
                   data[4] == 2
                       ? MEMDL_ARCH_X86_64
                       : // 64位
                       data[4] == 40
                           ? MEMDL_ARCH_ARM
                           : // ARM
                           data[4] == 183
                               ? MEMDL_ARCH_ARM64
                               : // ARM64
                               MEMDL_ARCH_UNKNOWN;
    }

    // 其他格式暂返回未知
    return MEMDL_ARCH_UNKNOWN;
}

// 平台特定实现
#ifdef MEMDL_WINDOWS

#include <stdio.h>

memdl_handle_t memdl_open_file(const char *filename, const int flags) {
    return LoadLibraryA(filename);
}

memdl_handle_t memdl_open(const void *so_data, size_t so_size, int flags) {
    if (memdl_validate(so_data, so_size) != 0) {
        return NULL;
    }

    // Windows内存加载比较复杂，使用临时文件方案
    char temp_path[MAX_PATH];
    DWORD ret = GetTempPathA(MAX_PATH, temp_path);
    if (ret == 0 || ret > MAX_PATH) {
        memdl_set_error("Failed to get temp path");
        return NULL;
    }

    char temp_file[MAX_PATH];
    if (GetTempFileNameA(temp_path, "memdl", 0, temp_file) == 0) {
        memdl_set_error("Failed to create temp file");
        return NULL;
    }

    // 写入临时文件
    HANDLE hFile = CreateFileA(temp_file, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        memdl_set_error("Failed to create temp file");
        return NULL;
    }

    DWORD written;
    if (!WriteFile(hFile, so_data, (DWORD) so_size, &written, NULL) ||
        written != so_size) {
        CloseHandle(hFile);
        DeleteFileA(temp_file);
        memdl_set_error("Failed to write temp file");
        return NULL;
    }
    CloseHandle(hFile);

    // 加载DLL
    HMODULE handle = LoadLibraryA(temp_file);

    // 删除临时文件
    DeleteFileA(temp_file);

    if (!handle) {
        memdl_set_error_format("LoadLibrary failed: %lu", GetLastError());
    }

    return (memdl_handle_t) handle;
}

void *memdl_sym(memdl_handle_t handle, const char *symbol) {
    if (!handle) {
        memdl_set_error("Invalid handle");
        return NULL;
    }
    return (void *) GetProcAddress((HMODULE) handle, symbol);
}

int memdl_close(memdl_handle_t handle) {
    if (!handle) {
        memdl_set_error("Invalid handle");
        return -1;
    }
    return FreeLibrary((HMODULE) handle) ? 0 : -1;
}

#elif defined(MEMDL_ANDROID)

static void set_error(const char* msg) {}

static bool is_android_7_plus() {
    char version[PROP_VALUE_MAX] = {0};
    if (__system_property_get("ro.build.version.sdk", version) <= 0) {
        return false;
    }
    return atoi(version) >= 24; // Android 7.0 = API 24
}

memdl_handle_t memdl_open_file(const char *filename, const int flags) {
    int dl_flags = (flags & MEMDL_NOW) ? RTLD_NOW : RTLD_LAZY;
    dl_flags |= (flags & MEMDL_LOCAL) ? RTLD_LOCAL : RTLD_GLOBAL;
    return dlopen(filename, dl_flags);
}

memdl_handle_t memdl_open(const void *so_data, size_t so_size, int flags) {
    if (memdl_validate(so_data, so_size) != 0) {
        return NULL;
    }

    int dl_flags = (flags & MEMDL_NOW) ? RTLD_NOW : RTLD_LAZY;
    dl_flags |= (flags & MEMDL_LOCAL) ? RTLD_LOCAL : RTLD_GLOBAL;

    // Android 7+ 使用memfd方案
    if (is_android_7_plus()) {
// 定义MFD_CLOEXEC常量（如果系统头文件没有定义）
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

int fd = syscall(SYS_memfd_create, "memdl_lib", MFD_CLOEXEC);
        if (fd>= 0) {
            ssize_t written = write(fd, so_data, so_size);
            if (written == (ssize_t)so_size) {
                lseek(fd, 0, SEEK_SET);

                android_dlextinfo info = {
                    .flags = ANDROID_DLEXT_USE_LIBRARY_FD,
                    .library_fd = fd,
                };

                void* handle = android_dlopen_ext("/memfd", dl_flags, &info);
                close(fd);

                if (handle) return handle;
            } else {
                close(fd);
            }
        }
    }

// 降级方案：临时文件
char template[] = "/data/local/tmp/memdl_tmp";
int fd = mkstemp(template);
    if (fd>= 0) {
        if (write(fd, so_data, so_size) == (ssize_t)so_size) {
            void* handle = dlopen(template, dl_flags);
            unlink(template);
            close(fd);

            if (!handle) {
                set_error(dlerror());
            }
            return handle;
        }
        close(fd);
        unlink(template);
    }

    set_error("All loading methods failed");
    return NULL;
}

void *memdl_sym(memdl_handle_t handle, const char *symbol) {
    if (!handle) {
        set_error("Invalid handle");
        return NULL;
    }
    void *sym = dlsym(handle, symbol);
    if (!sym) {
        set_error(dlerror());
    }
    return sym;
}

int memdl_close(memdl_handle_t handle) {
    if (!handle) {
        set_error("Invalid handle");
        return -1;
    }
    int result = dlclose(handle);
    if (result != 0) {
        set_error(dlerror());
    }
    return result;
}

#elif defined(MEMDL_IOS) || defined(MEMDL_MACOS)

memdl_handle_t memdl_open_file(const char *filename, const int flags) {
    int dl_flags = (flags & MEMDL_NOW) ? RTLD_NOW : RTLD_LAZY;
    dl_flags |= (flags & MEMDL_LOCAL) ? RTLD_LOCAL : RTLD_GLOBAL;
    return dlopen(filename, dl_flags);
}

memdl_handle_t memdl_open(const void *so_data, size_t so_size, int flags) {
    if (memdl_validate(so_data, so_size) != 0) {
        return NULL;
    }

    int dl_flags = (flags & MEMDL_NOW) ? RTLD_NOW : RTLD_LAZY;
    dl_flags |= (flags & MEMDL_LOCAL) ? RTLD_LOCAL : RTLD_GLOBAL;

    // macOS/iOS 使用临时文件方案
    char template[] = "/tmp/memdl_tmp";
    int fd = mkstemp(template);
    if (fd >= 0) {
        if (write(fd, so_data, so_size) == (ssize_t) so_size) {
            void *handle = dlopen(template, dl_flags);
            unlink(template);
            close(fd);

            if (!handle) {
                memdl_set_error(dlerror());
            }
            return handle;
        }
        close(fd);
        unlink(template);
    }

    memdl_set_error("Failed to load library");
    return NULL;
}

void *memdl_sym(memdl_handle_t handle, const char *symbol) {
    if (!handle) {
        memdl_set_error("Invalid handle");
        return NULL;
    }
    void *sym = dlsym(handle, symbol);
    if (!sym) {
        memdl_set_error(dlerror());
    }
    return sym;
}

int memdl_close(memdl_handle_t handle) {
    if (!handle) {
        memdl_set_error("Invalid handle");
        return -1;
    }
    int result = dlclose(handle);
    if (result != 0) {
        memdl_set_error(dlerror());
    }
    return result;
}

#elif defined(MEMDL_LINUX)

memdl_handle_t memdl_open_file(const char *filename, const int flags) {
    int dl_flags = (flags & MEMDL_NOW) ? RTLD_NOW : RTLD_LAZY;
    dl_flags |= (flags & MEMDL_LOCAL) ? RTLD_LOCAL : RTLD_GLOBAL;
    return dlopen(filename, dl_flags);
}

memdl_handle_t memdl_open(const void *so_data, const size_t so_size, const int flags) {
    if (memdl_validate(so_data, so_size) != 0) {
        return NULL;
    }

    int dl_flags = (flags & MEMDL_NOW) ? RTLD_NOW : RTLD_LAZY;
    dl_flags |= (flags & MEMDL_LOCAL) ? RTLD_LOCAL : RTLD_GLOBAL;

    // Linux 使用memfd方案
    // 定义MFD_CLOEXEC常量（如果系统头文件没有定义）
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

    int fd = (int) syscall(SYS_memfd_create, "memdl_lib", MFD_CLOEXEC);
    if (fd >= 0) {
        const ssize_t written = write(fd, so_data, so_size);
        if (written == (ssize_t) so_size) {
            lseek(fd, 0, SEEK_SET);

            char fd_path[64];
            snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", fd);
            void *handle = dlopen(fd_path, dl_flags);
            close(fd);

            if (handle) return handle;
        } else {
            close(fd);
        }
    }

    // 降级方案：临时文件
    char template[] = "/tmp/memdl_tmp";
    fd = mkstemp(template);
    if (fd >= 0) {
        if (write(fd, so_data, so_size) == (ssize_t) so_size) {
            void *handle = dlopen(template, dl_flags);
            unlink(template);
            close(fd);

            if (!handle) {
                memdl_set_error(dlerror());
            }
            return handle;
        }
        close(fd);
        unlink(template);
    }

    memdl_set_error("All loading methods failed");
    return NULL;
}

void *memdl_sym(memdl_handle_t handle, const char *symbol) {
    if (!handle) {
        memdl_set_error("Invalid handle");
        return NULL;
    }
    void *sym = dlsym(handle, symbol);
    if (!sym) {
        memdl_set_error(dlerror());
    }
    return sym;
}

int memdl_close(memdl_handle_t handle) {
    if (!handle) {
        memdl_set_error("Invalid handle");
        return -1;
    }
    const int result = dlclose(handle);
    if (result != 0) {
        memdl_set_error(dlerror());
    }
    return result;
}

#endif

// 公共API实现
const char *memdl_error(void) {
    return memdl_error_msg[0] ? memdl_error_msg : "No error";
}
