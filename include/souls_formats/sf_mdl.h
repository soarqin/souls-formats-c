/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SOULS_FORMATS_SF_MDL_H
#define SOULS_FORMATS_SF_MDL_H

#include "sf_common.h"

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_mdl sf_mdl_t;
typedef struct sf_mdl0 sf_mdl0_t;

typedef enum sf_mdl_weight_index {
    SF_MDL_WEIGHT_INDEX_0 = 0,
    SF_MDL_WEIGHT_INDEX_1 = 4,
    SF_MDL_WEIGHT_INDEX_2 = 8,
    SF_MDL_WEIGHT_INDEX_3 = 12,
} sf_mdl_weight_index_t;

_Static_assert(SF_MDL_WEIGHT_INDEX_0 == 0, "MDLWeightIndex drift (Index0)");
_Static_assert(SF_MDL_WEIGHT_INDEX_3 == 12, "MDLWeightIndex drift (Index3)");

SF_API sf_result_t sf_mdl_read_from_memory(sf_mdl_t **out, const void *bytes,
                                           size_t size, const sf_allocator_t *a);
SF_API sf_result_t sf_mdl_read_from_path(sf_mdl_t **out, const wchar_t *path,
                                         const sf_allocator_t *a);
SF_API void sf_mdl_destroy(sf_mdl_t *m);

SF_API int32_t sf_mdl_unk0c(const sf_mdl_t *m);
SF_API int32_t sf_mdl_unk10(const sf_mdl_t *m);
SF_API int32_t sf_mdl_unk14(const sf_mdl_t *m);
SF_API size_t sf_mdl_node_count(const sf_mdl_t *m);
SF_API size_t sf_mdl_index_count(const sf_mdl_t *m);
SF_API size_t sf_mdl_vertex_count_a(const sf_mdl_t *m);
SF_API size_t sf_mdl_vertex_count_b(const sf_mdl_t *m);
SF_API size_t sf_mdl_vertex_count_c(const sf_mdl_t *m);
SF_API size_t sf_mdl_vertex_count_d(const sf_mdl_t *m);
SF_API size_t sf_mdl_material_count(const sf_mdl_t *m);
SF_API size_t sf_mdl_texture_count(const sf_mdl_t *m);

SF_API sf_result_t sf_mdl0_read_from_memory(sf_mdl0_t **out, const void *bytes,
                                            size_t size, const sf_allocator_t *a);
SF_API sf_result_t sf_mdl0_read_from_path(sf_mdl0_t **out, const wchar_t *path,
                                          const sf_allocator_t *a);
SF_API void sf_mdl0_destroy(sf_mdl0_t *m);

SF_API int32_t sf_mdl0_unk04(const sf_mdl0_t *m);
SF_API int32_t sf_mdl0_unk08(const sf_mdl0_t *m);
SF_API size_t sf_mdl0_node_count(const sf_mdl0_t *m);
SF_API size_t sf_mdl0_index_count(const sf_mdl0_t *m);
SF_API size_t sf_mdl0_vertex_count_a(const sf_mdl0_t *m);
SF_API size_t sf_mdl0_vertex_count_b(const sf_mdl0_t *m);
SF_API size_t sf_mdl0_vertex_count_c(const sf_mdl0_t *m);
SF_API size_t sf_mdl0_material_count(const sf_mdl0_t *m);
SF_API size_t sf_mdl0_texture_count(const sf_mdl0_t *m);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_MDL_H */
