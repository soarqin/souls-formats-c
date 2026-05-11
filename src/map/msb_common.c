/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — common MSB list-of-lists skeleton.
 *
 * Mirrors:
 *   SoulsFormats/Formats/MSB/MSB.cs              (AssertHeader / WriteHeader)
 *   SoulsFormats/Formats/MSB/MSBS/MSBS.cs        (Param<T>.Read / .Write)
 *   SoulsFormats/Formats/MSB/MSBE/MSBE.cs        (same pattern)
 *   SoulsFormats/Formats/MSB/MSBVI/MSBVI.cs      (same pattern)
 *
 * The MSB on-disk layout is a 16-byte magic header followed by a chain of
 * "param" sections. Each param section has its own header followed by
 * (possibly zero) entries. The chain is linked through each param's
 * trailing nextParamOffset, with 0 marking the final section:
 *
 *   [MSB Magic]            16 bytes
 *   [Param 0 header]       i32 version, i32 offsetCount,
 *                          i64 nameOffset,
 *                          i64 entryOffsets[offsetCount - 1],
 *                          i64 nextParamOffset
 *   [Param 0 name]         UTF-16, padded to 8
 *   [Param 0 entries...]   (variant-specific; not handled here)
 *   [Param 1 header]       ...
 *   ...
 */

#include "msb_internal.h"

#include "internal/sf_internal.h"
#include "souls_formats/sf_io.h"

#include <stdio.h>
#include <string.h>

/* Reservation-name buffer size for "MsbNextList<id>" — fits int + slack. */
#define MSB_RESERVE_NAME_MAX 32

static void msb_format_reserve_name(char buf[MSB_RESERVE_NAME_MAX], int reserve_id) {
    snprintf(buf, MSB_RESERVE_NAME_MAX, "MsbNextList%d", reserve_id);
}

/* ──────────────────────────────────────────────────────────────────────
 * Read side
 * ──────────────────────────────────────────────────────────────────── */

sf_result_t msb_common_assert_header(sf_binary_reader_t *r) {
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

void msb_common_free_layout(msb_layout_t *layout, const sf_allocator_t *a) {
    if (!layout) return;
    for (int i = 0; i < layout->list_count; i++) {
        sf_free(a, layout->lists[i].name);
        layout->lists[i].name = NULL;
    }
    layout->list_count = 0;
}

sf_result_t msb_common_read_header(sf_binary_reader_t *r,
                                   msb_layout_t        *out_layout,
                                   const sf_allocator_t *a) {
    if (!r || !out_layout) return SF_ERR_INVALID_ARG;

    memset(out_layout, 0, sizeof *out_layout);
    out_layout->big_endian = sf_binary_reader_big_endian(r);
    out_layout->is64_bit   = true;
    out_layout->version    = 1;

    sf_result_t rc = msb_common_assert_header(r);
    if (rc != SF_OK) return rc;

    sf_istream_t *stream = sf_binary_reader_stream(r);
    int32_t count = 0;

    for (;;) {
        if (count >= MSB_MAX_LISTS) {
            rc = SF_ERR_OUT_OF_RANGE;
            goto fail;
        }

        int32_t version = 0, offset_count = 0;
        int64_t name_offset = 0;
        rc = sf_binary_reader_read_i32(r, &version);      if (rc != SF_OK) goto fail;
        rc = sf_binary_reader_read_i32(r, &offset_count); if (rc != SF_OK) goto fail;
        rc = sf_binary_reader_read_i64(r, &name_offset);  if (rc != SF_OK) goto fail;

        if (offset_count < 1) {
            rc = SF_ERR_OUT_OF_RANGE;
            goto fail;
        }
        int32_t entry_count = offset_count - 1;

        int64_t data_offset = sf_binary_reader_position(r);

        rc = sf_binary_reader_skip(r, (int64_t)entry_count * 8);
        if (rc != SF_OK) goto fail;

        int64_t next_param_offset = 0;
        rc = sf_binary_reader_read_i64(r, &next_param_offset);
        if (rc != SF_OK) goto fail;

        char *name = NULL;
        rc = sf_binary_reader_get_utf16(r, name_offset, &name, NULL);
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
    msb_common_free_layout(out_layout, a);
    return rc;
}

sf_result_t msb_common_iter_lists(sf_binary_reader_t   *r,
                                  const msb_layout_t   *layout,
                                  msb_list_cb_t         cb,
                                  void                 *ctx,
                                  const sf_allocator_t *a) {
    (void)a;
    if (!r || !layout || !cb) return SF_ERR_INVALID_ARG;

    sf_istream_t *stream = sf_binary_reader_stream(r);
    for (int32_t i = 0; i < layout->list_count; i++) {
        const msb_list_header_t *lh = &layout->lists[i];
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

sf_result_t msb_common_write_header(sf_binary_writer_t *w) {
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

sf_result_t msb_common_reserve_list(sf_binary_writer_t *w,
                                    const char         *name,
                                    int                 reserve_id) {
    if (!w || !name) return SF_ERR_INVALID_ARG;

    char reserve_name[MSB_RESERVE_NAME_MAX];
    char name_offset_name[MSB_RESERVE_NAME_MAX];
    msb_format_reserve_name(reserve_name, reserve_id);
    snprintf(name_offset_name, sizeof name_offset_name, "MsbNameOff%d", reserve_id);

    sf_result_t rc;
    rc = sf_binary_writer_write_i32   (w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32   (w, 1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64 (w, name_offset_name); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64 (w, reserve_name);     if (rc != SF_OK) return rc;

    rc = sf_binary_writer_fill_i64(w, name_offset_name,
                                    sf_binary_writer_position(w));
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, name, true);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8);
    if (rc != SF_OK) return rc;
    return SF_OK;
}

sf_result_t msb_common_fill_list(sf_binary_writer_t *w,
                                 int                 reserve_id,
                                 int32_t             entry_count,
                                 int64_t             data_offset) {
    if (!w) return SF_ERR_INVALID_ARG;
    if (entry_count != 0) return SF_ERR_INVALID_ARG;

    char reserve_name[MSB_RESERVE_NAME_MAX];
    msb_format_reserve_name(reserve_name, reserve_id);
    return sf_binary_writer_fill_i64(w, reserve_name, data_offset);
}
