/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — SMD4 (shadow mesh model) public surface.
 *
 * A shadow mesh model format in Armored Core 4thgen and 5thgen games.
 * Big-endian only. Only version 0x40001 is supported.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/SMD4/SMD4.cs
 *   SoulsFormats/Formats/SMD4/Node.cs
 *   SoulsFormats/Formats/SMD4/Mesh.cs
 *   SoulsFormats/Formats/SMD4/Unk10.cs
 *   SoulsFormats/Formats/SMD4/Vertex.cs
 *   SoulsFormats/Formats/SMD4/VertexBoneIndices.cs
 *   SoulsFormats/Formats/SMD4/VertexBoneWeights.cs
 */

#ifndef SOULS_FORMATS_SF_SMD4_H
#define SOULS_FORMATS_SF_SMD4_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Only supported SMD4 version. */
#define SF_SMD4_VERSION_0x40001 0x40001

typedef struct sf_smd4 sf_smd4_t;

/* Unknown 36-byte block.
 * Layout: unk00(u8) unk01(u8) unk02(u8) unk03(u8) name(32 ASCII bytes). */
typedef struct sf_smd4_unk10 {
    uint8_t unk00;
    uint8_t unk01;
    uint8_t unk02;
    uint8_t unk03;
    char name[33]; /* null-terminated, max 32 chars */
} sf_smd4_unk10_t;

/* A joint available for vertices to be attached to (0x90 bytes on disk). */
typedef struct sf_smd4_node {
    char name[33]; /* null-terminated, max 32 chars */
    float translation_x, translation_y, translation_z;
    float rotation_x, rotation_y, rotation_z; /* euler radians, XZY order */
    float scale_x, scale_y, scale_z;
    float bb_min_x, bb_min_y, bb_min_z;
    float bb_max_x, bb_max_y, bb_max_z;
    int16_t parent_index;        /* -1 for none */
    int16_t first_child_index;   /* -1 for none */
    int16_t next_sibling_index;  /* -1 for none */
    int16_t prev_sibling_index;  /* -1 for none */
    int32_t unk64;
    int32_t unk68;
    int32_t unk6c;
    int32_t unk70[8]; /* typically all -1 */
} sf_smd4_node_t;

/* A single point in a mesh. Fields populated depend on the parent mesh's
 * vertex_format:
 *   - Format 0 (16 bytes): position + bone_indices[0]. uv is unused;
 *     bone_indices[1..3] = -1; bone_weights = (1, 0, 0, 0).
 *   - Format 1 (24 bytes): position + uv + bone_indices[0]. bone_indices[1..3]
 *     = -1; bone_weights = (1, 0, 0, 0).
 *   - Format 2 (36 bytes): position + bone_indices[4] + bone_weights[4]. uv
 *     is unused. */
typedef struct sf_smd4_vertex {
    float x, y, z;
    float uv_x, uv_y;
    int16_t bone_indices[4];
    float bone_weights[4];
} sf_smd4_vertex_t;

/* An individual chunk of a model.
 *
 * `indices` / `vertices` are heap-allocated and owned by the parent sf_smd4_t
 * after sf_smd4_add_mesh() (the add call deep-copies). Pointers returned via
 * sf_smd4_get_mesh() are borrows of those owned arrays and MUST NOT be freed
 * by the caller. */
typedef struct sf_smd4_mesh {
    uint8_t vertex_format;     /* 0, 1, or 2 */
    uint8_t unk01;
    bool unk02;
    bool unk03;
    int16_t unk06;
    int16_t bone_indices[28];  /* unused entries = -1 */
    uint16_t *indices;
    size_t index_count;
    sf_smd4_vertex_t *vertices;
    size_t vertex_count;
} sf_smd4_mesh_t;

SF_API sf_result_t sf_smd4_create(sf_smd4_t **out, const sf_allocator_t *alloc);
SF_API void        sf_smd4_destroy(sf_smd4_t *smd4);

SF_API sf_result_t sf_smd4_read_from_memory(sf_smd4_t **out, const void *bytes,
                                            size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_smd4_write_to_memory(const sf_smd4_t *smd4, void **out_bytes,
                                           size_t *out_size, const sf_allocator_t *alloc);

SF_API bool sf_smd4_is(const void *bytes, size_t size);

/* Header accessors. */
SF_API int32_t sf_smd4_version(const sf_smd4_t *smd4);
SF_API void    sf_smd4_set_version(sf_smd4_t *smd4, int32_t version);
SF_API void    sf_smd4_get_bounding_box(const sf_smd4_t *smd4,
                                        float *min_x, float *min_y, float *min_z,
                                        float *max_x, float *max_y, float *max_z);
SF_API void    sf_smd4_set_bounding_box(sf_smd4_t *smd4,
                                        float min_x, float min_y, float min_z,
                                        float max_x, float max_y, float max_z);

/* Unk10 accessors. */
SF_API size_t      sf_smd4_unk10_count(const sf_smd4_t *smd4);
SF_API sf_result_t sf_smd4_get_unk10  (const sf_smd4_t *smd4, size_t index,
                                       sf_smd4_unk10_t *out);
SF_API sf_result_t sf_smd4_add_unk10  (sf_smd4_t *smd4, sf_smd4_unk10_t unk10);

/* Node accessors. */
SF_API size_t      sf_smd4_node_count(const sf_smd4_t *smd4);
SF_API sf_result_t sf_smd4_get_node  (const sf_smd4_t *smd4, size_t index,
                                      sf_smd4_node_t *out);
SF_API sf_result_t sf_smd4_add_node  (sf_smd4_t *smd4, sf_smd4_node_t node);

/* Mesh accessors. The pointer returned by sf_smd4_get_mesh is a borrow; do
 * NOT free it. sf_smd4_add_mesh deep-copies indices/vertices into storage
 * owned by the smd4 object. */
SF_API size_t      sf_smd4_mesh_count(const sf_smd4_t *smd4);
SF_API sf_result_t sf_smd4_get_mesh  (const sf_smd4_t *smd4, size_t index,
                                      const sf_smd4_mesh_t **out);
SF_API sf_result_t sf_smd4_add_mesh  (sf_smd4_t *smd4, sf_smd4_mesh_t mesh);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_SMD4_H */
