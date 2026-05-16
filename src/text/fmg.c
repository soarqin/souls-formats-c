/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — FMG string container reader + writer.
 *
 * Mirrors pinned upstream (commit 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a):
 *   SoulsFormats/Formats/FMG.cs:68-139  (Read)
 *   SoulsFormats/Formats/FMG.cs:144-276 (Write + WriteStrings*)
 *
 * The FMG layout has three width-sensitive regions: header, group table,
 * and string-offset table. The "wide" mode (DarkSouls3 / Bloodborne / DS3 /
 * ER) uses 8-byte varints and 16-byte group entries; "narrow" (DemonsSouls
 * and DarkSouls1/2) uses 4-byte varints and 12-byte group entries.
 *
 * Optional 16-byte MD5 prefix (Gundam Unicorn) is detected by peeking byte
 * 0: if non-zero, prefix is present and is skipped (NOT verified, mirroring
 * upstream limitation). The writer DOES compute and prepend the hash when
 * has_md5=true; verification on read is still intentionally absent.
 */

#include "souls_formats/sf_fmg.h"

#include "crypto/md5_cng.h"
#include "internal/sf_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * Internal data layout
 *===========================================================================*/

struct sf_fmg_entry {
    int32_t  id;
    char    *text_utf8; /* NULL = deleted (tombstone). "" = valid empty. */
};

struct sf_fmg {
    const sf_allocator_t *alloc;

    sf_fmg_version_t version;
    bool             big_endian;
    bool             unicode;
    bool             has_md5;
    bool             reuse_offsets;

    sf_fmg_entry_t *entries;
    size_t          entry_count;
    size_t          entry_capacity;
};

/*===========================================================================
 * Helpers
 *===========================================================================*/

static void entry_clear(sf_fmg_entry_t *e, const sf_allocator_t *alloc) {
    if (!e) return;
    sf_xfree(alloc, e->text_utf8);
    e->text_utf8 = NULL;
    e->id = 0;
}

static sf_result_t entries_reserve(sf_fmg_t *fmg, size_t need) {
    if (need <= fmg->entry_capacity) return SF_OK;
    size_t new_cap = fmg->entry_capacity ? fmg->entry_capacity * 2 : 16;
    while (new_cap < need) {
        if (new_cap > SIZE_MAX / 2) return SF_ERR_OUT_OF_RANGE;
        new_cap *= 2;
    }
    size_t old_bytes = fmg->entry_capacity * sizeof(sf_fmg_entry_t);
    size_t new_bytes = new_cap * sizeof(sf_fmg_entry_t);
    sf_fmg_entry_t *p = (sf_fmg_entry_t *)sf_xrealloc(fmg->alloc, fmg->entries,
                                                      old_bytes, new_bytes);
    if (!p) return SF_ERR_OOM;
    /* Zero new tail to keep destroy safe on partial fill. */
    memset((char *)p + old_bytes, 0, new_bytes - old_bytes);
    fmg->entries = p;
    fmg->entry_capacity = new_cap;
    return SF_OK;
}

static sf_result_t entries_append(sf_fmg_t *fmg, int32_t id, char *text_utf8) {
    sf_result_t r = entries_reserve(fmg, fmg->entry_count + 1);
    if (r != SF_OK) return r;
    fmg->entries[fmg->entry_count].id = id;
    fmg->entries[fmg->entry_count].text_utf8 = text_utf8;
    fmg->entry_count++;
    return SF_OK;
}

static bool version_is_known(uint8_t v) {
    return v == SF_FMG_VERSION_DEMONS_SOULS ||
           v == SF_FMG_VERSION_DARK_SOULS_1 ||
           v == SF_FMG_VERSION_DARK_SOULS_3;
}

static sf_result_t fmg_alloc_blank(const sf_allocator_t *alloc, sf_fmg_t **out) {
    sf_fmg_t *fmg = (sf_fmg_t *)sf_xalloc(alloc, sizeof(*fmg));
    if (!fmg) return SF_ERR_OOM;
    memset(fmg, 0, sizeof(*fmg));
    fmg->alloc = alloc;
    fmg->version = SF_FMG_VERSION_DARK_SOULS_1;
    fmg->big_endian = false;
    fmg->unicode = true;
    fmg->has_md5 = false;
    fmg->reuse_offsets = false;
    *out = fmg;
    return SF_OK;
}

/*===========================================================================
 * Read path — mirrors FMG.cs:68-139
 *===========================================================================*/

static sf_result_t fmg_read(sf_binary_reader_t *br, sf_fmg_t **out,
                            const sf_allocator_t *alloc) {
    SF_CHECK_ARG(br != NULL && out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_result_t r;
    sf_fmg_t *fmg = NULL;
    r = fmg_alloc_blank(alloc, &fmg);
    if (r != SF_OK) return r;

    /*  MD5 prefix detection (FMG.cs:70-74). */
    uint8_t first_byte = 0;
    r = sf_binary_reader_get_u8(br, 0, &first_byte);
    if (r != SF_OK) goto fail;
    if (first_byte != 0) {
        fmg->has_md5 = true;
        r = sf_binary_reader_skip(br, 16);
        if (r != SF_OK) goto fail;
    }

    /*  Header (FMG.cs:76-99). */
    r = sf_binary_reader_assert_u8_one(br, 0); if (r != SF_OK) goto fail;

    bool big_endian = false;
    r = sf_binary_reader_read_bool(br, &big_endian); if (r != SF_OK) goto fail;
    fmg->big_endian = big_endian;
    sf_binary_reader_set_big_endian(br, big_endian);

    uint8_t version_byte = 0;
    r = sf_binary_reader_read_u8(br, &version_byte); if (r != SF_OK) goto fail;
    if (!version_is_known(version_byte)) {
        r = SF_ERR_UNSUPPORTED_VERSION;
        goto fail;
    }
    fmg->version = (sf_fmg_version_t)version_byte;

    r = sf_binary_reader_assert_u8_one(br, 0); if (r != SF_OK) goto fail;

    bool wide = (fmg->version == SF_FMG_VERSION_DARK_SOULS_3);
    sf_binary_reader_set_varint_long(br, wide);

    int32_t file_size = 0;
    r = sf_binary_reader_read_i32(br, &file_size); if (r != SF_OK) goto fail;
    (void)file_size; /* upstream reads but does not validate */

    bool unicode = false;
    r = sf_binary_reader_read_bool(br, &unicode); if (r != SF_OK) goto fail;
    fmg->unicode = unicode;

    /*  Aux byte: DemonsSouls writes 0xFF, others 0x00 (FMG.cs:85). Stored
     *  implicitly by version; we just enforce the upstream invariant. */
    uint8_t expected_aux =
        (fmg->version == SF_FMG_VERSION_DEMONS_SOULS) ? 0xFFu : 0x00u;
    r = sf_binary_reader_assert_u8_one(br, expected_aux); if (r != SF_OK) goto fail;

    r = sf_binary_reader_assert_u8_one(br, 0); if (r != SF_OK) goto fail;
    r = sf_binary_reader_assert_u8_one(br, 0); if (r != SF_OK) goto fail;

    int32_t group_count = 0;
    r = sf_binary_reader_read_i32(br, &group_count); if (r != SF_OK) goto fail;
    if (group_count < 0) { r = SF_ERR_OUT_OF_RANGE; goto fail; }

    int32_t string_count = 0;
    r = sf_binary_reader_read_i32(br, &string_count); if (r != SF_OK) goto fail;
    (void)string_count; /* upstream reads but does not validate */

    if (wide) {
        r = sf_binary_reader_assert_i32_one(br, (int32_t)0xFF);
        if (r != SF_OK) goto fail;
    }

    int64_t string_offsets_offset = 0;
    r = sf_binary_reader_read_varint(br, &string_offsets_offset);
    if (r != SF_OK) goto fail;
    if (fmg->has_md5) string_offsets_offset += 16;

    r = sf_binary_reader_assert_varint_one(br, 0); if (r != SF_OK) goto fail;

    /*  Group table (FMG.cs:101-138). */
    const int64_t per_offset = wide ? 8 : 4;

    for (int32_t gi = 0; gi < group_count; gi++) {
        int32_t offset_index = 0;
        int32_t first_id     = 0;
        int32_t last_id      = 0;

        r = sf_binary_reader_read_i32(br, &offset_index); if (r != SF_OK) goto fail;
        r = sf_binary_reader_read_i32(br, &first_id);     if (r != SF_OK) goto fail;
        r = sf_binary_reader_read_i32(br, &last_id);      if (r != SF_OK) goto fail;

        if (wide) {
            r = sf_binary_reader_assert_i32_one(br, 0);
            if (r != SF_OK) goto fail;
        }

        if (last_id < first_id) { r = SF_ERR_OUT_OF_RANGE; goto fail; }
        if (offset_index < 0)   { r = SF_ERR_OUT_OF_RANGE; goto fail; }

        /*  Validate group span won't overflow when computing the table
         *  position; mirrors the StepIn target in FMG.cs:113. */
        int64_t span = (int64_t)last_id - (int64_t)first_id + 1;
        if (span <= 0) { r = SF_ERR_OUT_OF_RANGE; goto fail; }

        int64_t table_pos =
            string_offsets_offset + (int64_t)offset_index * per_offset;

        r = sf_binary_reader_step_in(br, table_pos);
        if (r != SF_OK) goto fail;

        sf_result_t inner = SF_OK;
        for (int64_t j = 0; j < span; j++) {
            int64_t string_offset = 0;
            inner = sf_binary_reader_read_varint(br, &string_offset);
            if (inner != SF_OK) break;
            if (fmg->has_md5 && string_offset > 0) string_offset += 16;

            char *text_utf8 = NULL;
            if (string_offset > 0) {
                if (fmg->unicode) {
                    inner = sf_binary_reader_get_utf16(br, string_offset,
                                                       &text_utf8, NULL);
                } else {
                    inner = sf_binary_reader_get_shift_jis(br, string_offset,
                                                           &text_utf8, NULL);
                }
                if (inner != SF_OK) break;
            }

            int32_t id = first_id + (int32_t)j;
            inner = entries_append(fmg, id, text_utf8);
            if (inner != SF_OK) {
                sf_xfree(alloc, text_utf8);
                break;
            }
        }

        sf_result_t out_r = sf_binary_reader_step_out(br);
        if (inner != SF_OK) { r = inner; goto fail; }
        if (out_r != SF_OK) { r = out_r; goto fail; }
    }

    *out = fmg;
    return SF_OK;

fail:
    sf_fmg_destroy(fmg);
    return r;
}

/*===========================================================================
 * Public read entry points
 *===========================================================================*/

sf_result_t sf_fmg_read_from_stream(sf_fmg_t **out, sf_istream_t *stream,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && stream != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_binary_reader_t *br = NULL;
    sf_result_t r = sf_binary_reader_create(&br, stream, false, alloc);
    if (r != SF_OK) return r;
    r = fmg_read(br, out, alloc);
    sf_binary_reader_destroy(br);
    return r;
}

sf_result_t sf_fmg_read_from_memory(sf_fmg_t **out, const uint8_t *data,
                                    size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || data != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_memory(&stream, data, size, alloc);
    if (r != SF_OK) return r;
    r = sf_fmg_read_from_stream(out, stream, alloc);
    sf_istream_close(stream);
    return r;
}

sf_result_t sf_fmg_read_from_path(sf_fmg_t **out, const char *utf8_path,
                                  const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && utf8_path != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_file(&stream, utf8_path, alloc);
    if (r != SF_OK) return r;
    r = sf_fmg_read_from_stream(out, stream, alloc);
    sf_istream_close(stream);
    return r;
}

/*===========================================================================
 * Public construction / destruction
 *===========================================================================*/

sf_result_t sf_fmg_create(const sf_allocator_t *alloc, sf_fmg_version_t version,
                          sf_fmg_t **out) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    if (!version_is_known((uint8_t)version)) return SF_ERR_INVALID_ARG;
    alloc = sf_alloc_or_default(alloc);

    sf_fmg_t *fmg = NULL;
    sf_result_t r = fmg_alloc_blank(alloc, &fmg);
    if (r != SF_OK) return r;
    fmg->version = version;
    /*  Upstream: BigEndian = (version == DemonsSouls) when constructing
     *  via FMG(version) ctor (FMG.cs:57-63). */
    fmg->big_endian = (version == SF_FMG_VERSION_DEMONS_SOULS);
    fmg->unicode = true;
    *out = fmg;
    return SF_OK;
}

void sf_fmg_destroy(sf_fmg_t *fmg) {
    if (!fmg) return;
    const sf_allocator_t *a = fmg->alloc;
    for (size_t i = 0; i < fmg->entry_count; i++) {
        entry_clear(&fmg->entries[i], a);
    }
    sf_xfree(a, fmg->entries);
    sf_xfree(a, fmg);
}

/*===========================================================================
 * Write path — mirrors FMG.cs:144-276
 *
 * Strategy: serialize the body to a memory ostream first. If has_md5 is
 * true, prepend a freshly-computed MD5 hash of the body to produce the
 * final buffer (offsets in the body itself remain MD5-unaware, mirroring
 * FMG.cs:209-217 — the +16 shift is recovered on read at FMG.cs:97-98).
 *
 * Sorting: the C# upstream sorts Entries in-place during Write. To honour
 * `const sf_fmg_t *` we take a shallow copy (id + text pointer) into a
 * scratch buffer and sort that instead; the original entry array is never
 * mutated. The text bytes themselves are still owned by the FMG.
 *
 * Offset table: rather than reserving one varint placeholder per string
 * (N reservations through the writer's reservation table), we pre-fill
 * the entire offset block with 0xFE pattern bytes, collect the actual
 * positions in a local int64_t[] while writing the string bodies, and
 * back-patch the whole block in a single step_in / step_out pair at the
 * end. This avoids O(N) reservation bookkeeping and per-entry snprintf
 * names, both of which dominated the previous implementation at FMG-scale
 * (10k+ entries) before the reservation table was hashed.
 *===========================================================================*/

typedef struct sf_fmg_sort_view {
    int32_t     id;
    const char *text_utf8;
} sf_fmg_sort_view_t;

static int compare_sort_view_by_id(const void *a, const void *b) {
    const sf_fmg_sort_view_t *va = (const sf_fmg_sort_view_t *)a;
    const sf_fmg_sort_view_t *vb = (const sf_fmg_sort_view_t *)b;
    if (va->id < vb->id) return -1;
    if (va->id > vb->id) return 1;
    return 0;
}

/*  ASCII-only fast path detection. Pure-ASCII strings (every byte < 0x80)
 *  are common in real FMGs — Western item names, IDs, format tokens, etc.
 *  Returns true if `s` contains only ASCII; false on the first high byte.
 *  Empty strings are ASCII by definition. */
static bool fmg_text_is_ascii(const char *s) {
    if (!s) return true;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p >= 0x80u) return false;
    }
    return true;
}

/*  Emit `s` as a NUL-terminated UTF-16 LE/BE string by direct byte
 *  injection — no Win32 round-trip, no encoding state machines. Valid
 *  ONLY when every byte of `s` is < 0x80 (ASCII subset of UTF-8, which is
 *  also the BMP subset of UTF-16). Short strings use a stack buffer;
 *  longer strings fall back to a single heap allocation (still much
 *  cheaper than two MultiByteToWideChar calls + two intermediate mallocs
 *  inside sf_utf8_to_utf16le). */
static sf_result_t fmg_write_utf16_ascii(sf_binary_writer_t *bw, bool big_endian,
                                         const char *s, const sf_allocator_t *alloc) {
    size_t len = s ? strlen(s) : 0;
    if (len > (SIZE_MAX / 2u) - 1u) return SF_ERR_OUT_OF_RANGE;
    size_t total = (len + 1u) * 2u;

    uint8_t stack_buf[512];
    uint8_t *buf = stack_buf;
    bool heap = (total > sizeof(stack_buf));
    if (heap) {
        buf = (uint8_t *)sf_xalloc(alloc, total);
        if (!buf) return SF_ERR_OOM;
    }

    if (big_endian) {
        for (size_t i = 0; i < len; i++) {
            buf[i * 2u]       = 0;
            buf[i * 2u + 1u]  = (uint8_t)s[i];
        }
    } else {
        for (size_t i = 0; i < len; i++) {
            buf[i * 2u]       = (uint8_t)s[i];
            buf[i * 2u + 1u]  = 0;
        }
    }
    buf[total - 2u] = 0;
    buf[total - 1u] = 0;

    sf_result_t r = sf_binary_writer_write_bytes(bw, buf, total);
    if (heap) sf_xfree(alloc, buf);
    return r;
}

/*  Emit `s` as a NUL-terminated Shift-JIS string. Shift-JIS is a strict
 *  superset of 7-bit ASCII: any byte < 0x80 maps to itself, and a 0 byte
 *  is a valid string terminator. So for ASCII-only `s` we can write the
 *  bytes (plus the existing trailing NUL) directly. */
static sf_result_t fmg_write_sjis_ascii(sf_binary_writer_t *bw, const char *s) {
    size_t len = s ? strlen(s) : 0;
    /*  +1 to also write the terminating NUL. */
    return sf_binary_writer_write_bytes(bw, s ? s : "", len + 1u);
}

static sf_result_t write_one_string(sf_binary_writer_t *bw, bool unicode,
                                    const char *text_utf8,
                                    const sf_allocator_t *alloc) {
    /*  Fast path: ASCII-only content (very common). Skips Win32 encoding,
     *  avoids two malloc/free pairs per string. */
    if (fmg_text_is_ascii(text_utf8)) {
        return unicode
            ? fmg_write_utf16_ascii(bw, sf_binary_writer_big_endian(bw),
                                    text_utf8, alloc)
            : fmg_write_sjis_ascii(bw, text_utf8);
    }
    /*  Fallback: full Win32 conversion path via encoding_win32.c. */
    return unicode
        ? sf_binary_writer_write_utf16(bw, text_utf8, true)
        : sf_binary_writer_write_shift_jis(bw, text_utf8, true);
}

/*  Simple writer: every text entry is emitted once. NULL entries (deleted
 *  tombstones) record offset 0. Mirrors WriteStringsSimple upstream behaviour
 *  but accumulates offsets into a caller-owned buffer instead of round-
 *  tripping through reservation bookkeeping. */
static sf_result_t fmg_emit_strings_simple(sf_binary_writer_t *bw,
                                           const sf_fmg_sort_view_t *view,
                                           size_t count, bool unicode,
                                           const sf_allocator_t *alloc,
                                           int64_t *offsets_out) {
    for (size_t i = 0; i < count; i++) {
        const char *text = view[i].text_utf8;
        if (!text) {
            offsets_out[i] = 0;
            continue;
        }
        offsets_out[i] = sf_binary_writer_position(bw);
        sf_result_t r = write_one_string(bw, unicode, text, alloc);
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

/*  Dedup-aware writer: identical UTF-8 strings share a single in-file copy
 *  and their offsets-table entries point to the same position. Replaces the
 *  former O(N²) strcmp scan with an open-addressing hash table keyed on the
 *  FNV-1a-64 of each string. Same on-disk layout. */
typedef struct sf_fmg_dedup_slot {
    uint64_t    hash;
    int64_t     offset;
    const char *text;  /* borrowed pointer into view */
} sf_fmg_dedup_slot_t;

static uint64_t fmg_text_hash(const char *s) {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= (uint64_t)*p;
        h *= 0x100000001B3ULL;
    }
    return h;
}

static sf_result_t fmg_emit_strings_dedup(sf_binary_writer_t *bw,
                                          const sf_fmg_sort_view_t *view,
                                          size_t count, bool unicode,
                                          const sf_allocator_t *alloc,
                                          int64_t *offsets_out) {
    /*  Capacity = next power of two >= count*2, minimum 16. Load factor
     *  caps near 50%, keeping probe runs short. */
    size_t cap = 16;
    while (cap < count * 2u) {
        if (cap > (SIZE_MAX / 2)) return SF_ERR_OUT_OF_RANGE;
        cap *= 2;
    }
    const size_t mask = cap - 1;

    sf_fmg_dedup_slot_t *slots = (sf_fmg_dedup_slot_t *)sf_xalloc(
        alloc, cap * sizeof(*slots));
    if (!slots) return SF_ERR_OOM;
    for (size_t k = 0; k < cap; k++) {
        slots[k].text   = NULL;
        slots[k].offset = 0;
        slots[k].hash   = 0;
    }

    sf_result_t r = SF_OK;
    for (size_t i = 0; i < count; i++) {
        const char *text = view[i].text_utf8;
        if (!text) {
            offsets_out[i] = 0;
            continue;
        }

        const uint64_t h = fmg_text_hash(text);
        size_t k = (size_t)h & mask;
        for (;;) {
            if (slots[k].text == NULL) {
                /*  First occurrence: emit and remember offset. */
                int64_t pos = sf_binary_writer_position(bw);
                r = write_one_string(bw, unicode, text, alloc);
                if (r != SF_OK) goto out;
                slots[k].hash   = h;
                slots[k].offset = pos;
                slots[k].text   = text;
                offsets_out[i]  = pos;
                break;
            }
            if (slots[k].hash == h && strcmp(slots[k].text, text) == 0) {
                offsets_out[i] = slots[k].offset;
                break;
            }
            k = (k + 1) & mask;
        }
    }

out:
    sf_xfree(alloc, slots);
    return r;
}

static sf_result_t fmg_write_body(sf_binary_writer_t *bw, const sf_fmg_t *fmg,
                                  const sf_allocator_t *alloc) {
    const bool wide = (fmg->version == SF_FMG_VERSION_DARK_SOULS_3);
    sf_binary_writer_set_big_endian(bw, fmg->big_endian);
    sf_binary_writer_set_varint_long(bw, wide);

    sf_result_t r;

    r = sf_binary_writer_write_u8(bw, 0);                       if (r != SF_OK) return r;
    r = sf_binary_writer_write_bool(bw, fmg->big_endian);       if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, (uint8_t)fmg->version);   if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);                       if (r != SF_OK) return r;

    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, "FileSize"), return r);
    r = sf_binary_writer_write_bool(bw, fmg->unicode);          if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw,
        (uint8_t)(fmg->version == SF_FMG_VERSION_DEMONS_SOULS ? 0xFFu : 0x00u));
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);                       if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);                       if (r != SF_OK) return r;

    if (fmg->entry_count > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, "GroupCount"), return r);
    r = sf_binary_writer_write_i32(bw, (int32_t)fmg->entry_count);
    if (r != SF_OK) return r;

    if (wide) {
        r = sf_binary_writer_write_i32(bw, (int32_t)0xFF);
        if (r != SF_OK) return r;
    }

    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "StringOffsets"), return r);
    r = sf_binary_writer_write_varint(bw, 0);                   if (r != SF_OK) return r;

    sf_fmg_sort_view_t *view = NULL;
    if (fmg->entry_count > 0) {
        view = (sf_fmg_sort_view_t *)sf_xalloc(alloc,
            fmg->entry_count * sizeof(*view));
        if (!view) return SF_ERR_OOM;
        for (size_t i = 0; i < fmg->entry_count; i++) {
            view[i].id = fmg->entries[i].id;
            view[i].text_utf8 = fmg->entries[i].text_utf8;
        }
        qsort(view, fmg->entry_count, sizeof(*view), compare_sort_view_by_id);
    }

    int32_t group_count = 0;
    {
        size_t i = 0;
        while (i < fmg->entry_count) {
            r = sf_binary_writer_write_i32(bw, (int32_t)i);
            if (r != SF_OK) goto fail;
            r = sf_binary_writer_write_i32(bw, view[i].id);
            if (r != SF_OK) goto fail;

            while (i < fmg->entry_count - 1 &&
                   view[i + 1].id == view[i].id + 1) {
                i++;
            }
            r = sf_binary_writer_write_i32(bw, view[i].id);
            if (r != SF_OK) goto fail;

            if (wide) {
                r = sf_binary_writer_write_i32(bw, 0);
                if (r != SF_OK) goto fail;
            }
            group_count++;
            i++;
        }
    }

    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, "GroupCount", group_count), goto fail);

    /*  Offset-table emission: pre-fill the block with 0xFE pattern bytes
     *  (matches the writer's reservation pattern semantically), record
     *  string offsets into a local array while writing the bodies, then
     *  back-patch the whole table in one step_in / step_out pair. */
    const int per_offset = wide ? 8 : 4;
    int64_t string_table_pos = sf_binary_writer_position(bw);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "StringOffsets",
                                                        string_table_pos), goto fail);

    int64_t *offsets = NULL;
    if (fmg->entry_count > 0) {
        if (fmg->entry_count > SIZE_MAX / (size_t)per_offset) {
            r = SF_ERR_OUT_OF_RANGE; goto fail;
        }
        r = sf_binary_writer_write_pattern(bw,
                fmg->entry_count * (size_t)per_offset, 0xFE);
        if (r != SF_OK) goto fail;

        offsets = (int64_t *)sf_xalloc(alloc,
                fmg->entry_count * sizeof(int64_t));
        if (!offsets) { r = SF_ERR_OOM; goto fail; }
    }

    r = fmg->reuse_offsets
        ? fmg_emit_strings_dedup(bw, view, fmg->entry_count, fmg->unicode,
                                 alloc, offsets)
        : fmg_emit_strings_simple(bw, view, fmg->entry_count, fmg->unicode,
                                  alloc, offsets);
    if (r != SF_OK) { sf_xfree(alloc, offsets); goto fail; }

    /*  Back-patch the offset table in one shot. */
    if (fmg->entry_count > 0) {
        r = sf_binary_writer_step_in(bw, string_table_pos);
        if (r == SF_OK) {
            for (size_t k = 0; k < fmg->entry_count; k++) {
                r = sf_binary_writer_write_varint(bw, offsets[k]);
                if (r != SF_OK) break;
            }
            sf_result_t r2 = sf_binary_writer_step_out(bw);
            if (r == SF_OK) r = r2;
        }
        sf_xfree(alloc, offsets);
        if (r != SF_OK) goto fail;
    }

    r = sf_binary_writer_fill_i32(bw, "FileSize",
                                  (int32_t)sf_binary_writer_position(bw));
fail:
    sf_xfree(alloc, view);
    return r;
}

/*  Estimate the on-disk size from the entry view. Used to pre-grow the
 *  memory ostream so that the doubling-growth realloc churn is avoided
 *  for typical mid-to-large FMGs. Over-estimates are harmless (the buffer
 *  is detached and reused at exactly the written size); under-estimates
 *  simply trigger one or two doubling reallocs at the tail. */
static size_t fmg_estimate_output_size(const sf_fmg_t *fmg) {
    const bool   wide = (fmg->version == SF_FMG_VERSION_DARK_SOULS_3);
    const size_t group_entry_size = wide ? 16u : 12u;
    const size_t per_offset       = wide ? 8u  : 4u;
    size_t est = 64u;                          /* header + group count etc. */
    if (fmg->has_md5) est += 16u;
    if (fmg->entry_count == 0) return est;

    /*  Worst case: every entry is its own group. */
    if (fmg->entry_count > SIZE_MAX / group_entry_size) return SIZE_MAX;
    est += fmg->entry_count * group_entry_size;
    if (fmg->entry_count > SIZE_MAX / per_offset) return SIZE_MAX;
    est += fmg->entry_count * per_offset;

    for (size_t i = 0; i < fmg->entry_count; i++) {
        const char *text = fmg->entries[i].text_utf8;
        if (!text) continue;
        size_t len = strlen(text);
        /*  Unicode FMG encodes via UTF-16 LE/BE; ASCII-heavy strings are
         *  the common case (Western item names). 2*(len+1) is a tight
         *  upper bound for BMP-only content. Non-BMP would balloon by ~2x
         *  via UTF-8 → UTF-16 expansion, but is exceedingly rare in FMGs. */
        size_t per_string = fmg->unicode ? (len + 1u) * 2u : (len + 1u);
        if (est > SIZE_MAX - per_string) return SIZE_MAX;
        est += per_string;
    }
    return est;
}

static sf_result_t fmg_serialize(const sf_fmg_t *fmg, uint8_t **out_data,
                                 size_t *out_size, const sf_allocator_t *alloc) {
    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_memory(&os, alloc);
    if (r != SF_OK) return r;

    /*  Best-effort preallocate to skip ~17 doubling reallocs on multi-MB
     *  FMGs. We ignore the return value because the writer falls back to
     *  the default doubling growth on failure. */
    size_t estimate = fmg_estimate_output_size(fmg);
    if (estimate != SIZE_MAX) {
        (void)sf_ostream_reserve(os, estimate);
    }

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, fmg->big_endian, alloc);
    if (r != SF_OK) { sf_ostream_close(os); return r; }

    r = fmg_write_body(bw, fmg, alloc);
    if (r == SF_OK) r = sf_binary_writer_finish(bw);
    sf_binary_writer_destroy(bw);
    if (r != SF_OK) { sf_ostream_close(os); return r; }

    /*  Zero-copy take of the ostream's internal buffer — no second
     *  full-file memcpy. The ostream is left empty before close. */
    void *body = NULL;
    size_t body_size = 0;
    r = sf_ostream_detach_buffer(os, &body, &body_size);
    sf_ostream_close(os);
    if (r != SF_OK) return r;

    if (!fmg->has_md5) {
        *out_data = (uint8_t *)body;
        *out_size = body_size;
        return SF_OK;
    }

    /*  MD5 prefix path (Gundam Unicorn-only, irrelevant for v1 targets).
     *  We allocate a fresh buffer of body_size+16 and prepend the hash.
     *  Cannot avoid this final copy because in-stream offsets in the body
     *  are body-relative; pre-emitting the 16 bytes would shift them. */
    uint8_t hash[16];
    r = sfi_md5_hash(body, body_size, hash);
    if (r != SF_OK) { sf_xfree(alloc, body); return r; }

    if (body_size > SIZE_MAX - 16u) {
        sf_xfree(alloc, body);
        return SF_ERR_OUT_OF_RANGE;
    }
    size_t total = body_size + 16u;
    uint8_t *out = (uint8_t *)sf_xalloc(alloc, total);
    if (!out) { sf_xfree(alloc, body); return SF_ERR_OOM; }
    memcpy(out, hash, 16);
    memcpy(out + 16, body, body_size);
    sf_xfree(alloc, body);
    *out_data = out;
    *out_size = total;
    return SF_OK;
}

sf_result_t sf_fmg_write_to_memory(const sf_fmg_t *fmg, uint8_t **out_data,
                                   size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(fmg != NULL && out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    return fmg_serialize(fmg, out_data, out_size, sf_alloc_or_default(alloc));
}

sf_result_t sf_fmg_write_to_stream(const sf_fmg_t *fmg, sf_ostream_t *stream,
                                   const sf_allocator_t *alloc) {
    SF_CHECK_ARG(fmg != NULL && stream != NULL);
    alloc = sf_alloc_or_default(alloc);

    uint8_t *buf = NULL;
    size_t   size = 0;
    sf_result_t r = fmg_serialize(fmg, &buf, &size, alloc);
    if (r != SF_OK) return r;
    if (size > 0) {
        r = sf_ostream_write(stream, buf, size);
    }
    sf_xfree(alloc, buf);
    return r;
}

sf_result_t sf_fmg_write_to_path(const sf_fmg_t *fmg, const char *utf8_path,
                                 const sf_allocator_t *alloc) {
    SF_CHECK_ARG(fmg != NULL && utf8_path != NULL);
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_file(&os, utf8_path, alloc);
    if (r != SF_OK) return r;
    r = sf_fmg_write_to_stream(fmg, os, alloc);
    sf_ostream_close(os);
    return r;
}

/*===========================================================================
 * Public getters
 *===========================================================================*/

sf_fmg_version_t sf_fmg_get_version(const sf_fmg_t *fmg) {
    return fmg ? fmg->version : SF_FMG_VERSION_DARK_SOULS_1;
}

bool sf_fmg_is_big_endian(const sf_fmg_t *fmg) {
    return fmg ? fmg->big_endian : false;
}

bool sf_fmg_is_unicode(const sf_fmg_t *fmg) {
    return fmg ? fmg->unicode : false;
}

bool sf_fmg_has_md5(const sf_fmg_t *fmg) {
    return fmg ? fmg->has_md5 : false;
}

bool sf_fmg_get_reuse_offsets(const sf_fmg_t *fmg) {
    return fmg ? fmg->reuse_offsets : false;
}

size_t sf_fmg_get_entry_count(const sf_fmg_t *fmg) {
    return fmg ? fmg->entry_count : 0;
}

const sf_fmg_entry_t *sf_fmg_get_entry(const sf_fmg_t *fmg, size_t index) {
    if (!fmg || index >= fmg->entry_count) return NULL;
    return &fmg->entries[index];
}

const sf_fmg_entry_t *sf_fmg_find_entry_by_id(const sf_fmg_t *fmg, int32_t id) {
    if (!fmg) return NULL;
    /*  Mirrors FMG.cs:283: `Entries.Find(entry => entry.ID == id)` — first
     *  match by linear scan. */
    for (size_t i = 0; i < fmg->entry_count; i++) {
        if (fmg->entries[i].id == id) return &fmg->entries[i];
    }
    return NULL;
}

/*===========================================================================
 * Public setters
 *===========================================================================*/

void sf_fmg_set_big_endian(sf_fmg_t *fmg, bool value) {
    if (fmg) fmg->big_endian = value;
}

void sf_fmg_set_unicode(sf_fmg_t *fmg, bool value) {
    if (fmg) fmg->unicode = value;
}

void sf_fmg_set_md5(sf_fmg_t *fmg, bool value) {
    if (fmg) fmg->has_md5 = value;
}

void sf_fmg_set_reuse_offsets(sf_fmg_t *fmg, bool value) {
    if (fmg) fmg->reuse_offsets = value;
}

/*===========================================================================
 * Entry mutation
 *===========================================================================*/

static char *dup_text_or_null(const sf_allocator_t *a, const char *text_utf8) {
    /*  NULL stays NULL (deleted tombstone). Empty string "" round-trips. */
    if (!text_utf8) return NULL;
    size_t n = strlen(text_utf8) + 1;
    char *p = (char *)sf_xalloc(a, n);
    if (!p) return NULL;
    memcpy(p, text_utf8, n);
    return p;
}

sf_result_t sf_fmg_add_entry(sf_fmg_t *fmg, int32_t id, const char *text_utf8,
                             const sf_allocator_t *alloc) {
    SF_CHECK_ARG(fmg != NULL);
    /*  alloc is accepted for API symmetry; the FMG owns its allocator. */
    (void)alloc;
    char *dup = NULL;
    if (text_utf8) {
        dup = dup_text_or_null(fmg->alloc, text_utf8);
        if (!dup) return SF_ERR_OOM;
    }
    sf_result_t r = entries_append(fmg, id, dup);
    if (r != SF_OK) sf_xfree(fmg->alloc, dup);
    return r;
}

sf_result_t sf_fmg_remove_entry(sf_fmg_t *fmg, int32_t id) {
    SF_CHECK_ARG(fmg != NULL);
    for (size_t i = 0; i < fmg->entry_count; i++) {
        if (fmg->entries[i].id == id) {
            entry_clear(&fmg->entries[i], fmg->alloc);
            /*  Shift down; preserve insertion order. */
            for (size_t j = i + 1; j < fmg->entry_count; j++) {
                fmg->entries[j - 1] = fmg->entries[j];
            }
            fmg->entry_count--;
            memset(&fmg->entries[fmg->entry_count], 0, sizeof(sf_fmg_entry_t));
            return SF_OK;
        }
    }
    return SF_ERR_NOT_FOUND;
}

sf_result_t sf_fmg_set_entry_text(sf_fmg_t *fmg, int32_t id, const char *text_utf8,
                                  const sf_allocator_t *alloc) {
    SF_CHECK_ARG(fmg != NULL);
    (void)alloc;
    /*  Mirrors FMG.cs:285-294 indexer setter: if entry exists, update text;
     *  otherwise add a new entry. */
    for (size_t i = 0; i < fmg->entry_count; i++) {
        if (fmg->entries[i].id == id) {
            char *dup = NULL;
            if (text_utf8) {
                dup = dup_text_or_null(fmg->alloc, text_utf8);
                if (!dup) return SF_ERR_OOM;
            }
            sf_xfree(fmg->alloc, fmg->entries[i].text_utf8);
            fmg->entries[i].text_utf8 = dup;
            return SF_OK;
        }
    }
    return sf_fmg_add_entry(fmg, id, text_utf8, alloc);
}

/*===========================================================================
 * Entry accessors
 *===========================================================================*/

int32_t sf_fmg_entry_get_id(const sf_fmg_entry_t *entry) {
    return entry ? entry->id : 0;
}

const char *sf_fmg_entry_get_text(const sf_fmg_entry_t *entry) {
    return entry ? entry->text_utf8 : NULL;
}
