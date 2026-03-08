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

#include "siphash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 辅助函数
static int is_little_endian(void) {
    static const uint32_t test = 0x01020304;
    return ((const uint8_t*)&test)[0] == 0x04;
}

static uint64_t read_le64(const uint8_t *data) {
    if (is_little_endian()) {
        // 小端序平台直接读取
        return *((const uint64_t*)data);
    } else {
        // 大端序平台需要字节交换
        return ((uint64_t)data[0]) |
               ((uint64_t)data[1] << 8) |
               ((uint64_t)data[2] << 16) |
               ((uint64_t)data[3] << 24) |
               ((uint64_t)data[4] << 32) |
               ((uint64_t)data[5] << 40) |
               ((uint64_t)data[6] << 48) |
               ((uint64_t)data[7] << 56);
    }
}

static uint64_t rotl64(const uint64_t x, const uint8_t b) {
    return (x << b) | (x >> (64 - b));
}

void siphash_init(siphash_ctx_t *ctx, const uint64_t key0, const uint64_t key1) {
    ctx->v0 = 0x736f6d6570736575ULL ^ key0;
    ctx->v1 = 0x646f72616e646f6dULL ^ key1;
    ctx->v2 = 0x6c7967656e657261ULL ^ key0;
    ctx->v3 = 0x7465646279746573ULL ^ key1;
    ctx->total_len = 0;
    ctx->buffer_len = 0;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void siphash_update(siphash_ctx_t *ctx, const uint8_t *data, size_t len) {
    ctx->total_len += len;

    // 处理缓冲区中已有的数据
    if (ctx->buffer_len > 0) {
        size_t copy_len = 8 - ctx->buffer_len;
        if (copy_len > len) copy_len = len;

        memcpy(ctx->buffer + ctx->buffer_len, data, copy_len);
        ctx->buffer_len += copy_len;
        data += copy_len;
        len -= copy_len;

        if (ctx->buffer_len == 8) {
            const uint64_t m = read_le64(ctx->buffer);
            ctx->v3 ^= m;

            // SipHash 轮函数 (2轮)
            for (int i = 0; i < 2; i++) {
                ctx->v0 += ctx->v1;
                ctx->v1 = rotl64(ctx->v1, 13);
                ctx->v1 ^= ctx->v0;
                ctx->v0 = rotl64(ctx->v0, 32);

                ctx->v2 += ctx->v3;
                ctx->v3 = rotl64(ctx->v3, 16);
                ctx->v3 ^= ctx->v2;

                ctx->v0 += ctx->v3;
                ctx->v3 = rotl64(ctx->v3, 21);
                ctx->v3 ^= ctx->v0;

                ctx->v2 += ctx->v1;
                ctx->v1 = rotl64(ctx->v1, 17);
                ctx->v1 ^= ctx->v2;
                ctx->v2 = rotl64(ctx->v2, 32);
            }
            ctx->v0 ^= m;
            ctx->buffer_len = 0;
        }
    }

    // 处理完整的8字节块
    while (len >= 8) {
        const uint64_t m = read_le64(data);
        data += 8;
        len -= 8;

        ctx->v3 ^= m;
        for (int i = 0; i < 2; i++) {
            ctx->v0 += ctx->v1;
            ctx->v1 = rotl64(ctx->v1, 13);
            ctx->v1 ^= ctx->v0;
            ctx->v0 = rotl64(ctx->v0, 32);

            ctx->v2 += ctx->v3;
            ctx->v3 = rotl64(ctx->v3, 16);
            ctx->v3 ^= ctx->v2;

            ctx->v0 += ctx->v3;
            ctx->v3 = rotl64(ctx->v3, 21);
            ctx->v3 ^= ctx->v0;

            ctx->v2 += ctx->v1;
            ctx->v1 = rotl64(ctx->v1, 17);
            ctx->v1 ^= ctx->v2;
            ctx->v2 = rotl64(ctx->v2, 32);
        }
        ctx->v0 ^= m;
    }

    // 保存剩余字节到缓冲区
    if (len > 0) {
        memcpy(ctx->buffer, data, len);
        ctx->buffer_len = len;
    }
}

uint64_t siphash_final(siphash_ctx_t *ctx) {
    // 构造最后的块（包含消息长度的小端序表示）
    uint64_t b = ((uint64_t)ctx->total_len & 0xff) << 56;

    // 处理缓冲区中的剩余数据
    if (ctx->buffer_len > 0) {
        // 将剩余字节复制到临时缓冲区并清零未使用部分
        uint8_t last_block[8] = {0};
        memcpy(last_block, ctx->buffer, ctx->buffer_len);

        // 确保使用平台无关的方式读取
        if (is_little_endian()) {
            b |= *((uint64_t*)last_block);
        } else {
            for (int i = 0; i < ctx->buffer_len; i++) {
                b |= ((uint64_t)last_block[i]) << (i * 8);
            }
        }
    }

    ctx->v3 ^= b;
    for (int i = 0; i < 2; i++) {
        ctx->v0 += ctx->v1;
        ctx->v1 = rotl64(ctx->v1, 13);
        ctx->v1 ^= ctx->v0;
        ctx->v0 = rotl64(ctx->v0, 32);

        ctx->v2 += ctx->v3;
        ctx->v3 = rotl64(ctx->v3, 16);
        ctx->v3 ^= ctx->v2;

        ctx->v0 += ctx->v3;
        ctx->v3 = rotl64(ctx->v3, 21);
        ctx->v3 ^= ctx->v0;

        ctx->v2 += ctx->v1;
        ctx->v1 = rotl64(ctx->v1, 17);
        ctx->v1 ^= ctx->v2;
        ctx->v2 = rotl64(ctx->v2, 32);
    }
    ctx->v0 ^= b;

    // 最终轮 (4轮)
    ctx->v2 ^= 0xff;
    for (int i = 0; i < 4; i++) {
        ctx->v0 += ctx->v1;
        ctx->v1 = rotl64(ctx->v1, 13);
        ctx->v1 ^= ctx->v0;
        ctx->v0 = rotl64(ctx->v0, 32);

        ctx->v2 += ctx->v3;
        ctx->v3 = rotl64(ctx->v3, 16);
        ctx->v3 ^= ctx->v2;

        ctx->v0 += ctx->v3;
        ctx->v3 = rotl64(ctx->v3, 21);
        ctx->v3 ^= ctx->v0;

        ctx->v2 += ctx->v1;
        ctx->v1 = rotl64(ctx->v1, 17);
        ctx->v1 ^= ctx->v2;
        ctx->v2 = rotl64(ctx->v2, 32);
    }

    return ctx->v0 ^ ctx->v1 ^ ctx->v2 ^ ctx->v3;
}

// 流式计算（128位密钥）
uint64_t siphash_stream(const uint8_t *data, const size_t len,
                        const uint64_t key0, const uint64_t key1) {
    siphash_ctx_t ctx;
    siphash_init(&ctx, key0, key1);
    siphash_update(&ctx, data, len);
    return siphash_final(&ctx);
}

// 验证函数
int siphash_verify_stream(const uint8_t *data, const size_t len,
                         const uint64_t key0, const uint64_t key1,
                         const uint64_t expected) {
    return siphash_stream(data, len, key0, key1) == expected;
}

void siphash_make_key128(const siphash_config_t config, uint64_t *key0, uint64_t *key1) {
    uint64_t seed = 0;

    if (config.author) {
        const char *p = config.author;
        while (*p) seed = seed * 131 + (uint8_t)(*p++);
    }
    if (config.organization) {
        const char *p = config.organization;
        while (*p) seed = seed * 131 + (uint8_t)(*p++);
    }
    if (config.location) {
        const char *p = config.location;
        while (*p) seed = seed * 131 + (uint8_t)(*p++);
    }

    seed ^= (uint64_t)time(NULL) << 32;
    seed ^= (uint64_t)clock();

    // 更好的随机化
    seed = (seed >> 32) | (seed << 32); // 交换高低32位
    seed ^= seed >> 33;
    seed *= 0xFF51AFD7ED558CCDULL;
    seed ^= seed >> 33;
    seed *= 0xC4CEB9FE1A85EC53ULL;
    seed ^= seed >> 33;

    *key0 = seed * 0x9E3779B97F4A7C15ULL;
    *key1 = seed * 0xBF58476D1CE4E5B9ULL;

    // 额外的混合
    *key0 ^= (*key0 >> 33) * 0xFF51AFD7ED558CCDULL;
    *key1 ^= (*key1 >> 33) * 0xC4CEB9FE1A85EC53ULL;

    // 交换密钥值以增加随机性
    const uint64_t temp = *key0;
    *key0 = *key1;
    *key1 = temp;
}

void siphash_make_key_to_file(const char *filename, const siphash_config_t config) {
    FILE* fp = fopen(filename, "wb");
    if (fp) {
        siphash_keyfile_t keyfile = {0};

        keyfile.magic = SIPHASH_KEY_MAGIC;
        keyfile.version = SIPHASH_KEY_VERSION;

        uint64_t k0, k1;
        siphash_make_key128(config, &k0, &k1);
        keyfile.key0 = k0;
        keyfile.key1 = k1;

        if (config.author) {
            strncpy(keyfile.author, config.author, sizeof(keyfile.author) - 1);
            keyfile.author[sizeof(keyfile.author) - 1] = '\0';
        }
        if (config.organization) {
            strncpy(keyfile.organization, config.organization, sizeof(keyfile.organization) - 1);
            keyfile.organization[sizeof(keyfile.organization) - 1] = '\0';
        }
        if (config.location) {
            strncpy(keyfile.location, config.location, sizeof(keyfile.location) - 1);
            keyfile.location[sizeof(keyfile.location) - 1] = '\0';
        }
        keyfile.timestamp = (uint64_t)time(NULL);

        fwrite(&keyfile, sizeof(keyfile), 1, fp);
        fclose(fp);
    }
}

bool siphash_load_key_from_file(const char *filename, siphash_keyfile_t **keyfile) {
    if (!filename || !keyfile) return false;

    FILE* fp = fopen(filename, "rb");
    if (!fp)
        return false;

    siphash_keyfile_t* new_keyfile = malloc(sizeof(siphash_keyfile_t));
    if (!new_keyfile) {
        fclose(fp);
        return false;
    }

    bool success = false;
    const size_t read_count = fread(new_keyfile, sizeof(siphash_keyfile_t), 1, fp);

    if (read_count == 1) {
        if (new_keyfile->magic == SIPHASH_KEY_MAGIC &&
            new_keyfile->version == SIPHASH_KEY_VERSION) {
            *keyfile = new_keyfile;
            success = true;
            } else
            free(new_keyfile);
    } else
        free(new_keyfile);

    fclose(fp);
    return success;
}

uint64_t siphash_generate_random_hash(const uint64_t seed) {
    const time_t current_time = time(NULL);
    const uint64_t time_data = (uint64_t)current_time ^ seed;

    const uint64_t key0 = 0x0123456789ABCDEFULL ^ current_time;
    const uint64_t key1 = 0xFEDCBA9876543210ULL ^ current_time >> 32;

    uint8_t data[sizeof(uint64_t)];
    for (int i = 0; i < sizeof(uint64_t); i++) {
        data[i] = (time_data >> (i * 8)) & 0xFF;
    }

    return siphash_stream(data, sizeof(data), key0, key1);
}

uint64_t siphash_generate_fingerprint(const uint64_t id, siphash_keyfile_t *keyfile) {
    if (keyfile == NULL)
        return 0;

    // 验证魔术字和版本号
    if (keyfile->magic != 0x53495048 || keyfile->version != 0x0100) {
        return 0;
    }

    // 创建包含所有相关数据的缓冲区
    const size_t total_size = sizeof(uint64_t) + // id
                       sizeof(siphash_keyfile_t) - offsetof(siphash_keyfile_t, timestamp); // keyfile除magic/version外的部分

    uint8_t *composite_data = malloc(total_size);
    if (composite_data == NULL) {
        return 0;
    }

    size_t offset = 0;

    // 添加ID
    for (int i = 0; i < sizeof(uint64_t); i++)
        composite_data[offset++] = id >> (i * 8) & 0xFF;

    // 添加keyfile的剩余部分（从timestamp开始）
    const uint8_t *keyfile_data = (uint8_t*)&keyfile->timestamp;
    const size_t keyfile_data_size = sizeof(siphash_keyfile_t) - offsetof(siphash_keyfile_t, timestamp);
    memcpy(composite_data + offset, keyfile_data, keyfile_data_size);
    offset += keyfile_data_size;

    const uint64_t fingerprint = siphash_stream(composite_data, offset, keyfile->key0, keyfile->key1);
    free(composite_data);

    return fingerprint;
}