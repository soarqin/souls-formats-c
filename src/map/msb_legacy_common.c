/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — legacy MSB 32-bit param-chain skeleton.
 *
 * Mirrors:
 *   SoulsFormats/Formats/MSB/MSB.cs       (AssertHeader / WriteHeader)
 *   SoulsFormats/Formats/MSB/MSB1/MSB1.cs (Param<T>.Read / .Write)
 *
 * Legacy MSB params use signed 32-bit absolute offsets and null-terminated
 * ASCII names padded to 4-byte alignment:
 *
 *   i32  version = 0
 *   i32  nameOffset
 *   i32  offsetCount = entry_count + 1
 *   i32  entryOffsets[offsetCount - 1]
 *   i32  nextParamOffset
 */

#include "msb_legacy_internal.h"

#include "internal/sf_internal.h"
#include "souls_formats/sf_io.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Reservation-name buffer size for "MsbLegacyNextList<id>" — fits int + slack. */
#define MSB_LEGACY_RESERVE_NAME_MAX 40

static void msb_legacy_format_reserve_name(char buf[MSB_LEGACY_RESERVE_NAME_MAX],
                                           int reserve_id) {
    snprintf(buf, MSB_LEGACY_RESERVE_NAME_MAX, "MsbLegacyNextList%d", reserve_id);
}

static sf_result_t msb_legacy_position_i32(sf_binary_writer_t *w, int32_t *out) {
    if (!w || !out) return SF_ERR_INVALID_ARG;

    int64_t pos = sf_binary_writer_position(w);
    if (pos < 0 || pos > INT32_MAX) return SF_ERR_OUT_OF_RANGE;

    *out = (int32_t)pos;
    return SF_OK;
}

static sf_result_t msb_legacy_checked_offset_i32(int64_t offset, int32_t *out) {
    if (!out) return SF_ERR_INVALID_ARG;
    if (offset < 0 || offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;

    *out = (int32_t)offset;
    return SF_OK;
}

/* ──────────────────────────────────────────────────────────────────────
 * Read side
 * ──────────────────────────────────────────────────────────────────── */

sf_result_t msb_legacy_assert_header(sf_binary_reader_t *r) {
    if (!r) return SF_ERR_INVALID_ARG;
    sf_result_t rc;

    rc = sf_binary_reader_assert_ascii(r, "MSB ");   if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 1);      if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0x10);   if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_bool_one(r, false); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_bool_one(r, false); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_u8_one(r, 1);       if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_u8_one(r, 0xFF);    if (rc != SF_OK) return rc;
    return SF_OK;
}

void msb_legacy_free_layout(msb_legacy_layout_t *layout, const sf_allocator_t *a) {
    if (!layout) return;
    for (int i = 0; i < layout->list_count; i++) {
        sf_free(a, layout->lists[i].name);
        layout->lists[i].name = NULL;
    }
    layout->list_count = 0;
}

sf_result_t msb_legacy_read_header(sf_binary_reader_t   *r,
                                   msb_legacy_layout_t  *out_layout,
                                   const sf_allocator_t *a) {
    if (!r || !out_layout) return SF_ERR_INVALID_ARG;

    memset(out_layout, 0, sizeof *out_layout);
    out_layout->big_endian = sf_binary_reader_big_endian(r);
    out_layout->is64_bit   = false;
    out_layout->version    = 0;

    sf_istream_t *stream = sf_binary_reader_stream(r);
    int32_t count = 0;
    sf_result_t rc = SF_OK;

    for (;;) {
        if (count >= MSB_MAX_LISTS) {
            rc = SF_ERR_OUT_OF_RANGE;
            goto fail;
        }

        int32_t name_offset = 0, offset_count = 0;
        rc = sf_binary_reader_assert_i32_one(r, 0);       if (rc != SF_OK) goto fail;
        rc = sf_binary_reader_read_i32(r, &name_offset);  if (rc != SF_OK) goto fail;
        rc = sf_binary_reader_read_i32(r, &offset_count); if (rc != SF_OK) goto fail;

        if (name_offset < 0 || offset_count < 1) {
            rc = SF_ERR_OUT_OF_RANGE;
            goto fail;
        }
        int32_t entry_count = offset_count - 1;

        int64_t data_offset = sf_binary_reader_position(r);
        if (data_offset < 0 || data_offset > INT32_MAX) {
            rc = SF_ERR_OUT_OF_RANGE;
            goto fail;
        }

        rc = sf_binary_reader_skip(r, (int64_t)entry_count * 4);
        if (rc != SF_OK) goto fail;

        int32_t next_param_offset = 0;
        rc = sf_binary_reader_read_i32(r, &next_param_offset);
        if (rc != SF_OK) goto fail;
        if (next_param_offset < 0) {
            rc = SF_ERR_OUT_OF_RANGE;
            goto fail;
        }

        char *name = NULL;
        rc = sf_binary_reader_get_ascii(r, name_offset, &name, NULL);
        if (rc != SF_OK) goto fail;

        out_layout->lists[count].name        = name;
        out_layout->lists[count].entry_count = entry_count;
        out_layout->lists[count].data_offset = data_offset;
        count++;

        if (next_param_offset == 0) break;

        rc = sf_istream_seek(stream, next_param_offset);
        if (rc != SF_OK) goto fail;
    }

    out_layout->list_count = count;
    return SF_OK;

fail:
    out_layout->list_count = count;
    msb_legacy_free_layout(out_layout, a);
    return rc;
}

sf_result_t msb_legacy_iter_lists(sf_binary_reader_t        *r,
                                  const msb_legacy_layout_t *layout,
                                  msb_list_cb_t              cb,
                                  void                      *ctx,
                                  const sf_allocator_t      *a) {
    (void)a;
    if (!r || !layout || !cb) return SF_ERR_INVALID_ARG;

    sf_istream_t *stream = sf_binary_reader_stream(r);
    for (int32_t i = 0; i < layout->list_count; i++) {
        const msb_legacy_list_header_t *lh = &layout->lists[i];
        sf_result_t rc = sf_istream_seek(stream, lh->data_offset);
        if (rc != SF_OK) return rc;
        rc = cb(lh->name, lh->entry_count, r, ctx);
        if (rc != SF_OK) return rc;
    }
    return SF_OK;
}

/* ──────────────────────────────────────────────────────────────────────
 * Write side
 * ──────────────────────────────────────────────────────────────────── */

sf_result_t msb_legacy_write_header(sf_binary_writer_t *w) {
    if (!w) return SF_ERR_INVALID_ARG;
    sf_result_t rc;

    rc = sf_binary_writer_write_ascii(w, "MSB ", false); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32 (w, 1);              if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32 (w, 0x10);           if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_bool(w, false);          if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_bool(w, false);          if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8  (w, 1);              if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8  (w, 0xFF);           if (rc != SF_OK) return rc;
    return SF_OK;
}

sf_result_t msb_legacy_entry_list_write(sf_binary_writer_t *w,
                                        const char         *type_name,
                                        const char         *next_list_key,
                                        const void         *entries,
                                        size_t              entry_count,
                                        size_t              entry_size,
                                        msb_entry_write_fn  write_fn,
                                        void               *ctx) {
    if (!w || !type_name || !next_list_key || !write_fn) return SF_ERR_INVALID_ARG;
    if (entry_count > 0 && (!entries || entry_size == 0)) return SF_ERR_INVALID_ARG;
    if (entry_count > (size_t)(INT32_MAX - 1)) return SF_ERR_OUT_OF_RANGE;

    sf_result_t rc;
    char name_key[MSB_LEGACY_RESERVE_NAME_MAX * 2];
    snprintf(name_key, sizeof name_key, "%sName", next_list_key);

    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, name_key), return rc);
    rc = sf_binary_writer_write_i32(w, (int32_t)entry_count + 1); if (rc != SF_OK) return rc;

    for (size_t i = 0; i < entry_count; i++) {
        char entry_key[MSB_LEGACY_RESERVE_NAME_MAX * 2];
        snprintf(entry_key, sizeof entry_key, "%sEntry%zu", next_list_key, i);
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, entry_key), return rc);
    }
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, next_list_key), return rc);

    int32_t pos32 = 0;
    rc = msb_legacy_position_i32(w, &pos32); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i32(w, name_key, pos32), return rc);
    rc = sf_binary_writer_write_ascii(w, type_name, true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;

    const unsigned char *entry_bytes = (const unsigned char *)entries;
    for (size_t i = 0; i < entry_count; i++) {
        char entry_key[MSB_LEGACY_RESERVE_NAME_MAX * 2];
        snprintf(entry_key, sizeof entry_key, "%sEntry%zu", next_list_key, i);
        rc = msb_legacy_position_i32(w, &pos32); if (rc != SF_OK) return rc;
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i32(w, entry_key, pos32), return rc);
        rc = write_fn(w, entry_bytes + (i * entry_size), i, ctx);
        if (rc != SF_OK) return rc;
    }
    return SF_OK;
}

sf_result_t msb_legacy_reserve_list(sf_binary_writer_t *w,
                                    const char         *name,
                                    int                 reserve_id) {
    if (!w || !name) return SF_ERR_INVALID_ARG;

    char reserve_name[MSB_LEGACY_RESERVE_NAME_MAX];
    char name_offset_name[MSB_LEGACY_RESERVE_NAME_MAX];
    msb_legacy_format_reserve_name(reserve_name, reserve_id);
    snprintf(name_offset_name, sizeof name_offset_name, "MsbLegacyNameOff%d", reserve_id);

    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, name_offset_name), return rc);
    rc = sf_binary_writer_write_i32(w, 1); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, reserve_name), return rc);

    int32_t pos32 = 0;
    rc = msb_legacy_position_i32(w, &pos32); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i32(w, name_offset_name, pos32), return rc);
    rc = sf_binary_writer_write_ascii(w, name, true);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 4);
    if (rc != SF_OK) return rc;
    return SF_OK;
}

sf_result_t msb_legacy_fill_list(sf_binary_writer_t *w,
                                 int                 reserve_id,
                                 int32_t             entry_count,
                                 int64_t             data_offset) {
    if (!w) return SF_ERR_INVALID_ARG;
    if (entry_count != 0) return SF_ERR_INVALID_ARG;

    int32_t offset32 = 0;
    sf_result_t rc = msb_legacy_checked_offset_i32(data_offset, &offset32);
    if (rc != SF_OK) return rc;

    char reserve_name[MSB_LEGACY_RESERVE_NAME_MAX];
    msb_legacy_format_reserve_name(reserve_name, reserve_id);
    return sf_binary_writer_fill_i32(w, reserve_name, offset32);
}
