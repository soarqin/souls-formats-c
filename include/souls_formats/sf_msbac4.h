/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Armored Core 4 MSB map layout container.
 */

#ifndef SOULS_FORMATS_SF_MSBAC4_H
#define SOULS_FORMATS_SF_MSBAC4_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_msb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_msbac4        sf_msbac4_t;
typedef struct sf_msbac4_model  sf_msbac4_model_t;
typedef struct sf_msbac4_event  sf_msbac4_event_t;
typedef struct sf_msbac4_region sf_msbac4_region_t;
typedef struct sf_msbac4_part   sf_msbac4_part_t;

SF_API sf_result_t sf_msbac4_read_from_memory(sf_msbac4_t **out, const uint8_t *data,
                                               size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_msbac4_write_to_memory(const sf_msbac4_t *msb, uint8_t **out_data,
                                             size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_msbac4_destroy(sf_msbac4_t *msb);

SF_API int32_t sf_msbac4_model_count (const sf_msbac4_t *m);
SF_API int32_t sf_msbac4_event_count (const sf_msbac4_t *m);
SF_API int32_t sf_msbac4_region_count(const sf_msbac4_t *m);
SF_API int32_t sf_msbac4_part_count  (const sf_msbac4_t *m);

SF_API const sf_msbac4_model_t  *sf_msbac4_model_at (const sf_msbac4_t *m, int32_t idx);
SF_API const sf_msbac4_event_t  *sf_msbac4_event_at (const sf_msbac4_t *m, int32_t idx);
SF_API const sf_msbac4_region_t *sf_msbac4_region_at(const sf_msbac4_t *m, int32_t idx);
SF_API const sf_msbac4_part_t   *sf_msbac4_part_at  (const sf_msbac4_t *m, int32_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SOULS_FORMATS_SF_MSBAC4_H */
