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
 * Created: 2026/11/7
 *******************************************************************************/

#pragma once

/* ============================================================================
 * 平台 / 架构宏（#if 必须用宏，C++ 里也保留）
 * ============================================================================ */

#if defined(_WIN32) || defined(_WIN64)
#   define TEF_PLATFORM_WINDOWS
#   define TEF_PLATFORM_NAME "windows"
#elif defined(__APPLE__) && defined(__MACH__)
#   include <TargetConditionals.h>
#   if TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
#       define TEF_PLATFORM_IOS
#       define TEF_PLATFORM_NAME "ios"
#   else
#       define TEF_PLATFORM_MACOS
#       define TEF_PLATFORM_NAME "macos"
#   endif
#elif defined(__ANDROID__)
#   define TEF_PLATFORM_ANDROID
#   define TEF_PLATFORM_NAME "android"
#elif defined(__linux__)
#   define TEF_PLATFORM_LINUX
#   define TEF_PLATFORM_NAME "linux"
#else
#   define TEF_PLATFORM_UNKNOWN
#   define TEF_PLATFORM_NAME "Unknown"
#endif

#if defined(__x86_64__) || defined(_M_X64)
#   define TEF_ARCH_X64
#   define TEF_ARCH_NAME "x64"
#elif defined(__i386__) || defined(_M_IX86)
#   define TEF_ARCH_X86
#   define TEF_ARCH_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
#   define TEF_ARCH_ARM64
#   define TEF_ARCH_NAME "arm64"
#elif defined(__arm__) || defined(_M_ARM)
#   define TEF_ARCH_ARM32
#   define TEF_ARCH_NAME "arm"
#endif

/* ============================================================================
 * TEFPKG 动态库 ID
 * ============================================================================ */

#define TEFPKG_ID_DYLIB_ANDROID_ARM64    1
#define TEFPKG_ID_DYLIB_ANDROID_ARM32    2
#define TEFPKG_ID_DYLIB_LINUX_X64        3
#define TEFPKG_ID_DYLIB_LINUX_X86        4
#define TEFPKG_ID_DYLIB_WINDOWS_X64      5
#define TEFPKG_ID_DYLIB_WINDOWS_X86      6
#define TEFPKG_ID_DYLIB_MACOS_ARM64      7
#define TEFPKG_ID_DYLIB_MACOS_X64        8
#define TEFPKG_ID_DYLIB_IOS_ARM64        9
#define TEFPKG_ID_DYLIB_IOS_X64          10
#define TEFPKG_ID_DYLIB_IOS_ARM64_SIM    11

#if defined(TEF_PLATFORM_ANDROID)
#   if defined(TEF_ARCH_ARM64)
#       define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_ANDROID_ARM64
#   elif defined(TEF_ARCH_ARM32)
#       define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_ANDROID_ARM32
#   endif
#elif defined(TEF_PLATFORM_LINUX)
#   if defined(TEF_ARCH_X64)
#       define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_LINUX_X64
#   elif defined(TEF_ARCH_X86)
#       define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_LINUX_X86
#   endif
#elif defined(TEF_PLATFORM_WINDOWS)
#   if defined(TEF_ARCH_X64)
#       define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_WINDOWS_X64
#   elif defined(TEF_ARCH_X86)
#       define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_WINDOWS_X86
#   endif
#elif defined(TEF_PLATFORM_MACOS)
#   if defined(TEF_ARCH_ARM64)
#       define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_MACOS_ARM64
#   elif defined(TEF_ARCH_X64)
#       define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_MACOS_X64
#   endif
#elif defined(TEF_PLATFORM_IOS)
#   if defined(TARGET_OS_SIMULATOR) && TARGET_OS_SIMULATOR && defined(TEF_ARCH_ARM64)
#       define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_IOS_ARM64_SIM
#   elif defined(TEF_ARCH_ARM64)
#       define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_IOS_ARM64
#   elif defined(TEF_ARCH_X64)
#       define TEFPKG_ID_DYLIB TEFPKG_ID_DYLIB_IOS_X64
#   endif
#endif

/* ============================================================================
 * C++ 状态
 * ============================================================================ */

namespace tef::state {
    /* ---- 平台标志（C++ 表达式里用；#if 用上面的宏） ---- */

#if defined(TEF_PLATFORM_WINDOWS)
    inline constexpr bool k_windows = true;
    inline constexpr bool k_linux = false;
    inline constexpr bool k_mac_os = false;
    inline constexpr bool k_ios = false;
    inline constexpr bool k_android = false;
#elif defined(TEF_PLATFORM_LINUX)
    inline constexpr bool k_windows = false;
    inline constexpr bool k_linux = true;
    inline constexpr bool k_mac_os = false;
    inline constexpr bool k_ios = false;
    inline constexpr bool k_android = false;
#elif defined(TEF_PLATFORM_MACOS)
    inline constexpr bool k_windows = false;
    inline constexpr bool k_linux = false;
    inline constexpr bool k_mac_os = true;
    inline constexpr bool k_ios = false;
    inline constexpr bool k_android = false;
#elif defined(TEF_PLATFORM_IOS)
    inline constexpr bool k_windows = false;
    inline constexpr bool k_linux = false;
    inline constexpr bool k_mac_os = false;
    inline constexpr bool k_ios = true;
    inline constexpr bool k_android = false;
#elif defined(TEF_PLATFORM_ANDROID)
    inline constexpr bool k_windows = false;
    inline constexpr bool k_linux = false;
    inline constexpr bool k_mac_os = false;
    inline constexpr bool k_ios = false;
    inline constexpr bool k_android = true;
#else
    inline constexpr bool k_windows = false;
    inline constexpr bool k_linux = false;
    inline constexpr bool k_mac_os = false;
    inline constexpr bool k_ios = false;
    inline constexpr bool k_android = true;
#endif

    inline constexpr bool k_posix = k_linux || k_mac_os || k_ios || k_android;

    /* ---- 架构标志 ---- */

#if defined(TEF_ARCH_X64)
    inline constexpr bool k_x64 = true;
    inline constexpr bool k_x86 = false;
    inline constexpr bool k_arm64 = false;
    inline constexpr bool k_arm32 = false;
#elif defined(TEF_ARCH_X86)
    inline constexpr bool k_x64 = false;
    inline constexpr bool k_x86 = true;
    inline constexpr bool k_arm64 = false;
    inline constexpr bool k_arm32 = false;
#elif defined(TEF_ARCH_ARM64)
    inline constexpr bool k_x64 = false;
    inline constexpr bool k_x86 = false;
    inline constexpr bool k_arm64 = true;
    inline constexpr bool k_arm32 = false;
#elif defined(TEF_ARCH_ARM32)
    inline constexpr bool k_x64 = false;
    inline constexpr bool k_x86 = false;
    inline constexpr bool k_arm64 = false;
    inline constexpr bool k_arm32 = true;
#else
    inline constexpr bool k_x64 = false;
    inline constexpr bool k_x86 = false;
    inline constexpr bool k_arm64 = false;
    inline constexpr bool k_arm32 = false;
#endif

    inline constexpr auto k_platform_name = TEF_PLATFORM_NAME;
    inline constexpr auto k_arch_name = TEF_ARCH_NAME;

    /* ---- 工作目录 ---- */

    /* 获取工作目录（未设置时为 nullptr） */
    [[nodiscard]] char *working_dir() noexcept;

    /* 设置工作目录（接管所有权；由 tefkernel_init 调用一次） */
    void set_working_dir(char *dir) noexcept;

    /* 设置工作目录的 std::string 重载（内部拷贝） */
    void set_working_dir(const char *dir);
} // namespace tef::state
