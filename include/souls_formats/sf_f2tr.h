/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — F2TR (FaceGen vertex indices) public surface.
 *
 * Indicates which vertices of a FLVER are relevant for FaceGen.
 * Extension: .flver2tri
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/F2TR.cs
 */

#ifndef SOULS_FORMATS_SF_F2TR_H
#define SOULS_FORMATS_SF_F2TR_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_f2tr sf_f2tr_t;

SF_API sf_result_t sf_f2tr_create(sf_f2tr_t **out, const sf_allocator_t *alloc);
SF_API void sf_f2tr_destroy(sf_f2tr_t *f2tr);

SF_API sf_result_t sf_f2tr_read_from_memory(sf_f2tr_t **out, const void *bytes, size_t size,
                                            const sf_allocator_t *alloc);
SF_API sf_result_t sf_f2tr_write_to_memory(const sf_f2tr_t *f2tr, void **out_bytes,
                                           size_t *out_size, const sf_allocator_t *alloc);

SF_API bool sf_f2tr_is(const void *bytes, size_t size);

SF_API bool sf_f2tr_big_endian(const sf_f2tr_t *f2tr);
SF_API void sf_f2tr_set_big_endian(sf_f2tr_t *f2tr, bool big_endian);

SF_API size_t sf_f2tr_entry_count(const sf_f2tr_t *f2tr);
SF_API const char *sf_f2tr_get_entry_name(const sf_f2tr_t *f2tr, size_t index);
SF_API sf_result_t sf_f2tr_get_entry_indices(const sf_f2tr_t *f2tr, size_t index,
                                             const int16_t **out_indices, size_t *out_count);
SF_API sf_result_t sf_f2tr_add_entry(sf_f2tr_t *f2tr, const char *name_utf8,
                                     const int16_t *indices, size_t count);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_F2TR_H */
