# osxcross ARM64 toolchain
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR arm64)

# 设置 osxcross 路径
set(OSXCROSS_PATH "/usr/local/osxcross")
set(OSXCROSS_BIN "${OSXCROSS_PATH}/bin")
set(OSXCROSS_SDK "${OSXCROSS_PATH}/SDK/MacOSX.sdk")

# 编译器 (ARM64)
set(CMAKE_C_COMPILER "${OSXCROSS_BIN}/oa64-clang" CACHE PATH "")
set(CMAKE_CXX_COMPILER "${OSXCROSS_BIN}/oa64-clang++" CACHE PATH "")

# 工具链工具
set(CMAKE_AR "${OSXCROSS_BIN}/arm64-apple-darwin25.1-ar" CACHE PATH "")
set(CMAKE_RANLIB "${OSXCROSS_BIN}/arm64-apple-darwin25.1-ranlib" CACHE PATH "")
set(CMAKE_LINKER "${OSXCROSS_BIN}/arm64-apple-darwin25.1-ld" CACHE PATH "")
set(CMAKE_STRIP "${OSXCROSS_BIN}/arm64-apple-darwin25.1-strip" CACHE PATH "")
set(CMAKE_NM "${OSXCROSS_BIN}/arm64-apple-darwin25.1-nm" CACHE PATH "")
set(CMAKE_OTOOL "${OSXCROSS_BIN}/arm64-apple-darwin25.1-otool" CACHE PATH "")

# macOS SDK 设置
set(CMAKE_OSX_SYSROOT "${OSXCROSS_SDK}" CACHE PATH "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")

# 链接器设置
set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=${OSXCROSS_BIN}/arm64-apple-darwin25.1-ld" CACHE STRING "")
set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=${OSXCROSS_BIN}/arm64-apple-darwin25.1-ld" CACHE STRING "")
set(CMAKE_MODULE_LINKER_FLAGS "-fuse-ld=${OSXCROSS_BIN}/arm64-apple-darwin25.1-ld" CACHE STRING "")

# 编译标志
set(CMAKE_C_FLAGS "-arch arm64 -isysroot ${OSXCROSS_SDK} -mmacosx-version-min=11.0" CACHE STRING "")
set(CMAKE_CXX_FLAGS "-arch arm64 -isysroot ${OSXCROSS_SDK} -mmacosx-version-min=11.0" CACHE STRING "")

# 查找路径
set(CMAKE_FIND_ROOT_PATH "${OSXCROSS_SDK}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 标记为可用的编译器
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)
