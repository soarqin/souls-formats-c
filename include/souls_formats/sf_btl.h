// Upstream: BTL.cs
/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SOULS_FORMATS_SF_BTL_H
#define SOULS_FORMATS_SF_BTL_H

#include "sf_common.h"
#include "sf_math.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sf_btl_light_type {
    SF_BTL_LIGHT_TYPE_POINT = 0,
    SF_BTL_LIGHT_TYPE_SPOT = 1,
    SF_BTL_LIGHT_TYPE_DIRECTIONAL = 2,
} sf_btl_light_type_t;

_Static_assert(SF_BTL_LIGHT_TYPE_POINT == 0, "BTL LightType drift (Point)");
_Static_assert(SF_BTL_LIGHT_TYPE_DIRECTIONAL == 2, "BTL LightType drift (Directional)");

typedef struct sf_btl sf_btl_t;
typedef struct sf_btl_light sf_btl_light_t;

SF_API sf_result_t sf_btl_read_from_memory(sf_btl_t **out, const void *bytes, size_t size,
                                           const sf_allocator_t *alloc);
SF_API sf_result_t sf_btl_write_to_buffer(const sf_btl_t *btl, void **out_bytes,
                                          size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_btl_destroy(sf_btl_t *btl);

SF_API int32_t sf_btl_version(const sf_btl_t *btl);
SF_API size_t sf_btl_light_count(const sf_btl_t *btl);
SF_API const sf_btl_light_t *sf_btl_get_light(const sf_btl_t *btl, size_t index);

SF_API const char *sf_btl_light_name(const sf_btl_light_t *light);
SF_API sf_btl_light_type_t sf_btl_light_type(const sf_btl_light_t *light);
SF_API sf_color_t sf_btl_light_diffuse_color(const sf_btl_light_t *light);
SF_API float sf_btl_light_diffuse_power(const sf_btl_light_t *light);
SF_API sf_vec3_t sf_btl_light_position(const sf_btl_light_t *light);
SF_API sf_vec3_t sf_btl_light_rotation(const sf_btl_light_t *light);
SF_API float sf_btl_light_radius(const sf_btl_light_t *light);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_BTL_H */
