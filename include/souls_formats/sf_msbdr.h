/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SOULS_FORMATS_SF_MSBDR_H
#define SOULS_FORMATS_SF_MSBDR_H

#include "sf_common.h"
#include "souls_formats/sf_msb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_msbdr        sf_msbdr_t;
typedef struct sf_msbdr_model  sf_msbdr_model_t;
typedef struct sf_msbdr_event  sf_msbdr_event_t;
typedef struct sf_msbdr_region sf_msbdr_region_t;
typedef struct sf_msbdr_part   sf_msbdr_part_t;

SF_API sf_result_t sf_msbdr_read_from_memory(sf_msbdr_t **out, const uint8_t *data,
                                             size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_msbdr_write_to_memory(const sf_msbdr_t *msbdr, uint8_t **out_data,
                                            size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_msbdr_destroy(sf_msbdr_t *msbdr);

SF_API int32_t sf_msbdr_model_count (const sf_msbdr_t *m);
SF_API int32_t sf_msbdr_event_count (const sf_msbdr_t *m);
SF_API int32_t sf_msbdr_region_count(const sf_msbdr_t *m);
SF_API int32_t sf_msbdr_part_count  (const sf_msbdr_t *m);
SF_API int32_t sf_msbdr_tree_count  (const sf_msbdr_t *m);

SF_API const sf_msbdr_model_t  *sf_msbdr_model_at (const sf_msbdr_t *m, int32_t idx);
SF_API const sf_msbdr_event_t  *sf_msbdr_event_at (const sf_msbdr_t *m, int32_t idx);
SF_API const sf_msbdr_region_t *sf_msbdr_region_at(const sf_msbdr_t *m, int32_t idx);
SF_API const sf_msbdr_part_t   *sf_msbdr_part_at  (const sf_msbdr_t *m, int32_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SOULS_FORMATS_SF_MSBDR_H */
