/*******************************************************************************
 * tefkernel - tef_save_save_format
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

#include "internal/terraria/save/tef_save_format.h"

#include <stdlib.h>
#include <string.h>

#include "internal/log.h"

uint8_t  tef_save_rd_u8 (const tef_save_tlv_t *t) { uint8_t v;  __builtin_memcpy(&v, t->val, 1); return v; }
uint16_t tef_save_rd_u16(const tef_save_tlv_t *t) { uint16_t v; __builtin_memcpy(&v, t->val, 2); return v; }
uint32_t tef_save_rd_u32(const tef_save_tlv_t *t) { uint32_t v; __builtin_memcpy(&v, t->val, 4); return v; }
uint64_t tef_save_rd_u64(const tef_save_tlv_t *t) { uint64_t v; __builtin_memcpy(&v, t->val, 8); return v; }
int32_t  tef_save_rd_i32(const tef_save_tlv_t *t) { int32_t v;  __builtin_memcpy(&v, t->val, 4); return v; }
double   tef_save_rd_f64(const tef_save_tlv_t *t) { double v;   __builtin_memcpy(&v, t->val, 8); return v; }
bool     tef_save_rd_bool(const tef_save_tlv_t *t){ return t->val[0] != 0; }

const char *tef_save_rd_str(const tef_save_tlv_t *t) {
  return (const char *)(t->val + 4);
}
const void *tef_save_rd_blob(const tef_save_tlv_t *t, uint32_t *out_n) {
  *out_n = tef_save_rd_u32(t);
  return t->val + 4;
}
bool tef_save_rd_enter(const tef_save_tlv_t *t, tef_save_rd_t *sub) {
  if (t->type != TEF_SAVE_T_OBJECT) return false;
  sub->base = t->val; sub->len = t->vlen; sub->off = 0; sub->depth = 0;
  return true;
}
bool tef_save_rd_array_begin(const tef_save_tlv_t *t, tef_save_rd_t *elems, uint32_t *count) {
  if (t->type != TEF_SAVE_T_ARRAY || t->vlen < 4) return false;
  __builtin_memcpy(count, t->val, 4);
  if (*count > TEF_SAVE_MAX_ARRAY) return false;
  elems->base = t->val + 4; elems->len = t->vlen - 4; elems->off = 0; elems->depth = 0;
  return true;
}

tef_save_rd_t tef_save_file_seg(const tef_save_file_t *f, const int i) {
  const tef_save_rd_t r = { .base = f->data, .len = (size_t)f->segs[i].off + f->segs[i].len, .off = f->segs[i].off, .depth = 0 };
  return r;
}

/* ================================================================
 * CRC32
 * ================================================================ */
uint32_t tef_save_crc32(const void *p, const size_t n) {
  const uint8_t *s = p;
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; ++i) {
    crc ^= s[i];
    for (int k = 0; k < 8; ++k)
      crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t) -(int32_t)(crc & 1u));
  }
  return ~crc;
}

/* ================================================================
 * 写入器
 * ================================================================ */
void tef_save_wb_free(tef_save_wbuf_t *b) {
  if (!b) return;
  free(b->data);
  b->data = NULL; b->len = b->cap = 0; b->ok = false;
}

static void wb_reserve(tef_save_wbuf_t *b, const size_t extra) {
  if (!b->ok) return;
  if (b->len + extra <= b->cap) return;
  size_t cap = b->cap ? b->cap : 256;
  while (cap < b->len + extra) cap <<= 1;
  uint8_t *p = realloc(b->data, cap);
  if (!p) { b->ok = false; return; }
  b->data = p; b->cap = cap;
}

static void wb_raw(tef_save_wbuf_t *b, const void *p, const size_t n) {
  if (!b->ok || n == 0) return;
  wb_reserve(b, n);
  if (!b->ok) return;
  memcpy(b->data + b->len, p, n);
  b->len += n;
}

static void wb_u8 (tef_save_wbuf_t *b, const uint8_t  v) { wb_raw(b, &v, 1); }
static void wb_u32(tef_save_wbuf_t *b, const uint32_t v) { wb_raw(b, &v, 4); }
static void wb_u64(tef_save_wbuf_t *b, const uint64_t v) { wb_raw(b, &v, 8); }

static void wb_hdr(tef_save_wbuf_t *b, const uint8_t type, const uint32_t tag, const uint32_t vlen) {
  wb_u8 (b, type);
  wb_u32(b, tag);
  wb_u32(b, vlen);
}

void tef_save_wb_tlv_u8  (tef_save_wbuf_t *b, const uint32_t tag, const uint8_t  v) { wb_hdr(b, TEF_SAVE_T_U8,   tag, 1); wb_u8 (b, v); }
void tef_save_wb_tlv_u16 (tef_save_wbuf_t *b, const uint32_t tag, const uint16_t v) { wb_hdr(b, TEF_SAVE_T_U16,  tag, 2); wb_raw(b, &v, 2); }
void tef_save_wb_tlv_u32 (tef_save_wbuf_t *b, const uint32_t tag, const uint32_t v) { wb_hdr(b, TEF_SAVE_T_U32,  tag, 4); wb_u32(b, v); }
void tef_save_wb_tlv_u64 (tef_save_wbuf_t *b, const uint32_t tag, const uint64_t v) { wb_hdr(b, TEF_SAVE_T_U64,  tag, 8); wb_u64(b, v); }
void tef_save_wb_tlv_i32 (tef_save_wbuf_t *b, const uint32_t tag, const int32_t  v) { wb_hdr(b, TEF_SAVE_T_I32,  tag, 4); wb_u32(b, (uint32_t)v); }
void tef_save_wb_tlv_f64 (tef_save_wbuf_t *b, const uint32_t tag, const double   v) {
  uint64_t bits; memcpy(&bits, &v, 8);
  wb_hdr(b, TEF_SAVE_T_F64, tag, 8); wb_u64(b, bits);
}
void tef_save_wb_tlv_bool(tef_save_wbuf_t *b, const uint32_t tag, const bool v) { wb_hdr(b, TEF_SAVE_T_BOOL, tag, 1); wb_u8(b, v ? 1 : 0); }

void tef_save_wb_tlv_str(tef_save_wbuf_t *b, const uint32_t tag, const char *s) {
  const uint32_t n = s ? (uint32_t)strlen(s) : 0;
  wb_hdr(b, TEF_SAVE_T_STR, tag, 4 + n + 1);
  wb_u32(b, n);
  if (n) wb_raw(b, s, n);
  wb_u8(b, 0);
}

void tef_save_wb_tlv_blob(tef_save_wbuf_t *b, const uint32_t tag, const void *p, const uint32_t n) {
  wb_hdr(b, TEF_SAVE_T_BLOB, tag, 4 + n);
  wb_u32(b, n);
  if (n) wb_raw(b, p, n);
}

size_t tef_save_wb_begin(tef_save_wbuf_t *b, const uint8_t type, const uint32_t tag) {
  const size_t mark = b->len;
  wb_hdr(b, type, tag, 0);
  return mark;
}
void tef_save_wb_end(const tef_save_wbuf_t *b, const size_t mark) {
  if (!b->ok || b->len < mark + 9) return;
  const uint32_t vlen = (uint32_t)(b->len - mark - 9);
  memcpy(b->data + mark + 5, &vlen, 4);
}

static uint32_t count_tlvs(const uint8_t *p, const size_t n) {
  uint32_t c = 0; size_t off = 0;
  while (off + 9 <= n) {
    uint32_t len; memcpy(&len, p + off + 5, 4);
    const size_t step = 9u + len;
    if (off + step > n) break;
    off += step; c++;
  }
  return c;
}

size_t tef_save_wb_begin_array(tef_save_wbuf_t *b, const uint32_t tag) {
  const size_t mark = tef_save_wb_begin(b, TEF_SAVE_T_ARRAY, tag);
  wb_u32(b, 0);
  return mark;
}
void tef_save_wb_end_array(const tef_save_wbuf_t *b, const size_t mark) {
  if (!b->ok || b->len < mark + 13) return;
  const uint32_t count = count_tlvs(b->data + mark + 13, b->len - mark - 13);
  memcpy(b->data + mark + 9, &count, 4);
  tef_save_wb_end(b, mark);
}
size_t tef_save_wb_begin_elem(tef_save_wbuf_t *b) {
  return tef_save_wb_begin(b, TEF_SAVE_T_OBJECT, 0);
}

/* ================================================================
 * 读取器
 * ================================================================ */
bool tef_save_rd_next(tef_save_rd_t *r, tef_save_tlv_t *out) {
  if (r->off + 9 > r->len) return false;
  const uint8_t *p = r->base + r->off;
  out->type = p[0];
  memcpy(&out->tag,  p + 1, 4);
  memcpy(&out->vlen, p + 5, 4);
  if (r->off + 9u + out->vlen > r->len) return false;
  out->val  = p + 9;
  out->next = r->off + 9u + out->vlen;
  r->off = out->next;
  return true;
}

/* ================================================================
 * 文件容器
 * ================================================================ */
void tef_save_file_close(tef_save_file_t *f) {
  if (!f) return;
  free(f->data);
  memset(f, 0, sizeof *f);
}

bool tef_save_file_open(const char *path, tef_save_file_t *out) {
  memset(out, 0, sizeof *out);
  FILE *fp = fopen(path, "rb");
  if (!fp) return false;

  if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return false; }
  const long sz = ftell(fp);
  rewind(fp);
  if (sz < (long)TEF_SAVE_HEADER_SIZE || sz > 64L * 1024 * 1024) { fclose(fp); return false; }

  uint8_t *buf = malloc((size_t)sz + 1);
  if (!buf) { fclose(fp); return false; }
  const size_t got = fread(buf, 1, (size_t)sz, fp);
  fclose(fp);
  if (got != (size_t)sz) { free(buf); return false; }
  buf[sz] = 0;   /* 尾部哨兵：保证最后一个 STR 的 '\0' 不越界 */

  if (memcmp(buf, TEF_SAVE_MAGIC, TEF_SAVE_MAGIC_LEN) != 0) { free(buf); return false; }

  uint32_t version, kind, flags, reserved, seg_count, hcrc;
  memcpy(&version,  buf + 8,  4);
  memcpy(&kind,     buf + 12, 4);
  memcpy(&flags,    buf + 16, 4);
  memcpy(&reserved, buf + 20, 4);
  memcpy(&seg_count,buf + 24, 4);
  memcpy(&hcrc,     buf + 28, 4);

  if (version > TEF_SAVE_VERSION || seg_count > TEF_SAVE_MAX_SEGS) { free(buf); return false; }
  if (tef_save_crc32(buf, 28) != hcrc) { free(buf); return false; }

  const size_t tab = TEF_SAVE_HEADER_SIZE;
  if (tab + (size_t)seg_count * TEF_SAVE_SEG_ENTRY_SIZE > (size_t)sz) { free(buf); return false; }

  for (uint32_t i = 0; i < seg_count; ++i) {
    const uint8_t *e = buf + tab + i * TEF_SAVE_SEG_ENTRY_SIZE;
    tef_save_seg_t s;
    memcpy(&s.id,    e + 0,  4);
    memcpy(&s.flags, e + 4,  4);
    memcpy(&s.off,   e + 8,  4);
    memcpy(&s.len,   e + 12, 4);
    if ((uint64_t)s.off + s.len > (uint64_t)sz) { free(buf); return false; }

    if ((s.flags & 1u) && s.len >= 4) {
      uint32_t stored;
      memcpy(&stored, buf + s.off + s.len - 4, 4);
      if (tef_save_crc32(buf + s.off, s.len - 4) != stored) {
        TEKLOG_WARN("tef_save_save_format: segment 0x%02X crc mismatch, skipped", s.id);
        s.len = 0;   /* 标记空段：读取时自然跳过 */
      }
    }
    out->segs[out->seg_count++] = s;
  }

  out->data = buf;
  out->len  = (size_t)sz;
  out->kind = kind;
  return true;
}

bool tef_save_file_write(const char *final_path, const uint32_t kind,
                    const tef_save_seg_out_t *segs, const int n) {
  if (n < 0 || n > (int)TEF_SAVE_MAX_SEGS) return false;

  size_t total = TEF_SAVE_HEADER_SIZE + (size_t)n * TEF_SAVE_SEG_ENTRY_SIZE;
  for (int i = 0; i < n; ++i) total += 4u + segs[i].len;

  uint8_t *buf = malloc(total);
  if (!buf) return false;

  memcpy(buf, TEF_SAVE_MAGIC, TEF_SAVE_MAGIC_LEN);
  const uint32_t version = TEF_SAVE_VERSION, flags = 1u, reserved = 0, seg_count = (uint32_t)n;
  memcpy(buf + 8,  &version,   4);
  memcpy(buf + 12, &kind,      4);
  memcpy(buf + 16, &flags,     4);
  memcpy(buf + 20, &reserved,  4);
  memcpy(buf + 24, &seg_count, 4);
  const uint32_t hcrc = tef_save_crc32(buf, 28);
  memcpy(buf + 28, &hcrc, 4);

  size_t payload = TEF_SAVE_HEADER_SIZE + (size_t)n * TEF_SAVE_SEG_ENTRY_SIZE;
  for (int i = 0; i < n; ++i) {
    uint32_t soff = (uint32_t)payload;
    uint32_t slen = segs[i].len + 4u;
    uint8_t *e = buf + TEF_SAVE_HEADER_SIZE + i * TEF_SAVE_SEG_ENTRY_SIZE;
    memcpy(e + 0,  &segs[i].id, 4);
    memcpy(e + 4,  &flags,      4);
    memcpy(e + 8,  &soff,       4);
    memcpy(e + 12, &slen,       4);

    memcpy(buf + soff, segs[i].data, segs[i].len);
    uint32_t scrc = tef_save_crc32(segs[i].data, segs[i].len);
    memcpy(buf + soff + segs[i].len, &scrc, 4);
    payload += slen;
  }

  char tmp[1400];
  if (snprintf(tmp, sizeof tmp, "%s.tmp", final_path) >= (int)sizeof tmp) { free(buf); return false; }
  FILE *f = fopen(tmp, "wb");
  if (!f) { free(buf); return false; }
  const bool ok = fwrite(buf, 1, total, f) == total && (fclose(f) == 0);
  free(buf);
  if (!ok) { remove(tmp); return false; }
  remove(final_path);
  if (rename(tmp, final_path) != 0) { remove(tmp); return false; }
  return true;
}

bool tef_save_make_path(const char *base, char *buf, const size_t n) {
  if (!base || !base[0]) return false;
  return snprintf(buf, n, "%s.tef", base) < (int)n;
}