/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SOULS_FORMATS_SF_MDL4_H
#define SOULS_FORMATS_SF_MDL4_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_mdl4 sf_mdl4_t;
typedef struct sf_mdl4_dummy sf_mdl4_dummy_t;
typedef struct sf_mdl4_material sf_mdl4_material_t;
typedef struct sf_mdl4_material_param sf_mdl4_material_param_t;
typedef struct sf_mdl4_node sf_mdl4_node_t;
typedef struct sf_mdl4_mesh sf_mdl4_mesh_t;

typedef enum sf_mdl4_param_type {
    SF_MDL4_PARAM_TYPE_INT = 0,
    SF_MDL4_PARAM_TYPE_FLOAT = 1,
    SF_MDL4_PARAM_TYPE_FLOAT4 = 4,
    SF_MDL4_PARAM_TYPE_STRING = 5,
} sf_mdl4_param_type_t;

_Static_assert(SF_MDL4_PARAM_TYPE_INT == 0, "MDL4 ParamType drift (Int)");
_Static_assert(SF_MDL4_PARAM_TYPE_STRING == 5, "MDL4 ParamType drift (String)");

SF_API sf_result_t sf_mdl4_read_from_memory(sf_mdl4_t **out, const void *bytes,
                                            size_t size, const sf_allocator_t *a);
SF_API sf_result_t sf_mdl4_read_from_path(sf_mdl4_t **out, const wchar_t *path,
                                          const sf_allocator_t *a);
SF_API sf_result_t sf_mdl4_write_to_memory(const sf_mdl4_t *m, void **out_bytes,
                                           size_t *out_size, const sf_allocator_t *a);
SF_API sf_result_t sf_mdl4_write_to_path(const sf_mdl4_t *m, const wchar_t *path);
SF_API void sf_mdl4_destroy(sf_mdl4_t *m);

SF_API uint32_t sf_mdl4_header_version(const sf_mdl4_t *m);
SF_API sf_vec3_t sf_mdl4_header_bounding_box_min(const sf_mdl4_t *m);
SF_API sf_vec3_t sf_mdl4_header_bounding_box_max(const sf_mdl4_t *m);
SF_API size_t sf_mdl4_dummy_count(const sf_mdl4_t *m);
SF_API size_t sf_mdl4_material_count(const sf_mdl4_t *m);
SF_API size_t sf_mdl4_node_count(const sf_mdl4_t *m);
SF_API size_t sf_mdl4_mesh_count(const sf_mdl4_t *m);
SF_API const sf_mdl4_material_t *sf_mdl4_material(const sf_mdl4_t *m, size_t i);
SF_API const sf_mdl4_node_t *sf_mdl4_node(const sf_mdl4_t *m, size_t i);
SF_API const sf_mdl4_mesh_t *sf_mdl4_mesh(const sf_mdl4_t *m, size_t i);

SF_API const char *sf_mdl4_material_name(const sf_mdl4_material_t *m);
SF_API const char *sf_mdl4_material_shader(const sf_mdl4_material_t *m);
SF_API size_t sf_mdl4_material_param_count(const sf_mdl4_material_t *m);
SF_API const sf_mdl4_material_param_t *sf_mdl4_material_param(
    const sf_mdl4_material_t *m, size_t i);
SF_API sf_mdl4_param_type_t sf_mdl4_material_param_type(const sf_mdl4_material_param_t *p);
SF_API const char *sf_mdl4_material_param_name(const sf_mdl4_material_param_t *p);

SF_API const char *sf_mdl4_node_name(const sf_mdl4_node_t *n);
SF_API uint8_t sf_mdl4_mesh_vertex_format(const sf_mdl4_mesh_t *m);
SF_API uint8_t sf_mdl4_mesh_material_index(const sf_mdl4_mesh_t *m);
SF_API size_t sf_mdl4_mesh_index_count(const sf_mdl4_mesh_t *m);
SF_API uint16_t sf_mdl4_mesh_index(const sf_mdl4_mesh_t *m, size_t i);
SF_API size_t sf_mdl4_mesh_vertex_count(const sf_mdl4_mesh_t *m);
SF_API const uint8_t *sf_mdl4_mesh_vertex_bytes(const sf_mdl4_mesh_t *m, size_t *out_size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_MDL4_H */
