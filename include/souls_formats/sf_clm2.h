/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — CLM2 (cloth mesh companion) public surface.
 *
 * Companion file to a FLVER associated with cloth simulation. One mesh
 * entry list per FLVER mesh. Little-endian only.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/CLM2.cs
 */

#ifndef SOULS_FORMATS_SF_CLM2_H
#define SOULS_FORMATS_SF_CLM2_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_clm2 sf_clm2_t;

typedef struct sf_clm2_entry {
    int16_t unk00;
    int16_t unk02;
} sf_clm2_entry_t;

SF_API sf_result_t sf_clm2_create(sf_clm2_t **out, const sf_allocator_t *alloc);
SF_API void sf_clm2_destroy(sf_clm2_t *clm2);

SF_API sf_result_t sf_clm2_read_from_memory(sf_clm2_t **out, const void *bytes, size_t size,
                                            const sf_allocator_t *alloc);
SF_API sf_result_t sf_clm2_write_to_memory(const sf_clm2_t *clm2, void **out_bytes,
                                           size_t *out_size, const sf_allocator_t *alloc);

SF_API bool sf_clm2_is(const void *bytes, size_t size);

SF_API size_t sf_clm2_mesh_count(const sf_clm2_t *clm2);
SF_API sf_result_t sf_clm2_add_mesh(sf_clm2_t *clm2, size_t *out_mesh_index);

SF_API sf_result_t sf_clm2_get_mesh_entry_count(const sf_clm2_t *clm2, size_t mesh_index,
                                                size_t *out_count);
SF_API sf_result_t sf_clm2_get_mesh_entry(const sf_clm2_t *clm2, size_t mesh_index,
                                          size_t entry_index, sf_clm2_entry_t *out_entry);
SF_API sf_result_t sf_clm2_add_mesh_entry(sf_clm2_t *clm2, size_t mesh_index,
                                          int16_t unk00, int16_t unk02);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_CLM2_H */
