/*******************************************************************************
 * tefkernel - kernel_state
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
 * Created: 2026/3/7
 *******************************************************************************/

#ifndef TEFKERNEL_KERNEL_STATE_H
#define TEFKERNEL_KERNEL_STATE_H
#ifdef __cplusplus
extern "C" {
#endif

extern char* tefkernel_working_dir;  ///< 工作目录

#if defined(_WIN32) || defined(_WIN64)
#define TEF_PLATFORM_WINDOWS
#define TEF_PLATFORM_NAME "windows"
#elif defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
#define TEF_PLATFORM_IOS
#define TEF_PLATFORM_NAME "ios"
#else
#define TEF_PLATFORM_MACOS
#define TEF_PLATFORM_NAME "macos"
#endif
#elif defined(__ANDROID__)
#define TEF_PLATFORM_ANDROID
#define TEF_PLATFORM_NAME "android"
#elif defined(__linux__)
#define TEF_PLATFORM_LINUX
#define TEF_PLATFORM_NAME "linux"
#else
#define TEF_PLATFORM_UNKNOWN
#define TEF_PLATFORM_NAME "Unknown"
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define TEF_ARCH_X64
#define TEF_ARCH_NAME "x64"
#elif defined(__i386__) || defined(_M_IX86)
#define TEF_ARCH_X86
#define TEF_ARCH_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define TEF_ARCH_ARM64
#define TEF_ARCH_NAME "arm64"
#elif defined(__arm__) || defined(_M_ARM)
#define TEF_ARCH_ARM32
#define TEF_ARCH_NAME "arm"
#endif

// TEFPKG中固定的动态库ID定义
// 这些ID对应特定平台和架构的动态库，在TEF包中有固定的条目索引
// 与PlatformArch结构中的priority字段对应

// Android
#define TEFPKG_ID_DYLIB_ANDROID_ARM64    1
#define TEFPKG_ID_DYLIB_ANDROID_ARM32    2

// Linux
#define TEFPKG_ID_DYLIB_LINUX_X64        3
#define TEFPKG_ID_DYLIB_LINUX_X86        4

// Windows
#define TEFPKG_ID_DYLIB_WINDOWS_X64      5
#define TEFPKG_ID_DYLIB_WINDOWS_X86      6

// macOS
#define TEFPKG_ID_DYLIB_MACOS_ARM64      7
#define TEFPKG_ID_DYLIB_MACOS_X64        8

// iOS
#define TEFPKG_ID_DYLIB_IOS_ARM64        9
#define TEFPKG_ID_DYLIB_IOS_X64          10
#define TEFPKG_ID_DYLIB_IOS_ARM64_SIM    11  // iOS模拟器(arm64)

// 当前编译环境的动态库ID
#if defined(TEF_PLATFORM_ANDROID)
    #if defined(TEF_ARCH_ARM64)
        #define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_ANDROID_ARM64
    #elif defined(TEF_ARCH_ARM32)
        #define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_ANDROID_ARM32
    #endif
#elif defined(TEF_PLATFORM_LINUX)
    #if defined(TEF_ARCH_X64)
        #define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_LINUX_X64
    #elif defined(TEF_ARCH_X86)
        #define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_LINUX_X86
    #endif
#elif defined(TEF_PLATFORM_WINDOWS)
    #if defined(TEF_ARCH_X64)
        #define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_WINDOWS_X64
    #elif defined(TEF_ARCH_X86)
        #define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_WINDOWS_X86
    #endif
#elif defined(TEF_PLATFORM_MACOS)
    #if defined(TEF_ARCH_ARM64)
        #define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_MACOS_ARM64
    #elif defined(TEF_ARCH_X64)
        #define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_MACOS_X64
    #endif
#elif defined(TEF_PLATFORM_IOS)
    #if defined(TARGET_OS_SIMULATOR) && TARGET_OS_SIMULATOR && defined(TEF_ARCH_ARM64)
        #define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_IOS_ARM64_SIM
    #elif defined(TEF_ARCH_ARM64)
        #define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_IOS_ARM64
    #elif defined(TEF_ARCH_X64)
        #define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_IOS_X64
    #endif
#endif

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_KERNEL_STATE_H