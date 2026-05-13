/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — BXF3 split-archive container.
 *
 * BXF3 is a general-purpose split header/data binder used before Dark
 * Souls 2 (Demon's Souls, Dark Souls 1, etc.). It comes in two files:
 *
 *   - Header file ("BHF3"): extension .bhd, .*bhd (e.g. .tpfbhd, .chrbhd)
 *   - Data   file ("BDF3"): extension .bdt, .*bdt (e.g. .tpfbdt, .chrbdt)
 *
 * The header file carries everything except the file payloads themselves;
 * the data file carries only the payloads (referenced by `data_offset`
 * fields in the header). Both files share a "version" field which must
 * match.
 *
 * Two complementary entry points:
 *
 *   - sf_bxf3_t: an eager, fully-decoded handle. Reading allocates and
 *     decompresses every entry up front. Writing serialises everything
 *     into a pair of buffers or files. Best for tools that mutate or
 *     rewrite archives.
 *
 *   - sf_bxf3_reader_t: an on-demand handle. Reading parses only the
 *     header (entry list); individual file data is materialised by
 *     sf_bxf3_reader_read_file_*. Best for processes that touch a small
 *     subset of a large archive.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/Binder/BXF3/BXF3.cs
 *   SoulsFormats/Formats/Binder/BXF3/BXF3Reader.cs
 *   SoulsFormats/Formats/Binder/BXF3/IBXF3.cs
 */

#ifndef SOULS_FORMATS_SF_BXF3_H
#define SOULS_FORMATS_SF_BXF3_H

#include "souls_formats/sf_binder.h"
#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* sf_bxf3_t and sf_bxf3_reader_t are forward-declared in sf_binder.h. */

/*===========================================================================
 * Eager API — fully-decoded in-memory archive.
 *
 * Lifetime: every "create"/"read_*" call returns a heap handle that must
 * be released with sf_bxf3_destroy. The handle owns deep copies of every
 * file's name and payload; the caller-supplied sf_binder_file_t buffers
 * passed to sf_bxf3_add_file may be freed immediately after the call
 * returns SF_OK.
 *===========================================================================*/

/** Create an empty BXF3 with default property values that match upstream's
 *  parameterless `new BXF3()` constructor: Files = [], Version = current
 *  binder timestamp, Format = IDs|Names1|Names2|Compression. */
SF_API sf_result_t sf_bxf3_create(sf_bxf3_t **out, const sf_allocator_t *a);

/** Free a BXF3 handle and every buffer it owns. NULL-safe. */
SF_API void sf_bxf3_destroy(sf_bxf3_t *b);

/** Read a BXF3 from a pair of UTF-16 paths (BHD + BDT). DCX wrappers on
 *  either file are unwrapped automatically before parsing. */
SF_API sf_result_t sf_bxf3_read_from_paths(sf_bxf3_t **out,
                                           const wchar_t *bhd_path,
                                           const wchar_t *bdt_path,
                                           const sf_allocator_t *a);

/** Read a BXF3 from a pair of in-memory buffers. Neither buffer is
 *  retained. */
SF_API sf_result_t sf_bxf3_read_from_memory(sf_bxf3_t **out,
                                            const uint8_t *bhd, size_t bhd_size,
                                            const uint8_t *bdt, size_t bdt_size,
                                            const sf_allocator_t *a);

/** Serialise the BXF3 to a pair of UTF-16 paths (BHD + BDT). */
SF_API sf_result_t sf_bxf3_write_to_paths(const sf_bxf3_t *b,
                                          const wchar_t *bhd_path,
                                          const wchar_t *bdt_path);

/** Serialise the BXF3 into a pair of freshly-allocated heap buffers. The
 *  caller owns both `*out_bhd` and `*out_bdt` and must release each via
 *  sf_free with the BXF3's allocator (or any allocator equivalent to the
 *  one passed at create time). */
SF_API sf_result_t sf_bxf3_write_to_memory(const sf_bxf3_t *b,
                                           uint8_t **out_bhd, size_t *out_bhd_size,
                                           uint8_t **out_bdt, size_t *out_bdt_size,
                                           const sf_allocator_t *a);

/** Number of entries currently stored. */
SF_API size_t sf_bxf3_file_count(const sf_bxf3_t *b);

/** Borrow a pointer to the entry at `index`. Returns NULL if out of range.
 *  The pointed-to fields remain owned by the BXF3. */
SF_API const sf_binder_file_t *sf_bxf3_get_file(const sf_bxf3_t *b, size_t index);

/** Append a copy of `file` to the entry list. The BXF3 deep-copies `file`'s
 *  `name_utf8` and `data` buffers; caller buffers may be freed afterwards. */
SF_API sf_result_t sf_bxf3_add_file(sf_bxf3_t *b, const sf_binder_file_t *file);

/** Remove the entry at `index`. Returns SF_ERR_OUT_OF_RANGE if absent. */
SF_API sf_result_t sf_bxf3_remove_file(sf_bxf3_t *b, size_t index);

/* --- Properties --- */

SF_API const char        *sf_bxf3_get_version       (const sf_bxf3_t *b);
SF_API sf_binder_format_t sf_bxf3_get_format        (const sf_bxf3_t *b);
SF_API bool               sf_bxf3_get_big_endian    (const sf_bxf3_t *b);
SF_API bool               sf_bxf3_get_bit_big_endian(const sf_bxf3_t *b);

SF_API void sf_bxf3_set_version       (sf_bxf3_t *b, const char *v);
SF_API void sf_bxf3_set_format        (sf_bxf3_t *b, sf_binder_format_t f);
SF_API void sf_bxf3_set_big_endian    (sf_bxf3_t *b, bool v);
SF_API void sf_bxf3_set_bit_big_endian(sf_bxf3_t *b, bool v);

/*===========================================================================
 * Reader API — lazy/streaming.
 *
 * The reader keeps the entire decompressed BHD + BDT images in memory but
 * materialises individual file payloads only on demand. Header metadata
 * (name, ID, flags, sizes, offsets) is parsed up front from the BHD.
 *===========================================================================*/

/** Open a BXF3 from a pair of UTF-16 paths. DCX wrappers are unwrapped
 *  automatically on both files. */
SF_API sf_result_t sf_bxf3_reader_open(sf_bxf3_reader_t **out,
                                       const wchar_t *bhd_path,
                                       const wchar_t *bdt_path,
                                       const sf_allocator_t *a);

/** Free a reader. NULL-safe. */
SF_API void sf_bxf3_reader_close(sf_bxf3_reader_t *r);

/** Number of files in the archive. */
SF_API size_t sf_bxf3_reader_file_count(const sf_bxf3_reader_t *r);

/** Read the payload of the entry at `idx`. The buffer is heap-allocated
 *  with the supplied allocator (or the reader's default if NULL); the
 *  caller owns it and must release it via sf_free. */
SF_API sf_result_t sf_bxf3_reader_read_file_by_index(sf_bxf3_reader_t *r, size_t idx,
                                                     uint8_t **out, size_t *out_size,
                                                     const sf_allocator_t *a);

/** Read the payload of the first entry whose `id` matches. Returns
 *  SF_ERR_NOT_FOUND if no such entry exists. */
SF_API sf_result_t sf_bxf3_reader_read_file_by_id(sf_bxf3_reader_t *r, int32_t id,
                                                  uint8_t **out, size_t *out_size,
                                                  const sf_allocator_t *a);

/** Borrow a pointer to the parsed header for entry `idx`. The pointer
 *  remains valid until sf_bxf3_reader_close. */
SF_API const sf_binder_file_t *sf_bxf3_reader_get_file(const sf_bxf3_reader_t *r,
                                                       size_t idx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_BXF3_H */
