/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — BTAB lightmap atlasing config.
 *
 * Upstream: BTAB.cs
 */

// Upstream: BTAB.cs

#include "souls_formats/sf_btab.h"
#include "souls_formats/sf_encoding.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <stdint.h>
#include <string.h>

/*===========================================================================
 * Internal struct definitions (NOT exposed in the public header)
 *===========================================================================*/

struct sf_btab_entry {
    const char *part_name;      /* borrowed from name_pool */
    const char *material_name;  /* borrowed from name_pool */
    int32_t     atlas_id;
    sf_vec2_t   uv_offset;
    sf_vec2_t   uv_scale;
};

struct sf_btab {
    bool                  big_endian;
    bool                  long_format;
    struct sf_btab_entry *entries;
    size_t                entry_count;
    char                 *name_pool;   /* single bulk allocation for all UTF-8 strings */
    size_t                name_pool_size;
    const sf_allocator_t *alloc;
};

/*===========================================================================
 * Internal helpers
 *===========================================================================*/

/*
 * Find a null-terminated UTF-16LE string at byte offset `off` within the
 * raw strings buffer `buf` of `buf_size` bytes, convert it to UTF-8, and
 * write the result into `pool` at `*pool_pos`. Returns the pointer to the
 * start of the written UTF-8 string in the pool.
 *
 * Returns NULL on error (sets *err).
 */
static const char *btab_pool_add_utf16(const uint8_t *buf, size_t buf_size,
                                       int64_t off, char *pool, size_t pool_size,
                                       size_t *pool_pos, sf_result_t *err,
                                       const sf_allocator_t *alloc) {
    if (off < 0 || (size_t)off >= buf_size) {
        *err = SF_ERR_OUT_OF_RANGE;
        return NULL;
    }

    /* Find null-terminator (two zero bytes) in the UTF-16LE data */
    const uint8_t *start = buf + (size_t)off;
    size_t remaining = buf_size - (size_t)off;
    size_t len_bytes = 0;
    while (len_bytes + 1 < remaining) {
        if (start[len_bytes] == 0 && start[len_bytes + 1] == 0) break;
        len_bytes += 2;
    }
    /* len_bytes is the number of bytes before the null terminator */

    /* Convert UTF-16LE to UTF-8 (temporary allocation) */
    char *utf8 = NULL;
    size_t utf8_len = 0;
    *err = sf_utf16le_to_utf8(start, len_bytes, &utf8, &utf8_len, alloc);
    if (*err != SF_OK) return NULL;

    /* Copy into pool */
    if (*pool_pos + utf8_len + 1 > pool_size) {
        sf_xfree(alloc, utf8);
        *err = SF_ERR_OOM;
        return NULL;
    }
    const char *result = pool + *pool_pos;
    memcpy(pool + *pool_pos, utf8, utf8_len);
    pool[*pool_pos + utf8_len] = '\0';
    *pool_pos += utf8_len + 1;

    sf_xfree(alloc, utf8);
    *err = SF_OK;
    return result;
}

/*
 * Compute the total UTF-8 pool size needed for all entry strings.
 * We do a dry-run conversion to measure sizes.
 */
static sf_result_t btab_measure_pool(const uint8_t *str_buf, size_t str_buf_size,
                                     const int64_t *part_offsets,
                                     const int64_t *mat_offsets,
                                     size_t entry_count,
                                     size_t *out_pool_size,
                                     const sf_allocator_t *alloc) {
    size_t total = 0;
    for (size_t i = 0; i < entry_count; i++) {
        /* part_name */
        {
            int64_t off = part_offsets[i];
            if (off < 0 || (size_t)off >= str_buf_size) return SF_ERR_OUT_OF_RANGE;
            const uint8_t *start = str_buf + (size_t)off;
            size_t remaining = str_buf_size - (size_t)off;
            size_t len_bytes = 0;
            while (len_bytes + 1 < remaining) {
                if (start[len_bytes] == 0 && start[len_bytes + 1] == 0) break;
                len_bytes += 2;
            }
            char *utf8 = NULL;
            size_t utf8_len = 0;
            sf_result_t r = sf_utf16le_to_utf8(start, len_bytes, &utf8, &utf8_len, alloc);
            if (r != SF_OK) return r;
            sf_xfree(alloc, utf8);
            total += utf8_len + 1;
        }
        /* material_name */
        {
            int64_t off = mat_offsets[i];
            if (off < 0 || (size_t)off >= str_buf_size) return SF_ERR_OUT_OF_RANGE;
            const uint8_t *start = str_buf + (size_t)off;
            size_t remaining = str_buf_size - (size_t)off;
            size_t len_bytes = 0;
            while (len_bytes + 1 < remaining) {
                if (start[len_bytes] == 0 && start[len_bytes + 1] == 0) break;
                len_bytes += 2;
            }
            char *utf8 = NULL;
            size_t utf8_len = 0;
            sf_result_t r = sf_utf16le_to_utf8(start, len_bytes, &utf8, &utf8_len, alloc);
            if (r != SF_OK) return r;
            sf_xfree(alloc, utf8);
            total += utf8_len + 1;
        }
    }
    *out_pool_size = total;
    return SF_OK;
}

/*===========================================================================
 * Read
 * Upstream: BTAB.cs:Read()
 *===========================================================================*/

sf_result_t sf_btab_read_from_memory(sf_btab_t **out, const void *bytes, size_t size,
                                     const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_btab_t *btab = NULL;
    uint8_t *str_buf = NULL;
    int64_t *part_offsets = NULL;
    int64_t *mat_offsets = NULL;
    sf_result_t e = SF_OK;

    e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;

    /* Upstream: br.BigEndian = br.GetBoolean(0x10)
     * Peek at offset 0x10 to determine endianness before reading header. */
    e = sf_binary_reader_create(&r, s, false /* start LE */, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    /* Peek at offset 0x10 for the BigEndian byte */
    bool big_endian_peek = false;
    e = sf_binary_reader_get_bool(r, 0x10, &big_endian_peek);
    if (e != SF_OK) goto cleanup;

    /* REFUSE BigEndian: return SF_ERR_UNSUPPORTED_VERSION (same policy as FLVER2) */
    if (big_endian_peek) {
        e = SF_ERR_UNSUPPORTED_VERSION;
        goto cleanup;
    }

    /* Upstream: br.AssertInt32(1), br.AssertInt32(0) */
    e = sf_binary_reader_assert_i32_one(r, 1);             if (e != SF_OK) goto cleanup;
    e = sf_binary_reader_assert_i32_one(r, 0);             if (e != SF_OK) goto cleanup;

    int32_t entry_count_i32 = 0;
    int32_t strings_length = 0;
    e = sf_binary_reader_read_i32(r, &entry_count_i32);    if (e != SF_OK) goto cleanup;
    e = sf_binary_reader_read_i32(r, &strings_length);     if (e != SF_OK) goto cleanup;

    /* BigEndian byte (already peeked, now consume) */
    uint8_t be_byte = 0;
    e = sf_binary_reader_read_u8(r, &be_byte);             if (e != SF_OK) goto cleanup;
    /* AssertByte(0) × 3 */
    e = sf_binary_reader_assert_u8_one(r, 0);              if (e != SF_OK) goto cleanup;
    e = sf_binary_reader_assert_u8_one(r, 0);              if (e != SF_OK) goto cleanup;
    e = sf_binary_reader_assert_u8_one(r, 0);              if (e != SF_OK) goto cleanup;

    /* entrySize: 0x1C = short format, 0x28 = long format */
    int32_t entry_size = 0;
    {
        static const int32_t k_entry_sizes[2] = {0x1C, 0x28};
        e = sf_binary_reader_assert_i32(r, 2, k_entry_sizes, &entry_size);
        if (e != SF_OK) goto cleanup;
    }
    bool long_format = (entry_size == 0x28);
    sf_binary_reader_set_varint_long(r, long_format);

    /* AssertPattern(0x24, 0x00) */
    e = sf_binary_reader_assert_pattern(r, 0x24, 0x00);    if (e != SF_OK) goto cleanup;

    if (entry_count_i32 < 0 || strings_length < 0) {
        e = SF_ERR_BAD_MAGIC;
        goto cleanup;
    }

    size_t entry_count = (size_t)entry_count_i32;

    /* Record strings section start position */
    int64_t strings_start = sf_binary_reader_position(r);

    /* Read the raw strings section bytes */
    if (strings_length > 0) {
        str_buf = (uint8_t *)sf_xalloc(alloc, (size_t)strings_length);
        if (!str_buf) { e = SF_ERR_OOM; goto cleanup; }
        e = sf_binary_reader_read_bytes(r, str_buf, (size_t)strings_length);
        if (e != SF_OK) goto cleanup;
    }

    /* Allocate offset arrays for entries */
    if (entry_count > 0) {
        part_offsets = (int64_t *)sf_xalloc(alloc, entry_count * sizeof(int64_t));
        mat_offsets  = (int64_t *)sf_xalloc(alloc, entry_count * sizeof(int64_t));
        if (!part_offsets || !mat_offsets) { e = SF_ERR_OOM; goto cleanup; }
    }

    /* Read entries: capture string offsets and field data */
    /* We need a temporary entry buffer to hold atlas_id, uv_offset, uv_scale */
    struct sf_btab_entry *tmp_entries = NULL;
    if (entry_count > 0) {
        tmp_entries = (struct sf_btab_entry *)sf_xalloc(
            alloc, entry_count * sizeof(*tmp_entries));
        if (!tmp_entries) { e = SF_ERR_OOM; goto cleanup; }
        memset(tmp_entries, 0, entry_count * sizeof(*tmp_entries));
    }

    for (size_t i = 0; i < entry_count; i++) {
        /* ReadVarint: int32 for short format, int64 for long format */
        int64_t part_off = 0, mat_off = 0;
        e = sf_binary_reader_read_varint(r, &part_off);    if (e != SF_OK) { sf_xfree(alloc, tmp_entries); goto cleanup; }
        e = sf_binary_reader_read_varint(r, &mat_off);     if (e != SF_OK) { sf_xfree(alloc, tmp_entries); goto cleanup; }
        part_offsets[i] = part_off;
        mat_offsets[i]  = mat_off;

        e = sf_binary_reader_read_i32(r, &tmp_entries[i].atlas_id);   if (e != SF_OK) { sf_xfree(alloc, tmp_entries); goto cleanup; }
        e = sf_binary_reader_read_vec2(r, &tmp_entries[i].uv_offset); if (e != SF_OK) { sf_xfree(alloc, tmp_entries); goto cleanup; }
        e = sf_binary_reader_read_vec2(r, &tmp_entries[i].uv_scale);  if (e != SF_OK) { sf_xfree(alloc, tmp_entries); goto cleanup; }

        /* LongFormat has an extra AssertInt32(0) */
        if (long_format) {
            e = sf_binary_reader_assert_i32_one(r, 0);    if (e != SF_OK) { sf_xfree(alloc, tmp_entries); goto cleanup; }
        }
    }

    /* Build name pool: measure first, then allocate and fill */
    size_t pool_size = 0;
    if (entry_count > 0) {
        e = btab_measure_pool(str_buf, (size_t)strings_length,
                              part_offsets, mat_offsets, entry_count,
                              &pool_size, alloc);
        if (e != SF_OK) { sf_xfree(alloc, tmp_entries); goto cleanup; }
    }

    /* Allocate the top-level struct */
    btab = (sf_btab_t *)sf_xalloc(alloc, sizeof(*btab));
    if (!btab) { sf_xfree(alloc, tmp_entries); e = SF_ERR_OOM; goto cleanup; }
    memset(btab, 0, sizeof(*btab));
    btab->alloc       = alloc;
    btab->big_endian  = false;
    btab->long_format = long_format;
    btab->entry_count = entry_count;

    if (entry_count > 0) {
        btab->entries = (struct sf_btab_entry *)sf_xalloc(
            alloc, entry_count * sizeof(*btab->entries));
        if (!btab->entries) {
            sf_xfree(alloc, tmp_entries);
            e = SF_ERR_OOM;
            goto cleanup;
        }
        memset(btab->entries, 0, entry_count * sizeof(*btab->entries));

        /* Copy numeric fields from tmp_entries */
        for (size_t i = 0; i < entry_count; i++) {
            btab->entries[i].atlas_id  = tmp_entries[i].atlas_id;
            btab->entries[i].uv_offset = tmp_entries[i].uv_offset;
            btab->entries[i].uv_scale  = tmp_entries[i].uv_scale;
        }
    }
    sf_xfree(alloc, tmp_entries);
    tmp_entries = NULL;

    /* Allocate and fill name pool */
    if (pool_size > 0) {
        btab->name_pool = (char *)sf_xalloc(alloc, pool_size);
        if (!btab->name_pool) { e = SF_ERR_OOM; goto cleanup; }
        btab->name_pool_size = pool_size;

        size_t pool_pos = 0;
        for (size_t i = 0; i < entry_count; i++) {
            sf_result_t err2 = SF_OK;
            btab->entries[i].part_name = btab_pool_add_utf16(
                str_buf, (size_t)strings_length,
                part_offsets[i], btab->name_pool, pool_size, &pool_pos, &err2, alloc);
            if (!btab->entries[i].part_name) { e = err2; goto cleanup; }

            btab->entries[i].material_name = btab_pool_add_utf16(
                str_buf, (size_t)strings_length,
                mat_offsets[i], btab->name_pool, pool_size, &pool_pos, &err2, alloc);
            if (!btab->entries[i].material_name) { e = err2; goto cleanup; }
        }
    } else if (entry_count > 0) {
        /* All strings are empty — pool_size == 0 means no allocation needed,
         * but we still need valid pointers. Use a single-byte pool. */
        btab->name_pool = (char *)sf_xalloc(alloc, 1);
        if (!btab->name_pool) { e = SF_ERR_OOM; goto cleanup; }
        btab->name_pool[0] = '\0';
        btab->name_pool_size = 1;
        for (size_t i = 0; i < entry_count; i++) {
            btab->entries[i].part_name     = btab->name_pool;
            btab->entries[i].material_name = btab->name_pool;
        }
    }

    (void)strings_start; /* used conceptually; actual offset tracking via str_buf */

    *out = btab;
    btab = NULL; /* ownership transferred */

cleanup:
    if (btab) {
        sf_xfree(alloc, btab->name_pool);
        sf_xfree(alloc, btab->entries);
        sf_xfree(alloc, btab);
    }
    sf_xfree(alloc, part_offsets);
    sf_xfree(alloc, mat_offsets);
    sf_xfree(alloc, str_buf);
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    return e;
}

/*===========================================================================
 * Write
 * Upstream: BTAB.cs:Write()
 *===========================================================================*/

sf_result_t sf_btab_write_to_buffer(const sf_btab_t *btab, void **out_bytes,
                                    size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(btab != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size  = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *s = NULL;
    sf_binary_writer_t *w = NULL;
    int64_t *part_offsets = NULL;
    int64_t *mat_offsets  = NULL;
    sf_result_t e = SF_OK;

    e = sf_ostream_open_memory(&s, alloc);
    if (e != SF_OK) return e;

    e = sf_binary_writer_create(&w, s, false /* LE */, alloc);
    if (e != SF_OK) { sf_ostream_close(s); return e; }

    /* Set varint mode to match long_format */
    sf_binary_writer_set_varint_long(w, btab->long_format);

    /* Header */
    e = sf_binary_writer_write_i32(w, 1);                              if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(w, 0);                              if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(w, (int32_t)btab->entry_count);    if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(w, "StringsLength");              if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_u8(w, btab->big_endian ? 1u : 0u);     if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_u8(w, 0);                               if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_u8(w, 0);                               if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_u8(w, 0);                               if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(w, btab->long_format ? 0x28 : 0x1C); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_pattern(w, 0x24, 0x00);                if (e != SF_OK) goto cleanup;

    /* Strings section */
    int64_t strings_start = sf_binary_writer_position(w);

    /* Allocate offset capture arrays */
    if (btab->entry_count > 0) {
        part_offsets = (int64_t *)sf_xalloc(alloc, btab->entry_count * sizeof(int64_t));
        mat_offsets  = (int64_t *)sf_xalloc(alloc, btab->entry_count * sizeof(int64_t));
        if (!part_offsets || !mat_offsets) { e = SF_ERR_OOM; goto cleanup; }
    }

    /* Write strings: for each entry, write part_name then material_name,
     * each followed by 8-byte relative padding.
     * Upstream: bw.WriteUTF16(entry.PartName, true); bw.PadRelative(stringsStart, 8) */
    for (size_t i = 0; i < btab->entry_count; i++) {
        const struct sf_btab_entry *entry = &btab->entries[i];

        part_offsets[i] = sf_binary_writer_position(w) - strings_start;
        e = sf_binary_writer_write_utf16(w, entry->part_name ? entry->part_name : "", true);
        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_pad_relative(w, strings_start, 8);
        if (e != SF_OK) goto cleanup;

        mat_offsets[i] = sf_binary_writer_position(w) - strings_start;
        e = sf_binary_writer_write_utf16(w, entry->material_name ? entry->material_name : "", true);
        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_pad_relative(w, strings_start, 8);
        if (e != SF_OK) goto cleanup;
    }

    /* Fill StringsLength */
    {
        int64_t strings_end = sf_binary_writer_position(w);
        int32_t strings_length = (int32_t)(strings_end - strings_start);
        e = sf_binary_writer_fill_i32(w, "StringsLength", strings_length);
        if (e != SF_OK) goto cleanup;
    }

    /* Write entries */
    for (size_t i = 0; i < btab->entry_count; i++) {
        const struct sf_btab_entry *entry = &btab->entries[i];

        e = sf_binary_writer_write_varint(w, part_offsets[i]);         if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_varint(w, mat_offsets[i]);          if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(w, entry->atlas_id);            if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_vec2(w, entry->uv_offset);          if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_vec2(w, entry->uv_scale);           if (e != SF_OK) goto cleanup;
        if (btab->long_format) {
            e = sf_binary_writer_write_i32(w, 0);                      if (e != SF_OK) goto cleanup;
        }
    }

    e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);
    w = NULL; /* finish_bytes destroys the writer */

cleanup:
    if (w) sf_binary_writer_destroy(w);
    sf_xfree(alloc, part_offsets);
    sf_xfree(alloc, mat_offsets);
    sf_ostream_close(s);
    return e;
}

/*===========================================================================
 * Destroy
 *===========================================================================*/

void sf_btab_destroy(sf_btab_t *btab) {
    if (!btab) return;
    sf_xfree(btab->alloc, btab->name_pool);
    sf_xfree(btab->alloc, btab->entries);
    sf_xfree(btab->alloc, btab);
}

/*===========================================================================
 * Accessors
 *===========================================================================*/

bool sf_btab_is_big_endian(const sf_btab_t *btab) {
    return btab ? btab->big_endian : false;
}

bool sf_btab_is_long_format(const sf_btab_t *btab) {
    return btab ? btab->long_format : false;
}

size_t sf_btab_entry_count(const sf_btab_t *btab) {
    return btab ? btab->entry_count : 0u;
}

const sf_btab_entry_t *sf_btab_get_entry(const sf_btab_t *btab, size_t index) {
    if (!btab || index >= btab->entry_count) return NULL;
    return (const sf_btab_entry_t *)&btab->entries[index];
}

const char *sf_btab_entry_part_name(const sf_btab_entry_t *entry) {
    return entry ? entry->part_name : NULL;
}

const char *sf_btab_entry_material_name(const sf_btab_entry_t *entry) {
    return entry ? entry->material_name : NULL;
}

int32_t sf_btab_entry_atlas_id(const sf_btab_entry_t *entry) {
    return entry ? entry->atlas_id : 0;
}

sf_vec2_t sf_btab_entry_uv_offset(const sf_btab_entry_t *entry) {
    sf_vec2_t zero = {0.0f, 0.0f};
    return entry ? entry->uv_offset : zero;
}

sf_vec2_t sf_btab_entry_uv_scale(const sf_btab_entry_t *entry) {
    sf_vec2_t zero = {0.0f, 0.0f};
    return entry ? entry->uv_scale : zero;
}
