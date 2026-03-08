/*******************************************************************************
 * tefpackage - compressor
 * Copyright (C) 2025 EternalFuture
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
 * Created: 2025/11/8
 *******************************************************************************/

#pragma once
#include <stdint.h>

#include "tefpackage/tefpkg.h"


#if __cplusplus
extern "C" {
#endif

typedef struct tefpkg_compress_ctx_s tefpkg_compress_ctx_t;
typedef struct tefpkg_decompress_ctx_s tefpkg_decompress_ctx_t;

/**
 * @brief 压缩数据
 * @param src 源数据指针
 * @param src_size 源数据大小
 * @param dst 目标缓冲区（必须足够大）
 * @param compress_type 压缩类型
 * @param compress_level 压缩级别（LZ4HC专用）
 * @return 压缩后大小，失败返回TEF_ERROR
 */
int tefpkg_compress_data(const uint8_t* src, uint32_t src_size,
                        uint8_t* dst, tefpkg_compress_t compress_type,
                        uint8_t compress_level);

/**
 * @brief 解压数据
 * @param src 压缩数据指针
 * @param src_size 压缩数据大小
 * @param dst 目标缓冲区（必须足够存放解压后数据）
 * @param original_size 预期的原始数据大小
 * @param compress_type 压缩类型
 * @return 解压后大小，失败返回TEF_ERROR
 */
int tefpkg_decompress_data(const uint8_t* src, uint32_t src_size,
                          uint8_t* dst, uint32_t original_size,
                          tefpkg_compress_t compress_type);

/**
 * @brief 压缩内存数据（自动分配内存）
 * @param src 源数据指针
 * @param src_size 源数据大小
 * @param dst 输出压缩数据指针（需要调用者释放）
 * @param dst_size 输出压缩数据大小
 * @param compress_type 压缩类型
 * @param compress_level 压缩级别
 * @return TEF_OK 成功，TEF_ERROR 失败
 */
int tefpkg_compress_memory(const uint8_t* src, uint32_t src_size,
                          uint8_t** dst, uint32_t* dst_size,
                          tefpkg_compress_t compress_type, uint8_t compress_level);

/**
 * @brief 解压内存数据（自动分配内存）
 * @param src 压缩数据指针
 * @param src_size 压缩数据大小
 * @param dst 输出解压数据指针（需要调用者释放）
 * @param original_size 预期的原始数据大小
 * @param compress_type 压缩类型
 * @return TEF_OK 成功，TEF_ERROR 失败
 */
int tefpkg_decompress_memory(const uint8_t* src, uint32_t src_size,
                            uint8_t** dst, uint32_t original_size,
                            tefpkg_compress_t compress_type);

/**
 * @brief 开始流式压缩
 * @param compress_type 压缩类型
 * @param compress_level 压缩级别
 * @return 压缩上下文指针，失败返回NULL
 */
tefpkg_compress_ctx_t* tefpkg_compress_begin(tefpkg_compress_t compress_type, uint8_t compress_level);

/**
 * @brief 压缩数据块
 * @param ctx 压缩上下文
 * @param src 源数据指针
 * @param src_size 源数据大小
 * @param dst 目标缓冲区（必须足够大）
 * @param dst_size 输出压缩后大小
 * @return TEF_OK 成功，TEF_ERROR 失败
 */
int tefpkg_compress_chunk(tefpkg_compress_ctx_t* ctx,
                         const uint8_t* src, uint32_t src_size,
                         uint8_t* dst, uint32_t* dst_size);

/**
 * @brief 结束流式压缩
 * @param ctx 压缩上下文
 */
void tefpkg_compress_end(tefpkg_compress_ctx_t* ctx);

/**
 * @brief 开始流式解压
 * @param compress_type 压缩类型
 * @return 解压上下文指针，失败返回NULL
 */
tefpkg_decompress_ctx_t* tefpkg_decompress_begin(tefpkg_compress_t compress_type);

/**
 * @brief 解压数据块
 * @param ctx 解压上下文
 * @param src 压缩数据指针
 * @param src_size 压缩数据大小
 * @param dst 目标缓冲区（必须足够存放解压后数据）
 * @param dst_size 目标缓冲区大小
 * @return 解压后大小，失败返回TEF_ERROR
 */
int tefpkg_decompress_chunk(tefpkg_decompress_ctx_t* ctx,
                           const uint8_t* src, uint32_t src_size,
                           uint8_t* dst, uint32_t dst_size);

/**
 * @brief 结束流式解压
 * @param ctx 解压上下文
 */
void tefpkg_decompress_end(tefpkg_decompress_ctx_t* ctx);

/**
 * @brief 压缩文件
 * @param input_file 输入文件名
 * @param output_file 输出文件名
 * @param compress_type 压缩类型
 * @param compress_level 压缩级别
 * @return TEF_OK 成功，TEF_ERROR 失败
 */
int tefpkg_compress_file(const char* input_file, const char* output_file,
                        tefpkg_compress_t compress_type, uint8_t compress_level);

/**
 * @brief 解压文件
 * @param input_file 输入文件名
 * @param output_file 输出文件名
 * @param compress_type 压缩类型
 * @param original_size 预期的原始文件大小
 * @return TEF_OK 成功，TEF_ERROR 失败
 */
int tefpkg_decompress_file(const char* input_file, const char* output_file,
                          tefpkg_compress_t compress_type, uint32_t original_size);

/**
 * @brief 计算压缩后数据的最大可能大小
 * @param original_size 原始数据大小
 * @param compress_type 压缩类型
 * @return 最大压缩后大小
 */
uint32_t tefpkg_max_compressed_size(uint32_t original_size, tefpkg_compress_t compress_type);

#if __cplusplus
}
#endif