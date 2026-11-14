cmake_minimum_required(VERSION 3.28.3)

# ==================== IDE Android 模拟 ====================
# 实际构建由外部工具链文件驱动，会设置 CMAKE_TOOLCHAIN_FILE。
set(IDE_ANDROID_MODE OFF)
if(IDE_ANDROID_MODE)
    set(NDK_PATH "/home/eternalfuture/Android/Sdk/ndk/28.0.13004108" CACHE PATH "Android NDK path")
    set(IDE_ANDROID_ABI "arm64-v8a" CACHE STRING "ABI for IDE indexing")
    set(IDE_ANDROID_API "21" CACHE STRING "API level for IDE indexing")

    set(TOOLCHAIN "${NDK_PATH}/toolchains/llvm/prebuilt/linux-x86_64")

    set(CMAKE_SYSTEM_NAME Android)
    set(CMAKE_SYSTEM_VERSION ${IDE_ANDROID_API})

    set(CMAKE_C_COMPILER   "${TOOLCHAIN}/bin/${IDE_ANDROID_ABI}${IDE_ANDROID_API}-clang")
    set(CMAKE_CXX_COMPILER "${TOOLCHAIN}/bin/${IDE_ANDROID_ABI}${IDE_ANDROID_API}-clang++")
    set(CMAKE_ASM_COMPILER "${TOOLCHAIN}/bin/clang")

    set(CMAKE_SYSROOT "${TOOLCHAIN}/sysroot")

    if(IDE_ANDROID_ABI STREQUAL "arm64-v8a")
        set(CMAKE_C_FLAGS_INIT   "--target=aarch64-linux-android${IDE_ANDROID_API}")
        set(CMAKE_CXX_FLAGS_INIT "--target=aarch64-linux-android${IDE_ANDROID_API}")
    elseif(IDE_ANDROID_ABI STREQUAL "armeabi-v7a")
        set(CMAKE_C_FLAGS_INIT   "--target=armv7a-linux-androideabi${IDE_ANDROID_API}")
        set(CMAKE_CXX_FLAGS_INIT "--target=armv7a-linux-androideabi${IDE_ANDROID_API}")
    endif()

    set(CMAKE_FIND_ROOT_PATH "${TOOLCHAIN}/sysroot")
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

    add_compile_definitions(__ANDROID__=1 ANDROID=1)
    set(ANDROID ON)

    # 给 shadowhook 等子工程提供 ANDROID_ABI
    if(NOT DEFINED ANDROID_ABI)
        set(ANDROID_ABI "${IDE_ANDROID_ABI}" CACHE STRING "" FORCE)
    endif()
endif()

project(tefkernel C)

set(CMAKE_C_STANDARD 17)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(NDEBUG)
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
else()
    set(CMAKE_C_FLAGS_DEBUG "-g -O0")
endif()

# 全局强制 PIC，所有子目录都生效
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

# 汇编文件也需要 PIC
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -DPIC")
set(CMAKE_ASM_FLAGS "-x assembler-with-cpp")

set(PATCHLIB_SRC
        src/patchlib/common/type.c
        src/patchlib/common/method.c
        src/patchlib/common/property.c
        src/patchlib/common/field.c
        src/patchlib/common/struct/dictionary.c
        src/patchlib/common/struct/list.c
        src/patchlib/common/struct/string.c
        src/patchlib/common/struct/array.c
)
if (ANDROID)
    list(APPEND PATCHLIB_SRC
            src/patchlib/android/type.c
            src/patchlib/android/field.c
            src/patchlib/android/method.c
            src/patchlib/android/thread.c
            src/patchlib/android/il2cpp_api.c
            src/patchlib/android/private.c
            src/patchlib/android/method_hook.c
            src/patchlib/android/struct/array.c
            src/patchlib/android/struct/string.c
    )
else ()
    list(APPEND PATCHLIB_SRC
            src/patchlib/desktop/type.c
            src/patchlib/desktop/field.c
            src/patchlib/desktop/method.c
            src/patchlib/desktop/il2cpp_api.c
            src/patchlib/desktop/struct/array.c
            src/patchlib/desktop/struct/string.c
    )
endif ()

# tefstd：list / scope 一并纳入
set(TEFSTD_SRC
        src/tefstd/vector.c
        src/tefstd/hashmap.c
        src/tefstd/skipmap.c
        src/tefstd/list.c
        src/tefstd/scope.c
        src/tefstd/log.c
)

set(TEFPLUGIN_SRC
        src/tefplugin/tpf_core.c
)

set(MODLOADER_SRC src/modloader/modloader_core.c)

set(MODULE_SRC src/module/module_core_imp.c)

set(MEMDL_SRC
        src/memdl/memdl.c
)

set(INTERNAL_SRC
        src/log.c
        src/runtime.c
        src/mod_core.c
        src/crash_handler.c
)

set(TERRARIA_SRC
        src/terraria/common/netmanager.c
        src/terraria/common/main.c
        src/terraria/common/asset.c
        src/terraria/common/content/item_manager.c
        src/terraria/common/content/projectile_manager.c
        src/terraria/common/content/npc_manager.c
        src/terraria/common/content/buff_manager.c
        src/terraria/common/gui.c
        src/terraria/common/recipe_manager.c
        src/terraria/common/language_manager.c
        src/terraria/common/save/tef_save_format.c
        src/terraria/common/save/tef_save_game_handles.c
        src/terraria/common/save/tef_player_save.c
        src/terraria/common/save/tef_world_save.c
        src/terraria/common/content/audio_manager.c
        src/terraria/common/content/prefix_manager.c
        src/terraria/common/entity.c
)

set(TEFPACKAGE_SRC
        src/tefpackage/compressor.c
        src/tefpackage/lz4.c
        src/tefpackage/lz4hc.c
        src/tefpackage/siphash.c
        src/tefpackage/tefpkg.c
)

if (ANDROID)
    list(APPEND INTERNAL_SRC
            src/android/android_runtime_core.c
            src/android/cpcall.c
            src/android/iohook.c
    )

    list(APPEND TERRARIA_SRC
            src/terraria/android/texture2d.c
            src/terraria/android/content/button_manager.c
            src/terraria/android/content/button_hook_args.c
            src/terraria/android/content/button_profile_cache.c
            src/terraria/android/content/button_state.c
            src/terraria/android/content/button_scroll.c
            src/terraria/android/content/button_store.c
            src/terraria/android/content/button_images.c
    )
else ()
    list(APPEND INTERNAL_SRC
            src/desktop/desktop_runtime_core.c
            src/desktop/desktop_test.c
    )

    list(APPEND TERRARIA_SRC
            src/terraria/desktop/texture2d.c
    )
endif ()

# miniaudio_config
add_library(miniaudio_config INTERFACE)

target_include_directories(miniaudio_config INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/miniaudio
)

# 禁用解码、编码、高级引擎、资源管理器、生成器；不要定义 MA_NO_THREADING
target_compile_definitions(miniaudio_config INTERFACE
        MA_NO_DECODING
        MA_NO_ENCODING
        MA_NO_ENGINE
        MA_NO_RESOURCE_MANAGER
        MA_NO_GENERATION
)

add_library(tefkernel SHARED
        # miniaudio 实现文件
        libs/miniaudio/miniaudio.c

        # 内核代码
        ${PATCHLIB_SRC}
        ${INTERNAL_SRC}
        ${TEFSTD_SRC}
        ${TEFPLUGIN_SRC}
        ${MEMDL_SRC}
        ${MODLOADER_SRC}
        ${MODULE_SRC}
        ${TEFPACKAGE_SRC}
        ${TERRARIA_SRC}
)

target_link_libraries(tefkernel PRIVATE miniaudio_config)

# ==================== 平台链接 ====================

if (ANDROID)
    target_link_libraries(tefkernel PRIVATE
            OpenSLES
            android
            log
            dl
            m
    )
    # AAudio 视 NDK 版本可选
    # target_link_libraries(tefkernel PRIVATE aaudio)

elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(tefkernel PRIVATE dl m pthread)

elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_link_libraries(tefkernel PRIVATE
            "-framework CoreFoundation"
            "-framework CoreAudio"
            "-framework AudioToolbox"
    )

elseif (WIN32 OR MINGW)
    # Windows：导出所有符号 + 链接系统库
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set_target_properties(tefkernel PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

    target_link_libraries(tefkernel PRIVATE
            ws2_32       # winsock
            bcrypt       # 加密
            winmm        # 多媒体/定时器
            dbghelp      # 崩溃回溯
            advapi32     # 注册表/安全
            user32       # 窗口/消息
            gdi32        # GDI
            shell32      # Shell API
            ole32        # COM
            oleaut32     # COM 自动化
            uuid         # GUID
            comdlg32     # 通用对话框
            shlwapi      # Shell 轻量 API
            version      # 版本信息
            psapi        # 进程状态
            iphlpapi     # IP 辅助
            dnsapi       # DNS
            secur32      # SSPI
            crypt32      # 证书/加密
            setupapi     # 设备安装
            imm32        # 输入法
            winhttp      # HTTP
            wininet      # WinINet
    )

    target_link_options(tefkernel PRIVATE
            -Wl,--export-all-symbols
            -Wl,--enable-auto-import
            -Wl,--whole-archive
            -Wl,--kill-at
    )
endif ()

target_compile_definitions(tefkernel PUBLIC IS_TEFKERNEL_BUILD)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(tefkernel PRIVATE _DARWIN_C_SOURCE)
endif ()

# ==================== Android 依赖 ====================

if (ANDROID)
    add_subdirectory(libs/libffi)
    add_subdirectory(libs/shadowhook)

    target_link_libraries(tefkernel PUBLIC
            shadowhook
            libffi
    )

    target_include_directories(tefkernel PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/libffi/include
            ${CMAKE_CURRENT_BINARY_DIR}/libs/libffi/include
    )



#[[    target_link_options(tefkernel PRIVATE -static-libstdc++)
    # 不导出静态链接进来的 C++ 运行时/libunwind/libffi 符号
    target_link_options(tefkernel PRIVATE -Wl,--exclude-libs,ALL)]]
endif ()

# ==================== MinGW 运行时静态链接 ====================

if (MINGW OR (WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU"))
    target_link_options(tefkernel PRIVATE
            -static-libgcc
            -static-libstdc++
            -Wl,-Bstatic
            -lwinpthread
            -Wl,-Bdynamic
    )
endif ()

target_include_directories(tefkernel
        PUBLIC
        include
        src
)

# ==================== Release strip ====================

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID)
        add_custom_command(TARGET tefkernel POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Stripping symbols from tefkernel..."
                COMMAND ${CMAKE_STRIP} --strip-all $<TARGET_FILE:tefkernel>
                COMMENT "Stripping symbols from tefkernel library using strip command"
                VERBATIM
        )
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        add_custom_command(TARGET tefkernel POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Stripping symbols from tefkernel..."
                COMMAND ${CMAKE_STRIP} -S $<TARGET_FILE:tefkernel>
                COMMENT "Stripping symbols from tefkernel library using strip command"
                VERBATIM
        )
    endif()
endif()

# ==================== 产物命名 ====================

if (ANDROID)
    if (ANDROID_ABI STREQUAL "arm64-v8a")
        set(ARCH "arm64-v8a")
    elseif (ANDROID_ABI STREQUAL "armeabi-v7a")
        set(ARCH "armeabi-v7a")
    else()
        set(ARCH "${ANDROID_ABI}")
    endif()
    set(SYSTEM_NAME "android")
elseif (MINGW OR WIN32)
    if (CMAKE_C_COMPILER MATCHES "x86_64-w64-mingw32")
        set(ARCH "x86_64")
    elseif (CMAKE_C_COMPILER MATCHES "i686-w64-mingw32")
        set(ARCH "x86")
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCH "x86_64")
    else()
        set(ARCH "x86")
    endif()
    set(SYSTEM_NAME "windows")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if (CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
        set(ARCH "x86_64")
    elseif (CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        set(ARCH "arm64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(ARCH "arm64")
    else()
        set(ARCH "x86_64")
    endif()
    set(SYSTEM_NAME "mac")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(ARCH "arm64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|arm")
        set(ARCH "arm32")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|x64")
        set(ARCH "x86_64")
    else()
        set(ARCH "x86")
    endif()
    set(SYSTEM_NAME "linux")
else()
    set(SYSTEM_NAME "unknown")
    set(ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

set_target_properties(tefkernel PROPERTIES
        OUTPUT_NAME "tefkernel.${SYSTEM_NAME}.${ARCH}"
)