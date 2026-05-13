/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — FSDATA public surface.
 *
 * A simplistic data archive format with a fixed-size entry table.
 * Files are addressed by integer ID (their index in the entry table).
 * Optionally zlib-compressed.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/FSDATA.cs
 */

#ifndef SOULS_FORMATS_SF_FSDATA_H
#define SOULS_FORMATS_SF_FSDATA_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_fsdata sf_fsdata_t;

#define SF_FSDATA_SECTOR_SIZE    0x800u
#define SF_FSDATA_ALIGNMENT_SIZE 0x8000u
#define SF_FSDATA_DEFAULT_ENTRY_COUNT 8192

SF_API sf_result_t sf_fsdata_create(sf_fsdata_t **out, int entry_count, bool compressed,
                                    const sf_allocator_t *alloc);
SF_API void sf_fsdata_destroy(sf_fsdata_t *fsdata);

SF_API sf_result_t sf_fsdata_read_from_memory(sf_fsdata_t **out, const void *bytes, size_t size,
                                              int entry_count, bool compressed,
                                              const sf_allocator_t *alloc);
SF_API sf_result_t sf_fsdata_write_to_memory(const sf_fsdata_t *fsdata, void **out_bytes,
                                             size_t *out_size, const sf_allocator_t *alloc);

SF_API bool sf_fsdata_is_compressed(const sf_fsdata_t *fsdata);
SF_API int sf_fsdata_entry_count(const sf_fsdata_t *fsdata);
SF_API size_t sf_fsdata_file_count(const sf_fsdata_t *fsdata);

SF_API sf_result_t sf_fsdata_get_file(const sf_fsdata_t *fsdata, size_t index,
                                      int *out_id, const uint8_t **out_bytes, size_t *out_size);
SF_API sf_result_t sf_fsdata_add_file(sf_fsdata_t *fsdata, int id,
                                      const uint8_t *bytes, size_t size);
SF_API sf_result_t sf_fsdata_find_file(const sf_fsdata_t *fsdata, int id, size_t *out_index);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_FSDATA_H */
