/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SOULS_FORMATS_SF_MSBB_H
#define SOULS_FORMATS_SF_MSBB_H

#include "sf_common.h"
#include "souls_formats/sf_msb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_msbb        sf_msbb_t;
typedef struct sf_msbb_model  sf_msbb_model_t;
typedef struct sf_msbb_event  sf_msbb_event_t;
typedef struct sf_msbb_region sf_msbb_region_t;
typedef struct sf_msbb_part   sf_msbb_part_t;

SF_API sf_result_t sf_msbb_read_from_memory(sf_msbb_t **out, const uint8_t *data,
                                            size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_msbb_write_to_memory(const sf_msbb_t *msbb, uint8_t **out_data,
                                           size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_msbb_destroy(sf_msbb_t *msbb);

SF_API int32_t sf_msbb_model_count (const sf_msbb_t *m);
SF_API int32_t sf_msbb_event_count (const sf_msbb_t *m);
SF_API int32_t sf_msbb_region_count(const sf_msbb_t *m);
SF_API int32_t sf_msbb_part_count  (const sf_msbb_t *m);

SF_API const sf_msbb_model_t  *sf_msbb_model_at (const sf_msbb_t *m, int32_t idx);
SF_API const sf_msbb_event_t  *sf_msbb_event_at (const sf_msbb_t *m, int32_t idx);
SF_API const sf_msbb_region_t *sf_msbb_region_at(const sf_msbb_t *m, int32_t idx);
SF_API const sf_msbb_part_t   *sf_msbb_part_at  (const sf_msbb_t *m, int32_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SOULS_FORMATS_SF_MSBB_H */
