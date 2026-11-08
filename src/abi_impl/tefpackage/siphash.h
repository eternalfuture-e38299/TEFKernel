/*******************************************************************************
 * tefpackage - siphash
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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#if __cplusplus
extern "C" {



#endif

#define SIPHASH_KEY_MAGIC 0x53495048  // "SIPH"
#define SIPHASH_KEY_VERSION 0x0100    // 0.1.0

/**
 * @brief SipHash 计算上下文结构体
 *
 * 用于流式计算SipHash值，支持分块处理大数据
 */
typedef struct {
    uint64_t v0, v1, v2, v3; ///< SipHash内部状态变量
    uint64_t total_len; ///< 已处理数据总长度
    uint8_t buffer[8]; ///< 数据缓冲区（处理不满8字节的数据）
    size_t buffer_len; ///< 缓冲区中有效数据长度
} siphash_ctx_t;

/**
 * @brief SipHash密钥生成配置
 *
 * 用于生成密钥时的配置参数，增强密钥的随机性和唯一性
 */
typedef struct {
    const char *author; ///< 作者信息（用于密钥派生）
    const char *organization; ///< 组织信息（用于密钥派生）
    const char *location; ///< 位置信息（用于密钥派生）
} siphash_config_t;

/**
 * @brief SipHash密钥文件结构
 *
 * 密钥文件的二进制格式，包含魔术字、版本和128位密钥
 */
typedef struct __attribute__((packed)) {
    uint32_t magic; ///< 魔术字: 0x53495048 ("SIPH")
    uint32_t version; ///< 版本号: 0x0100
    uint64_t timestamp; ///< 创建时间戳
    uint64_t key0; ///< 128位密钥的低64位
    uint64_t key1; ///< 128位密钥的高64位
    char author[64]; ///< 作者信息（可选，用于验证）
    char organization[64]; ///< 组织信息（可选，用于验证）
    char location[64]; ///< 位置信息（可选，用于验证）
} siphash_keyfile_t;

// 平台无关的类型定义
typedef uint64_t sip_uint64_t; ///< 平台无关的64位无符号整数
typedef uint32_t sip_uint32_t; ///< 平台无关的32位无符号整数
typedef uint8_t sip_uint8_t; ///< 平台无关的8位无符号整数
typedef size_t sip_size_t; ///< 平台无关的大小类型

// 核心函数

/**
 * @brief 初始化SipHash计算上下文
 *
 * @param ctx  SipHash上下文指针
 * @param key0 128位密钥的低64位
 * @param key1 128位密钥的高64位
 */
void siphash_init(siphash_ctx_t *ctx, uint64_t key0, uint64_t key1);

/**
 * @brief 更新SipHash计算状态（流式处理）
 *
 * @param ctx  SipHash上下文指针
 * @param data 输入数据指针
 * @param len  输入数据长度
 */
void siphash_update(siphash_ctx_t *ctx, const uint8_t *data, size_t len);

/**
 * @brief 完成SipHash计算并返回最终哈希值
 *
 * @param ctx SipHash上下文指针
 * @return uint64_t 64位SipHash值
 */
uint64_t siphash_final(siphash_ctx_t *ctx);

/**
 * @brief 单次计算数据的SipHash值（非流式）
 *
 * @param data 输入数据指针
 * @param len  输入数据长度
 * @param key0 128位密钥的低64位
 * @param key1 128位密钥的高64位
 * @return uint64_t 64位SipHash值
 */
uint64_t siphash_stream(const uint8_t *data, size_t len,
                        uint64_t key0, uint64_t key1);

/**
 * @brief 验证数据的SipHash值是否匹配
 *
 * @param data     输入数据指针
 * @param len      输入数据长度
 * @param key0     128位密钥的低64位
 * @param key1     128位密钥的高64位
 * @param expected 期望的哈希值
 * @return int     匹配返回1，不匹配返回0
 */
int siphash_verify_stream(const uint8_t *data, size_t len,
                          uint64_t key0, uint64_t key1, uint64_t expected);

/**
 * @brief 根据配置生成128位SipHash密钥
 *
 * @param config 密钥生成配置
 * @param key0   输出：128位密钥的低64位
 * @param key1   输出：128位密钥的高64位
 */
void siphash_make_key128(siphash_config_t config, uint64_t *key0, uint64_t *key1);

/**
 * @brief 生成SipHash密钥并保存到文件
 *
 * @param filename 密钥文件名
 * @param config   密钥生成配置
 */
void siphash_make_key_to_file(const char *filename, siphash_config_t config);

/**
 * @brief 从文件加载SipHash密钥
 *
 * @param filename 密钥文件名
 * @param keyfile 输出的密钥指针
 * @return bool    成功返回true，失败返回false
 */
bool siphash_load_key_from_file(const char *filename, siphash_keyfile_t **keyfile);

/**
 * @brief 生成基于时间戳的随机哈希值
 *
 * 使用当前时间戳和随机种子生成伪随机哈希值
 *
 * @param seed 随机种子（可选，为0时使用时间戳）
 * @return uint64_t 64位随机哈希值
 */
uint64_t siphash_generate_random_hash(uint64_t seed);

/**
 * @brief 根据ID和密钥生成指纹
 *
 * 将ID与密钥结合生成唯一的指纹值，可用于身份验证等场景
 *
 * @param id 用户ID或标识符
 * @param keyfile 密钥
 * @return uint64_t 64位指纹值
 */
uint64_t siphash_generate_fingerprint(uint64_t id, siphash_keyfile_t *keyfile);

#if __cplusplus
}
#endif
