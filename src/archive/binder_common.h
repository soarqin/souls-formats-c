/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — internal helpers shared across BND3/BND4/BXF3/BXF4.
 *
 * This header is INTERNAL. It MUST NOT be installed and MUST NOT be
 * included from any public header under include/souls_formats/. The
 * functions declared here are not exported from the shared library and
 * carry no SF_API decoration.
 *
 * Upstream references (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/Binder/Binder.cs
 *   SoulsFormats/Formats/Binder/BinderFileHeader.cs
 *   SoulsFormats/Formats/Binder/BinderHashTable.cs
 */

#ifndef SF_ARCHIVE_BINDER_COMMON_H
#define SF_ARCHIVE_BINDER_COMMON_H

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * sfi_binder_file_header_t
 *
 * Mirrors upstream BinderFileHeader (BinderFileHeader.cs:8). This is the
 * "raw" per-entry record used DURING serialization: it carries the on-disk
 * sizes and offsets that the public sf_binder_file_t (which models the
 * decompressed user view) intentionally hides.
 *
 * Lifetime: `name_utf8` is heap-allocated by sfi_binder3_read_file_header /
 * sfi_binder4_read_file_header using the caller-supplied allocator and
 * must be released via sfi_binder_file_header_destroy(..., a) using the
 * same allocator.
 *
 * Sizes/offsets are widened to uint64_t internally so a single struct can
 * carry both BND3 (Int32) and BND4 (Int64, possibly 32-bit DataOffset
 * when LongOffsets is unset) values without truncation.
 *===========================================================================*/
typedef struct sfi_binder_file_header {
    sf_binder_file_flags_t    flags;             /* per-entry feature byte             */
    int32_t                   id;                /* upstream ID; -1 means "no ID"      */
    char                     *name_utf8;         /* NULL if no name; heap-owned        */
    uint64_t                  compressed_size;   /* on-disk size (compressed if any)   */
    uint64_t                  uncompressed_size; /* logical size (raw payload)         */
    uint64_t                  data_offset;       /* absolute file offset of the data   */
    sf_dcx_compression_info_t compression_info;  /* preset for round-trip writing      */
} sfi_binder_file_header_t;

/*===========================================================================
 * Format byte read/write
 *
 * Mirrors Binder.ReadFormat / Binder.WriteFormat (Binder.cs:68-83). The
 * on-disk byte may have its bits in reversed order; the helper uses the
 * caller-supplied bit_big_endian flag (the same flag stored at the top
 * of every BND/BXF header) plus a content heuristic on the read path.
 *===========================================================================*/

/** Read one Format byte and apply bit reversal as needed. */
sf_binder_format_t     sfi_binder_read_format    (sf_binary_reader_t *br,
                                                  bool                bit_big_endian);

/** Write one Format byte and apply bit reversal as needed. */
void                   sfi_binder_write_format   (sf_binary_writer_t *bw,
                                                  sf_binder_format_t  f,
                                                  bool                bit_big_endian);

/** Read a FileFlags byte; reversed iff `bit_big_endian` is false. */
sf_binder_file_flags_t sfi_binder_read_file_flags(sf_binary_reader_t *br,
                                                  bool bit_big_endian);

/** Write a FileFlags byte; reversed iff `bit_big_endian` is false. */
void sfi_binder_write_file_flags(sf_binary_writer_t   *bw,
                                 sf_binder_file_flags_t flags,
                                 bool                   bit_big_endian);

/*===========================================================================
 * BND3-style file header (32-bit sizes/offsets)
 *
 * Mirrors BinderFileHeader.ReadBinder3FileHeader and the matching write
 * helpers. The bit_big_endian flag is implicit in the binder header that
 * was read just before — the BND3 reader passes it through.
 *===========================================================================*/

sf_result_t sfi_binder3_read_file_header (sf_binary_reader_t        *br,
                                          sf_binder_format_t         f,
                                          bool                       bit_big_endian,
                                          sfi_binder_file_header_t  *out,
                                          const sf_allocator_t      *a);

sf_result_t sfi_binder3_write_file_header(sf_binary_writer_t              *bw,
                                          sf_binder_format_t               f,
                                          bool                             bit_big_endian,
                                          const sfi_binder_file_header_t  *h,
                                          size_t                           entry_index);

/** Compress (if needed), pad, and write file data; fills the size and
 *  offset reservations created by sfi_binder3_write_file_header. The raw
 *  buffer's length MUST equal h->uncompressed_size. */
sf_result_t sfi_binder3_write_file_data  (sf_binary_writer_t              *bw,
                                          sf_binder_format_t               f,
                                          const sfi_binder_file_header_t  *h,
                                          const uint8_t                   *raw,
                                          size_t                           entry_index);

sf_result_t sfi_binder3_write_file_name  (sf_binary_writer_t              *bw,
                                          sf_binder_format_t               f,
                                          const sfi_binder_file_header_t  *h,
                                          size_t                           entry_index);

/*===========================================================================
 * BND4-style file header (64-bit sizes; offset is 64-bit when LongOffsets)
 *
 * Mirrors BinderFileHeader.ReadBinder4FileHeader / WriteBinder4FileHeader.
 * `unicode` selects UTF-16LE name encoding when true, Shift-JIS otherwise.
 *===========================================================================*/

sf_result_t sfi_binder4_read_file_header (sf_binary_reader_t        *br,
                                          sf_binder_format_t         f,
                                          bool                       bit_big_endian,
                                          bool                       unicode,
                                          sfi_binder_file_header_t  *out,
                                          const sf_allocator_t      *a);

sf_result_t sfi_binder4_write_file_header(sf_binary_writer_t              *bw,
                                          sf_binder_format_t               f,
                                          bool                             bit_big_endian,
                                          const sfi_binder_file_header_t  *h,
                                          size_t                           entry_index);

sf_result_t sfi_binder4_write_file_data  (sf_binary_writer_t              *bw,
                                          sf_binder_format_t               f,
                                          const sfi_binder_file_header_t  *h,
                                          const uint8_t                   *raw,
                                          size_t                           entry_index);

sf_result_t sfi_binder4_write_file_name  (sf_binary_writer_t              *bw,
                                          sf_binder_format_t               f,
                                          bool                             unicode,
                                          const sfi_binder_file_header_t  *h,
                                          size_t                           entry_index);

/*===========================================================================
 * BND4/BXF4 hash table
 *
 * Mirrors BinderHashTable.Assert / BinderHashTable.Write. The assert path
 * does not validate hash content (upstream comment: "Don't actually care
 * about the hashes, I just like asserting"); only the layout markers.
 *===========================================================================*/

sf_result_t sfi_binder_hash_table_assert(sf_binary_reader_t *br,
                                         size_t              file_count);

sf_result_t sfi_binder_hash_table_write (sf_binary_writer_t       *bw,
                                         const sf_binder_file_t   *files,
                                         size_t                    file_count,
                                         const sf_allocator_t     *a);

/*===========================================================================
 * Misc helpers
 *===========================================================================*/

/** Size in bytes of a single BND4 file header entry under format `f`.
 *  Mirrors Binder.GetBND4FileHeaderSize (Binder.cs:123-131). */
size_t sfi_binder_get_bnd4_file_header_size(sf_binder_format_t f);

/** Free the heap-owned `name_utf8` field. Safe on a header that has been
 *  zero-initialised but not yet read. NULL-safe on `h`. */
void   sfi_binder_file_header_destroy(sfi_binder_file_header_t *h,
                                      const sf_allocator_t      *a);

/** Smallest prime p such that p >= ⌊file_count / 7⌋ and p <= 100000.
 *  Mirrors the loop at BinderHashTable.cs:23-30 verbatim. Returns 0 if
 *  no prime in [floor(file_count/7), 100000] exists (upstream throws). */
uint32_t sfi_binder_hash_table_group_count(size_t file_count);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SF_ARCHIVE_BINDER_COMMON_H */
