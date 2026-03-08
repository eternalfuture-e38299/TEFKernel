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

#include "tefpackage/tefpkg.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "compressor.h"
#include "siphash.h"

// 统一绝对偏移计算宏
#define GET_ABSOLUTE_OFFSET(pkg, relative_offset) ((pkg)->header.data_offset + (relative_offset))

// 内部工具函数声明
static uint64_t get_current_timestamp();
static uint64_t calculate_header_checksum(const tefpkg_header_t *header);
static uint64_t calculate_file_checksum(const tefpkg_entry_t *entry, const uint8_t *file_data);
static uint64_t calculate_content_hash_checksum(const tefpkg_t* pkg);
static uint64_t calculate_package_signature(const tefpkg_t *pkg, uint64_t fingerprint);
static tefpkg_result_t validate_header(const tefpkg_header_t* header);

static tefpkg_result_t write_header_to_file(FILE* file, const tefpkg_header_t* header);
static tefpkg_result_t read_header_from_file(FILE* file, tefpkg_header_t* header);
static tefpkg_result_t write_file_entry_to_file(FILE* file, const tefpkg_entry_t* entry);
static tefpkg_result_t read_file_entry_from_file(FILE* file, tefpkg_entry_t* entry);
static tefpkg_result_t write_data_to_file(FILE* file, const uint8_t* content, size_t content_size);
static tefpkg_result_t read_data_from_file(FILE* file, uint8_t* buffer, size_t size);

static tefpkg_result_t add_file_to_memory_pkg(tefpkg_t *pkg, tefpkg_entry_t* entry,
                                             uint8_t* compressed_data, uint32_t compressed_size);
static tefpkg_result_t add_file_to_file_pkg(const tefpkg_t *pkg, tefpkg_entry_t* entry,
                                           uint8_t* compressed_data, uint32_t compressed_size);
static tefpkg_result_t read_compressed_data(const tefpkg_t *pkg, const tefpkg_entry_t* entry,
                                           uint8_t** compressed_data, uint32_t* compressed_size);

// 时间戳函数
static uint64_t get_current_timestamp() {
    return (uint64_t)time(NULL);
}

// 校验和计算函数
static uint64_t calculate_header_checksum(const tefpkg_header_t *header) {
    tefpkg_header_t temp_header = *header;
    temp_header.checksum = 0;
    temp_header.signature = 0;
    temp_header.content_hash = 0;
    return siphash_stream((uint8_t*)&temp_header, sizeof(tefpkg_header_t) - 3*sizeof(uint64_t),
                         0x0318030920211212, 0x49204C6F76652059);
}

static uint64_t calculate_file_checksum(const tefpkg_entry_t *entry, const uint8_t *file_data) {
    if (!entry || !file_data) return 0;

    siphash_ctx_t state;
    siphash_init(&state, 0x6F75000000000000, 0xE5B08FE9B9AC);

    siphash_update(&state, file_data, entry->compressed_size);

    tefpkg_entry_t temp_entry = *entry;
    temp_entry.checksum = 0;
    temp_entry.data_offset = 0;
    siphash_update(&state, (const uint8_t*)&temp_entry, sizeof(tefpkg_entry_t));

    return siphash_final(&state);
}

static uint64_t calculate_content_hash_checksum(const tefpkg_t* pkg) {
    siphash_ctx_t state;
    siphash_init(&state, 0x6F75000000000000, 0xE5B08FE9B9AC);

    tefpkg_header_t temp_header = pkg->header;
    temp_header.content_hash = 0;
    temp_header.signature = 0;
    siphash_update(&state, (uint8_t*)&temp_header, sizeof(tefpkg_header_t) - 2*sizeof(uint64_t));

    for (uint16_t i = 0; i < pkg->header.reserved_entries; i++) {
        if (pkg->entries[i] != NULL) {
            siphash_update(&state, (uint8_t*)pkg->entries[i], sizeof(tefpkg_entry_t));
        } else {
            // 对于空条目，使用全零
            tefpkg_entry_t zero_entry = {0};
            siphash_update(&state, (uint8_t*)&zero_entry, sizeof(tefpkg_entry_t));
        }
    }

    return siphash_final(&state);
}

static uint64_t calculate_package_signature(const tefpkg_t *pkg, uint64_t fingerprint) {
    if (fingerprint == 0) return 0;

    siphash_ctx_t state;

    // 生成密钥
    const uint64_t key0 = siphash_stream((uint8_t *) &fingerprint, sizeof(uint64_t), 0x0318030920211212, 0x49204C6F76652059);
    const uint64_t key1 = siphash_stream((uint8_t *) &fingerprint, sizeof(uint64_t), 0x6F75000000000000, 0xE5B08FE9B9AC);

    siphash_init(&state, key0, key1);

    tefpkg_header_t temp_header = pkg->header;
    temp_header.signature = 0;
    siphash_update(&state, (uint8_t*)&temp_header, sizeof(tefpkg_header_t) - sizeof(uint64_t));

    for (uint16_t i = 0; i < pkg->header.reserved_entries; i++) {
        if (pkg->entries[i] != NULL) {
            siphash_update(&state, (uint8_t*)pkg->entries[i], sizeof(tefpkg_entry_t));
        } else {
            tefpkg_entry_t zero_entry = {0};
            siphash_update(&state, (uint8_t*)&zero_entry, sizeof(tefpkg_entry_t));
        }
    }

    return siphash_final(&state);
}

// 头部验证
static tefpkg_result_t validate_header(const tefpkg_header_t* header) {
    if (header->magic != TEFPKG_MAGIC) return TEF_ERROR_SIGNATURE;
    if (header->version != TEFPKG_VERSION) return TEF_ERROR_SIGNATURE;
    if (header->reserved_entries == 0 || header->reserved_entries > TEFPKG_MAX_FILES) {
        return TEF_ERROR_CORRUPT;
    }
    if (header->file_count > header->reserved_entries) {
        return TEF_ERROR_CORRUPT;
    }
    return TEF_OK;
}

// 文件IO函数
static tefpkg_result_t write_header_to_file(FILE* file, const tefpkg_header_t* header) {
    if (!file || !header) return TEF_ERROR;
    fseek(file, 0, SEEK_SET);
    return fwrite(header, sizeof(tefpkg_header_t), 1, file) == 1 ? TEF_OK : TEF_ERROR_IO;
}

static tefpkg_result_t read_header_from_file(FILE* file, tefpkg_header_t* header) {
    if (!file || !header) return TEF_ERROR;
    fseek(file, 0, SEEK_SET);
    return fread(header, sizeof(tefpkg_header_t), 1, file) == 1 ? TEF_OK : TEF_ERROR_IO;
}

static tefpkg_result_t write_file_entry_to_file(FILE* file, const tefpkg_entry_t* entry) {
    if (!file || !entry) return TEF_ERROR;
    return fwrite(entry, sizeof(tefpkg_entry_t), 1, file) == 1 ? TEF_OK : TEF_ERROR_IO;
}

static tefpkg_result_t read_file_entry_from_file(FILE* file, tefpkg_entry_t* entry) {
    if (!file || !entry) return TEF_ERROR;
    return fread(entry, sizeof(tefpkg_entry_t), 1, file) == 1 ? TEF_OK : TEF_ERROR_IO;
}

static tefpkg_result_t write_data_to_file(FILE* file, const uint8_t* content, const size_t content_size) {
    if (!file || !content) return TEF_ERROR;
    if (content_size == 0) return TEF_OK;

    size_t total_written = 0;
    while (total_written < content_size) {
        const size_t chunk_size = (content_size - total_written) < 65536 ?
                           (content_size - total_written) : 65536;
        const size_t written = fwrite(content + total_written, 1, chunk_size, file);
        if (written != chunk_size) return TEF_ERROR_IO;
        total_written += written;
    }
    return TEF_OK;
}

static tefpkg_result_t read_data_from_file(FILE* file, uint8_t* buffer, const size_t size) {
    if (!file || !buffer) return TEF_ERROR;
    if (size == 0) return TEF_OK;

    size_t total_read = 0;
    while (total_read < size) {
        const size_t chunk_size = size - total_read < 65536 ? size - total_read : 65536;
        const size_t read = fread(buffer + total_read, 1, chunk_size, file);
        if (read != chunk_size) return TEF_ERROR_IO;
        total_read += read;
    }
    return TEF_OK;
}

// 内存模式添加文件
static tefpkg_result_t add_file_to_memory_pkg(tefpkg_t *pkg, tefpkg_entry_t* entry,
                                             uint8_t* compressed_data, const uint32_t compressed_size) {
    uint8_t* new_data = realloc(pkg->data, pkg->header.data_size + compressed_size);
    if (!new_data) return TEF_ERROR_MEMORY;

    pkg->data = new_data;
    memcpy(pkg->data + pkg->header.data_size, compressed_data, compressed_size);

    // 设置相对偏移（从数据区开始）
    entry->data_offset = pkg->header.data_size;
    pkg->header.data_size += compressed_size;
    free(compressed_data);
    return TEF_OK;
}

// 文件模式添加文件
static tefpkg_result_t add_file_to_file_pkg(const tefpkg_t *pkg, tefpkg_entry_t* entry,
                                           uint8_t* compressed_data, const uint32_t compressed_size) {
    if (!pkg->file_handle) return TEF_ERROR_IO;

    // 计算已存在数据的总大小（用于相对偏移）
    uint32_t accumulated_size = 0;
    for (uint16_t i = 0; i < pkg->header.file_count; i++) {
        if (pkg->entries[i] != NULL) {
            accumulated_size += pkg->entries[i]->compressed_size;
        }
    }

    // 设置条目在数据区内的相对偏移
    entry->data_offset = accumulated_size;

    // 计算文件中的绝对写入位置
    const uint32_t absolute_offset = GET_ABSOLUTE_OFFSET(pkg, entry->data_offset);

    if (fseek(pkg->file_handle, absolute_offset, SEEK_SET) != 0) {
        return TEF_ERROR_IO;
    }

    const size_t written = fwrite(compressed_data, 1, compressed_size, pkg->file_handle);
    free(compressed_data);

    if (written != compressed_size) {
        return TEF_ERROR_IO;
    }

    return TEF_OK;
}

// 统一数据读取函数
static tefpkg_result_t read_compressed_data(const tefpkg_t *pkg, const tefpkg_entry_t* entry,
                                           uint8_t** compressed_data, uint32_t* compressed_size) {
    *compressed_size = entry->compressed_size;
    *compressed_data = malloc(*compressed_size);
    if (!*compressed_data) return TEF_ERROR_MEMORY;

    const uint32_t absolute_offset = GET_ABSOLUTE_OFFSET(pkg, entry->data_offset);

    switch (pkg->access_mode) {
        case TEF_ACCESS_MEMORY: // 内存模式
            if (absolute_offset + *compressed_size > pkg->header.data_size + pkg->header.data_offset) {
                free(*compressed_data);
                *compressed_data = NULL;
                return TEF_ERROR_CORRUPT;
            }
            memcpy(*compressed_data, pkg->data + entry->data_offset, *compressed_size);
            return TEF_OK;

        case TEF_ACCESS_READWRITE: // 读写模式
        case TEF_ACCESS_FILE: // 文件模式
            if (!pkg->file_handle) {
                free(*compressed_data);
                *compressed_data = NULL;
                return TEF_ERROR_IO;
            }

            if (fseek(pkg->file_handle, absolute_offset, SEEK_SET) != 0) {
                free(*compressed_data);
                *compressed_data = NULL;
                return TEF_ERROR_IO;
            }

            return read_data_from_file(pkg->file_handle, *compressed_data, *compressed_size);

        case TEF_ACCESS_READONLY:
        {
            FILE* file = fopen(pkg->filename, "rb");
            if (!file) {
                free(*compressed_data);
                *compressed_data = NULL;
                return TEF_ERROR_IO;
            }

            fseek(file, 0, SEEK_END);
            const long file_size = ftell(file);
            if (absolute_offset + *compressed_size > (uint32_t)file_size) {
                fclose(file);
                free(*compressed_data);
                *compressed_data = NULL;
                return TEF_ERROR_CORRUPT;
            }

            if (fseek(file, absolute_offset, SEEK_SET) != 0) {
                fclose(file);
                free(*compressed_data);
                *compressed_data = NULL;
                return TEF_ERROR_IO;
            }

            tefpkg_result_t result = read_data_from_file(file, *compressed_data, *compressed_size);
            fclose(file);

            if (result != TEF_OK) {
                free(*compressed_data);
                *compressed_data = NULL;
            }
            return result;
        }

        case TEF_ACCESS_MEMDATA: // 内存数据模式
            if (absolute_offset + *compressed_size > pkg->mem_data_size) {
                free(*compressed_data);
                *compressed_data = NULL;
                return TEF_ERROR_CORRUPT;
            }
            memcpy(*compressed_data, pkg->mem_data + absolute_offset, *compressed_size);
            return TEF_OK;

        default:
            free(*compressed_data);
            *compressed_data = NULL;
            return TEF_ERROR;
    }
}

// 公共API实现
tefpkg_result_t tefpkg_create_reserved_from_file(const char *filename, uint16_t reserved_entries, tefpkg_t **pkg) {
    if (!filename || !pkg || reserved_entries == 0 || reserved_entries > TEFPKG_MAX_FILES) {
        return TEF_ERROR_INVALID;
    }

    tefpkg_t* new_pkg = calloc(1, sizeof(tefpkg_t));
    if (!new_pkg) return TEF_ERROR_MEMORY;

    // 初始化头部
    new_pkg->header.magic = TEFPKG_MAGIC;
    new_pkg->header.version = TEFPKG_VERSION;
    new_pkg->header.file_count = 0;
    new_pkg->header.reserved_entries = reserved_entries;
    new_pkg->header._reserved = 0;
    new_pkg->header.timestamp = get_current_timestamp();
    new_pkg->header.data_size = 0;
    new_pkg->header.data_offset = sizeof(tefpkg_header_t) + (sizeof(tefpkg_entry_t) * reserved_entries);

    // 创建文件，使用读写模式
    FILE* file = fopen(filename, "wb+");
    if (!file) {
        free(new_pkg);
        return TEF_ERROR_IO;
    }

    // 写入初始头部
    tefpkg_result_t result = write_header_to_file(file, &new_pkg->header);
    if (result != TEF_OK) {
        fclose(file);
        free(new_pkg);
        return result;
    }

    // 预分配并写入空白的条目表（零填充）
    const tefpkg_entry_t blank_entry = {0};
    for (uint16_t i = 0; i < reserved_entries; i++) {
        result = write_file_entry_to_file(file, &blank_entry);
        if (result != TEF_OK) {
            fclose(file);
            free(new_pkg);
            return result;
        }
    }

    // 为内存中的条目指针数组分配空间
    new_pkg->entries = calloc(reserved_entries, sizeof(tefpkg_entry_t*));
    if (!new_pkg->entries) {
        fclose(file);
        free(new_pkg);
        return TEF_ERROR_MEMORY;
    }

    new_pkg->access_mode = TEF_ACCESS_READWRITE;
    new_pkg->file_handle = file;
    *pkg = new_pkg;
    return TEF_OK;
}

tefpkg_result_t tefpkg_create_reserved_from_memory(uint16_t reserved_entries, tefpkg_t **pkg) {
    if (!pkg || reserved_entries == 0 || reserved_entries > TEFPKG_MAX_FILES) {
        return TEF_ERROR_INVALID;
    }

    tefpkg_t* new_pkg = calloc(1, sizeof(tefpkg_t));
    if (!new_pkg) return TEF_ERROR_MEMORY;

    new_pkg->header.magic = TEFPKG_MAGIC;
    new_pkg->header.version = TEFPKG_VERSION;
    new_pkg->header.file_count = 0;
    new_pkg->header.reserved_entries = reserved_entries;
    new_pkg->header._reserved = 0;
    new_pkg->header.timestamp = get_current_timestamp();
    new_pkg->header.data_size = 0;
    new_pkg->header.data_offset = sizeof(tefpkg_header_t) + (sizeof(tefpkg_entry_t) * reserved_entries);
    new_pkg->access_mode = TEF_ACCESS_MEMORY;
    new_pkg->data = NULL;

    // 为内存中的条目指针数组分配空间
    new_pkg->entries = calloc(reserved_entries, sizeof(tefpkg_entry_t*));
    if (!new_pkg->entries) {
        free(new_pkg);
        return TEF_ERROR_MEMORY;
    }

    *pkg = new_pkg;
    return TEF_OK;
}

tefpkg_result_t tefpkg_open_readonly(const char *filename, tefpkg_t **pkg) {
    if (!filename || !pkg) return TEF_ERROR;

    FILE* file = fopen(filename, "rb");
    if (!file) return TEF_ERROR_IO;

    tefpkg_t* new_pkg = calloc(1, sizeof(tefpkg_t));
    if (!new_pkg) {
        fclose(file);
        return TEF_ERROR_MEMORY;
    }

    // 读取头部
    tefpkg_result_t result = read_header_from_file(file, &new_pkg->header);
    if (result != TEF_OK) {
        fclose(file);
        free(new_pkg);
        return result;
    }

    // 验证头部
    result = validate_header(&new_pkg->header);
    if (result != TEF_OK) {
        fclose(file);
        free(new_pkg);
        return result;
    }

    // 读取文件条目
    if (new_pkg->header.reserved_entries > 0) {
        new_pkg->entries = calloc(new_pkg->header.reserved_entries, sizeof(tefpkg_entry_t*));
        if (!new_pkg->entries) {
            fclose(file);
            free(new_pkg);
            return TEF_ERROR_MEMORY;
        }

        fseek(file, sizeof(tefpkg_header_t), SEEK_SET);
        for (uint16_t i = 0; i < new_pkg->header.reserved_entries; i++) {
            new_pkg->entries[i] = malloc(sizeof(tefpkg_entry_t));
            if (!new_pkg->entries[i]) {
                for (uint16_t j = 0; j < i; j++) free(new_pkg->entries[j]);
                free(new_pkg->entries);
                fclose(file);
                free(new_pkg);
                return TEF_ERROR_MEMORY;
            }
            result = read_file_entry_from_file(file, new_pkg->entries[i]);
            if (result != TEF_OK) {
                for (uint16_t j = 0; j <= i; j++) free(new_pkg->entries[j]);
                free(new_pkg->entries);
                fclose(file);
                free(new_pkg);
                return result;
            }
        }
    }

    new_pkg->access_mode = TEF_ACCESS_READONLY;
    new_pkg->filename = strdup(filename);
    fclose(file);
    *pkg = new_pkg;
    return TEF_OK;
}

tefpkg_result_t tefpkg_open_from_memory(const uint8_t* data, uint32_t data_size, tefpkg_t** pkg) {
    if (!data || data_size < sizeof(tefpkg_header_t) || !pkg) return TEF_ERROR;

    tefpkg_t* new_pkg = calloc(1, sizeof(tefpkg_t));
    if (!new_pkg) return TEF_ERROR_MEMORY;

    // 解析头部
    memcpy(&new_pkg->header, data, sizeof(tefpkg_header_t));
    tefpkg_result_t result = validate_header(&new_pkg->header);
    if (result != TEF_OK) {
        free(new_pkg);
        return result;
    }

    // 解析文件条目
    if (new_pkg->header.reserved_entries > 0) {
        uint32_t entries_size = new_pkg->header.reserved_entries * sizeof(tefpkg_entry_t);
        if (sizeof(tefpkg_header_t) + entries_size > data_size) {
            free(new_pkg);
            return TEF_ERROR_CORRUPT;
        }

        new_pkg->entries = calloc(new_pkg->header.reserved_entries, sizeof(tefpkg_entry_t*));
        if (!new_pkg->entries) {
            free(new_pkg);
            return TEF_ERROR_MEMORY;
        }

        for (uint16_t i = 0; i < new_pkg->header.reserved_entries; i++) {
            new_pkg->entries[i] = malloc(sizeof(tefpkg_entry_t));
            if (!new_pkg->entries[i]) {
                for (uint16_t j = 0; j < i; j++) free(new_pkg->entries[j]);
                free(new_pkg->entries);
                free(new_pkg);
                return TEF_ERROR_MEMORY;
            }
            memcpy(new_pkg->entries[i], data + sizeof(tefpkg_header_t) + i * sizeof(tefpkg_entry_t),
                   sizeof(tefpkg_entry_t));
        }
    }

    new_pkg->access_mode = TEF_ACCESS_MEMDATA;
    new_pkg->mem_data = data;
    new_pkg->mem_data_size = data_size;
    *pkg = new_pkg;
    return TEF_OK;
}

tefpkg_result_t tefpkg_save_file(tefpkg_t *pkg, const uint64_t fingerprint) {
    if (!pkg || (pkg->access_mode != TEF_ACCESS_READWRITE && pkg->access_mode != TEF_ACCESS_MEMORY)) {
        return TEF_ERROR;
    }

    if (pkg->access_mode == TEF_ACCESS_READWRITE && !pkg->file_handle) {
        return TEF_ERROR_IO;
    }

    // 重新计算数据大小
    pkg->header.data_size = 0;
    for (uint16_t i = 0; i < pkg->header.reserved_entries; i++) {
        if (pkg->entries[i] != NULL) {
            pkg->header.data_size += pkg->entries[i]->compressed_size;
        }
    }

    // 计算签名
    tefpkg_sign_package(pkg, fingerprint);

    // 读写模式需要更新文件
    if (pkg->access_mode == TEF_ACCESS_READWRITE) {
        // 更新头部
        fseek(pkg->file_handle, 0, SEEK_SET);
        tefpkg_result_t result = write_header_to_file(pkg->file_handle, &pkg->header);
        if (result != TEF_OK) return result;

        // 更新所有条目（包括空条目）
        fseek(pkg->file_handle, sizeof(tefpkg_header_t), SEEK_SET);
        for (uint16_t i = 0; i < pkg->header.reserved_entries; i++) {
            if (pkg->entries[i] != NULL) {
                result = write_file_entry_to_file(pkg->file_handle, pkg->entries[i]);
            } else {
                // 写入空条目
                tefpkg_entry_t blank_entry = {0};
                result = write_file_entry_to_file(pkg->file_handle, &blank_entry);
            }
            if (result != TEF_OK) return result;
        }

        fflush(pkg->file_handle);
    }

    return TEF_OK;
}

tefpkg_result_t tefpkg_save_memory_file(const char *filename, tefpkg_t *pkg, const uint64_t fingerprint) {
    if (!pkg || !filename || pkg->access_mode != TEF_ACCESS_MEMORY) {
        return TEF_ERROR;
    }

    // 先更新包信息
    tefpkg_result_t result = tefpkg_save_file(pkg, fingerprint);
    if (result != TEF_OK) return result;

    // 写入文件
    FILE* file = fopen(filename, "wb");
    if (!file) return TEF_ERROR_IO;

    // 写入头部
    result = write_header_to_file(file, &pkg->header);
    if (result != TEF_OK) {
        fclose(file);
        return result;
    }

    // 写入条目
    for (uint16_t i = 0; i < pkg->header.reserved_entries; i++) {
        if (pkg->entries[i] != NULL) {
            result = write_file_entry_to_file(file, pkg->entries[i]);
        } else {
            tefpkg_entry_t blank_entry = {0};
            result = write_file_entry_to_file(file, &blank_entry);
        }
        if (result != TEF_OK) {
            fclose(file);
            return result;
        }
    }

    // 写入数据
    if (pkg->header.data_size > 0) {
        result = write_data_to_file(file, pkg->data, pkg->header.data_size);
        if (result != TEF_OK) {
            fclose(file);
            return result;
        }
    }

    fclose(file);
    return TEF_OK;
}

void tefpkg_close(tefpkg_t *pkg) {
    if (!pkg) return;

    // 释放条目数组
    if (pkg->entries) {
        for (uint16_t i = 0; i < pkg->header.reserved_entries; i++) {
            if (pkg->entries[i]) free(pkg->entries[i]);
        }
        free(pkg->entries);
    }

    // 根据访问模式释放资源
    switch (pkg->access_mode) {
        case TEF_ACCESS_MEMORY:
            if (pkg->data) free(pkg->data);
            break;
        case TEF_ACCESS_READWRITE:
            if (pkg->file_handle) fclose(pkg->file_handle);
            break;
        case TEF_ACCESS_READONLY:
            if (pkg->filename) free((void*)pkg->filename);
            break;
        default: break;
    }

    free(pkg);
}

tefpkg_result_t tefpkg_add_entry_from_memory(tefpkg_t *pkg, tefpkg_compress_t compress_type,
                                           uint8_t compress_level, uint8_t *data, uint32_t data_size) {
    if (!pkg || !data || data_size == 0) {
        return TEF_ERROR;
    }

    if (compress_type > COMPRESS_LZ4HC)
        return TEF_ERROR;

    // 检查预留空间
    if (pkg->header.file_count >= pkg->header.reserved_entries) {
        return TEF_ERROR_NO_SPACE;
    }

    // 压缩数据
    uint8_t* compressed_data = NULL;
    uint32_t compressed_size = 0;
    const tefpkg_result_t compress_result = tefpkg_compress_memory(
        data, data_size, &compressed_data, &compressed_size, compress_type, compress_level);

    if (compress_result != TEF_OK || !compressed_data || compressed_size == 0) {
        if (compressed_data) free(compressed_data);
        return compress_result != TEF_OK ? compress_result : TEF_ERROR;
    }

    // 创建新条目
    tefpkg_entry_t* new_entry = malloc(sizeof(tefpkg_entry_t));
    if (!new_entry) {
        free(compressed_data);
        return TEF_ERROR_MEMORY;
    }

    new_entry->index = pkg->header.file_count;
    new_entry->compressed_size = compressed_size;
    new_entry->original_size = data_size;
    new_entry->timestamp = (uint64_t)time(NULL);
    new_entry->compress_type = (uint8_t)compress_type;
    new_entry->compress_level = compress_level;

    // 计算文件校验和
    new_entry->checksum = calculate_file_checksum(new_entry, compressed_data);

    // 根据访问模式处理
    tefpkg_result_t result = TEF_OK;
    if (pkg->access_mode == TEF_ACCESS_MEMORY) { // 内存模式
        result = add_file_to_memory_pkg(pkg, new_entry, compressed_data, compressed_size);
    } else if (pkg->access_mode == TEF_ACCESS_READWRITE) { // 读写模式
        result = add_file_to_file_pkg(pkg, new_entry, compressed_data, compressed_size);
    } else {
        result = TEF_ERROR;
    }

    if (result != TEF_OK) {
        free(new_entry);
        free(compressed_data);
        return result;
    }

    // 存储条目指针
    pkg->entries[pkg->header.file_count] = new_entry;
    pkg->header.file_count++;
    pkg->header.timestamp = (uint64_t)time(NULL);

    return TEF_OK;
}

tefpkg_result_t tefpkg_add_entry_from_file(tefpkg_t *pkg, const char *filepath,
                                          const tefpkg_compress_t compress_type, const uint8_t compress_level) {
    if (!pkg || !filepath) {
        return TEF_ERROR;
    }

    // 读取文件内容
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        return TEF_ERROR_IO;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    const long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(file);
        return TEF_ERROR;
    }

    // 读取文件数据
    uint8_t* file_data = malloc(file_size);
    if (!file_data) {
        fclose(file);
        return TEF_ERROR_MEMORY;
    }

    if (fread(file_data, 1, file_size, file) != (size_t)file_size) {
        free(file_data);
        fclose(file);
        return TEF_ERROR_IO;
    }
    fclose(file);

    // 调用内存添加函数
    const tefpkg_result_t result = tefpkg_add_entry_from_memory(pkg, compress_type, compress_level,
                                                         file_data, file_size);
    free(file_data);
    return result;
}

tefpkg_result_t tefpkg_extract_entry_to_memory(const tefpkg_t *pkg, const uint32_t entry_index,
                                              uint8_t **data, uint32_t *data_size) {
    if (!pkg || !data || !data_size || entry_index >= pkg->header.reserved_entries) {
        return TEF_ERROR_NOT_FOUND;
    }

    const tefpkg_entry_t* entry = pkg->entries[entry_index];
    if (!entry) {
        return TEF_ERROR_NOT_FOUND;
    }

    *data = malloc(entry->original_size);
    if (!*data) {
        return TEF_ERROR_MEMORY;
    }

    uint8_t* compressed_data = NULL;
    uint32_t compressed_size = 0;
    tefpkg_result_t result = TEF_OK;

    // 根据访问模式读取压缩数据
    if (pkg->access_mode == TEF_ACCESS_MEMORY) { // 内存模式
        if (entry->data_offset + entry->compressed_size > pkg->header.data_size) {
            free(*data);
            return TEF_ERROR_CORRUPT;
        }
        compressed_data = pkg->data + entry->data_offset;
        compressed_size = entry->compressed_size;
    } else if (pkg->access_mode == TEF_ACCESS_READONLY) { // 只读模式
        result = read_compressed_data(pkg, entry, &compressed_data, &compressed_size);
        if (result != TEF_OK) {
            free(*data);
            return result;
        }
    } else if (pkg->access_mode == TEF_ACCESS_READWRITE) { // 读写模式
        result = read_compressed_data(pkg, entry, &compressed_data, &compressed_size);
        if (result != TEF_OK) {
            free(*data);
            return result;
        }
    } else if (pkg->access_mode == TEF_ACCESS_MEMDATA) { // 内存数据模式
        result = read_compressed_data(pkg, entry, &compressed_data, &compressed_size);
        if (result != TEF_OK) {
            free(*data);
            return result;
        }
    } else {
        free(*data);
        return TEF_ERROR;
    }

    // 解压数据
    const int decompressed_size = tefpkg_decompress_data(compressed_data, compressed_size, *data,
                                                  entry->original_size,
                                                  (tefpkg_compress_t)entry->compress_type);

    if (pkg->access_mode == TEF_ACCESS_READONLY || pkg->access_mode == TEF_ACCESS_READWRITE ||
        pkg->access_mode == TEF_ACCESS_MEMDATA) {
        free(compressed_data);
    }

    if (decompressed_size != (int)entry->original_size) {
        free(*data);
        *data = NULL;
        return TEF_ERROR_CORRUPT;
    }

    *data_size = entry->original_size;
    return TEF_OK;
}

tefpkg_result_t tefpkg_extract_entry_to_file(const tefpkg_t *pkg, const uint32_t entry_index,
                                            const char *output_path) {
    if (!pkg || !output_path || entry_index >= pkg->header.reserved_entries) {
        return TEF_ERROR_NOT_FOUND;
    }

    const tefpkg_entry_t* entry = pkg->entries[entry_index];
    if (!entry) {
        return TEF_ERROR_NOT_FOUND;
    }

    // 提取到内存
    uint8_t* data = NULL;
    uint32_t data_size = 0;
    const tefpkg_result_t result = tefpkg_extract_entry_to_memory(pkg, entry_index, &data, &data_size);
    if (result != TEF_OK) {
        return result;
    }

    // 写入文件
    FILE* file = fopen(output_path, "wb");
    if (!file) {
        free(data);
        return TEF_ERROR_IO;
    }

    const size_t written = fwrite(data, 1, data_size, file);
    fclose(file);
    free(data);

    if (written != data_size) {
        return TEF_ERROR_IO;
    }

    return TEF_OK;
}

tefpkg_result_t tefpkg_get_entry_info(const tefpkg_t *pkg, const uint32_t file_index, tefpkg_entry_t **info) {
    if (!pkg || !info || file_index >= pkg->header.reserved_entries) {
        return TEF_ERROR_NOT_FOUND;
    }

    if (pkg->entries[file_index] == NULL) {
        return TEF_ERROR_NOT_FOUND;
    }

    *info = pkg->entries[file_index];
    return TEF_OK;
}

// 验证函数
tefpkg_result_t tefpkg_verify_entry(const tefpkg_t *pkg, const uint32_t entry_index) {
    if (!pkg || entry_index >= pkg->header.reserved_entries) return TEF_ERROR_NOT_FOUND;

    const tefpkg_entry_t* entry = pkg->entries[entry_index];
    if (!entry) return TEF_ERROR_NOT_FOUND;

    uint8_t* compressed_data = NULL;
    uint32_t compressed_size = 0;

    const tefpkg_result_t result = read_compressed_data(pkg, entry, &compressed_data, &compressed_size);

    if (result != TEF_OK) return result;

    const uint64_t calculated_checksum = calculate_file_checksum(entry, compressed_data);
    free(compressed_data);

    if (calculated_checksum != entry->checksum) return TEF_ERROR_SIGNATURE;
    return TEF_OK;
}

tefpkg_result_t tefpkg_verify_pkg(const tefpkg_t *pkg) {
    if (calculate_header_checksum(&pkg->header) != pkg->header.checksum)
        return TEF_ERROR_INTEGRITY;

    if (calculate_content_hash_checksum(pkg) != pkg->header.content_hash)
        return TEF_ERROR_INTEGRITY;

    return TEF_OK;
}

tefpkg_result_t tefpkg_verify_signature(const tefpkg_t *pkg, const uint64_t fingerprint) {
    if (!pkg) return TEF_ERROR;

    // 如果密钥为0，不进行验证
    if (fingerprint == 0)
        return TEF_ERROR_NOT_SIGNATURE;

    // 验证头部校验和
    if (pkg->header.checksum != calculate_header_checksum(&pkg->header))
        return TEF_ERROR_SIGNATURE;

    // 验证包签名
    if (pkg->header.signature != calculate_package_signature(pkg, fingerprint))
        return TEF_ERROR_SIGNATURE;

    return TEF_OK;
}

tefpkg_result_t tefpkg_sign_package(tefpkg_t *pkg, const uint64_t fingerprint) {
    if (!pkg) return TEF_ERROR;

    // 计算头部校验和
    pkg->header.checksum = calculate_header_checksum(&pkg->header);

    // 计算内容完整性校验和
    pkg->header.content_hash = calculate_content_hash_checksum(pkg);

    // 计算包签名
    if (fingerprint != 0)
        pkg->header.signature = calculate_package_signature(pkg, fingerprint);

    return TEF_OK;
}

uint16_t tefpkg_get_entries_count(const tefpkg_t *pkg) {
    if (!pkg) return 0;
    return pkg->header.file_count;
}

uint16_t tefpkg_get_reserved_entries(const tefpkg_t *pkg) {
    if (!pkg) return 0;
    return pkg->header.reserved_entries;
}