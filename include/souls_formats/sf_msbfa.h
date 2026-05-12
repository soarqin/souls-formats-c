/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Armored Core For Answer MSB map layout container.
 */

#ifndef SOULS_FORMATS_SF_MSBFA_H
#define SOULS_FORMATS_SF_MSBFA_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_msb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_msbfa        sf_msbfa_t;
typedef struct sf_msbfa_model  sf_msbfa_model_t;
typedef struct sf_msbfa_event  sf_msbfa_event_t;
typedef struct sf_msbfa_region sf_msbfa_region_t;
typedef struct sf_msbfa_part   sf_msbfa_part_t;

SF_API sf_result_t sf_msbfa_read_from_memory(sf_msbfa_t **out, const uint8_t *data,
                                               size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_msbfa_write_to_memory(const sf_msbfa_t *msb, uint8_t **out_data,
                                             size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_msbfa_destroy(sf_msbfa_t *msb);

SF_API int32_t sf_msbfa_model_count (const sf_msbfa_t *m);
SF_API int32_t sf_msbfa_event_count (const sf_msbfa_t *m);
SF_API int32_t sf_msbfa_region_count(const sf_msbfa_t *m);
SF_API int32_t sf_msbfa_part_count  (const sf_msbfa_t *m);

SF_API const sf_msbfa_model_t  *sf_msbfa_model_at (const sf_msbfa_t *m, int32_t idx);
SF_API const sf_msbfa_event_t  *sf_msbfa_event_at (const sf_msbfa_t *m, int32_t idx);
SF_API const sf_msbfa_region_t *sf_msbfa_region_at(const sf_msbfa_t *m, int32_t idx);
SF_API const sf_msbfa_part_t   *sf_msbfa_part_at  (const sf_msbfa_t *m, int32_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SOULS_FORMATS_SF_MSBFA_H */
