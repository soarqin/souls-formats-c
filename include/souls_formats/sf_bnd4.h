/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — BND4 archive container.
 *
 * BND4 is the general-purpose binder used since Dark Souls 2 and is the
 * primary archive container for Dark Souls 3, Sekiro, Elden Ring, and AC6.
 * Extension: .*bnd, commonly wrapped as .*bnd.dcx.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/Binder/BND4/BND4.cs
 *   SoulsFormats/Formats/Binder/BND4/BND4Reader.cs
 *   SoulsFormats/Formats/Binder/BND4/IBND4.cs
 */

#ifndef SOULS_FORMATS_SF_BND4_H
#define SOULS_FORMATS_SF_BND4_H

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* sf_bnd4_t and sf_bnd4_reader_t are forward-declared in sf_binder.h. */

/*===========================================================================
 * Eager API — fully-decoded in-memory archive.
 *===========================================================================*/

/** Create an empty BND4 with upstream constructor defaults: Files = [],
 *  Version = current binder timestamp, Format = IDs|Names1|Names2|Compression,
 *  Unicode = true, Extended = 4. */
SF_API sf_result_t sf_bnd4_create(sf_bnd4_t **out, const sf_allocator_t *a);

/** Free a BND4 handle and every buffer it owns. NULL-safe. */
SF_API void sf_bnd4_destroy(sf_bnd4_t *b);

/** Read a BND4 from a UTF-16 path. DCX wrappers are unwrapped automatically. */
SF_API sf_result_t sf_bnd4_read_from_path(sf_bnd4_t **out, const wchar_t *path,
                                          const sf_allocator_t *a);

/** Read a BND4 from an in-memory buffer. The buffer is not retained. */
SF_API sf_result_t sf_bnd4_read_from_memory(sf_bnd4_t **out, const uint8_t *data,
                                            size_t size, const sf_allocator_t *a);

/** Serialise the BND4 to a UTF-16 path. */
SF_API sf_result_t sf_bnd4_write_to_path(const sf_bnd4_t *b, const wchar_t *path);

/** Serialise the BND4 into a freshly-allocated heap buffer. */
SF_API sf_result_t sf_bnd4_write_to_memory(const sf_bnd4_t *b, uint8_t **out,
                                           size_t *out_size,
                                           const sf_allocator_t *a);

/** Number of entries currently stored. */
SF_API size_t sf_bnd4_file_count(const sf_bnd4_t *b);

/** Borrow a pointer to the entry at `index`. Returns NULL if out of range. */
SF_API const sf_binder_file_t *sf_bnd4_get_file(const sf_bnd4_t *b, size_t index);

/** Append a deep copy of `file` to the entry list. */
SF_API sf_result_t sf_bnd4_add_file(sf_bnd4_t *b, const sf_binder_file_t *file);

/** Remove the entry at `index`. Returns SF_ERR_OUT_OF_RANGE if absent. */
SF_API sf_result_t sf_bnd4_remove_file(sf_bnd4_t *b, size_t index);

/** Linear scan by FromPathHash over entry names. */
SF_API sf_result_t sf_bnd4_find_by_path_hash(const sf_bnd4_t *b, uint32_t hash,
                                             size_t *index_out);

/* --- Properties --- */

SF_API const char        *sf_bnd4_get_version(const sf_bnd4_t *b);
SF_API sf_binder_format_t sf_bnd4_get_format(const sf_bnd4_t *b);
SF_API bool               sf_bnd4_get_big_endian(const sf_bnd4_t *b);
SF_API bool               sf_bnd4_get_bit_big_endian(const sf_bnd4_t *b);
SF_API bool               sf_bnd4_get_unicode(const sf_bnd4_t *b);
SF_API uint8_t            sf_bnd4_get_extended(const sf_bnd4_t *b);
SF_API bool               sf_bnd4_get_unk04(const sf_bnd4_t *b);
SF_API bool               sf_bnd4_get_unk05(const sf_bnd4_t *b);

SF_API void sf_bnd4_set_version(sf_bnd4_t *b, const char *v);
SF_API void sf_bnd4_set_format(sf_bnd4_t *b, sf_binder_format_t f);
SF_API void sf_bnd4_set_big_endian(sf_bnd4_t *b, bool v);
SF_API void sf_bnd4_set_bit_big_endian(sf_bnd4_t *b, bool v);
SF_API void sf_bnd4_set_unicode(sf_bnd4_t *b, bool v);
SF_API void sf_bnd4_set_extended(sf_bnd4_t *b, uint8_t v);
SF_API void sf_bnd4_set_unk04(sf_bnd4_t *b, bool v);
SF_API void sf_bnd4_set_unk05(sf_bnd4_t *b, bool v);

/*===========================================================================
 * Reader API — lazy/streaming.
 *===========================================================================*/

/** Open a BND4 from a UTF-16 path. DCX wrappers are unwrapped automatically. */
SF_API sf_result_t sf_bnd4_reader_open(sf_bnd4_reader_t **out, const wchar_t *path,
                                       const sf_allocator_t *a);

/** Free a reader. NULL-safe. */
SF_API void sf_bnd4_reader_close(sf_bnd4_reader_t *r);

/** Number of files in the archive. */
SF_API size_t sf_bnd4_reader_file_count(const sf_bnd4_reader_t *r);

/** Borrow a pointer to the parsed header for entry `idx`. */
SF_API const sf_binder_file_t *sf_bnd4_reader_get_file(const sf_bnd4_reader_t *r,
                                                       size_t idx);

/** Outer DCX compression info captured while opening the reader. */
SF_API sf_dcx_compression_info_t sf_bnd4_reader_get_outer_compression(
    const sf_bnd4_reader_t *r);

/** Read the payload of the entry at `idx`. Caller owns `*out`. */
SF_API sf_result_t sf_bnd4_reader_read_file_by_index(sf_bnd4_reader_t *r, size_t idx,
                                                     uint8_t **out, size_t *out_size,
                                                     const sf_allocator_t *a);

/** Read the payload of the first entry whose `id` matches. */
SF_API sf_result_t sf_bnd4_reader_read_file_by_id(sf_bnd4_reader_t *r, int32_t id,
                                                  uint8_t **out, size_t *out_size,
                                                  const sf_allocator_t *a);

/** Read the payload of the first entry whose FromPathHash(name) matches. */
SF_API sf_result_t sf_bnd4_reader_read_file_by_path_hash(sf_bnd4_reader_t *r,
                                                         uint32_t path_hash,
                                                         uint8_t **out,
                                                         size_t *out_size,
                                                         const sf_allocator_t *a);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_BND4_H */
