/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Elden Ring MSB map layout container.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/MSB/MSBE/MSBE.cs
 */

#ifndef SOULS_FORMATS_SF_MSBE_H
#define SOULS_FORMATS_SF_MSBE_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_msb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MSBE opaque types. MSBE has 6 wire-segments: Models, Events, Regions,
 * Routes, Layers, Parts. The Layers segment is an EmptyParam (no typed
 * layer entry) so no sf_msbe_layer_t is exposed. */
typedef struct sf_msbe        sf_msbe_t;
typedef struct sf_msbe_model  sf_msbe_model_t;
typedef struct sf_msbe_event  sf_msbe_event_t;
typedef struct sf_msbe_region sf_msbe_region_t;
typedef struct sf_msbe_part   sf_msbe_part_t;
typedef struct sf_msbe_route  sf_msbe_route_t;

/* Read/Write/Destroy */
SF_API sf_result_t sf_msbe_read_from_memory(sf_msbe_t **out, const uint8_t *data,
                                            size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_msbe_write_to_memory(const sf_msbe_t *msbe, uint8_t **out_data,
                                           size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_msbe_destroy(sf_msbe_t *msbe);

/* Entry count accessors */
SF_API int32_t sf_msbe_model_count (const sf_msbe_t *m);
SF_API int32_t sf_msbe_event_count (const sf_msbe_t *m);
SF_API int32_t sf_msbe_region_count(const sf_msbe_t *m);
SF_API int32_t sf_msbe_part_count  (const sf_msbe_t *m);
SF_API int32_t sf_msbe_route_count (const sf_msbe_t *m);

/* Entry at index accessors */
SF_API const sf_msbe_model_t  *sf_msbe_model_at (const sf_msbe_t *m, int32_t idx);
SF_API const sf_msbe_event_t  *sf_msbe_event_at (const sf_msbe_t *m, int32_t idx);
SF_API const sf_msbe_region_t *sf_msbe_region_at(const sf_msbe_t *m, int32_t idx);
SF_API const sf_msbe_part_t   *sf_msbe_part_at  (const sf_msbe_t *m, int32_t idx);
SF_API const sf_msbe_route_t  *sf_msbe_route_at (const sf_msbe_t *m, int32_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SOULS_FORMATS_SF_MSBE_H */
