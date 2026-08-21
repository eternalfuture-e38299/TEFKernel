cmake_minimum_required(VERSION 3.28.3)
project(tefkernel C CXX)

set(CMAKE_C_STANDARD 17)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(NDEBUG)
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
else()
    set(CMAKE_C_FLAGS_DEBUG "-g -O0")
endif()

set(ANDROID_MODE OFF)
if (${ANDROID_MODE})
    set(ANDROID ON)
    add_compile_definitions(__ANDROID__)
    set(CMAKE_SYSROOT /home/eternalfuture/Android/Sdk/ndk/28.0.13004108/toolchains/llvm/prebuilt/linux-x86_64/sysroot/)
endif ()

set(PATCHLIB_SRC src/patchlib/common/type.c
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

set(TEFSTD_SRC
        src/tefstd/vector.c
        src/tefstd/hashmap.c
        src/tefstd/skipmap.c
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
            src/android/android_runtime_core.cpp
            src/android/android_test.cpp
            src/android/cpcall.cpp
            src/android/iohook.cpp
    )

    list(APPEND TERRARIA_SRC
            src/terraria/android/texture2d.c
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

add_library(tefkernel SHARED
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

# Windows 动态库：强制导出所有符号
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR MINGW)
    # 启用自动导出所有符号
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set_target_properties(tefkernel PROPERTIES
            WINDOWS_EXPORT_ALL_SYMBOLS TRUE
    )

    # 方法2：链接器选项强制导出
    target_link_options(tefkernel PRIVATE
            -Wl,--export-all-symbols
            -Wl,--enable-auto-import
            -Wl,--whole-archive
            -Wl,--kill-at
    )

    # 链接 Windows 系统库
    target_link_libraries(tefkernel PRIVATE
            ws2_32
            bcrypt
            winmm
            dbghelp
    )
endif()

target_compile_definitions(tefkernel PUBLIC IS_TEFKERNEL_BUILD)

add_subdirectory(libffi)

target_link_libraries(tefkernel
        PRIVATE
        libffi
)

target_include_directories(tefkernel PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libffi/include
        ${CMAKE_CURRENT_BINARY_DIR}/libffi/include
)

if (${ANDROID})
    # set(Plugin.SymbolResolver OFF CACHE BOOL "Enable symbol resolver" FORCE)

    set(Plugin.ImportTableReplace OFF CACHE BOOL "Enable import table replace" FORCE)
    set(Plugin.Android.BionicLinkerUtil OFF CACHE BOOL "Enable android bionic linker util" FORCE)
    set(DOBBY_BUILD_EXAMPLE OFF CACHE BOOL "Build example" FORCE)
    set(DOBBY_BUILD_TEST OFF CACHE BOOL "Build test" FORCE)
    set(DOBBY_BUILD_KERNEL_MODE OFF CACHE BOOL "Build xnu kernel mode" FORCE)
    set(Private.Obfuscation OFF CACHE BOOL "Enable llvm obfuscation" FORCE)
    set(FullFloatingPointRegisterPack OFF CACHE BOOL "Save and pack all floating-point registers" FORCE)
    set(DOBBY_DEBUG OFF CACHE BOOL "Enable debug logging" FORCE) # 生产环境建议关闭

    add_subdirectory(xdl)
    add_subdirectory(dobby)
    target_link_libraries(tefkernel
            PUBLIC
            android
            log
            dobby_static
            xdl
    )
endif ()

target_include_directories(tefkernel
        PUBLIC
        include
        src
)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID)
        add_custom_command(TARGET tefkernel POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Stripping symbols from tefkernel..."
                COMMAND ${CMAKE_STRIP} --strip-all $<TARGET_FILE:tefkernel>
                COMMENT "Stripping symbols from tefkernel library using strip command"
                VERBATIM
        )
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # macOS 使用不同的 strip 选项
        add_custom_command(TARGET tefkernel POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Stripping symbols from tefkernel..."
                COMMAND ${CMAKE_STRIP} -S $<TARGET_FILE:tefkernel>
                COMMENT "Stripping symbols from tefkernel library using strip command"
                VERBATIM
        )
    endif()
endif()

# 获取系统名称和架构
if(ANDROID)
    set(SYSTEM_NAME "android")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SYSTEM_NAME "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(SYSTEM_NAME "mac")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SYSTEM_NAME "windows")
else()
    set(SYSTEM_NAME "unknown")
endif()

# 获取架构

if(ANDROID)
    # Android 使用 NDK 的 ABI 名称
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(ARCH "arm64-v8a")
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(ARCH "armeabi-v7a")
    else()
        set(ARCH "${ANDROID_ABI}")
    endif()
elseif(MINGW OR WIN32)
    # Windows: 通过编译器名称判断架构
    if(CMAKE_C_COMPILER MATCHES "x86_64-w64-mingw32" OR CMAKE_CXX_COMPILER MATCHES "x86_64-w64-mingw32")
        set(ARCH "x86_64")
    elseif(CMAKE_C_COMPILER MATCHES "i686-w64-mingw32" OR CMAKE_CXX_COMPILER MATCHES "i686-w64-mingw32")
        set(ARCH "x86")
    else()
        # 备用方案：通过指针大小判断
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(ARCH "x86_64")
        else()
            set(ARCH "x86")
        endif()
    endif()
else()
    # 非 Android 平台使用简化的架构名称
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(ARCH "arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|armv7a|ARMv7|arm")
        set(ARCH "arm32")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64|x64")
        set(ARCH "x86_64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686|i386|x86")
        set(ARCH "x86")
    else()
        set(ARCH "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
endif()

#[[
message(STATUS "========== DEBUG ==========")
message(STATUS "SYSTEM_NAME: ${SYSTEM_NAME}")
message(STATUS "ARCH: ${ARCH}")
message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "WIN32: ${WIN32}")
message(STATUS "MINGW: ${MINGW}")
message(STATUS "============================")]]


# 设置输出文件名
set_target_properties(tefkernel PROPERTIES
    OUTPUT_NAME "tefkernel.${SYSTEM_NAME}.${ARCH}"
)
#[[

set(TEST ON)
if (${TEST})
    include_directories(include)

    add_library(test_module SHARED
            test/module.c
    )
endif ()]]
