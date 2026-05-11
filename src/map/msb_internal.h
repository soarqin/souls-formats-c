/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — common MSB list-of-lists skeleton (internal API).
 *
 * NEVER include this from public headers under include/souls_formats/.
 * The skeleton describes the shared infrastructure used by every MSB
 * variant (MSBS / MSBE / MSBVI). Variant-specific entry types live in
 * each variant's source file and are not part of this header.
 *
 * Mirrors the static helpers and Param<T> base class found in:
 *   SoulsFormats/Formats/MSB/MSB.cs              (AssertHeader / WriteHeader)
 *   SoulsFormats/Formats/MSB/MSBS/MSBS.cs        (Param<T>.Read / .Write)
 *   SoulsFormats/Formats/MSB/MSBE/MSBE.cs        (same pattern)
 *   SoulsFormats/Formats/MSB/MSBVI/MSBVI.cs      (same pattern)
 */

#ifndef SF_MAP_MSB_INTERNAL_H
#define SF_MAP_MSB_INTERNAL_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Upper bound on named param-lists in any single MSB.
 * MSBS (Sekiro) defines 7, MSBE (Elden Ring) 8, MSBVI (AC6) 7+, so 16 is
 * a comfortable ceiling with room for unused params. */
#define MSB_MAX_LISTS 16

/* A single named param-list (a.k.a. "Param" in upstream) inside an MSB.
 *
 * `name`        — heap-owned UTF-8 string. Caller frees via sf_free().
 * `entry_count` — number of entries in this list (== upstream
 *                 `offsetCount - 1`).
 * `data_offset` — absolute byte offset where the param's entry-offsets
 *                 table starts (i.e. immediately after
 *                 [version, offsetCount, nameOffset]).
 *                 For a zero-entry list this is the position of the
 *                 nextParamOffset field.
 */
typedef struct msb_list_header {
    char   *name;
    int32_t entry_count;
    int64_t data_offset;
} msb_list_header_t;

/* Results from msb_common_read_header — the high-level layout of an
 * MSB's list-of-lists. */
typedef struct msb_layout {
    int32_t           list_count;
    msb_list_header_t lists[MSB_MAX_LISTS];
    bool              big_endian;
    bool              is64_bit; /* True for Sekiro/ER/AC6: 64-bit offsets. */
    int32_t           version;  /* MSB header version. Always 1 for v1 formats. */
} msb_layout_t;

/* ──────────────────────────────────────────────────────────────────────
 * Reader side
 * ──────────────────────────────────────────────────────────────────── */

/* Assert and consume the 16-byte MSB magic header
 * ("MSB ", i32 1, i32 0x10, false, false, byte 1, byte 0xFF).
 * Mirrors MSB.AssertHeader in upstream. */
sf_result_t msb_common_assert_header(sf_binary_reader_t *r);

/* Read the MSB header, then walk the param chain (linked via each param's
 * trailing nextParamOffset) and record (name, entry_count, data_offset)
 * for every list.
 *
 * On success: out_layout owns the heap-allocated `name` of each list;
 * call msb_common_free_layout to release them.
 *
 * Errors:
 *   SF_ERR_INVALID_ARG          — null arguments
 *   SF_ERR_BAD_MAGIC            — magic bytes are not "MSB "
 *   SF_ERR_UNSUPPORTED_VERSION  — header version mismatch
 *   SF_ERR_OUT_OF_RANGE         — list count exceeds MSB_MAX_LISTS, or a
 *                                 param reports an impossible offsetCount.
 *   SF_ERR_TRUNCATED            — stream too short.
 */
sf_result_t msb_common_read_header(sf_binary_reader_t *r,
                                   msb_layout_t        *out_layout,
                                   const sf_allocator_t *a);

/* Frees a layout's owned `name` strings and clears its counters. NULL-safe. */
void msb_common_free_layout(msb_layout_t *layout, const sf_allocator_t *a);

/* Callback invoked for each list during msb_common_iter_lists.
 *   name        — UTF-8 list name (owned by layout — do not free).
 *   entry_count — number of entries this list carries.
 *   r           — reader positioned at the start of the param's
 *                 entry-offsets table (data_offset).
 *   ctx         — opaque caller-supplied pointer.
 *
 * Returns SF_OK to continue; any non-zero result aborts iteration and is
 * propagated back to the caller of msb_common_iter_lists.
 */
typedef sf_result_t (*msb_list_cb_t)(const char         *name,
                                      int32_t             entry_count,
                                      sf_binary_reader_t *r,
                                      void               *ctx);

/* Iterate the named lists in declaration order, repositioning the reader
 * to each list's data_offset and invoking cb. The reader position after
 * the function returns is undefined; callers must reposition.
 */
sf_result_t msb_common_iter_lists(sf_binary_reader_t   *r,
                                  const msb_layout_t   *layout,
                                  msb_list_cb_t         cb,
                                  void                 *ctx,
                                  const sf_allocator_t *a);

/* ──────────────────────────────────────────────────────────────────────
 * Writer side
 * ──────────────────────────────────────────────────────────────────── */

/* Writes the 16-byte MSB magic header. Mirrors MSB.WriteHeader. */
sf_result_t msb_common_write_header(sf_binary_writer_t *w);

/* Writes a placeholder param/list header for a zero-entry list with the
 * given UTF-16 name. The internal layout written is:
 *
 *   i32  version         = 0
 *   i32  offsetCount     = 1   (== entry_count + 1)
 *   i64  nameOffset      (filled here, points to UTF-16 name below)
 *   i64  nextParamOffset (RESERVED — backpatch with msb_common_fill_list)
 *
 * Then the UTF-16 name is appended and padded to 8-byte alignment.
 *
 * `reserve_id` must be unique among un-filled reservations. The function
 * derives its reservation key as "MsbNextList<reserve_id>".
 */
sf_result_t msb_common_reserve_list(sf_binary_writer_t *w,
                                    const char         *name,
                                    int                 reserve_id);

/* Backpatch the nextParamOffset slot reserved by msb_common_reserve_list.
 * Pass data_offset = 0 to mark the final list. entry_count is reserved
 * for future use (currently unused) and must be 0 in the skeleton phase.
 */
sf_result_t msb_common_fill_list(sf_binary_writer_t *w,
                                 int                 reserve_id,
                                 int32_t             entry_count,
                                 int64_t             data_offset);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSB_INTERNAL_H */
