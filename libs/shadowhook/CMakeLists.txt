cmake_minimum_required(VERSION 3.20)
project(shadowhook)
enable_language(ASM)

if(${ANDROID_ABI} STREQUAL "arm64-v8a")
    set(ARCH "arm64")
    set(ARCH_LINK_FLAGS "-Wl,-z,max-page-size=16384")
elseif(${ANDROID_ABI} STREQUAL "armeabi-v7a")
    set(ARCH "arm")
    set(ARCH_LINK_FLAGS "")
else()
    message(FATAL_ERROR "Unsupported ANDROID_ABI: ${ANDROID_ABI}")
endif()

set(TARGET "shadowhook")

# 排除 sh_jni.c（Java 层专用）和 nothing/ 目录
file(GLOB SRC *.c arch/${ARCH}/*.c arch/${ARCH}/*.S common/*.c third_party/*/*.c)
list(REMOVE_ITEM SRC ${CMAKE_CURRENT_SOURCE_DIR}/sh_jni.c)

add_library(${TARGET} STATIC ${SRC})

target_compile_features(${TARGET} PRIVATE c_std_11)

target_include_directories(${TARGET}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xdl
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/arch/${ARCH}
        ${CMAKE_CURRENT_SOURCE_DIR}/common
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/bsd
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lss
)

target_link_libraries(${TARGET} PRIVATE log)

# 静态库不设置任何链接选项，链接选项由宿主 tefkernel 决定
# 特别注意：不要加 -Wl,--gc-sections，否则 constructor 会被回收

if(USEASAN)
    target_compile_options(${TARGET} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
else()
    target_compile_options(${TARGET} PRIVATE -Os -ffunction-sections -fdata-sections)
endif()