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
    sf_fmg_destroy(fmg, alloc);
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

void sf_fmg_destroy(sf_fmg_t *fmg, const sf_allocator_t *alloc) {
    if (!fmg) return;
    /*  Honour the allocator the object was created with; the parameter is
     *  accepted for symmetry with other modules but ignored. */
    (void)alloc;
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
 *===========================================================================*/

typedef struct sf_fmg_sort_view {
    int32_t     id;
    const char *text_utf8;
} sf_fmg_sort_view_t;

typedef struct sf_fmg_dedup_slot {
    const char *text;
    int64_t     offset;
} sf_fmg_dedup_slot_t;

static int compare_sort_view_by_id(const void *a, const void *b) {
    const sf_fmg_sort_view_t *va = (const sf_fmg_sort_view_t *)a;
    const sf_fmg_sort_view_t *vb = (const sf_fmg_sort_view_t *)b;
    if (va->id < vb->id) return -1;
    if (va->id > vb->id) return 1;
    return 0;
}

static int format_offset_name(char *buf, size_t cap, size_t i) {
    int n = snprintf(buf, cap, "StringOffset%zu", i);
    if (n < 0 || (size_t)n >= cap) return -1;
    return 0;
}

static sf_result_t write_one_string(sf_binary_writer_t *bw, bool unicode,
                                    const char *text_utf8) {
    return unicode
        ? sf_binary_writer_write_utf16(bw, text_utf8, true)
        : sf_binary_writer_write_shift_jis(bw, text_utf8, true);
}

static sf_result_t write_strings_simple(sf_binary_writer_t *bw,
                                        const sf_fmg_sort_view_t *view,
                                        size_t count, bool unicode) {
    char name[32];
    for (size_t i = 0; i < count; i++) {
        if (format_offset_name(name, sizeof(name), i) != 0) return SF_ERR_INTERNAL;

        const char *text = view[i].text_utf8;
        if (text) {
            int64_t pos = sf_binary_writer_position(bw);
            sf_result_t r = sf_binary_writer_fill_varint(bw, name, pos);
            if (r != SF_OK) return r;
            r = write_one_string(bw, unicode, text);
            if (r != SF_OK) return r;
        } else {
            sf_result_t r = sf_binary_writer_fill_varint(bw, name, 0);
            if (r != SF_OK) return r;
        }
    }
    return SF_OK;
}

static sf_result_t write_strings_reuse_offsets(sf_binary_writer_t *bw,
                                               const sf_fmg_sort_view_t *view,
                                               size_t count, bool unicode,
                                               const sf_allocator_t *alloc) {
    sf_fmg_dedup_slot_t *table = NULL;
    size_t table_size = 0;
    sf_result_t r = SF_OK;
    char name[32];

    if (count > 0) {
        table = (sf_fmg_dedup_slot_t *)sf_xalloc(alloc, count * sizeof(*table));
        if (!table) return SF_ERR_OOM;
    }

    for (size_t i = 0; i < count; i++) {
        if (format_offset_name(name, sizeof(name), i) != 0) {
            r = SF_ERR_INTERNAL; goto out;
        }
        const char *text = view[i].text_utf8;
        if (!text) {
            r = sf_binary_writer_fill_varint(bw, name, 0);
            if (r != SF_OK) goto out;
            continue;
        }

        int64_t found = -1;
        for (size_t k = 0; k < table_size; k++) {
            if (strcmp(table[k].text, text) == 0) {
                found = table[k].offset;
                break;
            }
        }

        if (found < 0) {
            int64_t offset = sf_binary_writer_position(bw);
            table[table_size].text = text;
            table[table_size].offset = offset;
            table_size++;
            r = sf_binary_writer_fill_varint(bw, name, offset);
            if (r != SF_OK) goto out;
            r = write_one_string(bw, unicode, text);
            if (r != SF_OK) goto out;
        } else {
            r = sf_binary_writer_fill_varint(bw, name, found);
            if (r != SF_OK) goto out;
        }
    }

out:
    sf_xfree(alloc, table);
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

    r = sf_binary_writer_reserve_i32(bw, "FileSize");           if (r != SF_OK) return r;
    r = sf_binary_writer_write_bool(bw, fmg->unicode);          if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw,
        (uint8_t)(fmg->version == SF_FMG_VERSION_DEMONS_SOULS ? 0xFFu : 0x00u));
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);                       if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);                       if (r != SF_OK) return r;

    if (fmg->entry_count > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    r = sf_binary_writer_reserve_i32(bw, "GroupCount");         if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, (int32_t)fmg->entry_count);
    if (r != SF_OK) return r;

    if (wide) {
        r = sf_binary_writer_write_i32(bw, (int32_t)0xFF);
        if (r != SF_OK) return r;
    }

    r = sf_binary_writer_reserve_varint(bw, "StringOffsets");   if (r != SF_OK) return r;
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

    r = sf_binary_writer_fill_i32(bw, "GroupCount", group_count);
    if (r != SF_OK) goto fail;
    r = sf_binary_writer_fill_varint(bw, "StringOffsets",
                                     sf_binary_writer_position(bw));
    if (r != SF_OK) goto fail;

    {
        char name[32];
        for (size_t k = 0; k < fmg->entry_count; k++) {
            if (format_offset_name(name, sizeof(name), k) != 0) {
                r = SF_ERR_INTERNAL; goto fail;
            }
            r = sf_binary_writer_reserve_varint(bw, name);
            if (r != SF_OK) goto fail;
        }
    }

    r = fmg->reuse_offsets
        ? write_strings_reuse_offsets(bw, view, fmg->entry_count,
                                      fmg->unicode, alloc)
        : write_strings_simple(bw, view, fmg->entry_count, fmg->unicode);
    if (r != SF_OK) goto fail;

    r = sf_binary_writer_fill_i32(bw, "FileSize",
                                  (int32_t)sf_binary_writer_position(bw));
fail:
    sf_xfree(alloc, view);
    return r;
}

static sf_result_t fmg_serialize(const sf_fmg_t *fmg, uint8_t **out_data,
                                 size_t *out_size, const sf_allocator_t *alloc) {
    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_memory(&os, alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, fmg->big_endian, alloc);
    if (r != SF_OK) { sf_ostream_close(os); return r; }

    r = fmg_write_body(bw, fmg, alloc);

    uint8_t *body = NULL;
    size_t   body_size = 0;
    if (r == SF_OK) {
        r = sf_binary_writer_finish_bytes(bw, &body, &body_size);
    } else {
        sf_binary_writer_destroy(bw);
    }
    sf_ostream_close(os);
    if (r != SF_OK) return r;

    if (!fmg->has_md5) {
        *out_data = body;
        *out_size = body_size;
        return SF_OK;
    }

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
