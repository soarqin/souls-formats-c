// Upstream: PMDCL.cs
/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SOULS_FORMATS_SF_PMDCL_H
#define SOULS_FORMATS_SF_PMDCL_H

#include "sf_common.h"
#include "sf_math.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_pmdcl sf_pmdcl_t;
typedef struct sf_pmdcl_decal sf_pmdcl_decal_t;

SF_API sf_result_t sf_pmdcl_read_from_memory(sf_pmdcl_t **out, const void *bytes, size_t size,
                                             const sf_allocator_t *alloc);
SF_API sf_result_t sf_pmdcl_write_to_buffer(const sf_pmdcl_t *pmdcl, void **out_bytes,
                                            size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_pmdcl_destroy(sf_pmdcl_t *pmdcl);

SF_API size_t sf_pmdcl_decal_count(const sf_pmdcl_t *pmdcl);
SF_API const sf_pmdcl_decal_t *sf_pmdcl_get_decal(const sf_pmdcl_t *pmdcl, size_t index);

SF_API sf_vec3_t sf_pmdcl_decal_x_angles(const sf_pmdcl_decal_t *decal);
SF_API sf_vec3_t sf_pmdcl_decal_y_angles(const sf_pmdcl_decal_t *decal);
SF_API sf_vec3_t sf_pmdcl_decal_z_angles(const sf_pmdcl_decal_t *decal);
SF_API sf_vec3_t sf_pmdcl_decal_position(const sf_pmdcl_decal_t *decal);
SF_API float sf_pmdcl_decal_unk3c(const sf_pmdcl_decal_t *decal);
SF_API int32_t sf_pmdcl_decal_param_id(const sf_pmdcl_decal_t *decal);
SF_API int16_t sf_pmdcl_decal_size1(const sf_pmdcl_decal_t *decal);
SF_API int16_t sf_pmdcl_decal_size2(const sf_pmdcl_decal_t *decal);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_PMDCL_H */
