/*******************************************************************************
 * tefkernel - tefpkg_internal
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
 * Created: 2026/11/6
 *******************************************************************************/

#pragma once

#include "result.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace tef::pkg {
/* ---- 磁盘布局：与旧 C 版二进制完全一致 ---- */

struct __attribute__((packed)) header {
    uint32_t magic;
    uint16_t version;
    uint16_t file_count;
    uint16_t reserved_entries;
    uint16_t reserved;
    uint32_t data_offset;
    uint32_t data_size;
    uint64_t timestamp;
    uint64_t checksum;
    uint64_t content_hash;
    uint64_t signature;
};

struct __attribute__((packed)) entry {
    uint32_t index;
    uint32_t data_offset;
    uint32_t compressed_size;
    uint32_t original_size;
    uint64_t checksum;
    uint64_t timestamp;
    uint8_t compress_type;
    uint8_t compress_level;
};

static_assert(sizeof(header) == 52, "Header layout must match old C version");
static_assert(sizeof(entry) == 34, "Entry layout must match old C version");

/* ---- 运行时结构 ---- */

using bytes = std::vector<uint8_t>;

/* 内存数据模式借用外部 buffer，不拥有 */
struct borrowed_data {
    const uint8_t* data = nullptr;
    size_t size = 0;
};

/* 后端变体：类型安全，不会误读 */
using backend = std::variant<
    std::monostate,           /* Closed */
    bytes,                    /* Memory */
    std::unique_ptr<std::fstream>,  /* ReadWrite */
    std::string,              /* ReadOnly：存文件名 */
    borrowed_data              /* MemData：借用外部 buffer */
>;

struct package {
    header header{};
    std::vector<std::optional<entry>> entries;
    backend backend;
};

    inline tefpkg_access_mode_t mode_of(const package& p) {
        switch (p.backend.index()) {
            case 0: return tef_access_closed;
            case 1: return tef_access_memory;
            case 2: return tef_access_readwrite;
            case 3: return tef_access_readonly;
            case 4: return tef_access_memdata;
            default: return tef_access_closed;
        }
    }

/* ---- 核心算法（独立于 C ABI，可单测） ---- */

result<bytes> read_compressed_data(const package& pkg, const entry& entry);
result<bytes> extract_entry(const package& pkg, const entry& entry);

status append_entry(package& pkg, bytes compressed, entry meta);
status save_to_file(package& pkg, const char* filename, uint64_t fingerprint);
status flush_to_existing(package& pkg, uint64_t fingerprint);

status sign_package(package& pkg, uint64_t fingerprint);
status verify_package(const package& pkg);
status verify_entry(const package& pkg, const entry& entry);
status verify_signature(const package& pkg, uint64_t fingerprint);

header  fresh_header(uint16_t reserved_entries);
entry   fresh_entry(uint32_t index, uint32_t compressed_size, uint32_t original_size,
                   tefpkg_compress_t type, uint8_t level);
uint64_t now_timestamp();

result<bytes> compress_bytes(const uint8_t* src, size_t size,
                            tefpkg_compress_t type, uint8_t level);
result<bytes> decompress_bytes(const uint8_t* src, size_t size,
                              size_t original_size, tefpkg_compress_t type);

/* ---- 工厂（导出层用） ---- */

namespace detail {
    result<package*> create_file(const char* filename, uint16_t reserved);
    result<package*> create_memory(uint16_t reserved);
    result<package*> open_read_only(const char* filename);
    result<package*> open_from_memory(const uint8_t* data, uint32_t size);
    void             destroy(const package* p);
}

}