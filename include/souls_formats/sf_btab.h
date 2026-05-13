// Upstream: BTAB.cs
/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SOULS_FORMATS_SF_BTAB_H
#define SOULS_FORMATS_SF_BTAB_H

#include "sf_common.h"
#include "sf_math.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_btab sf_btab_t;
typedef struct sf_btab_entry sf_btab_entry_t;

SF_API sf_result_t sf_btab_read_from_memory(sf_btab_t **out, const void *bytes, size_t size,
                                             const sf_allocator_t *alloc);
SF_API sf_result_t sf_btab_write_to_buffer(const sf_btab_t *btab, void **out_bytes,
                                           size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_btab_destroy(sf_btab_t *btab);

SF_API bool sf_btab_is_big_endian(const sf_btab_t *btab);
SF_API bool sf_btab_is_long_format(const sf_btab_t *btab);
SF_API size_t sf_btab_entry_count(const sf_btab_t *btab);
SF_API const sf_btab_entry_t *sf_btab_get_entry(const sf_btab_t *btab, size_t index);

SF_API const char *sf_btab_entry_part_name(const sf_btab_entry_t *entry);
SF_API const char *sf_btab_entry_material_name(const sf_btab_entry_t *entry);
SF_API int32_t sf_btab_entry_atlas_id(const sf_btab_entry_t *entry);
SF_API sf_vec2_t sf_btab_entry_uv_offset(const sf_btab_entry_t *entry);
SF_API sf_vec2_t sf_btab_entry_uv_scale(const sf_btab_entry_t *entry);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_BTAB_H */
