/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Ninja Blade MSB map layout container.
 */

#ifndef SOULS_FORMATS_SF_MSBN_H
#define SOULS_FORMATS_SF_MSBN_H

#include "sf_common.h"
#include "souls_formats/sf_msb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_msbn       sf_msbn_t;
typedef struct sf_msbn_model sf_msbn_model_t;
typedef struct sf_msbn_part  sf_msbn_part_t;

SF_API sf_result_t sf_msbn_read_from_memory(sf_msbn_t **out, const uint8_t *data,
                                            size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_msbn_write_to_memory(const sf_msbn_t *msbn, uint8_t **out_data,
                                           size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_msbn_destroy(sf_msbn_t *msbn);

SF_API int32_t sf_msbn_model_count(const sf_msbn_t *m);
SF_API int32_t sf_msbn_part_count (const sf_msbn_t *m);

SF_API const sf_msbn_model_t *sf_msbn_model_at(const sf_msbn_t *m, int32_t idx);
SF_API const sf_msbn_part_t  *sf_msbn_part_at (const sf_msbn_t *m, int32_t idx);

#ifdef __cplusplus
}
#endif

#endif /* SOULS_FORMATS_SF_MSBN_H */
