/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — legacy MSB (32-bit offset) common skeleton (internal API).
 * Used by MSB1, MSBD, MSBB, MSBDR, MSBN.
 * Mirrors MSB1.Param<T>.Read/Write in:
 *   SoulsFormats/Formats/MSB/MSB1/MSB1.cs
 */

#ifndef SF_MAP_MSB_LEGACY_INTERNAL_H
#define SF_MAP_MSB_LEGACY_INTERNAL_H

#include "msb_internal.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A single named legacy param-list. Legacy MSBs use ASCII names and signed
 * 32-bit absolute offsets on disk; data_offset is widened to int64_t only to
 * match stream seek APIs and the modern msb_layout_t shape. */
typedef struct msb_legacy_list_header {
    char   *name;
    int32_t entry_count;
    int64_t data_offset;
} msb_legacy_list_header_t;

/* Results from msb_legacy_read_header — the high-level layout of a legacy
 * MSB's 32-bit param chain. */
typedef struct msb_legacy_layout {
    int32_t                  list_count;
    msb_legacy_list_header_t lists[MSB_MAX_LISTS];
    bool                     big_endian;
    bool                     is64_bit; /* Always false for this helper. */
    int32_t                  version;  /* Param version. MSB1 asserts 0. */
} msb_legacy_layout_t;

/* ──────────────────────────────────────────────────────────────────────
 * Reader side
 * ──────────────────────────────────────────────────────────────────── */

/* Assert and consume the 16-byte MSB magic header. MSB1/MSBD/MSBN do not use
 * this; MSBB/MSBDR do. Mirrors MSB.AssertHeader in upstream. */
sf_result_t msb_legacy_assert_header(sf_binary_reader_t *r);

/* Walk a 32-bit legacy param chain from the reader's current position. This
 * deliberately performs no magic-header assertion because MSB1/MSBD/MSBN begin
 * directly with their first param. Call msb_legacy_assert_header first for
 * formats that carry the "MSB " header.
 *
 * Param layout:
 *   i32 version (assert == 0)
 *   i32 nameOffset
 *   i32 offsetCount = entry_count + 1
 *   i32 entryOffsets[entry_count]
 *   i32 nextParamOffset
 */
sf_result_t msb_legacy_read_header(sf_binary_reader_t     *r,
                                   msb_legacy_layout_t    *out_layout,
                                   const sf_allocator_t   *a);

/* Frees a layout's owned ASCII/UTF-8 `name` strings and clears counters. */
void msb_legacy_free_layout(msb_legacy_layout_t *layout, const sf_allocator_t *a);

/* Iterate the named lists in declaration order, repositioning the reader to
 * each list's entry-offset table and invoking the same callback type used by
 * modern MSB helpers. */
sf_result_t msb_legacy_iter_lists(sf_binary_reader_t       *r,
                                  const msb_legacy_layout_t *layout,
                                  msb_list_cb_t             cb,
                                  void                     *ctx,
                                  const sf_allocator_t     *a);

/* ──────────────────────────────────────────────────────────────────────
 * Writer side
 * ──────────────────────────────────────────────────────────────────── */

/* Writes the 16-byte MSB magic header for legacy variants that include it. */
sf_result_t msb_legacy_write_header(sf_binary_writer_t *w);

/* Write a complete legacy MSB param/list section:
 *
 *   i32  version = 0
 *   i32  nameOffset
 *   i32  offsetCount = entry_count + 1
 *   i32  entryOffsets[entry_count]
 *   i32  nextParamOffset
 *   ASCII type_name, padded to 4
 *   entries, each emitted by write_fn
 *
 * `next_list_key` is the caller-owned reservation name for nextParamOffset;
 * a later caller fills it with the next list's start offset (or zero for the
 * final list), preserving upstream Param<T>.Write chaining. */
sf_result_t msb_legacy_entry_list_write(sf_binary_writer_t *w,
                                        const char         *type_name,
                                        const char         *next_list_key,
                                        const void         *entries,
                                        size_t              entry_count,
                                        size_t              entry_size,
                                        msb_entry_write_fn  write_fn,
                                        void               *ctx);

/* Writes a zero-entry placeholder param/list using 32-bit offsets and an
 * ASCII name. The reservation key is "MsbLegacyNextList<reserve_id>". */
sf_result_t msb_legacy_reserve_list(sf_binary_writer_t *w,
                                    const char         *name,
                                    int                 reserve_id);

/* Backpatch the nextParamOffset slot reserved by msb_legacy_reserve_list.
 * Pass data_offset = 0 to mark the final list. entry_count is reserved for
 * future use and must be 0 in the skeleton phase. */
sf_result_t msb_legacy_fill_list(sf_binary_writer_t *w,
                                 int                 reserve_id,
                                 int32_t             entry_count,
                                 int64_t             data_offset);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSB_LEGACY_INTERNAL_H */
