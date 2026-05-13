/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — EDGE (Sekiro grapple/hang/hug edges).
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormatsNEXT @ 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a
 *   SoulsFormats/Formats/EDGE.cs
 *
 * A Sekiro file that defines grapple points and hangable edges for a model.
 * Each edge has three positions (V1, V2 endpoints + V3 grapple target),
 * a type (Grapple / Hang / Hug), and several unknown auxiliary fields.
 *
 * Wire format is always little-endian. Header is 16 bytes followed by a
 * flat array of fixed 64-byte edge records.
 */

#ifndef SOULS_FORMATS_SF_EDGE_H
#define SOULS_FORMATS_SF_EDGE_H

#include "sf_common.h"
#include "souls_formats/sf_io.h"
#include "sf_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Opaque types
 *===========================================================================*/
typedef struct sf_edge_file sf_edge_file_t;
typedef struct sf_edge      sf_edge_t;

/*===========================================================================
 * Enums (mirror upstream EDGE.cs)
 *===========================================================================*/

/** Mirrors upstream EDGE.EdgeType. */
typedef enum sf_edge_type {
    SF_EDGE_TYPE_GRAPPLE = 1,
    SF_EDGE_TYPE_HANG    = 2,
    SF_EDGE_TYPE_HUG     = 3,
} sf_edge_type_t;
_Static_assert(SF_EDGE_TYPE_HUG == 3, "EDGE EdgeType drift");

/*===========================================================================
 * Read / write / destroy
 *===========================================================================*/
SF_API sf_result_t sf_edge_read_from_memory(sf_edge_file_t **out, const void *data,
                                            size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_edge_write_to_memory(const sf_edge_file_t *edge,
                                           void **out_data, size_t *out_size,
                                           const sf_allocator_t *alloc);
SF_API void sf_edge_destroy(sf_edge_file_t *edge);

/*===========================================================================
 * Top-level field accessors
 *===========================================================================*/
SF_API int32_t sf_edge_id(const sf_edge_file_t *edge);
SF_API void    sf_edge_set_id(sf_edge_file_t *edge, int32_t id);

SF_API size_t           sf_edge_count(const sf_edge_file_t *edge);
SF_API const sf_edge_t *sf_edge_get(const sf_edge_file_t *edge, size_t index);

/*===========================================================================
 * Builder API: create / append edges
 *===========================================================================*/
SF_API sf_result_t sf_edge_create_empty(sf_edge_file_t **out,
                                        const sf_allocator_t *alloc);
SF_API sf_result_t sf_edge_append(sf_edge_file_t *edge,
                                  sf_edge_t **out_edge);

/*===========================================================================
 * Edge field accessors
 *===========================================================================*/
SF_API sf_vec3_t      sf_edge_v1(const sf_edge_t *edge);
SF_API sf_vec3_t      sf_edge_v2(const sf_edge_t *edge);
SF_API sf_vec3_t      sf_edge_v3(const sf_edge_t *edge);
SF_API float          sf_edge_unk2c(const sf_edge_t *edge);
SF_API int32_t        sf_edge_unk30(const sf_edge_t *edge);
SF_API sf_edge_type_t sf_edge_edge_type(const sf_edge_t *edge);
SF_API uint8_t        sf_edge_variation_id(const sf_edge_t *edge);
SF_API uint8_t        sf_edge_unk36(const sf_edge_t *edge);

SF_API void sf_edge_set_v1(sf_edge_t *edge, sf_vec3_t v);
SF_API void sf_edge_set_v2(sf_edge_t *edge, sf_vec3_t v);
SF_API void sf_edge_set_v3(sf_edge_t *edge, sf_vec3_t v);
SF_API void sf_edge_set_unk2c(sf_edge_t *edge, float v);
SF_API void sf_edge_set_unk30(sf_edge_t *edge, int32_t v);
SF_API void sf_edge_set_edge_type(sf_edge_t *edge, sf_edge_type_t v);
SF_API void sf_edge_set_variation_id(sf_edge_t *edge, uint8_t v);
SF_API void sf_edge_set_unk36(sf_edge_t *edge, uint8_t v);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_EDGE_H */
