/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — GRASS public surface.
 *
 * Defines a dynamic grass mesh attached to a model; only used in Sekiro.
 * Extension: .grass. Little-endian only.
 *
 * The file consists of:
 *   - A 0x28 byte header describing element counts and sizes.
 *   - A flat list of `Volume` records forming a bounding volume hierarchy.
 *   - A flat list of `Vertex` records (position + per-grass-type density).
 *   - A flat list of triangular `Face` records.
 *   - A flat list of axis-aligned bounding boxes, one per volume (in order).
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/GRASS.cs
 */

#ifndef SOULS_FORMATS_SF_GRASS_H
#define SOULS_FORMATS_SF_GRASS_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_grass sf_grass_t;

/* A volume in the bounding volume hierarchy. The bounding box is stored
 * separately in the file (after all volumes/vertices/faces) but is
 * associated with its volume by position. */
typedef struct sf_grass_volume {
    int32_t start_child_index;  /* Index of first child volume. */
    int32_t end_child_index;    /* Index of last child volume, exclusive. */
    int32_t start_face_index;   /* Index of first contained face. */
    int32_t end_face_index;     /* Index of last contained face, exclusive. */
    int32_t unk10;              /* Unknown. */
    /* Axis-aligned bounding box associated with this volume. */
    float bb_min_x, bb_min_y, bb_min_z;
    float bb_max_x, bb_max_y, bb_max_z;
} sf_grass_volume_t;

/* A vertex in the grass mesh. */
typedef struct sf_grass_vertex {
    float x, y, z;              /* Position, relative to parent model. */
    float grass_densities[6];   /* Densities for the six grass types; usual range 0..1. */
} sf_grass_vertex_t;

/* A triangular patch of grass. */
typedef struct sf_grass_face {
    float normal_x, normal_y, normal_z; /* Triangle normal. */
    int32_t vertex_index_a;             /* First vertex index. */
    int32_t vertex_index_b;             /* Second vertex index. */
    int32_t vertex_index_c;             /* Third vertex index. */
} sf_grass_face_t;

SF_API sf_result_t sf_grass_create(sf_grass_t **out, const sf_allocator_t *alloc);
SF_API void sf_grass_destroy(sf_grass_t *grass);

SF_API sf_result_t sf_grass_read_from_memory(sf_grass_t **out, const void *bytes, size_t size,
                                             const sf_allocator_t *alloc);
SF_API sf_result_t sf_grass_write_to_memory(const sf_grass_t *grass, void **out_bytes,
                                            size_t *out_size, const sf_allocator_t *alloc);

SF_API bool sf_grass_is(const void *bytes, size_t size);

SF_API size_t sf_grass_volume_count(const sf_grass_t *grass);
SF_API sf_result_t sf_grass_get_volume(const sf_grass_t *grass, size_t index,
                                       sf_grass_volume_t *out);
SF_API sf_result_t sf_grass_add_volume(sf_grass_t *grass, sf_grass_volume_t volume);

SF_API size_t sf_grass_vertex_count(const sf_grass_t *grass);
SF_API sf_result_t sf_grass_get_vertex(const sf_grass_t *grass, size_t index,
                                       sf_grass_vertex_t *out);
SF_API sf_result_t sf_grass_add_vertex(sf_grass_t *grass, sf_grass_vertex_t vertex);

SF_API size_t sf_grass_face_count(const sf_grass_t *grass);
SF_API sf_result_t sf_grass_get_face(const sf_grass_t *grass, size_t index,
                                     sf_grass_face_t *out);
SF_API sf_result_t sf_grass_add_face(sf_grass_t *grass, sf_grass_face_t face);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_GRASS_H */
