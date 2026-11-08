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

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace terraria::save {
    inline constexpr char k_magic[] = "TEFSAVE";
    inline constexpr size_t k_magic_len = 7;
    inline constexpr uint32_t k_version = 1;
    inline constexpr uint32_t k_kind_player = 1;
    inline constexpr uint32_t k_kind_world = 2;
    inline constexpr size_t k_header_size = 32;
    inline constexpr size_t k_seg_entry_size = 16;
    inline constexpr size_t k_max_segs = 16;
    inline constexpr uint32_t k_max_array = 1000000;

    enum class tlv_type : uint8_t {
        i8 = 0x01, i16 = 0x02, i32 = 0x03, i64 = 0x04,
        u8 = 0x05, u16 = 0x06, u32 = 0x07, u64 = 0x08,
        f32 = 0x09, f64 = 0x0A, boolean = 0x0B, str = 0x0C,
        blob = 0x0D, object = 0x0E, array = 0x0F, null = 0x10,
    };

    inline constexpr uint32_t k_seg_meta = 0x01;
    inline constexpr uint32_t k_seg_items = 0x02;
    inline constexpr uint32_t k_seg_buffs = 0x03;
    inline constexpr uint32_t k_seg_research = 0x04;
    inline constexpr uint32_t k_seg_chests = 0x05;

    constexpr uint32_t tag(const uint32_t seg, const uint32_t sub, const uint32_t field) {
        return (seg << 24) | (sub << 16) | field;
    }

    // META
    inline constexpr uint32_t k_meta_version = tag(k_seg_meta, 0, 1);
    inline constexpr uint32_t k_meta_kind = tag(k_seg_meta, 0, 2);
    inline constexpr uint32_t k_meta_game = tag(k_seg_meta, 0, 3);
    inline constexpr uint32_t k_meta_saved = tag(k_seg_meta, 0, 4);

    // ITEMS
    inline constexpr uint32_t k_items_array = tag(k_seg_items, 0, 0x100);
    inline constexpr uint32_t k_item_container = tag(k_seg_items, 1, 1);
    inline constexpr uint32_t k_item_slot = tag(k_seg_items, 1, 2);
    inline constexpr uint32_t k_item_loader = tag(k_seg_items, 1, 3);
    inline constexpr uint32_t k_item_mod = tag(k_seg_items, 1, 4);
    inline constexpr uint32_t k_item_name = tag(k_seg_items, 1, 5);
    inline constexpr uint32_t k_item_stack = tag(k_seg_items, 1, 6);
    inline constexpr uint32_t k_item_prefix = tag(k_seg_items, 1, 7);
    inline constexpr uint32_t k_item_orig_id = tag(k_seg_items, 1, 8);
    inline constexpr uint32_t k_item_prefix_loader = tag(k_seg_items, 1, 9);
    inline constexpr uint32_t k_item_prefix_mod = tag(k_seg_items, 1, 10);
    inline constexpr uint32_t k_item_prefix_name = tag(k_seg_items, 1, 11);

    // BUFFS
    inline constexpr uint32_t k_buffs_array = tag(k_seg_buffs, 0, 0x100);
    inline constexpr uint32_t k_buff_loader = tag(k_seg_buffs, 1, 1);
    inline constexpr uint32_t k_buff_mod = tag(k_seg_buffs, 1, 2);
    inline constexpr uint32_t k_buff_name = tag(k_seg_buffs, 1, 3);
    inline constexpr uint32_t k_buff_time = tag(k_seg_buffs, 1, 4);

    // CHESTS
    inline constexpr uint32_t k_chests_array = tag(k_seg_chests, 0, 0x100);
    inline constexpr uint32_t k_chest_index = tag(k_seg_chests, 1, 1);
    inline constexpr uint32_t k_chest_slot = tag(k_seg_chests, 1, 2);
    inline constexpr uint32_t k_chest_loader = tag(k_seg_chests, 1, 3);
    inline constexpr uint32_t k_chest_mod = tag(k_seg_chests, 1, 4);
    inline constexpr uint32_t k_chest_name = tag(k_seg_chests, 1, 5);
    inline constexpr uint32_t k_chest_stack = tag(k_seg_chests, 1, 6);
    inline constexpr uint32_t k_chest_prefix = tag(k_seg_chests, 1, 7);
    inline constexpr uint32_t k_chest_orig_id = tag(k_seg_chests, 1, 8);
    inline constexpr uint32_t k_chest_prefix_loader = tag(k_seg_chests, 1, 9);
    inline constexpr uint32_t k_chest_prefix_mod = tag(k_seg_chests, 1, 10);
    inline constexpr uint32_t k_chest_prefix_name = tag(k_seg_chests, 1, 11);

    struct wbuf {
        std::vector<uint8_t> data;
        bool ok = true;
    };

    void wb_tlv_u8(wbuf &b, uint32_t tag, uint8_t v);

    void wb_tlv_u16(wbuf &b, uint32_t tag, uint16_t v);

    void wb_tlv_u32(wbuf &b, uint32_t tag, uint32_t v);

    void wb_tlv_u64(wbuf &b, uint32_t tag, uint64_t v);

    void wb_tlv_i32(wbuf &b, uint32_t tag, int32_t v);

    void wb_tlv_f64(wbuf &b, uint32_t tag, double v);

    void wb_tlv_bool(wbuf &b, uint32_t tag, bool v);

    void wb_tlv_str(wbuf &b, uint32_t tag, const char *s);

    void wb_tlv_blob(wbuf &b, uint32_t tag, const void *p, uint32_t n);

    size_t wb_begin(wbuf &b, uint8_t type, uint32_t tag);

    void wb_end(const wbuf &b, size_t mark);

    size_t wb_begin_array(wbuf &b, uint32_t tag);

    void wb_end_array(const wbuf &b, size_t mark);

    size_t wb_begin_elem(wbuf &b);

    struct rd {
        const uint8_t *base = nullptr;
        size_t len = 0;
        size_t off = 0;
        int depth = 0;
    };

    struct tlv {
        uint8_t type = 0;
        uint32_t tag = 0;
        const uint8_t *val = nullptr;
        uint32_t vlen = 0;
        size_t next = 0;
    };

    bool rd_next(rd &r, tlv &out);

    uint8_t rd_u8(const tlv &t);

    uint16_t rd_u16(const tlv &t);

    uint32_t rd_u32(const tlv &t);

    uint64_t rd_u64(const tlv &t);

    int32_t rd_i32(const tlv &t);

    double rd_f64(const tlv &t);

    bool rd_bool(const tlv &t);

    const char *rd_str(const tlv &t);

    const void *rd_blob(const tlv &t, uint32_t *out_n);

    bool rd_enter(const tlv &t, rd &sub);

    bool rd_array_begin(const tlv &t, rd &elems, uint32_t *count);

    struct seg {
        uint32_t id = 0;
        uint32_t flags = 0;
        uint32_t off = 0;
        uint32_t len = 0;
    };

    struct file {
        std::vector<uint8_t> data;
        uint32_t kind = 0;
        seg segs[k_max_segs]{};
        int seg_count = 0;
    };

    bool file_open(const char *path, file &out);

    void file_close(file &f);

    rd file_seg(const file &f, int i);

    struct seg_out {
        uint32_t id = 0;
        const uint8_t *data = nullptr;
        uint32_t len = 0;
    };

    bool file_write(const char *final_path, uint32_t kind,
                    const seg_out *segs, int n);

    bool make_path(const char *base, char *buf, size_t n);

    uint32_t crc32(const void *p, size_t n);
} // namespace terraria::save
