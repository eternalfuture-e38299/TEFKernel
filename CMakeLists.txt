cmake_minimum_required(VERSION 3.5)
project(tefkernel C CXX)

set(CMAKE_C_STANDARD 11)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(NDEBUG)
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
else()
    set(CMAKE_C_FLAGS_DEBUG "-g -O0")
endif()

set(ANDROID_MODE ON)
if (${ANDROID_MODE})
    set(ANDROID ON)
    add_compile_definitions(__ANDROID__)
    set(CMAKE_SYSROOT /home/eternalfuture/Android/Sdk/ndk/28.0.13004108/toolchains/llvm/prebuilt/linux-x86_64/sysroot/)
endif ()

set(PATCHLIB_SRC)
if (ANDROID)
    list(APPEND PATCHLIB_SRC
            src/patchlib/android/type.c
            src/patchlib/android/field.c
            src/patchlib/android/property.c
            src/patchlib/android/method.c
            src/patchlib/android/il2cpp_api.c
            src/patchlib/android/private.c
            src/patchlib/android/method_hook.c
            src/patchlib/android/struct/array.c
            src/patchlib/android/struct/string.c
            src/patchlib/android/struct/list.c
            src/patchlib/android/struct/dictionary.c
    )
else ()
    list(APPEND PATCHLIB_SRC
            src/patchlib/desktop/type.c
            src/patchlib/desktop/field.c
            src/patchlib/desktop/property.c
            src/patchlib/desktop/method.c
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

set(MEMDL_SRC
        src/memdl/memdl.c
)

set(INTERNAL_SRC
        src/log.c
)

if (ANDROID)
    list(APPEND INTERNAL_SRC
            src/android/android_runtime_core.cpp
            src/android/android_test.cpp
    )
endif ()

add_library(tefkernel SHARED
        ${PATCHLIB_SRC}
        ${INTERNAL_SRC}
        ${TEFSTD_SRC}
        ${TEFPLUGIN_SRC}
        ${MEMDL_SRC}
        ${INTERNAL_SRC}
        ${MODLOADER_SRC}
)

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

    add_subdirectory(dobby)
    target_link_libraries(tefkernel
            PUBLIC
            android
            log
            dobby_static
    )
endif ()

target_include_directories(tefkernel
        PUBLIC
        include
        src
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_custom_command(TARGET tefkernel POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Stripping symbols from tefkernel..."
            COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:tefkernel>
            COMMENT "Stripping symbols from tefkernel library using strip command"
            VERBATIM
    )
endif()

set(TEST OFF)
if (${TEST})


    add_executable(test
            test/test.c
    )

    target_link_libraries(test
            tefkernel
            libffi
    )

    add_library(test_plugin SHARED
            test/test_plugin.c
    )

    add_library(test_modloader SHARED
            test/test_modloader.c
    )

    add_library(test_mod SHARED
            test/test_mod.c
    )
endif ()
