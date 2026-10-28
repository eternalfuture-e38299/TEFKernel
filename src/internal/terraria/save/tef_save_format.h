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
 * Created: 2026/10/28
 *******************************************************************************/

#ifndef TEF_SAVE_FORMAT_H
#define TEF_SAVE_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 文件级常量 ---- */
#define TEF_SAVE_MAGIC          "TEFSAVE"
#define TEF_SAVE_MAGIC_LEN      7u
#define TEF_SAVE_VERSION        1u
#define TEF_SAVE_KIND_PLAYER    1u
#define TEF_SAVE_KIND_WORLD     2u
#define TEF_SAVE_HEADER_SIZE    32u
#define TEF_SAVE_SEG_ENTRY_SIZE 16u
#define TEF_SAVE_MAX_SEGS       16u
#define TEF_SAVE_MAX_ARRAY      1000000u
#define TEF_SAVE_MAX_DEPTH      64u

/* ---- 类型枚举 ---- */
typedef enum : uint8_t {
  TEF_SAVE_T_I8    = 0x01,
  TEF_SAVE_T_I16   = 0x02,
  TEF_SAVE_T_I32   = 0x03,
  TEF_SAVE_T_I64   = 0x04,
  TEF_SAVE_T_U8    = 0x05,
  TEF_SAVE_T_U16   = 0x06,
  TEF_SAVE_T_U32   = 0x07,
  TEF_SAVE_T_U64   = 0x08,
  TEF_SAVE_T_F32   = 0x09,
  TEF_SAVE_T_F64   = 0x0A,
  TEF_SAVE_T_BOOL  = 0x0B,
  TEF_SAVE_T_STR   = 0x0C,
  TEF_SAVE_T_BLOB  = 0x0D,
  TEF_SAVE_T_OBJECT= 0x0E,
  TEF_SAVE_T_ARRAY = 0x0F,
  TEF_SAVE_T_NULL  = 0x10,
} tef_save_type_t;

/* ---- 段号 ---- */
#define TEF_SAVE_SEG_META      0x01u
#define TEF_SAVE_SEG_ITEMS     0x02u
#define TEF_SAVE_SEG_BUFFS     0x03u
#define TEF_SAVE_SEG_RESEARCH  0x04u
#define TEF_SAVE_SEG_CHESTS    0x05u

/* ---- tag 构造 ---- */
#define TEF_SAVE_TAG(seg, sub, field) \
  (((uint32_t)(seg) << 24) | ((uint32_t)(sub) << 16) | ((uint32_t)(field)))
#define TEF_SAVE_SEG_OF(tag)   (((tag) >> 24) & 0xFFu)
#define TEF_SAVE_SUB_OF(tag)   (((tag) >> 16) & 0xFFu)
#define TEF_SAVE_FIELD_OF(tag) ((tag) & 0xFFFFu)

/* ---- META ---- */
#define TEF_SAVE_META_VERSION  TEF_SAVE_TAG(TEF_SAVE_SEG_META, 0x00, 0x0001)
#define TEF_SAVE_META_KIND     TEF_SAVE_TAG(TEF_SAVE_SEG_META, 0x00, 0x0002)
#define TEF_SAVE_META_GAME     TEF_SAVE_TAG(TEF_SAVE_SEG_META, 0x00, 0x0003)
#define TEF_SAVE_META_SAVED    TEF_SAVE_TAG(TEF_SAVE_SEG_META, 0x00, 0x0004)

/* ---- ITEMS ---- */
#define TEF_SAVE_ITEMS_ARRAY    TEF_SAVE_TAG(TEF_SAVE_SEG_ITEMS, 0x00, 0x0100)
#define TEF_SAVE_ITEM_CONTAINER TEF_SAVE_TAG(TEF_SAVE_SEG_ITEMS, 0x01, 0x0001)
#define TEF_SAVE_ITEM_SLOT      TEF_SAVE_TAG(TEF_SAVE_SEG_ITEMS, 0x01, 0x0002)
#define TEF_SAVE_ITEM_LOADER    TEF_SAVE_TAG(TEF_SAVE_SEG_ITEMS, 0x01, 0x0003)
#define TEF_SAVE_ITEM_MOD       TEF_SAVE_TAG(TEF_SAVE_SEG_ITEMS, 0x01, 0x0004)
#define TEF_SAVE_ITEM_NAME      TEF_SAVE_TAG(TEF_SAVE_SEG_ITEMS, 0x01, 0x0005)
#define TEF_SAVE_ITEM_STACK     TEF_SAVE_TAG(TEF_SAVE_SEG_ITEMS, 0x01, 0x0006)
#define TEF_SAVE_ITEM_PREFIX    TEF_SAVE_TAG(TEF_SAVE_SEG_ITEMS, 0x01, 0x0007)

/* ---- BUFFS ---- */
#define TEF_SAVE_BUFFS_ARRAY    TEF_SAVE_TAG(TEF_SAVE_SEG_BUFFS, 0x00, 0x0100)
#define TEF_SAVE_BUFF_LOADER    TEF_SAVE_TAG(TEF_SAVE_SEG_BUFFS, 0x01, 0x0001)
#define TEF_SAVE_BUFF_MOD       TEF_SAVE_TAG(TEF_SAVE_SEG_BUFFS, 0x01, 0x0002)
#define TEF_SAVE_BUFF_NAME      TEF_SAVE_TAG(TEF_SAVE_SEG_BUFFS, 0x01, 0x0003)
#define TEF_SAVE_BUFF_TIME      TEF_SAVE_TAG(TEF_SAVE_SEG_BUFFS, 0x01, 0x0004)

/* ---- CHESTS ---- */
#define TEF_SAVE_CHESTS_ARRAY   TEF_SAVE_TAG(TEF_SAVE_SEG_CHESTS, 0x00, 0x0100)
#define TEF_SAVE_CHEST_INDEX    TEF_SAVE_TAG(TEF_SAVE_SEG_CHESTS, 0x01, 0x0001)
#define TEF_SAVE_CHEST_SLOT     TEF_SAVE_TAG(TEF_SAVE_SEG_CHESTS, 0x01, 0x0002)
#define TEF_SAVE_CHEST_LOADER   TEF_SAVE_TAG(TEF_SAVE_SEG_CHESTS, 0x01, 0x0003)
#define TEF_SAVE_CHEST_MOD      TEF_SAVE_TAG(TEF_SAVE_SEG_CHESTS, 0x01, 0x0004)
#define TEF_SAVE_CHEST_NAME     TEF_SAVE_TAG(TEF_SAVE_SEG_CHESTS, 0x01, 0x0005)
#define TEF_SAVE_CHEST_STACK    TEF_SAVE_TAG(TEF_SAVE_SEG_CHESTS, 0x01, 0x0006)
#define TEF_SAVE_CHEST_PREFIX   TEF_SAVE_TAG(TEF_SAVE_SEG_CHESTS, 0x01, 0x0007)

/* ================================================================
 * 写入器
 * ================================================================ */
typedef struct {
  uint8_t *data;
  size_t   len;
  size_t   cap;
  bool     ok;
} tef_save_wbuf_t;

void tef_save_wb_free(tef_save_wbuf_t *b);

void tef_save_wb_tlv_u8  (tef_save_wbuf_t *b, uint32_t tag, uint8_t  v);
void tef_save_wb_tlv_u16 (tef_save_wbuf_t *b, uint32_t tag, uint16_t v);
void tef_save_wb_tlv_u32 (tef_save_wbuf_t *b, uint32_t tag, uint32_t v);
void tef_save_wb_tlv_u64 (tef_save_wbuf_t *b, uint32_t tag, uint64_t v);
void tef_save_wb_tlv_i32 (tef_save_wbuf_t *b, uint32_t tag, int32_t  v);
void tef_save_wb_tlv_f64 (tef_save_wbuf_t *b, uint32_t tag, double   v);
void tef_save_wb_tlv_bool(tef_save_wbuf_t *b, uint32_t tag, bool     v);
void tef_save_wb_tlv_str (tef_save_wbuf_t *b, uint32_t tag, const char *s);
void tef_save_wb_tlv_blob(tef_save_wbuf_t *b, uint32_t tag, const void *p, uint32_t n);

size_t tef_save_wb_begin      (tef_save_wbuf_t *b, uint8_t type, uint32_t tag);
void   tef_save_wb_end        (const tef_save_wbuf_t *b, size_t mark);
size_t tef_save_wb_begin_array(tef_save_wbuf_t *b, uint32_t tag);
void   tef_save_wb_end_array  (const tef_save_wbuf_t *b, size_t mark);
size_t tef_save_wb_begin_elem (tef_save_wbuf_t *b);

/* ================================================================
 * 读取器（零拷贝）
 * ================================================================ */
typedef struct {
  const uint8_t *base;
  size_t         len;
  size_t         off;
  int            depth;
} tef_save_rd_t;

typedef struct {
  uint8_t        type;
  uint32_t       tag;
  const uint8_t *val;
  uint32_t       vlen;
  size_t         next;
} tef_save_tlv_t;

bool tef_save_rd_next(tef_save_rd_t *r, tef_save_tlv_t *out);

uint8_t  tef_save_rd_u8 (const tef_save_tlv_t *t);
uint16_t tef_save_rd_u16(const tef_save_tlv_t *t);
uint32_t tef_save_rd_u32(const tef_save_tlv_t *t);
uint64_t tef_save_rd_u64(const tef_save_tlv_t *t);
int32_t  tef_save_rd_i32(const tef_save_tlv_t *t);
double   tef_save_rd_f64(const tef_save_tlv_t *t);
bool     tef_save_rd_bool(const tef_save_tlv_t *t);

const char *tef_save_rd_str(const tef_save_tlv_t *t);
const void *tef_save_rd_blob(const tef_save_tlv_t *t, uint32_t *out_n);
bool tef_save_rd_enter(const tef_save_tlv_t *t, tef_save_rd_t *sub);
bool tef_save_rd_array_begin(const tef_save_tlv_t *t, tef_save_rd_t *elems, uint32_t *count);

/* ================================================================
 * 文件容器
 * ================================================================ */
typedef struct {
  uint32_t id;
  uint32_t flags;
  uint32_t off;
  uint32_t len;
} tef_save_seg_t;

typedef struct {
  uint8_t   *data;
  size_t     len;
  uint32_t   kind;
  tef_save_seg_t  segs[TEF_SAVE_MAX_SEGS];
  int        seg_count;
} tef_save_file_t;

/* 打开：整块读入内存，尾部哨兵 0；header CRC + 段边界校验；段 CRC 校验（坏段置 len=0） */
bool tef_save_file_open (const char *path, tef_save_file_t *out);
void tef_save_file_close(tef_save_file_t *f);

/* 段游标：调用方按需构造 */
tef_save_rd_t tef_save_file_seg(const tef_save_file_t *f, int i);

/* 写文件：原子替换（.tmp + rename） */
typedef struct {
  uint32_t id;
  const uint8_t *data;
  uint32_t len;
} tef_save_seg_out_t;

bool tef_save_file_write(const char *final_path, uint32_t kind,
                    const tef_save_seg_out_t *segs, int n);

/* 路径辅助：<base>.tef */
bool tef_save_make_path(const char *base, char *buf, size_t n);

/* CRC32（仅供内部/测试使用） */
uint32_t tef_save_crc32(const void *p, size_t n);

#ifdef __cplusplus
}
#endif
#endif /* TEF_SAVE_FORMAT_H */