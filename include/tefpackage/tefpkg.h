/*******************************************************************************
 * File: tefpkg
 * Project: tefkernel
 * Created: 2025/11/23
 * Author: eternalfuture-e38299
 * Github: https://github.com/eternalfuture-e38299
 *
 * MIT License
 *
 * Copyright (c) 2025 eternalfuture-e38299
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

#ifndef TEFKERNEL_TEFPKG_H
#define TEFKERNEL_TEFPKG_H

#include <stdint.h>

#include "../tef_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 基本类型定义 ====================
typedef void tefpkg_handle_t;

// ==================== 枚举定义 ====================
typedef enum {
    TEFPKG_SUCCESS = 0,
    TEFPKG_ERROR = -1,
    TEFPKG_ERROR_SIGNATURE = -2,
    TEFPKG_ERROR_CORRUPT = -3,
    TEFPKG_ERROR_MEMORY = -4,
    TEFPKG_ERROR_IO = -5,
    TEFPKG_ERROR_KEYFILE = -6,
    TEFPKG_ERROR_NOT_FOUND = -7,
    TEFPKG_ERROR_INVALID = -8,
    TEFPKG_ERROR_NOT_SIGNATURE = -9,
    TEFPKG_ERROR_INTEGRITY = -10
} tefpkg_result_t;

// ==================== 核心IO函数 ====================
/**
 * @brief 以只读方式打开包文件
 * @param filename 包文件名
 * @param pkg 输出的包指针
 * @return 操作结果
 */
DEFINE_API_FUNCTION(tefpkg_result_t, tefpkg_open_readonly, const char *filename, void **pkg)

/**
 * @brief 关闭包并释放资源
 * @param pkg 包指针
 */
DEFINE_API_FUNCTION(void, tefpkg_close, tefpkg_handle_t* pkg)

// ==================== 包信息获取API ====================

/**
 * @brief 获取包中文件数量
 * @param pkg 包句柄
 * @return 文件数量
 */
DEFINE_API_FUNCTION(uint32_t, tefpkg_get_file_count, tefpkg_handle_t* pkg)

// ==================== 文件提取API ====================

/**
 * @brief 提取文件到指定路径
 * @param pkg 包句柄
 * @param file_index 文件索引
 * @param output_path 输出文件路径
 * @return 操作结果
 */
DEFINE_API_FUNCTION(tefpkg_result_t, tefpkg_extract_file, tefpkg_handle_t* pkg, uint32_t file_index,
                const char *output_path)

/**
 * @brief 提取文件到内存
 * @param pkg 包句柄
 * @param file_index 文件索引
 * @param[out] data 输出的数据指针（需要调用tefpkg_free_memory释放）
 * @param[out] data_size 输出的数据大小
 * @return 操作结果
 */
DEFINE_API_FUNCTION(tefpkg_result_t, tefpkg_extract_to_memory, tefpkg_handle_t* pkg, uint32_t file_index,
                uint8_t **data, uint32_t *data_size)

// ==================== 完整性校验API ====================

/**
 * @brief 验证包签名
 * @param pkg 包句柄
 * @param fingerprint 包指纹
 * @return 操作结果
 */
DEFINE_API_FUNCTION(tefpkg_result_t, tefpkg_verify_signature, tefpkg_handle_t* pkg, uint64_t fingerprint)

/**
 * @brief 验证文件完整性
 * @param pkg 包句柄
 * @param file_index 文件索引
 * @return 操作结果
 */
DEFINE_API_FUNCTION(tefpkg_result_t, tefpkg_verify_file, tefpkg_handle_t* pkg, uint32_t file_index)

/**
 * @brief 验证整个包的完整性
 * @param pkg 包句柄
 * @return 操作结果
 */
DEFINE_API_FUNCTION(tefpkg_result_t, tefpkg_verify_package, tefpkg_handle_t* pkg)

// ==================== 工具函数 ====================

/**
 * @brief 获取错误描述信息
 * @param result 错误码
 * @return 错误描述字符串
 */
DEFINE_API_FUNCTION(const char*, tefpkg_get_error_string, tefpkg_result_t result)

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_TEFPKG_H
