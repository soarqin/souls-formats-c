/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Dark Souls 2 MSB map layout container.
 */

#ifndef SOULS_FORMATS_SF_MSB2_H
#define SOULS_FORMATS_SF_MSB2_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_msb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_msb2        sf_msb2_t;
typedef struct sf_msb2_model  sf_msb2_model_t;
typedef struct sf_msb2_event  sf_msb2_event_t;
typedef struct sf_msb2_region sf_msb2_region_t;
typedef struct sf_msb2_part   sf_msb2_part_t;

SF_API sf_result_t sf_msb2_read_from_memory(sf_msb2_t **out, const uint8_t *data,
                                            size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_msb2_write_to_memory(const sf_msb2_t *msb2, uint8_t **out_data,
                                           size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_msb2_destroy(sf_msb2_t *msb2);

SF_API int32_t sf_msb2_model_count (const sf_msb2_t *m);
SF_API int32_t sf_msb2_event_count (const sf_msb2_t *m);
SF_API int32_t sf_msb2_region_count(const sf_msb2_t *m);
SF_API int32_t sf_msb2_part_count  (const sf_msb2_t *m);

SF_API const sf_msb2_model_t  *sf_msb2_model_at (const sf_msb2_t *m, int32_t idx);
SF_API const sf_msb2_event_t  *sf_msb2_event_at (const sf_msb2_t *m, int32_t idx);
SF_API const sf_msb2_region_t *sf_msb2_region_at(const sf_msb2_t *m, int32_t idx);
SF_API const sf_msb2_part_t   *sf_msb2_part_at  (const sf_msb2_t *m, int32_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SOULS_FORMATS_SF_MSB2_H */
