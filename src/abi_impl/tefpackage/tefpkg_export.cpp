/*******************************************************************************
 * tefkernel - tefpkg_export
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
#include "tefpackage/tefpkg.h"

#include <cstdlib>
#include <cstring>
#include <new>

using namespace tef::pkg;

static package*       as_package(tefpkg_t* p)       { return reinterpret_cast<package*>(p); }
static const package* as_package(const tefpkg_t* p) { return reinterpret_cast<const package*>(p); }
static tefpkg_t*      as_c_package(package* p)      { return reinterpret_cast<tefpkg_t*>(p); }

#define TEF_EXPORT_BEGIN try {
#define TEF_EXPORT_END(ret) \
} catch (const std::bad_alloc&) { return to_c(error::memory); } \
  catch (...) { return ret; }

/* ============================================================================
 * 生命周期
 * ============================================================================ */

extern "C" tefpkg_result_t
tefpkg_create_reserved_from_file(const char* filename, const uint16_t reserved, tefpkg_t** pkg) {
    TEF_EXPORT_BEGIN
    if (!pkg) return to_c(error::invalid);
    auto r = detail::create_file(filename, reserved);
    if (!r) return to_c(r.err());
    *pkg = as_c_package(r.value());
    return tef_ok;
    TEF_EXPORT_END(tef_error)
}

extern "C" tefpkg_result_t
tefpkg_create_reserved_from_memory(const uint16_t reserved, tefpkg_t** pkg) {
    TEF_EXPORT_BEGIN
    if (!pkg) return to_c(error::invalid);
    auto r = detail::create_memory(reserved);
    if (!r) return to_c(r.err());
    *pkg = as_c_package(r.value());
    return tef_ok;
    TEF_EXPORT_END(tef_error)
}

extern "C" tefpkg_result_t
tefpkg_open_readonly(const char* filename, tefpkg_t** pkg) {
    TEF_EXPORT_BEGIN
    if (!pkg) return to_c(error::invalid);
    auto r = detail::open_read_only(filename);
    if (!r) return to_c(r.err());
    *pkg = as_c_package(r.value());
    return tef_ok;
    TEF_EXPORT_END(tef_error)
}

extern "C" tefpkg_result_t
tefpkg_open_from_memory(const uint8_t* data, const uint32_t size, tefpkg_t** pkg) {
    TEF_EXPORT_BEGIN
    if (!pkg) return to_c(error::invalid);
    auto r = detail::open_from_memory(data, size);
    if (!r) return to_c(r.err());
    *pkg = as_c_package(r.value());
    return tef_ok;
    TEF_EXPORT_END(tef_error)
}

extern "C" tefpkg_result_t
tefpkg_save_file(tefpkg_t* pkg, const uint64_t fingerprint) {
    TEF_EXPORT_BEGIN
    if (!pkg) return to_c(error::invalid);
    auto* p = as_package(pkg);

    if (std::holds_alternative<std::unique_ptr<std::fstream>>(p->backend)) {
        const auto r = flush_to_existing(*p, fingerprint);
        return r ? tef_ok : to_c(r.err());
    }
    if (std::holds_alternative<bytes>(p->backend)) {
        /* memory 模式：tefpkg_save_file 只更新头部和条目，数据留在内存 */
        const auto r = sign_package(*p, fingerprint);
        return r ? tef_ok : to_c(r.err());
    }
    return to_c(error::invalid);
    TEF_EXPORT_END(tef_error)
}

extern "C" tefpkg_result_t
tefpkg_save_memory_file(const char* filename, tefpkg_t* pkg, const uint64_t fingerprint) {
    TEF_EXPORT_BEGIN
    if (!pkg || !filename) return to_c(error::invalid);
    auto* p = as_package(pkg);
    if (!std::holds_alternative<bytes>(p->backend))
        return to_c(error::invalid);
    const auto r = save_to_file(*p, filename, fingerprint);
    return r ? tef_ok : to_c(r.err());
    TEF_EXPORT_END(tef_error)
}

extern "C" void
tefpkg_close(tefpkg_t* pkg) {
    if (pkg) detail::destroy(as_package(pkg));
}

/* ============================================================================
 * 条目操作
 * ============================================================================ */

extern "C" tefpkg_result_t
tefpkg_add_entry_from_memory(tefpkg_t* pkg, const tefpkg_compress_t type,
                             const uint8_t level, const uint8_t* data, const uint32_t size) {
    TEF_EXPORT_BEGIN
    if (!pkg || !data || size == 0) return to_c(error::invalid);
    auto* p = as_package(pkg);

    if (type > compress_lz4_hc) return to_c(error::invalid);
    if (p->header.file_count >= p->header.reserved_entries)
        return to_c(error::no_space);

    auto compressed = compress_bytes(data, size, type, level);
    if (!compressed) return to_c(compressed.err());

    const entry meta = fresh_entry(p->header.file_count,
                            static_cast<uint32_t>(compressed.value().size()),
                            size, type, level);

    const auto r = append_entry(*p, std::move(compressed.value()), meta);
    return r ? tef_ok : to_c(r.err());
    TEF_EXPORT_END(tef_error)
}

extern "C" tefpkg_result_t
tefpkg_add_entry_from_file(tefpkg_t* pkg, const char* filepath,
                           const tefpkg_compress_t type, const uint8_t level) {
    TEF_EXPORT_BEGIN
    if (!pkg || !filepath) return to_c(error::invalid);

    std::ifstream f(filepath, std::ios::binary);
    if (!f) return to_c(error::io);

    f.seekg(0, std::ios::end);
    const auto sz = static_cast<std::streamoff>(f.tellg());
    f.seekg(0, std::ios::beg);
    if (sz <= 0) return to_c(error::io);

    bytes data(static_cast<size_t>(sz));
    f.read(reinterpret_cast<char*>(data.data()), sz);
    if (static_cast<size_t>(f.gcount()) != data.size()) return to_c(error::io);

    return tefpkg_add_entry_from_memory(pkg, type, level,
                                        data.data(),
                                        static_cast<uint32_t>(data.size()));
    TEF_EXPORT_END(tef_error)
}

extern "C" tefpkg_result_t
tefpkg_extract_entry_to_memory(const tefpkg_t* pkg, const uint32_t entry_index,
                               uint8_t** data, uint32_t* size) {
    TEF_EXPORT_BEGIN
    if (!pkg || !data || !size) return to_c(error::invalid);
    auto* p = as_package(pkg);

    if (entry_index >= p->entries.size() || !p->entries[entry_index])
        return to_c(error::not_found);

    auto r = extract_entry(*p, *p->entries[entry_index]);
    if (!r) return to_c(r.err());

    const auto& bytes = r.value();
    *data = static_cast<uint8_t*>(std::malloc(bytes.size()));
    if (!*data) return to_c(error::memory);
    std::memcpy(*data, bytes.data(), bytes.size());
    *size = static_cast<uint32_t>(bytes.size());
    return tef_ok;
    TEF_EXPORT_END(tef_error)
}

extern "C" tefpkg_result_t
tefpkg_extract_entry_to_file(const tefpkg_t* pkg, uint32_t entry_index,
                             const char* output_path) {
    TEF_EXPORT_BEGIN
    if (!pkg || !output_path) return to_c(error::invalid);
    auto* p = as_package(pkg);

    if (entry_index >= p->entries.size() || !p->entries[entry_index])
        return to_c(error::not_found);

    auto r = extract_entry(*p, *p->entries[entry_index]);
    if (!r) return to_c(r.err());

    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!out) return to_c(error::io);
    const auto& bytes = r.value();
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    if (!out.good()) return to_c(error::io);
    return tef_ok;
    TEF_EXPORT_END(tef_error)
}

/* ============================================================================
 * 信息查询
 * ============================================================================ */

extern "C" tefpkg_result_t
tefpkg_get_entry_info(const tefpkg_t* pkg, const uint32_t entry_index, tefpkg_entry_t** info) {
    TEF_EXPORT_BEGIN
    if (!pkg || !info) return to_c(error::invalid);
    auto* p = as_package(pkg);
    if (entry_index >= p->entries.size() || !p->entries[entry_index])
        return to_c(error::not_found);

    /* pkg 是 const，但 get_entry_info 的 ABI 是旧的、返回非 const 指针。
     * 外部不应通过它修改数据；这里 const_cast 只是为了匹配 ABI。 */
    auto* entry_ptr = const_cast<entry*>(&p->entries[entry_index].value());
    *info = reinterpret_cast<tefpkg_entry_t*>(entry_ptr);
    return tef_ok;
    TEF_EXPORT_END(tef_error)
}

extern "C" uint16_t tefpkg_get_entries_count(const tefpkg_t* pkg) {
    if (!pkg) return 0;
    return as_package(pkg)->header.file_count;
}

extern "C" uint16_t tefpkg_get_reserved_entries(const tefpkg_t* pkg) {
    if (!pkg) return 0;
    return as_package(pkg)->header.reserved_entries;
}

/* ============================================================================
 * 验证 / 签名
 * ============================================================================ */

extern "C" tefpkg_result_t
tefpkg_verify_entry(const tefpkg_t* pkg, const uint32_t entry_index) {
    TEF_EXPORT_BEGIN
    if (!pkg) return to_c(error::invalid);
    auto* p = as_package(pkg);
    if (entry_index >= p->entries.size() || !p->entries[entry_index])
        return to_c(error::not_found);
    const auto r = verify_entry(*p, *p->entries[entry_index]);
    return r ? tef_ok : to_c(r.err());
    TEF_EXPORT_END(tef_error)
}

extern "C" tefpkg_result_t tefpkg_verify_pkg(const tefpkg_t* pkg) {
    TEF_EXPORT_BEGIN
    if (!pkg) return to_c(error::invalid);
    const auto r = verify_package(*as_package(pkg));
    return r ? tef_ok : to_c(r.err());
    TEF_EXPORT_END(tef_error)
}

extern "C" tefpkg_result_t
tefpkg_verify_signature(const tefpkg_t* pkg, const uint64_t fingerprint) {
    TEF_EXPORT_BEGIN
    if (!pkg) return to_c(error::invalid);
    const auto r = verify_signature(*as_package(pkg), fingerprint);
    return r ? tef_ok : to_c(r.err());
    TEF_EXPORT_END(tef_error)
}

extern "C" tefpkg_result_t
tefpkg_sign_package(tefpkg_t* pkg, const uint64_t fingerprint) {
    TEF_EXPORT_BEGIN
    if (!pkg) return to_c(error::invalid);
    const auto r = sign_package(*as_package(pkg), fingerprint);
    return r ? tef_ok : to_c(r.err());
    TEF_EXPORT_END(tef_error)
}

/* ============================================================================
 * 包头 getter
 * ============================================================================ */

extern "C" uint16_t tefpkg_header_get_version(const tefpkg_t* pkg) {
    return pkg ? as_package(pkg)->header.version : 0;
}
extern "C" uint16_t tefpkg_header_get_file_count(const tefpkg_t* pkg) {
    return pkg ? as_package(pkg)->header.file_count : 0;
}
extern "C" uint16_t tefpkg_header_get_reserved_entries(const tefpkg_t* pkg) {
    return pkg ? as_package(pkg)->header.reserved_entries : 0;
}
extern "C" uint64_t tefpkg_header_get_timestamp(const tefpkg_t* pkg) {
    return pkg ? as_package(pkg)->header.timestamp : 0;
}
extern "C" uint64_t tefpkg_header_get_content_hash(const tefpkg_t* pkg) {
    return pkg ? as_package(pkg)->header.content_hash : 0;
}
extern "C" uint64_t tefpkg_header_get_signature(const tefpkg_t* pkg) {
    return pkg ? as_package(pkg)->header.signature : 0;
}
extern "C" tefpkg_access_mode_t tefpkg_get_access_mode(const tefpkg_t* pkg) {
    return pkg ? mode_of(*as_package(pkg)) : tef_access_closed;
}

/* ============================================================================
 * 条目 getter
 * ============================================================================ */

#define TEF_ENTRY_GETTER(name, field, type) \
extern "C" tefpkg_result_t name(const tefpkg_t* pkg, uint32_t idx, type* out) { \
    if (!pkg || !out) return to_c(error::invalid); \
    auto* p = as_package(pkg); \
    if (idx >= p->entries.size() || !p->entries[idx]) return to_c(error::not_found); \
    *out = p->entries[idx]->field; \
    return tef_ok; \
}

TEF_ENTRY_GETTER(tefpkg_entry_get_index,           index,           uint32_t)
TEF_ENTRY_GETTER(tefpkg_entry_get_data_offset,     data_offset,     uint32_t)
TEF_ENTRY_GETTER(tefpkg_entry_get_compressed_size, compressed_size, uint32_t)
TEF_ENTRY_GETTER(tefpkg_entry_get_original_size,   original_size,   uint32_t)
TEF_ENTRY_GETTER(tefpkg_entry_get_checksum,        checksum,        uint64_t)
TEF_ENTRY_GETTER(tefpkg_entry_get_timestamp,       timestamp,       uint64_t)
TEF_ENTRY_GETTER(tefpkg_entry_get_compress_type,   compress_type,   uint8_t)
TEF_ENTRY_GETTER(tefpkg_entry_get_compress_level,  compress_level,  uint8_t)

#undef TEF_ENTRY_GETTER