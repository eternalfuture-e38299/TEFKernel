/*******************************************************************************
 * tefpackage - tefpkg
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
 * Created: 2026/2/23
 *******************************************************************************/

#ifndef TEFPACKAGE_TEFPKG_H
#define TEFPACKAGE_TEFPKG_H

#include <stdint.h>
#include <stdio.h>
#include "../tef_api.h"

#if __cplusplus
extern "C" {
#endif

#define TEFPKG_MAGIC       0x50464554  // "TEFP"
#define TEFPKG_VERSION     0x0200      // 版本号升级到2.0，支持预留条目
#define TEFPKG_MAX_FILES   65535       // 最大文件数量

typedef enum {
    TEF_ACCESS_CLOSED = 0,   // 未打开
    TEF_ACCESS_MEMORY = 1,   // 内存模式
    TEF_ACCESS_FILE = 2,     // 文件模式（已弃用，使用READWRITE替代）
    TEF_ACCESS_READONLY = 3, // 只读模式
    TEF_ACCESS_MEMDATA = 4,  // 内存数据模式
    TEF_ACCESS_READWRITE = 5 // 读写模式（支持预留空间）
} tefpkg_access_mode_t;

typedef enum {
    COMPRESS_NONE = 0x00,
    COMPRESS_LZ4 = 0x01,
    COMPRESS_LZ4HC = 0x02
} tefpkg_compress_t;

typedef enum {
    TEF_OK = 0,                 ///< 操作成功
    TEF_ERROR = -1,             ///< 一般性错误
    TEF_ERROR_SIGNATURE = -2,   ///< 签名验证失败（魔术字、版本号不匹配）
    TEF_ERROR_CORRUPT = -3,     ///< 数据损坏或格式错误
    TEF_ERROR_MEMORY = -4,      ///< 内存分配失败
    TEF_ERROR_IO = -5,         ///< 输入输出错误（文件读写失败）
    TEF_ERROR_KEYFILE = -6,     ///< 密钥文件错误（格式无效或密钥错误）
    TEF_ERROR_NOT_FOUND = -7,   ///< 文件或资源未找到
    TEF_ERROR_INVALID = -8,     ///< 参数无效或状态不正确
    TEF_ERROR_NOT_SIGNATURE = -9, ///< 未签名（包没有签名信息）
    TEF_ERROR_INTEGRITY = -10,  ///< 完整性校验不通过（数据被篡改或损坏）
    TEF_ERROR_NO_SPACE = -11    ///< 没有更多空间（预留条目已用完）
} tefpkg_result_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;             // "TEFP"
    uint16_t version;          // 版本
    uint16_t file_count;        // 当前已使用的文件总数
    uint16_t reserved_entries;  // 创建时预留的条目总数
    uint16_t _reserved;         // 保留字段，用于对齐
    uint32_t data_offset;       // 数据区相对偏移（从文件开始）
    uint32_t data_size;         // 数据总大小
    uint64_t timestamp;         // 时间戳
    uint64_t checksum;          // 头部校验和
    uint64_t content_hash;      // 内容完整性
    uint64_t signature;         // 签名
} tefpkg_header_t;

typedef struct __attribute__((packed)) {
    uint32_t index;             // 索引
    uint32_t data_offset;       // 数据相对偏移（从数据区开始）
    uint32_t compressed_size;   // 压缩大小
    uint32_t original_size;     // 原始大小
    uint64_t checksum;          // 校验和
    uint64_t timestamp;         // 时间戳
    uint8_t compress_type;      // 压缩类型
    uint8_t compress_level;     // 压缩等级
} tefpkg_entry_t;

typedef struct {
    tefpkg_header_t header;     // 文件头部
    tefpkg_entry_t **entries;   // 文件条目数组指针

    // 访问模式相关数据
    union {
        uint8_t *data;          // 内存模式：数据指针
        FILE *file_handle;      // 读写/文件模式：文件句柄
        const char *filename;   // 只读模式：文件名
        const uint8_t *mem_data; // 内存数据模式：源数据指针
    };

    uint8_t access_mode;        // 访问模式
    uint32_t mem_data_size;     // 内存数据大小（仅内存数据模式使用）
} tefpkg_t;

// ==================== 包生命周期管理 ====================

/**
 * @brief 从文件创建TEF包（预留空间版本）
 * @param filename 文件名
 * @param reserved_entries 预留的条目数量
 * @param pkg 输出的包指针
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_create_reserved_from_file,
                const char *filename, uint16_t reserved_entries, tefpkg_t **pkg)

/**
 * @brief 从内存创建TEF包（预留空间版本）
 * @param reserved_entries 预留的条目数量
 * @param pkg 输出的包指针
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_create_reserved_from_memory,
                uint16_t reserved_entries, tefpkg_t **pkg)

/**
 * @brief 以只读方式打开TEF包
 * @param filename 包文件名
 * @param pkg 输出的包指针
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_open_readonly,
                const char *filename, tefpkg_t **pkg)

/**
 * @brief 从内存数据打开TEF包
 * @param data 内存数据指针
 * @param data_size 数据大小
 * @param pkg 输出的包指针
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_open_from_memory,
                const uint8_t *data, uint32_t data_size, tefpkg_t **pkg)

/**
 * @brief 保存包到文件
 * @param pkg 包指针
 * @param fingerprint 指纹
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_save_file,
                tefpkg_t *pkg, uint64_t fingerprint)

/**
 * @brief 从内存保存包到文件
 * @param filename 文件名
 * @param pkg 包指针
 * @param fingerprint 指纹
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_save_memory_file,
                const char *filename, tefpkg_t *pkg, uint64_t fingerprint)

/**
 * @brief 关闭TEF包并释放资源
 * @param pkg 包指针
 */
DEFINE_FUNCTION(void, tefpkg_close, tefpkg_t* pkg)

// ==================== 条目操作 ====================

/**
 * @brief 从内存添加条目（检查预留空间）
 * @param pkg 包指针
 * @param compress_type 压缩类型
 * @param compress_level 压缩等级
 * @param data 数据指针
 * @param data_size 数据大小
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_add_entry_from_memory,
                tefpkg_t *pkg, tefpkg_compress_t compress_type,
                uint8_t compress_level, uint8_t *data, uint32_t data_size)

/**
 * @brief 从文件添加条目（检查预留空间）
 * @param pkg 包指针
 * @param filepath 文件路径
 * @param compress_type 压缩类型
 * @param compress_level 压缩等级
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_add_entry_from_file,
                tefpkg_t *pkg, const char *filepath,
                tefpkg_compress_t compress_type, uint8_t compress_level)

/**
 * @brief 提取条目到内存
 * @param pkg 包指针
 * @param entry_index 条目索引
 * @param data 输出的数据指针
 * @param data_size 输出的数据大小
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_extract_entry_to_memory,
                const tefpkg_t *pkg, uint32_t entry_index,
                uint8_t **data, uint32_t *data_size)

/**
 * @brief 提取条目到文件
 * @param pkg 包指针
 * @param entry_index 条目索引
 * @param output_path 输出文件路径
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_extract_entry_to_file,
                const tefpkg_t *pkg, uint32_t entry_index,
                const char *output_path)

/**
 * @brief 获取条目信息
 * @param pkg 包指针
 * @param entry_index 条目索引
 * @param info 输出的条目信息
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_get_entry_info,
                const tefpkg_t *pkg, uint32_t entry_index,
                tefpkg_entry_t **info)

/**
 * @brief 获取包中条目数量
 * @param pkg 包指针
 * @return 条目数量
 */
DEFINE_FUNCTION(uint16_t, tefpkg_get_entries_count, const tefpkg_t *pkg)

/**
 * @brief 获取预留条目数量
 * @param pkg 包指针
 * @return 预留条目数量
 */
DEFINE_FUNCTION(uint16_t, tefpkg_get_reserved_entries, const tefpkg_t *pkg)

/**
 * @brief 验证条目完整性
 * @param pkg 包指针
 * @param entry_index 条目索引
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_verify_entry,
                const tefpkg_t *pkg, uint32_t entry_index)

/**
 * @brief 验证整个包的完整性
 * @param pkg 包指针
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_verify_pkg, const tefpkg_t *pkg)

/**
 * @brief 验证包签名
 * @param pkg 包指针
 * @param fingerprint 指纹
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_verify_signature,
                const tefpkg_t *pkg, uint64_t fingerprint)

/**
 * @brief 签名TEF包
 * @param pkg 包指针
 * @param fingerprint 指纹
 * @return 操作结果
 */
DEFINE_FUNCTION(tefpkg_result_t, tefpkg_sign_package,
                tefpkg_t *pkg, uint64_t fingerprint)

#if __cplusplus
}
#endif
#endif //TEFPACKAGE_TEFPKG_H