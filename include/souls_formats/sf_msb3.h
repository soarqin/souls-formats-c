/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Dark Souls 3 MSB map layout container.
 */

#ifndef SOULS_FORMATS_SF_MSB3_H
#define SOULS_FORMATS_SF_MSB3_H

#include "sf_common.h"
#include "souls_formats/sf_msb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_msb3        sf_msb3_t;
typedef struct sf_msb3_model  sf_msb3_model_t;
typedef struct sf_msb3_event  sf_msb3_event_t;
typedef struct sf_msb3_region sf_msb3_region_t;
typedef struct sf_msb3_part   sf_msb3_part_t;

SF_API sf_result_t sf_msb3_read_from_memory(sf_msb3_t **out, const uint8_t *data,
                                            size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_msb3_write_to_memory(const sf_msb3_t *msb3, uint8_t **out_data,
                                           size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_msb3_destroy(sf_msb3_t *msb3);

SF_API int32_t sf_msb3_model_count (const sf_msb3_t *m);
SF_API int32_t sf_msb3_event_count (const sf_msb3_t *m);
SF_API int32_t sf_msb3_region_count(const sf_msb3_t *m);
SF_API int32_t sf_msb3_part_count  (const sf_msb3_t *m);

SF_API const sf_msb3_model_t  *sf_msb3_model_at (const sf_msb3_t *m, int32_t idx);
SF_API const sf_msb3_event_t  *sf_msb3_event_at (const sf_msb3_t *m, int32_t idx);
SF_API const sf_msb3_region_t *sf_msb3_region_at(const sf_msb3_t *m, int32_t idx);
SF_API const sf_msb3_part_t   *sf_msb3_part_at  (const sf_msb3_t *m, int32_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SOULS_FORMATS_SF_MSB3_H */
