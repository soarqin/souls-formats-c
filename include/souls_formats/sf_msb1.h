/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Dark Souls 1 MSB map layout container.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/MSB/MSB1/MSB1.cs
 */

#ifndef SOULS_FORMATS_SF_MSB1_H
#define SOULS_FORMATS_SF_MSB1_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_msb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_msb1        sf_msb1_t;
typedef struct sf_msb1_model  sf_msb1_model_t;
typedef struct sf_msb1_event  sf_msb1_event_t;
typedef struct sf_msb1_region sf_msb1_region_t;
typedef struct sf_msb1_part   sf_msb1_part_t;

SF_API sf_result_t sf_msb1_read_from_memory(sf_msb1_t **out, const uint8_t *data,
                                            size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_msb1_write_to_memory(const sf_msb1_t *msb1, uint8_t **out_data,
                                           size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_msb1_destroy(sf_msb1_t *msb1);

SF_API int32_t sf_msb1_model_count (const sf_msb1_t *m);
SF_API int32_t sf_msb1_event_count (const sf_msb1_t *m);
SF_API int32_t sf_msb1_region_count(const sf_msb1_t *m);
SF_API int32_t sf_msb1_part_count  (const sf_msb1_t *m);

SF_API const sf_msb1_model_t  *sf_msb1_model_at (const sf_msb1_t *m, int32_t idx);
SF_API const sf_msb1_event_t  *sf_msb1_event_at (const sf_msb1_t *m, int32_t idx);
SF_API const sf_msb1_region_t *sf_msb1_region_at(const sf_msb1_t *m, int32_t idx);
SF_API const sf_msb1_part_t   *sf_msb1_part_at  (const sf_msb1_t *m, int32_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SOULS_FORMATS_SF_MSB1_H */
