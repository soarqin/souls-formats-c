/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Armored Core V MSB map layout container.
 */

#ifndef SOULS_FORMATS_SF_MSBV_H
#define SOULS_FORMATS_SF_MSBV_H

#include "sf_common.h"
#include "souls_formats/sf_msb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_msbv        sf_msbv_t;
typedef struct sf_msbv_model  sf_msbv_model_t;
typedef struct sf_msbv_event  sf_msbv_event_t;
typedef struct sf_msbv_region sf_msbv_region_t;
typedef struct sf_msbv_part   sf_msbv_part_t;

SF_API sf_result_t sf_msbv_read_from_memory(sf_msbv_t **out, const uint8_t *data,
                                               size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_msbv_write_to_memory(const sf_msbv_t *msb, uint8_t **out_data,
                                             size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_msbv_destroy(sf_msbv_t *msb);

SF_API int32_t sf_msbv_model_count (const sf_msbv_t *m);
SF_API int32_t sf_msbv_event_count (const sf_msbv_t *m);
SF_API int32_t sf_msbv_region_count(const sf_msbv_t *m);
SF_API int32_t sf_msbv_part_count  (const sf_msbv_t *m);

SF_API const sf_msbv_model_t  *sf_msbv_model_at (const sf_msbv_t *m, int32_t idx);
SF_API const sf_msbv_event_t  *sf_msbv_event_at (const sf_msbv_t *m, int32_t idx);
SF_API const sf_msbv_region_t *sf_msbv_region_at(const sf_msbv_t *m, int32_t idx);
SF_API const sf_msbv_part_t   *sf_msbv_part_at  (const sf_msbv_t *m, int32_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SOULS_FORMATS_SF_MSBV_H */
