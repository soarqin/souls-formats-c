/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — BXF4 split-archive container.
 *
 * BXF4 is the modern split header/data binder used since Dark Souls 2 and
 * is the primary split-archive format for Dark Souls 3, Sekiro, Elden Ring,
 * and AC6. It comes in two files:
 *
 *   - Header file ("BHF4"): extension .bhd, .*bhd (e.g. .tpfbhd, .chrbhd)
 *   - Data   file ("BDF4"): extension .bdt, .*bdt (e.g. .tpfbdt, .chrbdt)
 *
 * Compared to BXF3 the BHD side carries BND4-style fields (Unicode flag,
 * Extended-byte filename hash table, Unk04/Unk05 booleans). Two
 * complementary entry points:
 *
 *   - sf_bxf4_t: an eager, fully-decoded handle. Reading allocates and
 *     decompresses every entry up front. Writing serialises everything
 *     into a pair of buffers or files. Best for tools that mutate or
 *     rewrite archives.
 *
 *   - sf_bxf4_reader_t: an on-demand handle. Reading parses only the
 *     header (entry list); individual file data is materialised by
 *     sf_bxf4_reader_read_file_*. Best for processes that touch a small
 *     subset of a large archive.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/Binder/BXF4/BXF4.cs
 *   SoulsFormats/Formats/Binder/BXF4/BXF4Reader.cs
 *   SoulsFormats/Formats/Binder/BXF4/IBXF4.cs
 */

#ifndef SOULS_FORMATS_SF_BXF4_H
#define SOULS_FORMATS_SF_BXF4_H

#include "souls_formats/sf_binder.h"
#include "sf_common.h"
#include "souls_formats/sf_dcx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* sf_bxf4_t and sf_bxf4_reader_t are forward-declared in sf_binder.h. */

/*===========================================================================
 * Eager API — fully-decoded in-memory archive.
 *
 * Lifetime: every "create"/"read_*" call returns a heap handle that must
 * be released with sf_bxf4_destroy. The handle owns deep copies of every
 * file's name and payload; the caller-supplied sf_binder_file_t buffers
 * passed to sf_bxf4_add_file may be freed immediately after the call
 * returns SF_OK.
 *===========================================================================*/

/** Create an empty BXF4 with default property values that match upstream's
 *  parameterless `new BXF4()` constructor: Files = [], Version = current
 *  binder timestamp, Format = IDs|Names1|Names2|Compression, Unicode = true,
 *  Extended = 4. */
SF_API sf_result_t sf_bxf4_create(sf_bxf4_t **out, const sf_allocator_t *a);

/** Free a BXF4 handle and every buffer it owns. NULL-safe. */
SF_API void sf_bxf4_destroy(sf_bxf4_t *b);

/** Read a BXF4 from a pair of UTF-16 paths (BHD + BDT). DCX wrappers on
 *  either file are unwrapped automatically before parsing. */
SF_API sf_result_t sf_bxf4_read_from_paths(sf_bxf4_t **out,
                                           const wchar_t *bhd_path,
                                           const wchar_t *bdt_path,
                                           const sf_allocator_t *a);

/** Read a BXF4 from a pair of in-memory buffers. Neither buffer is
 *  retained. */
SF_API sf_result_t sf_bxf4_read_from_memory(sf_bxf4_t **out,
                                            const uint8_t *bhd, size_t bhd_size,
                                            const uint8_t *bdt, size_t bdt_size,
                                            const sf_allocator_t *a);

/** Serialise the BXF4 to a pair of UTF-16 paths (BHD + BDT). */
SF_API sf_result_t sf_bxf4_write_to_paths(const sf_bxf4_t *b,
                                          const wchar_t *bhd_path,
                                          const wchar_t *bdt_path);

/** Serialise the BXF4 into a pair of freshly-allocated heap buffers. The
 *  caller owns both `*out_bhd` and `*out_bdt` and must release each via
 *  sf_free with the BXF4's allocator (or any allocator equivalent to the
 *  one passed at create time). */
SF_API sf_result_t sf_bxf4_write_to_memory(const sf_bxf4_t *b,
                                           uint8_t **out_bhd, size_t *out_bhd_size,
                                           uint8_t **out_bdt, size_t *out_bdt_size,
                                           const sf_allocator_t *a);

/** Number of entries currently stored. */
SF_API size_t sf_bxf4_file_count(const sf_bxf4_t *b);

/** Borrow a pointer to the entry at `index`. Returns NULL if out of range.
 *  The pointed-to fields remain owned by the BXF4. */
SF_API const sf_binder_file_t *sf_bxf4_get_file(const sf_bxf4_t *b, size_t index);

/** Append a deep copy of `file` to the entry list. */
SF_API sf_result_t sf_bxf4_add_file(sf_bxf4_t *b, const sf_binder_file_t *file);

/** Remove the entry at `index`. Returns SF_ERR_OUT_OF_RANGE if absent. */
SF_API sf_result_t sf_bxf4_remove_file(sf_bxf4_t *b, size_t index);

/** Linear scan by FromPathHash over entry names. */
SF_API sf_result_t sf_bxf4_find_by_path_hash(const sf_bxf4_t *b, uint32_t hash,
                                             size_t *index_out);

/* --- Properties --- */

SF_API const char        *sf_bxf4_get_version(const sf_bxf4_t *b);
SF_API sf_binder_format_t sf_bxf4_get_format(const sf_bxf4_t *b);
SF_API bool               sf_bxf4_get_big_endian(const sf_bxf4_t *b);
SF_API bool               sf_bxf4_get_bit_big_endian(const sf_bxf4_t *b);
SF_API bool               sf_bxf4_get_unicode(const sf_bxf4_t *b);
SF_API uint8_t            sf_bxf4_get_extended(const sf_bxf4_t *b);
SF_API bool               sf_bxf4_get_unk04(const sf_bxf4_t *b);
SF_API bool               sf_bxf4_get_unk05(const sf_bxf4_t *b);

SF_API void sf_bxf4_set_version(sf_bxf4_t *b, const char *v);
SF_API void sf_bxf4_set_format(sf_bxf4_t *b, sf_binder_format_t f);
SF_API void sf_bxf4_set_big_endian(sf_bxf4_t *b, bool v);
SF_API void sf_bxf4_set_bit_big_endian(sf_bxf4_t *b, bool v);
SF_API void sf_bxf4_set_unicode(sf_bxf4_t *b, bool v);
SF_API void sf_bxf4_set_extended(sf_bxf4_t *b, uint8_t v);
SF_API void sf_bxf4_set_unk04(sf_bxf4_t *b, bool v);
SF_API void sf_bxf4_set_unk05(sf_bxf4_t *b, bool v);

/*===========================================================================
 * Reader API — lazy/streaming.
 *
 * The reader keeps the entire decompressed BHD + BDT images in memory but
 * materialises individual file payloads only on demand. Header metadata
 * (name, ID, flags, sizes, offsets) is parsed up front from the BHD.
 *===========================================================================*/

/** Open a BXF4 from a pair of UTF-16 paths. DCX wrappers are unwrapped
 *  automatically on both files. */
SF_API sf_result_t sf_bxf4_reader_open(sf_bxf4_reader_t **out,
                                       const wchar_t *bhd_path,
                                       const wchar_t *bdt_path,
                                       const sf_allocator_t *a);

/** Free a reader. NULL-safe. */
SF_API void sf_bxf4_reader_close(sf_bxf4_reader_t *r);

/** Number of files in the archive. */
SF_API size_t sf_bxf4_reader_file_count(const sf_bxf4_reader_t *r);

/** Borrow a pointer to the parsed header for entry `idx`. */
SF_API const sf_binder_file_t *sf_bxf4_reader_get_file(const sf_bxf4_reader_t *r,
                                                       size_t idx);

/** Outer DCX compression info captured for the BHD while opening the
 *  reader (matches upstream BinderReader.DataBR conventions). */
SF_API sf_dcx_compression_info_t sf_bxf4_reader_get_outer_compression(
    const sf_bxf4_reader_t *r);

/** Read the payload of the entry at `idx`. Caller owns `*out`. */
SF_API sf_result_t sf_bxf4_reader_read_file_by_index(sf_bxf4_reader_t *r, size_t idx,
                                                     uint8_t **out, size_t *out_size,
                                                     const sf_allocator_t *a);

/** Read the payload of the first entry whose `id` matches. */
SF_API sf_result_t sf_bxf4_reader_read_file_by_id(sf_bxf4_reader_t *r, int32_t id,
                                                  uint8_t **out, size_t *out_size,
                                                  const sf_allocator_t *a);

/** Read the payload of the first entry whose FromPathHash(name) matches. */
SF_API sf_result_t sf_bxf4_reader_read_file_by_path_hash(sf_bxf4_reader_t *r,
                                                         uint32_t path_hash,
                                                         uint8_t **out,
                                                         size_t *out_size,
                                                         const sf_allocator_t *a);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_BXF4_H */
