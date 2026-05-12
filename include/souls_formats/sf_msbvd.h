/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Armored Core Verdict Day MSB map layout container.
 */

#ifndef SOULS_FORMATS_SF_MSBVD_H
#define SOULS_FORMATS_SF_MSBVD_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_msb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_msbvd        sf_msbvd_t;
typedef struct sf_msbvd_model  sf_msbvd_model_t;
typedef struct sf_msbvd_event  sf_msbvd_event_t;
typedef struct sf_msbvd_region sf_msbvd_region_t;
typedef struct sf_msbvd_part   sf_msbvd_part_t;

SF_API sf_result_t sf_msbvd_read_from_memory(sf_msbvd_t **out, const uint8_t *data,
                                               size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_msbvd_write_to_memory(const sf_msbvd_t *msb, uint8_t **out_data,
                                             size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_msbvd_destroy(sf_msbvd_t *msb);

SF_API int32_t sf_msbvd_model_count (const sf_msbvd_t *m);
SF_API int32_t sf_msbvd_event_count (const sf_msbvd_t *m);
SF_API int32_t sf_msbvd_region_count(const sf_msbvd_t *m);
SF_API int32_t sf_msbvd_part_count  (const sf_msbvd_t *m);

SF_API const sf_msbvd_model_t  *sf_msbvd_model_at (const sf_msbvd_t *m, int32_t idx);
SF_API const sf_msbvd_event_t  *sf_msbvd_event_at (const sf_msbvd_t *m, int32_t idx);
SF_API const sf_msbvd_region_t *sf_msbvd_region_at(const sf_msbvd_t *m, int32_t idx);
SF_API const sf_msbvd_part_t   *sf_msbvd_part_at  (const sf_msbvd_t *m, int32_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SOULS_FORMATS_SF_MSBVD_H */
