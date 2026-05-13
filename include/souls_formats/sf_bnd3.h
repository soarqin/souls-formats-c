/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — BND3 archive container.
 *
 * The BND3 format is the general-purpose binder used before Dark Souls 2
 * (Demon's Souls, Dark Souls 1, etc.). Extension: .*bnd
 *
 * Two complementary entry points:
 *
 *   - sf_bnd3_t: an eager, fully-decoded handle. Reading allocates and
 *     decompresses every entry up front. Writing serialises everything
 *     into one buffer or one file. Best for tools that mutate or rewrite
 *     archives.
 *
 *   - sf_bnd3_reader_t: an on-demand handle. Reading parses only the
 *     header (entry list); individual file data is materialised by
 *     sf_bnd3_reader_read_file_*. Best for processes that touch a small
 *     subset of a large archive.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/Binder/BND3/BND3.cs
 *   SoulsFormats/Formats/Binder/BND3/BND3Reader.cs
 *   SoulsFormats/Formats/Binder/BND3/IBND3.cs
 */

#ifndef SOULS_FORMATS_SF_BND3_H
#define SOULS_FORMATS_SF_BND3_H

#include "souls_formats/sf_binder.h"
#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* sf_bnd3_t and sf_bnd3_reader_t are forward-declared in sf_binder.h. */

/*===========================================================================
 * Eager API — fully-decoded in-memory archive.
 *
 * Lifetime: every "create"/"read_*" call returns a heap handle that must
 * be released with sf_bnd3_destroy. The handle owns deep copies of every
 * file's name and payload; the caller-supplied sf_binder_file_t buffers
 * passed to sf_bnd3_add_file may be freed immediately after the call
 * returns SF_OK.
 *===========================================================================*/

/** Create an empty BND3 with default property values that match upstream's
 *  parameterless `new BND3()` constructor: Files = [], Version = current
 *  binder timestamp, Format = IDs|Names1|Names2|Compression. */
SF_API sf_result_t sf_bnd3_create(sf_bnd3_t **out, const sf_allocator_t *a);

/** Free a BND3 handle and every buffer it owns. NULL-safe. */
SF_API void sf_bnd3_destroy(sf_bnd3_t *b);

/** Read a BND3 from a UTF-16 path. If the file is wrapped in DCX, it is
 *  unwrapped automatically before parsing. */
SF_API sf_result_t sf_bnd3_read_from_path(sf_bnd3_t **out, const wchar_t *path,
                                          const sf_allocator_t *a);

/** Read a BND3 from an in-memory buffer. The buffer is not retained. */
SF_API sf_result_t sf_bnd3_read_from_memory(sf_bnd3_t **out, const uint8_t *data,
                                            size_t size, const sf_allocator_t *a);

/** Serialise the BND3 to a UTF-16 path. */
SF_API sf_result_t sf_bnd3_write_to_path(const sf_bnd3_t *b, const wchar_t *path);

/** Serialise the BND3 into a freshly-allocated heap buffer. The caller
 *  owns `*out` and must release it via sf_free with the BND3's allocator
 *  (or any allocator equivalent to the one passed at create time). */
SF_API sf_result_t sf_bnd3_write_to_memory(const sf_bnd3_t *b, uint8_t **out,
                                           size_t *out_size,
                                           const sf_allocator_t *a);

/** Number of entries currently stored. */
SF_API size_t sf_bnd3_file_count(const sf_bnd3_t *b);

/** Borrow a pointer to the entry at `index`. Returns NULL if out of range.
 *  The pointed-to fields remain owned by the BND3. */
SF_API const sf_binder_file_t *sf_bnd3_get_file(const sf_bnd3_t *b, size_t index);

/** Append a copy of `file` to the entry list. The BND3 deep-copies `file`'s
 *  `name_utf8` and `data` buffers; caller buffers may be freed afterwards. */
SF_API sf_result_t sf_bnd3_add_file(sf_bnd3_t *b, const sf_binder_file_t *file);

/** Remove the entry at `index`. Returns SF_ERR_OUT_OF_RANGE if absent. */
SF_API sf_result_t sf_bnd3_remove_file(sf_bnd3_t *b, size_t index);

/* --- Properties --- */

SF_API const char        *sf_bnd3_get_version(const sf_bnd3_t *b);
SF_API sf_binder_format_t sf_bnd3_get_format(const sf_bnd3_t *b);
SF_API bool               sf_bnd3_get_big_endian(const sf_bnd3_t *b);
SF_API bool               sf_bnd3_get_bit_big_endian(const sf_bnd3_t *b);
SF_API int32_t            sf_bnd3_get_unk18(const sf_bnd3_t *b);
SF_API bool               sf_bnd3_get_write_file_headers_end(const sf_bnd3_t *b);

SF_API void sf_bnd3_set_version(sf_bnd3_t *b, const char *v);
SF_API void sf_bnd3_set_format(sf_bnd3_t *b, sf_binder_format_t f);
SF_API void sf_bnd3_set_big_endian(sf_bnd3_t *b, bool v);
SF_API void sf_bnd3_set_bit_big_endian(sf_bnd3_t *b, bool v);
SF_API void sf_bnd3_set_unk18(sf_bnd3_t *b, int32_t v);
SF_API void sf_bnd3_set_write_file_headers_end(sf_bnd3_t *b, bool v);

/*===========================================================================
 * Reader API — lazy/streaming.
 *
 * The reader keeps the entire decompressed image in memory but materialises
 * individual file payloads only on demand. Header metadata (name, ID,
 * flags, sizes, offsets) is parsed up front.
 *===========================================================================*/

/** Open a BND3 from a UTF-16 path. DCX wrappers are unwrapped automatically. */
SF_API sf_result_t sf_bnd3_reader_open(sf_bnd3_reader_t **out, const wchar_t *path,
                                       const sf_allocator_t *a);

/** Free a reader. NULL-safe. */
SF_API void sf_bnd3_reader_close(sf_bnd3_reader_t *r);

/** Number of files in the archive. */
SF_API size_t sf_bnd3_reader_file_count(const sf_bnd3_reader_t *r);

/** Read the payload of the entry at `idx`. The buffer is heap-allocated
 *  with the reader's allocator; the caller owns it and must release it
 *  via sf_free. */
SF_API sf_result_t sf_bnd3_reader_read_file_by_index(sf_bnd3_reader_t *r, size_t idx,
                                                     uint8_t **out, size_t *out_size,
                                                     const sf_allocator_t *a);

/** Read the payload of the first entry whose `id` matches. Returns
 *  SF_ERR_NOT_FOUND if no such entry exists. */
SF_API sf_result_t sf_bnd3_reader_read_file_by_id(sf_bnd3_reader_t *r, int32_t id,
                                                  uint8_t **out, size_t *out_size,
                                                  const sf_allocator_t *a);

/** Borrow a pointer to the parsed header for entry `idx`. The pointer
 *  remains valid until sf_bnd3_reader_close. */
SF_API const sf_binder_file_t *sf_bnd3_reader_get_file(const sf_bnd3_reader_t *r,
                                                       size_t idx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_BND3_H */
