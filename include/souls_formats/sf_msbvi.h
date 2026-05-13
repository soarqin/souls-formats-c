/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Armored Core VI MSB map layout container.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/MSB/MSBVI/MSBVI.cs
 */

#ifndef SOULS_FORMATS_SF_MSBVI_H
#define SOULS_FORMATS_SF_MSBVI_H

#include "sf_common.h"
#include "souls_formats/sf_msb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MSBVI opaque types. MSBVI has 6 wire-segments: Models, Events, Regions,
 * Routes, Layers, Parts. UNLIKE MSBE, MSBVI's Layers segment is a TYPED
 * LayerParam (AC6 ships real layer entries), so a sf_msbvi_layer_t handle
 * is exposed alongside the other entry types. */
typedef struct sf_msbvi        sf_msbvi_t;
typedef struct sf_msbvi_model  sf_msbvi_model_t;
typedef struct sf_msbvi_event  sf_msbvi_event_t;
typedef struct sf_msbvi_region sf_msbvi_region_t;
typedef struct sf_msbvi_part   sf_msbvi_part_t;
typedef struct sf_msbvi_route  sf_msbvi_route_t;
typedef struct sf_msbvi_layer  sf_msbvi_layer_t; /* AC6-specific, typed */

/* Read/Write/Destroy */
SF_API sf_result_t sf_msbvi_read_from_memory(sf_msbvi_t **out, const uint8_t *data,
                                             size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_msbvi_write_to_memory(const sf_msbvi_t *msbvi, uint8_t **out_data,
                                            size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_msbvi_destroy(sf_msbvi_t *msbvi);

/* Entry count accessors */
SF_API int32_t sf_msbvi_model_count (const sf_msbvi_t *m);
SF_API int32_t sf_msbvi_event_count (const sf_msbvi_t *m);
SF_API int32_t sf_msbvi_region_count(const sf_msbvi_t *m);
SF_API int32_t sf_msbvi_part_count  (const sf_msbvi_t *m);
SF_API int32_t sf_msbvi_route_count (const sf_msbvi_t *m);
SF_API int32_t sf_msbvi_layer_count (const sf_msbvi_t *m);

/* Entry at index accessors */
SF_API const sf_msbvi_model_t  *sf_msbvi_model_at (const sf_msbvi_t *m, int32_t idx);
SF_API const sf_msbvi_event_t  *sf_msbvi_event_at (const sf_msbvi_t *m, int32_t idx);
SF_API const sf_msbvi_region_t *sf_msbvi_region_at(const sf_msbvi_t *m, int32_t idx);
SF_API const sf_msbvi_part_t   *sf_msbvi_part_at  (const sf_msbvi_t *m, int32_t idx);
SF_API const sf_msbvi_route_t  *sf_msbvi_route_at (const sf_msbvi_t *m, int32_t idx);
SF_API const sf_msbvi_layer_t  *sf_msbvi_layer_at (const sf_msbvi_t *m, int32_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SOULS_FORMATS_SF_MSBVI_H */
