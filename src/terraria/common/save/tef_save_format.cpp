/*******************************************************************************
 * tefkernel - tef_save_format
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
 * Created: 2026/11/8
 *******************************************************************************/

#include "terraria/save/tef_save_format.hpp"

#include <cstdio>
#include <cstring>

#include "tef/core/log.hpp"

namespace terraria::save {
    using namespace tef;

    // ---------------- 读 ----------------
    uint8_t rd_u8(const tlv &t) {
        uint8_t v;
        std::memcpy(&v, t.val, 1);
        return v;
    }

    uint16_t rd_u16(const tlv &t) {
        uint16_t v;
        std::memcpy(&v, t.val, 2);
        return v;
    }

    uint32_t rd_u32(const tlv &t) {
        uint32_t v;
        std::memcpy(&v, t.val, 4);
        return v;
    }

    uint64_t rd_u64(const tlv &t) {
        uint64_t v;
        std::memcpy(&v, t.val, 8);
        return v;
    }

    int32_t rd_i32(const tlv &t) {
        int32_t v;
        std::memcpy(&v, t.val, 4);
        return v;
    }

    double rd_f64(const tlv &t) {
        double v;
        std::memcpy(&v, t.val, 8);
        return v;
    }

    bool rd_bool(const tlv &t) { return t.val[0] != 0; }

    const char *rd_str(const tlv &t) { return reinterpret_cast<const char *>(t.val + 4); }

    const void *rd_blob(const tlv &t, uint32_t *out_n) {
        *out_n = rd_u32(t);
        return t.val + 4;
    }

    bool rd_enter(const tlv &t, rd &sub) {
        if (t.type != static_cast<uint8_t>(tlv_type::object)) return false;
        sub.base = t.val;
        sub.len = t.vlen;
        sub.off = 0;
        sub.depth = 0;
        return true;
    }

    bool rd_array_begin(const tlv &t, rd &elems, uint32_t *count) {
        if (t.type != static_cast<uint8_t>(tlv_type::array) || t.vlen < 4) return false;
        std::memcpy(count, t.val, 4);
        if (*count > k_max_array) return false;
        elems.base = t.val + 4;
        elems.len = t.vlen - 4;
        elems.off = 0;
        elems.depth = 0;
        return true;
    }

    rd file_seg(const file &f, const int i) {
        rd r;
        r.base = f.data.data();
        r.len = static_cast<size_t>(f.segs[i].off) + f.segs[i].len;
        r.off = f.segs[i].off;
        return r;
    }

    // ---------------- CRC32 ----------------
    uint32_t crc32(const void *p, const size_t n) {
        auto *s = static_cast<const uint8_t *>(p);
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < n; ++i) {
            crc ^= s[i];
            for (int k = 0; k < 8; ++k)
                crc = (crc >> 1) ^ (0xEDB88320u & static_cast<uint32_t>(-static_cast<int32_t>(crc & 1u)));
        }
        return ~crc;
    }

    // ---------------- 写 ----------------
    namespace {
        void wb_raw(wbuf &b, const void *p, const size_t n) {
            if (!b.ok || n == 0) return;
            auto *src = static_cast<const uint8_t *>(p);
            b.data.insert(b.data.end(), src, src + n);
        }

        void wb_u8(wbuf &b, const uint8_t v) { wb_raw(b, &v, 1); }
        void wb_u32(wbuf &b, const uint32_t v) { wb_raw(b, &v, 4); }
        void wb_u64(wbuf &b, const uint64_t v) { wb_raw(b, &v, 8); }

        void wb_hdr(wbuf &b, const uint8_t type, const uint32_t tag, const uint32_t vlen) {
            wb_u8(b, type);
            wb_u32(b, tag);
            wb_u32(b, vlen);
        }
    }

    void wb_tlv_u8(wbuf &b, const uint32_t tag, const uint8_t v) {
        wb_hdr(b, static_cast<uint8_t>(tlv_type::u8), tag, 1);
        wb_u8(b, v);
    }

    void wb_tlv_u16(wbuf &b, const uint32_t tag, const uint16_t v) {
        wb_hdr(b, static_cast<uint8_t>(tlv_type::u16), tag, 2);
        wb_raw(b, &v, 2);
    }

    void wb_tlv_u32(wbuf &b, const uint32_t tag, const uint32_t v) {
        wb_hdr(b, static_cast<uint8_t>(tlv_type::u32), tag, 4);
        wb_u32(b, v);
    }

    void wb_tlv_u64(wbuf &b, const uint32_t tag, const uint64_t v) {
        wb_hdr(b, static_cast<uint8_t>(tlv_type::u64), tag, 8);
        wb_u64(b, v);
    }

    void wb_tlv_i32(wbuf &b, const uint32_t tag, const int32_t v) {
        wb_hdr(b, static_cast<uint8_t>(tlv_type::i32), tag, 4);
        wb_u32(b, static_cast<uint32_t>(v));
    }

    void wb_tlv_f64(wbuf &b, const uint32_t tag, const double v) {
        uint64_t bits;
        std::memcpy(&bits, &v, 8);
        wb_hdr(b, static_cast<uint8_t>(tlv_type::f64), tag, 8);
        wb_u64(b, bits);
    }

    void wb_tlv_bool(wbuf &b, const uint32_t tag, const bool v) {
        wb_hdr(b, static_cast<uint8_t>(tlv_type::boolean), tag, 1);
        wb_u8(b, v ? 1 : 0);
    }

    void wb_tlv_str(wbuf &b, const uint32_t tag, const char *s) {
        const auto n = s ? static_cast<uint32_t>(std::strlen(s)) : 0;
        wb_hdr(b, static_cast<uint8_t>(tlv_type::str), tag, 4 + n + 1);
        wb_u32(b, n);
        if (n) wb_raw(b, s, n);
        wb_u8(b, 0);
    }

    void wb_tlv_blob(wbuf &b, const uint32_t tag, const void *p, const uint32_t n) {
        wb_hdr(b, static_cast<uint8_t>(tlv_type::blob), tag, 4 + n);
        wb_u32(b, n);
        if (n) wb_raw(b, p, n);
    }

    size_t wb_begin(wbuf &b, const uint8_t type, const uint32_t tag) {
        const size_t mark = b.data.size();
        wb_hdr(b, type, tag, 0);
        return mark;
    }

    void wb_end(const wbuf &b, const size_t mark) {
        if (!b.ok || b.data.size() < mark + 9) return;
        const auto vlen = static_cast<uint32_t>(b.data.size() - mark - 9);
        auto *p = const_cast<uint8_t *>(b.data.data() + mark + 5);
        std::memcpy(p, &vlen, 4);
    }

    namespace {
        uint32_t count_tlvs(const uint8_t *p, const size_t n) {
            uint32_t c = 0;
            size_t off = 0;
            while (off + 9 <= n) {
                uint32_t len;
                std::memcpy(&len, p + off + 5, 4);
                const size_t step = 9u + len;
                if (off + step > n) break;
                off += step;
                c++;
            }
            return c;
        }
    }

    size_t wb_begin_array(wbuf &b, const uint32_t tag) {
        const size_t mark = wb_begin(b, static_cast<uint8_t>(tlv_type::array), tag);
        wb_u32(b, 0);
        return mark;
    }

    void wb_end_array(const wbuf &b, const size_t mark) {
        if (!b.ok || b.data.size() < mark + 13) return;
        const uint32_t count = count_tlvs(b.data.data() + mark + 13, b.data.size() - mark - 13);
        auto *p = const_cast<uint8_t *>(b.data.data() + mark + 9);
        std::memcpy(p, &count, 4);
        wb_end(b, mark);
    }

    size_t wb_begin_elem(wbuf &b) { return wb_begin(b, static_cast<uint8_t>(tlv_type::object), 0); }

    // ---------------- 读取 ----------------
    bool rd_next(rd &r, tlv &out) {
        if (r.off + 9 > r.len) return false;
        const uint8_t *p = r.base + r.off;
        out.type = p[0];
        std::memcpy(&out.tag, p + 1, 4);
        std::memcpy(&out.vlen, p + 5, 4);
        if (r.off + 9u + out.vlen > r.len) return false;
        out.val = p + 9;
        out.next = r.off + 9u + out.vlen;
        r.off = out.next;
        return true;
    }

    // ---------------- 文件容器 ----------------
    void file_close(file &f) {
        f.data.clear();
        f.seg_count = 0;
        f.kind = 0;
    }

    bool file_open(const char *path, file &out) {
        file_close(out);
        FILE *fp = std::fopen(path, "rb");
        if (!fp) return false;

        std::fseek(fp, 0, SEEK_END);
        const long sz = std::ftell(fp);
        std::rewind(fp);
        if (sz < static_cast<long>(k_header_size) || sz > 64L * 1024 * 1024) {
            std::fclose(fp);
            return false;
        }

        out.data.resize(static_cast<size_t>(sz) + 1);
        const size_t got = std::fread(out.data.data(), 1, static_cast<size_t>(sz), fp);
        std::fclose(fp);
        if (got != static_cast<size_t>(sz)) {
            out.data.clear();
            return false;
        }
        out.data[sz] = 0;

        if (std::memcmp(out.data.data(), k_magic, k_magic_len) != 0) {
            out.data.clear();
            return false;
        }

        uint32_t version, kind, flags, reserved, seg_count, hcrc;
        std::memcpy(&version, out.data.data() + 8, 4);
        std::memcpy(&kind, out.data.data() + 12, 4);
        std::memcpy(&flags, out.data.data() + 16, 4);
        std::memcpy(&reserved, out.data.data() + 20, 4);
        std::memcpy(&seg_count, out.data.data() + 24, 4);
        std::memcpy(&hcrc, out.data.data() + 28, 4);

        if (version > k_version || seg_count > k_max_segs) {
            out.data.clear();
            return false;
        }
        if (crc32(out.data.data(), 28) != hcrc) {
            out.data.clear();
            return false;
        }

        constexpr size_t tab = k_header_size;
        if (tab + static_cast<size_t>(seg_count) * k_seg_entry_size > static_cast<size_t>(sz)) {
            out.data.clear();
            return false;
        }

        for (uint32_t i = 0; i < seg_count; ++i) {
            const uint8_t *e = out.data.data() + tab + i * k_seg_entry_size;
            seg s;
            std::memcpy(&s.id, e + 0, 4);
            std::memcpy(&s.flags, e + 4, 4);
            std::memcpy(&s.off, e + 8, 4);
            std::memcpy(&s.len, e + 12, 4);
            if (static_cast<uint64_t>(s.off) + s.len > static_cast<uint64_t>(sz)) {
                out.data.clear();
                return false;
            }

            if ((s.flags & 1u) && s.len >= 4) {
                uint32_t stored;
                std::memcpy(&stored, out.data.data() + s.off + s.len - 4, 4);
                if (crc32(out.data.data() + s.off, s.len - 4) != stored) {
                    log::warn("save_format: segment 0x{:02X} crc mismatch, skipped", s.id);
                    s.len = 0;
                }
            }
            out.segs[out.seg_count++] = s;
        }

        out.kind = kind;
        return true;
    }

    bool file_write(const char *final_path, const uint32_t kind,
                    const seg_out *segs, const int n) {
        if (n < 0 || n > static_cast<int>(k_max_segs)) return false;

        size_t total = k_header_size + static_cast<size_t>(n) * k_seg_entry_size;
        for (int i = 0; i < n; ++i) total += 4u + segs[i].len;

        std::vector<uint8_t> buf(total);
        std::memcpy(buf.data(), k_magic, k_magic_len);
        const uint32_t version = k_version, flags = 1u, reserved = 0, seg_count = static_cast<uint32_t>(n);
        std::memcpy(buf.data() + 8, &version, 4);
        std::memcpy(buf.data() + 12, &kind, 4);
        std::memcpy(buf.data() + 16, &flags, 4);
        std::memcpy(buf.data() + 20, &reserved, 4);
        std::memcpy(buf.data() + 24, &seg_count, 4);
        const uint32_t hcrc = crc32(buf.data(), 28);
        std::memcpy(buf.data() + 28, &hcrc, 4);

        size_t payload = k_header_size + static_cast<size_t>(n) * k_seg_entry_size;
        for (int i = 0; i < n; ++i) {
            auto soff = static_cast<uint32_t>(payload);
            uint32_t slen = segs[i].len + 4u;
            uint8_t *e = buf.data() + k_header_size + i * k_seg_entry_size;
            std::memcpy(e + 0, &segs[i].id, 4);
            std::memcpy(e + 4, &flags, 4);
            std::memcpy(e + 8, &soff, 4);
            std::memcpy(e + 12, &slen, 4);

            std::memcpy(buf.data() + soff, segs[i].data, segs[i].len);
            uint32_t scrc = crc32(segs[i].data, segs[i].len);
            std::memcpy(buf.data() + soff + segs[i].len, &scrc, 4);
            payload += slen;
        }

        char tmp[1400];
        if (std::snprintf(tmp, sizeof tmp, "%s.tmp", final_path) >= static_cast<int>(sizeof tmp)) return false;
        FILE *f = std::fopen(tmp, "wb");
        if (!f) return false;
        if (const bool ok = std::fwrite(buf.data(), 1, total, f) == total && (std::fclose(f) == 0); !ok) {
            std::remove(tmp);
            return false;
        }
        std::remove(final_path);
        if (std::rename(tmp, final_path) != 0) {
            std::remove(tmp);
            return false;
        }
        return true;
    }

    bool make_path(const char *base, char *buf, const size_t n) {
        if (!base || !base[0]) return false;
        return std::snprintf(buf, n, "%s.tef", base) < static_cast<int>(n);
    }
} // namespace terraria::save
