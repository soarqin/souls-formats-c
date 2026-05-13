/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file sf_flver0.h
 * @brief FLVER0 — legacy FromSoftware mesh format (AC4/ACFA/ACER era).
 */
#ifndef SOULS_FORMATS_SF_FLVER0_H
#define SOULS_FORMATS_SF_FLVER0_H

#include "sf_common.h"
#include "souls_formats/sf_flver.h"
#include "sf_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_flver0               sf_flver0_t;
typedef struct sf_flver0_material      sf_flver0_material_t;
typedef struct sf_flver0_texture       sf_flver0_texture_t;
typedef struct sf_flver0_mesh          sf_flver0_mesh_t;
typedef struct sf_flver0_buffer_layout sf_flver0_buffer_layout_t;

SF_API sf_result_t sf_flver0_read_from_memory(sf_flver0_t **out, const void *bytes,
                                              size_t size, const sf_allocator_t *a);
SF_API sf_result_t sf_flver0_read_from_path(sf_flver0_t **out, const wchar_t *path,
                                            const sf_allocator_t *a);
SF_API sf_result_t sf_flver0_write_to_memory(const sf_flver0_t *f, void **out_bytes,
                                             size_t *out_size, const sf_allocator_t *a);
SF_API sf_result_t sf_flver0_write_to_path(const sf_flver0_t *f, const wchar_t *path);
SF_API void sf_flver0_destroy(sf_flver0_t *f);

SF_API uint32_t  sf_flver0_header_version(const sf_flver0_t *f);
SF_API bool      sf_flver0_header_big_endian(const sf_flver0_t *f);
SF_API bool      sf_flver0_header_unicode(const sf_flver0_t *f);
SF_API sf_vec3_t sf_flver0_header_bounding_box_min(const sf_flver0_t *f);
SF_API sf_vec3_t sf_flver0_header_bounding_box_max(const sf_flver0_t *f);

SF_API size_t sf_flver0_dummy_count(const sf_flver0_t *f);
SF_API size_t sf_flver0_node_count(const sf_flver0_t *f);
SF_API size_t sf_flver0_material_count(const sf_flver0_t *f);
SF_API size_t sf_flver0_mesh_count(const sf_flver0_t *f);

SF_API const sf_flver_dummy_t      *sf_flver0_dummy(const sf_flver0_t *f, size_t i);
SF_API const sf_flver_node_t       *sf_flver0_node(const sf_flver0_t *f, size_t i);
SF_API const sf_flver0_material_t  *sf_flver0_material(const sf_flver0_t *f, size_t i);
SF_API const sf_flver0_mesh_t      *sf_flver0_mesh(const sf_flver0_t *f, size_t i);

SF_API const char *sf_flver0_material_name(const sf_flver0_material_t *m);
SF_API const char *sf_flver0_material_mtd(const sf_flver0_material_t *m);
SF_API size_t sf_flver0_material_texture_count(const sf_flver0_material_t *m);
SF_API const sf_flver0_texture_t *sf_flver0_material_texture(const sf_flver0_material_t *m,
                                                             size_t i);
SF_API size_t sf_flver0_material_layout_count(const sf_flver0_material_t *m);
SF_API const sf_flver0_buffer_layout_t *sf_flver0_material_layout(const sf_flver0_material_t *m,
                                                                  size_t i);

SF_API const char *sf_flver0_texture_path(const sf_flver0_texture_t *t);
SF_API const char *sf_flver0_texture_param_name(const sf_flver0_texture_t *t);

SF_API uint8_t sf_flver0_mesh_dynamic(const sf_flver0_mesh_t *m);
SF_API uint8_t sf_flver0_mesh_material_index(const sf_flver0_mesh_t *m);
SF_API bool    sf_flver0_mesh_cull_backfaces(const sf_flver0_mesh_t *m);
SF_API bool    sf_flver0_mesh_triangle_strip(const sf_flver0_mesh_t *m);
SF_API int16_t sf_flver0_mesh_node_index(const sf_flver0_mesh_t *m);
SF_API size_t  sf_flver0_mesh_index_count(const sf_flver0_mesh_t *m);
SF_API uint32_t sf_flver0_mesh_index(const sf_flver0_mesh_t *m, size_t i);
SF_API size_t  sf_flver0_mesh_vertex_count(const sf_flver0_mesh_t *m);
SF_API int32_t sf_flver0_mesh_layout_index(const sf_flver0_mesh_t *m);
SF_API const uint8_t *sf_flver0_mesh_vertex_bytes(const sf_flver0_mesh_t *m,
                                                  size_t *out_size);

SF_API size_t sf_flver0_buffer_layout_member_count(const sf_flver0_buffer_layout_t *bl);
SF_API uint32_t sf_flver0_buffer_layout_size(const sf_flver0_buffer_layout_t *bl);
SF_API sf_flver_layout_type_t sf_flver0_buffer_layout_member_type(
    const sf_flver0_buffer_layout_t *bl, size_t i);
SF_API sf_flver_layout_semantic_t sf_flver0_buffer_layout_member_semantic(
    const sf_flver0_buffer_layout_t *bl, size_t i);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_FLVER0_H */
