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

#include "compressor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lz4.h"
#include "lz4hc.h"

struct tefpkg_compress_ctx_s {
    void* lz4_stream;  // LZ4_stream_t* 或 LZ4_streamHC_t*
    tefpkg_compress_t compress_type;
    uint8_t compress_level;
    int is_initialized;
};

struct tefpkg_decompress_ctx_s {
    LZ4_streamDecode_t* lz4_stream;
    tefpkg_compress_t compress_type;
    int is_initialized;
};

int tefpkg_compress_data(const uint8_t* src, const uint32_t src_size,
                        uint8_t* dst, const tefpkg_compress_t compress_type,
                        uint8_t compress_level) {
    // 参数检查
    if (!src || !dst || src_size == 0) {
        return TEF_ERROR;
    }

    int compressed_size = 0;

    switch (compress_type) {
        case COMPRESS_NONE:
            // 不压缩，直接拷贝
            memcpy(dst, src, src_size);
            compressed_size = (int)src_size;
            break;

        case COMPRESS_LZ4:
            // 快速LZ4压缩
            compressed_size = LZ4_compress_default((const char*)src, (char*)dst,
                                                   (int)src_size, LZ4_compressBound((int)src_size));
            if (compressed_size <= 0) {
                return TEF_ERROR;
            }
            break;

        case COMPRESS_LZ4HC:
            // 高压缩比LZ4
            if (compress_level < 1) compress_level = 1;
            if (compress_level > 12) compress_level = 12;

            compressed_size = LZ4_compress_HC((const char*)src, (char*)dst,
                                             (int)src_size, LZ4_compressBound((int)src_size),
                                             compress_level);
            if (compressed_size <= 0) {
                return TEF_ERROR;
            }
            break;

        default:
            return TEF_ERROR;
    }

    return compressed_size;
}

int tefpkg_decompress_data(const uint8_t* src, const uint32_t src_size,
                          uint8_t* dst, const uint32_t original_size,
                          const tefpkg_compress_t compress_type) {
    // 参数检查
    if (!src || !dst || src_size == 0 || original_size == 0) {
        return TEF_ERROR;
    }

    int decompressed_size = 0;

    switch (compress_type) {
        case COMPRESS_NONE:
            // 未压缩，直接拷贝
            if (src_size != original_size) {
                return TEF_ERROR;
            }
            memcpy(dst, src, src_size);
            decompressed_size = (int)src_size;
            break;

        case COMPRESS_LZ4:
        case COMPRESS_LZ4HC:
            // LZ4解压（LZ4和LZ4HC压缩的数据使用相同的解压函数）
            decompressed_size = LZ4_decompress_safe((const char*)src, (char*)dst,
                                                   (int)src_size, (int)original_size);
            if (decompressed_size < 0 || (uint32_t)decompressed_size != original_size) {
                return TEF_ERROR;
            }
            break;

        default:
            return TEF_ERROR;
    }

    return decompressed_size;
}

int tefpkg_compress_memory(const uint8_t* src, const uint32_t src_size,
                          uint8_t** dst, uint32_t* dst_size,
                          const tefpkg_compress_t compress_type, const uint8_t compress_level) {
    // 参数检查
    if (!src || !dst || !dst_size || src_size == 0) {
        return TEF_ERROR;
    }

    // 计算最大压缩后大小
    const uint32_t max_compressed_size = tefpkg_max_compressed_size(src_size, compress_type);
    *dst = (uint8_t*)malloc(max_compressed_size);
    if (!*dst) {
        return TEF_ERROR;
    }

    // 执行压缩
    const int compressed_size = tefpkg_compress_data(src, src_size, *dst, compress_type, compress_level);
    if (compressed_size == TEF_ERROR) {
        free(*dst);
        *dst = NULL;
        return TEF_ERROR;
    }

    *dst_size = compressed_size;
    return TEF_OK;
}

int tefpkg_decompress_memory(const uint8_t* src, const uint32_t src_size,
                            uint8_t** dst, const uint32_t original_size,
                            const tefpkg_compress_t compress_type) {
    // 参数检查
    if (!src || !dst || src_size == 0 || original_size == 0) {
        return TEF_ERROR;
    }

    // 分配输出缓冲区
    *dst = (uint8_t*)malloc(original_size);
    if (!*dst) {
        return TEF_ERROR;
    }

    // 执行解压
    const int decompressed_size = tefpkg_decompress_data(src, src_size, *dst, original_size, compress_type);
    if (decompressed_size == TEF_ERROR) {
        free(*dst);
        *dst = NULL;
        return TEF_ERROR;
    }

    return TEF_OK;
}

tefpkg_compress_ctx_t* tefpkg_compress_begin(const tefpkg_compress_t compress_type, const uint8_t compress_level) {
    // 分配上下文
    tefpkg_compress_ctx_t* ctx = calloc(1, sizeof(tefpkg_compress_ctx_t));
    if (!ctx) {
        return NULL;
    }

    ctx->compress_type = compress_type;
    ctx->compress_level = compress_level;

    // 根据压缩类型初始化相应的流
    switch (compress_type) {
        case COMPRESS_LZ4:
            ctx->lz4_stream = LZ4_createStream();
            if (!ctx->lz4_stream) {
                free(ctx);
                return NULL;
            }
            LZ4_resetStream(ctx->lz4_stream);
            break;

        case COMPRESS_LZ4HC:
            ctx->lz4_stream = LZ4_createStreamHC();
            if (!ctx->lz4_stream) {
                free(ctx);
                return NULL;
            }
            LZ4_resetStreamHC(ctx->lz4_stream, compress_level);
            break;

        case COMPRESS_NONE:
            // 无压缩不需要特殊初始化
            break;

        default:
            free(ctx);
            return NULL;
    }

    ctx->is_initialized = 1;
    return ctx;
}

int tefpkg_compress_chunk(tefpkg_compress_ctx_t* ctx,
                         const uint8_t* src, const uint32_t src_size,
                         uint8_t* dst, uint32_t* dst_size) {
    // 参数检查
    if (!ctx || !src || !dst || !dst_size || src_size == 0) {
        return TEF_ERROR;
    }

    if (!ctx->is_initialized) {
        return TEF_ERROR;
    }

    int compressed_size = 0;

    switch (ctx->compress_type) {
        case COMPRESS_NONE:
            // 无压缩，直接拷贝
            memcpy(dst, src, src_size);
            compressed_size = (int)src_size;
            break;

        case COMPRESS_LZ4:
            compressed_size = LZ4_compress_fast_continue(
                ctx->lz4_stream,
                (const char*)src, (char*)dst,
                (int)src_size, LZ4_compressBound((int)src_size),
                1  // 加速参数
            );
            break;

        case COMPRESS_LZ4HC:
            compressed_size = LZ4_compress_HC_continue(
                ctx->lz4_stream,
                (const char*)src, (char*)dst,
                (int)src_size, LZ4_compressBound((int)src_size)
            );
            break;

        default:
            return TEF_ERROR;
    }

    if (compressed_size <= 0) {
        return TEF_ERROR;
    }

    *dst_size = compressed_size;
    return TEF_OK;
}

void tefpkg_compress_end(tefpkg_compress_ctx_t* ctx) {
    if (!ctx) return;

    // 释放LZ4流资源
    if (ctx->lz4_stream) {
        switch (ctx->compress_type) {
            case COMPRESS_LZ4:
                LZ4_freeStream(ctx->lz4_stream);
                break;
            case COMPRESS_LZ4HC:
                LZ4_freeStreamHC(ctx->lz4_stream);
                break;
            default:
                break;
        }
    }

    free(ctx);
}


tefpkg_decompress_ctx_t* tefpkg_decompress_begin(const tefpkg_compress_t compress_type) {
    // 无压缩类型不需要流式解压
    if (compress_type == COMPRESS_NONE) {
        return NULL;
    }

    // 分配上下文
    tefpkg_decompress_ctx_t* ctx = calloc(1, sizeof(tefpkg_decompress_ctx_t));
    if (!ctx) {
        return NULL;
    }

    ctx->compress_type = compress_type;

    // 创建LZ4解压流
    ctx->lz4_stream = LZ4_createStreamDecode();
    if (!ctx->lz4_stream) {
        free(ctx);
        return NULL;
    }

    ctx->is_initialized = 1;
    return ctx;
}

int tefpkg_decompress_chunk(tefpkg_decompress_ctx_t* ctx,
                           const uint8_t* src, const uint32_t src_size,
                           uint8_t* dst, const uint32_t dst_size) {
    // 参数检查
    if (!ctx || !src || !dst || src_size == 0 || dst_size == 0) {
        return TEF_ERROR;
    }

    if (!ctx->is_initialized) {
        return TEF_ERROR;
    }

    int decompressed_size = 0;

    switch (ctx->compress_type) {
        case COMPRESS_LZ4:
        case COMPRESS_LZ4HC:
            decompressed_size = LZ4_decompress_safe_continue(
                ctx->lz4_stream,
                (const char*)src, (char*)dst,
                (int)src_size, (int)dst_size
            );
            break;

        default:
            return TEF_ERROR;
    }

    if (decompressed_size <= 0) {
        return TEF_ERROR;
    }

    return decompressed_size;
}

void tefpkg_decompress_end(tefpkg_decompress_ctx_t* ctx) {
    if (!ctx) return;

    if (ctx->lz4_stream) {
        LZ4_freeStreamDecode(ctx->lz4_stream);
    }

    free(ctx);
}


int tefpkg_compress_file(const char* input_file, const char* output_file,
                        const tefpkg_compress_t compress_type, const uint8_t compress_level) {
    // 打开文件
    FILE* fin = fopen(input_file, "rb");
    FILE* fout = fopen(output_file, "wb");
    if (!fin || !fout) {
        if (fin) fclose(fin);
        if (fout) fclose(fout);
        return TEF_ERROR;
    }

    // 获取文件大小
    fseek(fin, 0, SEEK_END);
    const long file_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(fin);
        fclose(fout);
        return TEF_ERROR;
    }

    // 读取文件数据
    uint8_t* input_data = malloc(file_size);
    if (!input_data) {
        fclose(fin);
        fclose(fout);
        return TEF_ERROR;
    }

    if (fread(input_data, 1, file_size, fin) != (size_t)file_size) {
        free(input_data);
        fclose(fin);
        fclose(fout);
        return TEF_ERROR;
    }
    fclose(fin);

    // 压缩数据
    const uint32_t max_compressed_size = tefpkg_max_compressed_size(file_size, compress_type);
    uint8_t* compressed_data = malloc(max_compressed_size);
    if (!compressed_data) {
        free(input_data);
        fclose(fout);
        return TEF_ERROR;
    }

    const int compressed_size = tefpkg_compress_data(input_data, file_size, compressed_data,
                                             compress_type, compress_level);
    if (compressed_size == TEF_ERROR) {
        free(input_data);
        free(compressed_data);
        fclose(fout);
        return TEF_ERROR;
    }

    // 写入压缩数据
    int result = TEF_OK;
    if (fwrite(compressed_data, 1, compressed_size, fout) != (size_t)compressed_size) {
        result = TEF_ERROR;
    }

    // 清理资源
    free(input_data);
    free(compressed_data);
    fclose(fout);

    return result;
}

int tefpkg_decompress_file(const char* input_file, const char* output_file,
                          const tefpkg_compress_t compress_type, const uint32_t original_size) {
    // 打开文件
    FILE* fin = fopen(input_file, "rb");
    FILE* fout = fopen(output_file, "wb");
    if (!fin || !fout) {
        if (fin) fclose(fin);
        if (fout) fclose(fout);
        return TEF_ERROR;
    }

    // 获取压缩文件大小
    fseek(fin, 0, SEEK_END);
    const long compressed_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    if (compressed_size <= 0) {
        fclose(fin);
        fclose(fout);
        return TEF_ERROR;
    }

    // 读取压缩数据
    uint8_t* compressed_data = malloc(compressed_size);
    if (!compressed_data) {
        fclose(fin);
        fclose(fout);
        return TEF_ERROR;
    }

    if (fread(compressed_data, 1, compressed_size, fin) != (size_t)compressed_size) {
        free(compressed_data);
        fclose(fin);
        fclose(fout);
        return TEF_ERROR;
    }
    fclose(fin);

    // 解压数据
    uint8_t* decompressed_data = malloc(original_size);
    if (!decompressed_data) {
        free(compressed_data);
        fclose(fout);
        return TEF_ERROR;
    }

    const int result_size = tefpkg_decompress_data(compressed_data, compressed_size,
                                           decompressed_data, original_size, compress_type);
    if (result_size == TEF_ERROR) {
        free(compressed_data);
        free(decompressed_data);
        fclose(fout);
        return TEF_ERROR;
    }

    // 写入解压数据
    int result = TEF_OK;
    if (fwrite(decompressed_data, 1, original_size, fout) != original_size) {
        result = TEF_ERROR;
    }

    // 清理资源
    free(compressed_data);
    free(decompressed_data);
    fclose(fout);

    return result;
}

uint32_t tefpkg_max_compressed_size(const uint32_t original_size, const tefpkg_compress_t compress_type) {
    if (compress_type == COMPRESS_NONE)
        return original_size;
    return LZ4_compressBound((int)original_size);
}