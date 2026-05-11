/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Sekiro MSB map layout container.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/MSB/MSBS/MSBS.cs
 */

#ifndef SOULS_FORMATS_SF_MSBS_H
#define SOULS_FORMATS_SF_MSBS_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_msb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MSBS opaque types */
typedef struct sf_msbs        sf_msbs_t;
typedef struct sf_msbs_model  sf_msbs_model_t;
typedef struct sf_msbs_event  sf_msbs_event_t;
typedef struct sf_msbs_region sf_msbs_region_t;
typedef struct sf_msbs_part   sf_msbs_part_t;
typedef struct sf_msbs_route  sf_msbs_route_t;

/* Read/Write/Destroy */
SF_API sf_result_t sf_msbs_read_from_memory(sf_msbs_t **out, const uint8_t *data,
                                            size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_msbs_write_to_memory(const sf_msbs_t *msbs, uint8_t **out_data,
                                           size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_msbs_destroy(sf_msbs_t *msbs);

/* Entry count accessors */
SF_API int32_t sf_msbs_model_count (const sf_msbs_t *m);
SF_API int32_t sf_msbs_event_count (const sf_msbs_t *m);
SF_API int32_t sf_msbs_region_count(const sf_msbs_t *m);
SF_API int32_t sf_msbs_part_count  (const sf_msbs_t *m);
SF_API int32_t sf_msbs_route_count (const sf_msbs_t *m);

/* Entry at index accessors */
SF_API const sf_msbs_model_t  *sf_msbs_model_at (const sf_msbs_t *m, int32_t idx);
SF_API const sf_msbs_event_t  *sf_msbs_event_at (const sf_msbs_t *m, int32_t idx);
SF_API const sf_msbs_region_t *sf_msbs_region_at(const sf_msbs_t *m, int32_t idx);
SF_API const sf_msbs_part_t   *sf_msbs_part_at  (const sf_msbs_t *m, int32_t idx);
SF_API const sf_msbs_route_t  *sf_msbs_route_at (const sf_msbs_t *m, int32_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SOULS_FORMATS_SF_MSBS_H */
