/*******************************************************************************
 * tefkernel - tefpkg
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

#include "tefpkg_internal.hpp"

#include <cstring>
#include <ctime>

#include "../../includes/tef/core/result.hpp"

extern "C" {
#include "compressor.h"
#include "siphash.h"
}

namespace tef::pkg {
    /* ============================================================================
     * 基础
     * ============================================================================ */

    uint64_t now_timestamp() {
        return static_cast<uint64_t>(std::time(nullptr));
    }

    header fresh_header(const uint16_t reserved) {
        header h{};
        h.magic = TEFPKG_MAGIC;
        h.version = TEFPKG_VERSION;
        h.file_count = 0;
        h.reserved_entries = reserved;
        h.data_offset = sizeof(header) + sizeof(entry) * reserved;
        h.timestamp = now_timestamp();
        return h;
    }

    entry fresh_entry(const uint32_t index, const uint32_t compressed_size, const uint32_t original_size,
                     const tefpkg_compress_t type, const uint8_t level) {
        entry e{};
        e.index = index;
        e.compressed_size = compressed_size;
        e.original_size = original_size;
        e.compress_type = static_cast<uint8_t>(type);
        e.compress_level = level;
        e.timestamp = now_timestamp();
        return e;
    }

    /* ============================================================================
     * 校验和
     * ============================================================================ */

    static uint64_t header_checksum(const header& h) {
        header tmp = h;
        tmp.checksum = 0;
        tmp.signature = 0;
        tmp.content_hash = 0;
        return siphash_stream(reinterpret_cast<const uint8_t*>(&tmp),
                              sizeof(header) - 3 * sizeof(uint64_t),
                              0x0318030920211212ULL, 0x49204C6F76652059ULL);
    }

    static uint64_t entry_checksum(const entry& e, const uint8_t* data) {
        siphash_ctx_t ctx;
        siphash_init(&ctx, 0x6F75000000000000ULL, 0xE5B08FE9B9ACULL);
        siphash_update(&ctx, data, e.compressed_size);

        entry tmp = e;
        tmp.checksum = 0;
        tmp.data_offset = 0;
        siphash_update(&ctx, reinterpret_cast<const uint8_t*>(&tmp), sizeof(entry));
        return siphash_final(&ctx);
    }

    static uint64_t content_hash(const package& pkg) {
        siphash_ctx_t ctx;
        siphash_init(&ctx, 0x6F75000000000000ULL, 0xE5B08FE9B9ACULL);

        header tmp = pkg.header;
        tmp.content_hash = 0;
        tmp.signature = 0;
        siphash_update(&ctx, reinterpret_cast<const uint8_t*>(&tmp),
                       sizeof(header) - 2 * sizeof(uint64_t));

        for (const auto& opt : pkg.entries) {
            entry zero{};
            const entry& e = opt ? *opt : zero;
            siphash_update(&ctx, reinterpret_cast<const uint8_t*>(&e), sizeof(entry));
        }
        return siphash_final(&ctx);
    }

    static uint64_t package_signature(const package& pkg, const uint64_t fingerprint) {
        if (fingerprint == 0) return 0;

        const uint64_t k0 = siphash_stream(
            reinterpret_cast<const uint8_t*>(&fingerprint), sizeof(uint64_t),
            0x0318030920211212ULL, 0x49204C6F76652059ULL);
        const uint64_t k1 = siphash_stream(
            reinterpret_cast<const uint8_t*>(&fingerprint), sizeof(uint64_t),
            0x6F75000000000000ULL, 0xE5B08FE9B9ACULL);

        siphash_ctx_t ctx;
        siphash_init(&ctx, k0, k1);

        header tmp = pkg.header;
        tmp.signature = 0;
        siphash_update(&ctx, reinterpret_cast<const uint8_t*>(&tmp),
                       sizeof(header) - sizeof(uint64_t));

        for (const auto& opt : pkg.entries) {
            entry zero{};
            const entry& e = opt ? *opt : zero;
            siphash_update(&ctx, reinterpret_cast<const uint8_t*>(&e), sizeof(entry));
        }
        return siphash_final(&ctx);
    }

    /* ============================================================================
     * 压缩器桥接
     * ============================================================================ */

    result<bytes> compress_bytes(const uint8_t* src, const size_t size,
                                const tefpkg_compress_t type, const uint8_t level) {
        if (!src || size == 0) return error::invalid;
        uint8_t* out = nullptr;
        uint32_t out_size = 0;
        if (tefpkg_compress_memory(src, static_cast<uint32_t>(size),
                                   &out, &out_size, type, level) != tef_ok) {
            return error::general;
                                   }
        bytes result(out, out + out_size);
        std::free(out);
        return result;
    }

    result<bytes> decompress_bytes(const uint8_t* src, const size_t size,
                                  const size_t original_size, const tefpkg_compress_t type) {
        if (!src || size == 0 || original_size == 0) return error::invalid;
        uint8_t* out = nullptr;
        if (tefpkg_decompress_memory(src, static_cast<uint32_t>(size),
                                     &out, static_cast<uint32_t>(original_size),
                                     type) != tef_ok) {
            return error::corrupt;
                                     }
        bytes result(out, out + original_size);
        std::free(out);
        return result;
    }

    /* ============================================================================
     * 读取压缩数据（全部走 std::fstream / std::ifstream）
     * ============================================================================ */

    result<bytes> read_compressed_data(const package& pkg, const entry& entry) {
        bytes out(entry.compressed_size);

        if (auto* mem = std::get_if<bytes>(&pkg.backend)) {
            if (entry.data_offset + entry.compressed_size > mem->size())
                return error::corrupt;
            std::memcpy(out.data(), mem->data() + entry.data_offset, entry.compressed_size);
            return out;
        }

        if (auto* fs = std::get_if<std::unique_ptr<std::fstream>>(&pkg.backend)) {
            if (!*fs) return error::io;
            (*fs)->seekg(pkg.header.data_offset + entry.data_offset);
            if (!(*fs)->good()) return error::io;
            (*fs)->read(reinterpret_cast<char*>(out.data()),
                        static_cast<std::streamsize>(out.size()));
            if (static_cast<size_t>((*fs)->gcount()) != out.size()) return error::io;
            return out;
        }

        if (auto* name = std::get_if<std::string>(&pkg.backend)) {
            std::ifstream f(*name, std::ios::binary);
            if (!f) return error::io;
            f.seekg(pkg.header.data_offset + entry.data_offset);
            if (!f.good()) return error::io;
            f.read(reinterpret_cast<char*>(out.data()),
                   static_cast<std::streamsize>(out.size()));
            if (static_cast<size_t>(f.gcount()) != out.size()) return error::io;
            return out;
        }

        if (auto* bd = std::get_if<borrowed_data>(&pkg.backend)) {
            const uint32_t abs = pkg.header.data_offset + entry.data_offset;
            if (abs + entry.compressed_size > bd->size) return error::corrupt;
            std::memcpy(out.data(), bd->data + abs, entry.compressed_size);
            return out;
        }

        return error::invalid;
    }
    result<bytes> extract_entry(const package& pkg, const entry& entry) {
        auto compressed = read_compressed_data(pkg, entry);
        return decompress_bytes(compressed.value().data(),
                               compressed.value().size(),
                               entry.original_size,
                               static_cast<tefpkg_compress_t>(entry.compress_type));
    }

    /* ============================================================================
     * 追加条目
     * ============================================================================ */

    status append_entry(package& pkg, bytes compressed, entry meta) {
        meta.checksum = entry_checksum(meta, compressed.data());

        if (auto* mem = std::get_if<bytes>(&pkg.backend)) {
            meta.data_offset = static_cast<uint32_t>(mem->size());
            mem->insert(mem->end(), compressed.begin(), compressed.end());
            pkg.header.data_size = static_cast<uint32_t>(mem->size());
        } else if (const auto* fs = std::get_if<std::unique_ptr<std::fstream>>(&pkg.backend)) {
            if (!*fs) return error::io;

            uint32_t accumulated = 0;
            for (const auto& opt : pkg.entries)
                if (opt) accumulated += opt->compressed_size;
            meta.data_offset = accumulated;

            const std::streamoff abs = pkg.header.data_offset + meta.data_offset;
            (*fs)->seekp(abs);
            if (!(*fs)->good()) return error::io;
            (*fs)->write(reinterpret_cast<const char*>(compressed.data()),
                         static_cast<std::streamsize>(compressed.size()));
            if (!(*fs)->good()) return error::io;
            pkg.header.data_size += static_cast<uint32_t>(compressed.size());
        } else {
            return error::invalid;
        }

        pkg.entries[pkg.header.file_count] = meta;
        pkg.header.file_count++;
        pkg.header.timestamp = now_timestamp();
        return {};
    }

    /* ============================================================================
     * 验证
     * ============================================================================ */

    status verify_entry(const package& pkg, const entry& entry) {
        if (auto compressed = read_compressed_data(pkg, entry); entry_checksum(entry, compressed.value().data()) != entry.checksum)
            return error::signature;
        return {};
    }

    status verify_package(const package& pkg) {
        if (header_checksum(pkg.header) != pkg.header.checksum) return error::integrity;
        if (content_hash(pkg) != pkg.header.content_hash) return error::integrity;
        return {};
    }

    status verify_signature(const package& pkg, const uint64_t fingerprint) {
        if (fingerprint == 0) return error::not_signature;
        if (pkg.header.checksum != header_checksum(pkg.header)) return error::signature;
        if (pkg.header.signature != package_signature(pkg, fingerprint)) return error::signature;
        return {};
    }

    status sign_package(package& pkg, const uint64_t fingerprint) {
        pkg.header.checksum = header_checksum(pkg.header);
        pkg.header.content_hash = content_hash(pkg);
        if (fingerprint != 0)
            pkg.header.signature = package_signature(pkg, fingerprint);
        return {};
    }

    /* ============================================================================
     * 序列化
     * ============================================================================ */

    static void write_header(std::ostream& os, const header& h) {
        os.seekp(0);
        os.write(reinterpret_cast<const char*>(&h), sizeof(header));
    }

    static void write_entries(std::ostream& os, const std::vector<std::optional<entry>>& es) {
        os.seekp(sizeof(header));
        for (const auto& opt : es) {
            entry blank{};
            const entry& e = opt ? *opt : blank;
            os.write(reinterpret_cast<const char*>(&e), sizeof(entry));
        }
    }

    static void read_header(std::istream& is, header& h) {
        is.seekg(0);
        is.read(reinterpret_cast<char*>(&h), sizeof(header));
    }

    static void read_entries(std::istream& is, std::vector<std::optional<entry>>& es) {
        is.seekg(sizeof(header));
        for (auto& opt : es) {
            entry e{};
            is.read(reinterpret_cast<char*>(&e), sizeof(entry));
            static constexpr entry zero{};
            if (std::memcmp(&e, &zero, sizeof(entry)) == 0)
                opt = std::nullopt;
            else
                opt = e;
        }
    }

    static status validate_header(const header& h) {
        if (h.magic != TEFPKG_MAGIC) return error::signature;
        if (h.version != TEFPKG_VERSION) return error::signature;
        if (h.reserved_entries == 0 || h.reserved_entries > TEFPKG_MAX_FILES)
            return error::corrupt;
        if (h.file_count > h.reserved_entries) return error::corrupt;
        return {};
    }

    /* ============================================================================
     * 工厂
     * ============================================================================ */

    namespace detail {

        result<package*> create_file(const char* filename, const uint16_t reserved) {
            if (!filename || reserved == 0 || reserved > TEFPKG_MAX_FILES)
                return error::invalid;

            auto fs = std::make_unique<std::fstream>(
                filename, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
            if (!*fs) return error::io;

            auto* p = new package();
            p->header = fresh_header(reserved);
            p->entries.resize(reserved);

            write_header(*fs, p->header);
            write_entries(*fs, p->entries);
            fs->flush();
            if (!fs->good()) {
                delete p;
                return error::io;
            }

            p->backend = std::move(fs);
            return p;
        }

        result<package*> create_memory(const uint16_t reserved) {
            if (reserved == 0 || reserved > TEFPKG_MAX_FILES)
                return error::invalid;

            auto* p = new package();
            p->header = fresh_header(reserved);
            p->entries.resize(reserved);
            p->backend = bytes{};
            return p;
        }

        result<package*> open_read_only(const char* filename) {
            if (!filename) return error::invalid;

            std::ifstream probe(filename, std::ios::binary);
            if (!probe) return error::io;

            auto* p = new package();
            read_header(probe, p->header);

            if (const auto rv = validate_header(p->header); !rv) { delete p; return rv.err(); }

            p->entries.resize(p->header.reserved_entries);
            read_entries(probe, p->entries);
            if (!probe.good() && !probe.eof()) { delete p; return error::io; }

            p->backend = std::string(filename);
            return p;
        }

        result<package*> open_from_memory(const uint8_t* data, const uint32_t size) {
            if (!data || size < sizeof(header)) return error::invalid;

            auto* p = new package();
            std::memcpy(&p->header, data, sizeof(header));

            if (const auto rv = validate_header(p->header); !rv) { delete p; return rv.err(); }

            if (const uint32_t need = sizeof(header) + sizeof(entry) * p->header.reserved_entries; need > size) { delete p; return error::corrupt; }

            p->entries.resize(p->header.reserved_entries);
            for (uint16_t i = 0; i < p->header.reserved_entries; ++i) {
                entry e{};
                std::memcpy(&e, data + sizeof(header) + i * sizeof(entry), sizeof(entry));
                static constexpr entry zero{};
                if (std::memcmp(&e, &zero, sizeof(entry)) != 0)
                    p->entries[i] = e;
            }

            p->backend = borrowed_data{.data = data, .size = size};
            return p;
        }

        void destroy(const package* p) {
            delete p;
        }

    } // namespace detail

    /* ============================================================================
     * 保存
     * ============================================================================ */

    status save_to_file(package& pkg, const char* filename, const uint64_t fingerprint) {
        TEF_TRY(sign_package(pkg, fingerprint));

        const auto fs = std::make_unique<std::fstream>(
            filename, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        if (!*fs) return error::io;

        write_header(*fs, pkg.header);
        write_entries(*fs, pkg.entries);

        if (const auto* mem = std::get_if<bytes>(&pkg.backend)) {
            if (!mem->empty()) {
                fs->seekp(pkg.header.data_offset);
                if (!fs->good()) return error::io;
                fs->write(reinterpret_cast<const char*>(mem->data()),
                             static_cast<std::streamsize>(mem->size()));
            }
        }

        fs->flush();
        if (!fs->good()) return error::io;
        return {};
    }

    status flush_to_existing(package& pkg, const uint64_t fingerprint) {
        TEF_TRY(sign_package(pkg, fingerprint));

        const auto* fs = std::get_if<std::unique_ptr<std::fstream>>(&pkg.backend);
        if (!fs || !*fs) return error::io;

        write_header(**fs, pkg.header);
        write_entries(**fs, pkg.entries);
        (*fs)->flush();
        if (!(*fs)->good()) return error::io;
        return {};
    }
}