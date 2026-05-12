/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Demon's Souls MSB map layout container.
 */

#ifndef SOULS_FORMATS_SF_MSBD_H
#define SOULS_FORMATS_SF_MSBD_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_msb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_msbd        sf_msbd_t;
typedef struct sf_msbd_model  sf_msbd_model_t;
typedef struct sf_msbd_event  sf_msbd_event_t;
typedef struct sf_msbd_region sf_msbd_region_t;
typedef struct sf_msbd_part   sf_msbd_part_t;

SF_API sf_result_t sf_msbd_read_from_memory(sf_msbd_t **out, const uint8_t *data,
                                            size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_msbd_write_to_memory(const sf_msbd_t *msbd, uint8_t **out_data,
                                           size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_msbd_destroy(sf_msbd_t *msbd);

SF_API int32_t sf_msbd_model_count (const sf_msbd_t *m);
SF_API int32_t sf_msbd_event_count (const sf_msbd_t *m);
SF_API int32_t sf_msbd_region_count(const sf_msbd_t *m);
SF_API int32_t sf_msbd_part_count  (const sf_msbd_t *m);
SF_API int32_t sf_msbd_tree_count  (const sf_msbd_t *m);

SF_API const sf_msbd_model_t  *sf_msbd_model_at (const sf_msbd_t *m, int32_t idx);
SF_API const sf_msbd_event_t  *sf_msbd_event_at (const sf_msbd_t *m, int32_t idx);
SF_API const sf_msbd_region_t *sf_msbd_region_at(const sf_msbd_t *m, int32_t idx);
SF_API const sf_msbd_part_t   *sf_msbd_part_at  (const sf_msbd_t *m, int32_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SOULS_FORMATS_SF_MSBD_H */
