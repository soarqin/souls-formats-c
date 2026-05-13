/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — NGP navmesh (DS2).
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormatsNEXT @ 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a
 *   SoulsFormats/Formats/NGP.cs
 *
 * The NVG2 navmesh format used by Dark Souls II. The on-disk layout uses
 * version-dependent varints (32-bit for Vanilla, 64-bit for Scholar) for
 * offsets, with five top-level tables (StructA, StructB, StructC, StructD,
 * and per-mesh structures). This C API preserves the file at byte level
 * (round-trip safe) and exposes high-level metadata for inspection; full
 * per-entry decoding is intentionally left to future work as the per-mesh
 * sub-arrays carry inter-element offsets that complicate mutation.
 */

#ifndef SOULS_FORMATS_SF_NGP_H
#define SOULS_FORMATS_SF_NGP_H

#include "sf_common.h"
#include "sf_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_ngp sf_ngp_t;

/** Mirrors upstream NGP.NGPVersion. */
typedef enum sf_ngp_version {
    SF_NGP_VERSION_VANILLA = 1,
    SF_NGP_VERSION_SCHOLAR = 2,
} sf_ngp_version_t;
_Static_assert(SF_NGP_VERSION_SCHOLAR == 2, "NGP Version drift");

SF_API sf_result_t sf_ngp_read_from_memory(sf_ngp_t **out, const void *data, size_t size,
                                           const sf_allocator_t *alloc);
SF_API sf_result_t sf_ngp_write_to_memory(const sf_ngp_t *ngp, void **out_data,
                                          size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_ngp_destroy(sf_ngp_t *ngp);

SF_API sf_result_t sf_ngp_create_from_bytes(sf_ngp_t **out, const void *data,
                                            size_t size, const sf_allocator_t *alloc);

SF_API bool             sf_ngp_big_endian(const sf_ngp_t *ngp);
SF_API sf_ngp_version_t sf_ngp_version(const sf_ngp_t *ngp);
SF_API int32_t          sf_ngp_unk1c(const sf_ngp_t *ngp);
SF_API int32_t          sf_ngp_mesh_count(const sf_ngp_t *ngp);
SF_API int32_t          sf_ngp_struct_a_count(const sf_ngp_t *ngp);
SF_API int32_t          sf_ngp_struct_b_count(const sf_ngp_t *ngp);
SF_API int32_t          sf_ngp_struct_c_count(const sf_ngp_t *ngp);
SF_API int32_t          sf_ngp_struct_d_count(const sf_ngp_t *ngp);

SF_API const uint8_t *sf_ngp_raw_data(const sf_ngp_t *ngp);
SF_API size_t         sf_ngp_raw_size(const sf_ngp_t *ngp);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_NGP_H */
