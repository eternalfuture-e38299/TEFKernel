cmake_minimum_required(VERSION 3.28.3)
project(tefkernel C CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(NDEBUG)
    set(CMAKE_C_FLAGS_RELEASE   "-O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
else()
    set(CMAKE_C_FLAGS_DEBUG   "-g -O0")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
endif()

# ---------------------------------------------------------------------------
# 平台判定
# ---------------------------------------------------------------------------
set(ANDROID_MODE OFF)
if(ANDROID_MODE)
    set(ANDROID ON)
    add_compile_definitions(__ANDROID__)
    set(CMAKE_SYSROOT
            /home/eternalfuture/Android/Sdk/ndk/28.0.13004108/toolchains/llvm/prebuilt/linux-x86_64/sysroot/
    )
endif()

add_subdirectory(libs/spdlog)

add_library(miniaudio_config INTERFACE)
target_include_directories(miniaudio_config INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/miniaudio)
target_compile_definitions(miniaudio_config INTERFACE
        MA_NO_DECODING MA_NO_ENCODING MA_NO_ENGINE MA_NO_RESOURCE_MANAGER MA_NO_GENERATION)

# ---------------------------------------------------------------------------
# 源文件分组
# ---------------------------------------------------------------------------
set(PATCHLIB_SRC
        src/abi_impl/patchlib/common/type.cpp
        src/abi_impl/patchlib/common/method.cpp
        src/abi_impl/patchlib/common/property.cpp
        src/abi_impl/patchlib/common/field.cpp
        src/abi_impl/patchlib/common/struct/array.cpp
        src/abi_impl/patchlib/common/struct/string.cpp
        src/abi_impl/patchlib/common/struct/dictionary.cpp
        src/abi_impl/patchlib/common/struct/list.cpp
)
if(ANDROID)
    list(APPEND PATCHLIB_SRC
            src/abi_impl/patchlib/android/type.cpp
            src/abi_impl/patchlib/android/thread.cpp
            src/abi_impl/patchlib/android/private.cpp
            src/abi_impl/patchlib/android/field.cpp
            src/abi_impl/patchlib/android/method.cpp
            src/abi_impl/patchlib/android/struct/string.cpp
            src/abi_impl/patchlib/android/struct/array.cpp
            src/abi_impl/patchlib/android/method_hook.cpp
            src/abi_impl/patchlib/android/il2cpp_api.c
    )
else()
    list(APPEND PATCHLIB_SRC
            src/abi_impl/patchlib/desktop/type.cpp
            src/abi_impl/patchlib/desktop/field.cpp
            src/abi_impl/patchlib/desktop/method.cpp
            src/abi_impl/patchlib/desktop/il2cpp_api.c
            src/abi_impl/patchlib/desktop/struct/array.cpp
            src/abi_impl/patchlib/desktop/struct/string.cpp
    )
endif()

set(TEFSTD_SRC
        src/abi_impl/tefstd/vector.c
        src/abi_impl/tefstd/hashmap.c
        src/abi_impl/tefstd/skipmap.c
        src/abi_impl/tefstd/log.cpp
)

# 内部 C++ 实现（不含 abi_impl/terraria）
set(TERRARIA_SRC
        src/terraria/common/init/bootstrap.cpp
        src/terraria/common/init/register_all.cpp
        src/terraria/common/content/item_registry.cpp
        src/terraria/common/content/prefix_registry.cpp
        src/terraria/common/content/projectile_registry.cpp
        src/terraria/common/content/npc_registry.cpp
        src/terraria/common/content/buff_registry.cpp
        src/terraria/common/content/texture2d.cpp
        src/terraria/common/content/asset_factory.cpp
        src/terraria/common/content/recipe_manager.cpp
        src/terraria/common/content/language_manager.cpp
        src/terraria/common/content/gui_overlay.cpp
        src/terraria/common/content/net_engine.cpp
        src/terraria/main_state.cpp
        src/terraria/common/save/tef_save_format.cpp
        src/terraria/common/save/tef_save_game_handles.cpp
        src/terraria/common/save/tef_player_save.cpp
        src/terraria/common/save/tef_world_save.cpp
)

set(TEFPACKAGE_SRC
        src/abi_impl/tefpackage/compressor.c
        src/abi_impl/tefpackage/lz4.c
        src/abi_impl/tefpackage/lz4hc.c
        src/abi_impl/tefpackage/siphash.c
        src/abi_impl/tefpackage/tefpkg.cpp
        src/abi_impl/tefpackage/tefpkg_export.cpp
)

set(INTERNAL_SRC)
if(ANDROID)
    list(APPEND INTERNAL_SRC
            src/android/android_runtime_core.cpp
            src/android/android_test.cpp
            src/android/cpcall.cpp
            src/android/iohook.cpp
    )
    ## 暂不支持
    #[[list(APPEND TERRARIA_SRC
            src/terraria/android/content/button_manager.c
            src/terraria/android/content/button_hook_args.c
            src/terraria/android/content/button_profile_cache.c
            src/terraria/android/content/button_state.c
            src/terraria/android/content/button_scroll.c
            src/terraria/android/content/button_store.c
            src/terraria/android/content/button_images.c
    )]]
else()
    list(APPEND INTERNAL_SRC src/desktop/desktop_runtime_core.cpp)
endif()

# ---------------------------------------------------------------------------
# ABI 转发静态库：abi_impl/terraria
# ---------------------------------------------------------------------------
set(ABI_TERRARIA_SRC
        src/abi_impl/terraria/asset.cpp
        src/abi_impl/terraria/gui.cpp
        src/abi_impl/terraria/language_manager.cpp
        src/abi_impl/terraria/main.cpp
        src/abi_impl/terraria/netmanager.cpp
        src/abi_impl/terraria/recipe_manager.cpp
        src/abi_impl/terraria/texture2d.cpp
        src/abi_impl/terraria/content/audio_manager.c
        src/abi_impl/terraria/content/buff_manager.cpp
        src/abi_impl/terraria/content/item_manager.cpp
        src/abi_impl/terraria/content/npc_manager.cpp
        src/abi_impl/terraria/content/prefix_manager.cpp
        src/abi_impl/terraria/content/projectile_manager.cpp
)

add_library(tefkernel_abi_terraria STATIC ${ABI_TERRARIA_SRC})

target_include_directories(tefkernel_abi_terraria
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include_abi
)
target_compile_definitions(tefkernel_abi_terraria PUBLIC IS_TEFKERNEL_BUILD)
set_target_properties(tefkernel_abi_terraria PROPERTIES POSITION_INDEPENDENT_CODE ON)

# audio_manager.c 需要 miniaudio.h + MA_NO_* 宏
target_link_libraries(tefkernel_abi_terraria PRIVATE miniaudio_config)

# ---------------------------------------------------------------------------
# 主 target：内部实现 + 链接 ABI 静态库
# ---------------------------------------------------------------------------
add_library(tefkernel SHARED
        libs/miniaudio/miniaudio.c

        ${PATCHLIB_SRC}
        ${INTERNAL_SRC}
        ${TEFSTD_SRC}
        ${TEFPACKAGE_SRC}
        ${TERRARIA_SRC}

        src/abi_impl/memdl/memdl.c
        src/tef/core/log.cpp
        src/tef/core/log_bridge.cpp
        src/tpf_core.cpp
        src/module_core.cpp
        src/modloader_core.cpp
        src/mod_core.cpp
        src/crash_handler.cpp
        src/kernel_state.cpp
        src/runtime.cpp
        src/tef/patchlib/type.cpp
        src/tef/patchlib/handle.cpp
        src/tef/patchlib/field.cpp
        src/tef/patchlib/method.cpp
        src/tef/patchlib/property.cpp
        src/tef/patchlib/struct/string.cpp
)

target_include_directories(tefkernel
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include_abi
)
target_compile_definitions(tefkernel PUBLIC IS_TEFKERNEL_BUILD)

# 链接 ABI 静态库 + 依赖
target_link_libraries(tefkernel PRIVATE
        tefkernel_abi_terraria
        miniaudio_config
        spdlog::spdlog
)

# ---------------------------------------------------------------------------
# 平台链接
# ---------------------------------------------------------------------------
if(ANDROID)
    target_link_libraries(tefkernel PRIVATE OpenSLES android log)

    set(Plugin.ImportTableReplace OFF CACHE BOOL "" FORCE)
    set(Plugin.Android.BionicLinkerUtil OFF CACHE BOOL "" FORCE)
    set(DOBBY_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(DOBBY_BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(DOBBY_BUILD_KERNEL_MODE OFF CACHE BOOL "" FORCE)
    set(Private.Obfuscation OFF CACHE BOOL "" FORCE)
    set(FullFloatingPointRegisterPack OFF CACHE BOOL "" FORCE)
    set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)

    add_subdirectory(libs/libffi)
    add_subdirectory(libs/xdl)
    add_subdirectory(libs/dobby)

    target_link_libraries(tefkernel PUBLIC dobby_static xdl libffi)
    target_include_directories(tefkernel PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/libffi/include
            ${CMAKE_CURRENT_BINARY_DIR}/libs/libffi/include
    )
    target_link_options(tefkernel PRIVATE
            -static-libstdc++
            -Wl,--exclude-libs,ALL
    )

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(tefkernel PRIVATE dl m pthread)
    target_link_options(tefkernel PRIVATE
            -static-libstdc++
            -static-libgcc
    )

elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_link_libraries(tefkernel PRIVATE
            "-framework CoreFoundation"
            "-framework CoreAudio"
            "-framework AudioToolbox"
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR MINGW)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set_target_properties(tefkernel PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    target_link_libraries(tefkernel PRIVATE ws2_32 bcrypt winmm dbghelp)
    target_link_options(tefkernel PRIVATE
            -static-libgcc
            -static-libstdc++
            -Wl,-Bstatic
            -lwinpthread
            -Wl,-Bdynamic
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(tefkernel PRIVATE _DARWIN_C_SOURCE)
endif()

# ---------------------------------------------------------------------------
# 输出名
# ---------------------------------------------------------------------------
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

if(ANDROID)
    set(ARCH "${ANDROID_ABI}")
elseif(MINGW OR WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCH "x86_64")
    else()
        set(ARCH "x86")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
        set(ARCH "x86_64")
    elseif(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        set(ARCH "arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(ARCH "arm64")
    else()
        set(ARCH "x86_64")
    endif()
else()
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

set_target_properties(tefkernel PROPERTIES OUTPUT_NAME "tefkernel.${SYSTEM_NAME}.${ARCH}")

# ---------------------------------------------------------------------------
# Release strip
# ---------------------------------------------------------------------------
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID)
        add_custom_command(TARGET tefkernel POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Stripping symbols from tefkernel..."
                COMMAND ${CMAKE_STRIP} --strip-all $<TARGET_FILE:tefkernel>
                VERBATIM
        )
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        add_custom_command(TARGET tefkernel POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Stripping symbols from tefkernel..."
                COMMAND ${CMAKE_STRIP} -S $<TARGET_FILE:tefkernel>
                VERBATIM
        )
    endif()
endif()