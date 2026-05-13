/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — NVM navmesh polygon mesh (DeS/DS1).
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormatsNEXT @ 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a
 *   SoulsFormats/Formats/NVM.cs
 */

#ifndef SOULS_FORMATS_SF_NVM_H
#define SOULS_FORMATS_SF_NVM_H

#include "sf_common.h"
#include "souls_formats/sf_io.h"
#include "sf_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_nvm          sf_nvm_t;
typedef struct sf_nvm_triangle sf_nvm_triangle_t;
typedef struct sf_nvm_box      sf_nvm_box_t;
typedef struct sf_nvm_entity   sf_nvm_entity_t;

/** Mirrors upstream NVM.TriangleFlags (bitmask). */
typedef enum sf_nvm_triangle_flags {
    SF_NVM_TRI_FLAG_NONE          = 0x0000,
    SF_NVM_TRI_FLAG_INSIDE_WALL   = 0x0001,
    SF_NVM_TRI_FLAG_BLOCK_GATE    = 0x0002,
    SF_NVM_TRI_FLAG_CLOSED_DOOR   = 0x0004,
    SF_NVM_TRI_FLAG_DOOR          = 0x0008,
    SF_NVM_TRI_FLAG_HOLE          = 0x0010,
    SF_NVM_TRI_FLAG_LADDER        = 0x0020,
    SF_NVM_TRI_FLAG_LARGE_SPACE   = 0x0040,
    SF_NVM_TRI_FLAG_EDGE          = 0x0080,
    SF_NVM_TRI_FLAG_EVENT         = 0x0100,
    SF_NVM_TRI_FLAG_LANDING_POINT = 0x0200,
    SF_NVM_TRI_FLAG_FLOOR_TO_WALL = 0x0400,
    SF_NVM_TRI_FLAG_DEGENERATE    = 0x0800,
    SF_NVM_TRI_FLAG_WALL          = 0x1000,
    SF_NVM_TRI_FLAG_BLOCK         = 0x2000,
    SF_NVM_TRI_FLAG_GATE          = 0x4000,
    SF_NVM_TRI_FLAG_DISABLE       = 0x8000,
} sf_nvm_triangle_flags_t;
_Static_assert(SF_NVM_TRI_FLAG_DISABLE == 0x8000, "NVM TriangleFlags drift");

SF_API sf_result_t sf_nvm_read_from_memory(sf_nvm_t **out, const void *data, size_t size,
                                           const sf_allocator_t *alloc);
SF_API sf_result_t sf_nvm_write_to_memory(const sf_nvm_t *nvm, void **out_data,
                                          size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_nvm_destroy(sf_nvm_t *nvm);

SF_API sf_result_t sf_nvm_create_empty(sf_nvm_t **out, const sf_allocator_t *alloc);

SF_API bool      sf_nvm_big_endian(const sf_nvm_t *nvm);
SF_API void      sf_nvm_set_big_endian(sf_nvm_t *nvm, bool be);
SF_API size_t    sf_nvm_vertex_count(const sf_nvm_t *nvm);
SF_API sf_vec3_t sf_nvm_vertex(const sf_nvm_t *nvm, size_t i);
SF_API sf_result_t sf_nvm_append_vertex(sf_nvm_t *nvm, sf_vec3_t v);

SF_API size_t                  sf_nvm_triangle_count(const sf_nvm_t *nvm);
SF_API const sf_nvm_triangle_t *sf_nvm_triangle(const sf_nvm_t *nvm, size_t i);
SF_API sf_result_t              sf_nvm_append_triangle(sf_nvm_t *nvm, sf_nvm_triangle_t **out);

SF_API int32_t sf_nvm_triangle_vertex_index_1(const sf_nvm_triangle_t *tri);
SF_API int32_t sf_nvm_triangle_vertex_index_2(const sf_nvm_triangle_t *tri);
SF_API int32_t sf_nvm_triangle_vertex_index_3(const sf_nvm_triangle_t *tri);
SF_API int32_t sf_nvm_triangle_edge_index_1  (const sf_nvm_triangle_t *tri);
SF_API int32_t sf_nvm_triangle_edge_index_2  (const sf_nvm_triangle_t *tri);
SF_API int32_t sf_nvm_triangle_edge_index_3  (const sf_nvm_triangle_t *tri);
SF_API int32_t sf_nvm_triangle_obstacle_count(const sf_nvm_triangle_t *tri);
SF_API sf_nvm_triangle_flags_t sf_nvm_triangle_flags(const sf_nvm_triangle_t *tri);

SF_API void sf_nvm_triangle_set_vertex_indices(sf_nvm_triangle_t *tri,
                                               int32_t v1, int32_t v2, int32_t v3);
SF_API void sf_nvm_triangle_set_edge_indices  (sf_nvm_triangle_t *tri,
                                               int32_t e1, int32_t e2, int32_t e3);
SF_API void sf_nvm_triangle_set_obstacle_count(sf_nvm_triangle_t *tri, int32_t v);
SF_API void sf_nvm_triangle_set_flags         (sf_nvm_triangle_t *tri,
                                               sf_nvm_triangle_flags_t v);

SF_API const sf_nvm_box_t *sf_nvm_root_box(const sf_nvm_t *nvm);
SF_API sf_nvm_box_t       *sf_nvm_root_box_mut(sf_nvm_t *nvm);
SF_API sf_result_t sf_nvm_set_root_box(sf_nvm_t *nvm, sf_nvm_box_t *box);
SF_API sf_result_t sf_nvm_create_box(sf_nvm_t *nvm, sf_nvm_box_t **out_box);

SF_API sf_vec3_t sf_nvm_box_min_corner(const sf_nvm_box_t *box);
SF_API sf_vec3_t sf_nvm_box_max_corner(const sf_nvm_box_t *box);
SF_API size_t    sf_nvm_box_triangle_index_count(const sf_nvm_box_t *box);
SF_API int32_t   sf_nvm_box_triangle_index      (const sf_nvm_box_t *box, size_t i);
SF_API const sf_nvm_box_t *sf_nvm_box_child(const sf_nvm_box_t *box, size_t which);
SF_API sf_nvm_box_t       *sf_nvm_box_child_mut(sf_nvm_box_t *box, size_t which);
SF_API void sf_nvm_box_set_min_corner(sf_nvm_box_t *box, sf_vec3_t v);
SF_API void sf_nvm_box_set_max_corner(sf_nvm_box_t *box, sf_vec3_t v);
SF_API sf_result_t sf_nvm_box_append_triangle_index(sf_nvm_box_t *box, int32_t idx);
SF_API sf_result_t sf_nvm_box_set_child(sf_nvm_box_t *box, size_t which, sf_nvm_box_t *child);

SF_API size_t                sf_nvm_entity_count(const sf_nvm_t *nvm);
SF_API const sf_nvm_entity_t *sf_nvm_entity(const sf_nvm_t *nvm, size_t i);
SF_API sf_result_t            sf_nvm_append_entity(sf_nvm_t *nvm, sf_nvm_entity_t **out);

SF_API int32_t sf_nvm_entity_entity_id              (const sf_nvm_entity_t *e);
SF_API size_t  sf_nvm_entity_triangle_index_count   (const sf_nvm_entity_t *e);
SF_API int32_t sf_nvm_entity_triangle_index         (const sf_nvm_entity_t *e, size_t i);
SF_API void    sf_nvm_entity_set_entity_id          (sf_nvm_entity_t *e, int32_t id);
SF_API sf_result_t sf_nvm_entity_append_triangle_index(sf_nvm_entity_t *e, int32_t idx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_NVM_H */
