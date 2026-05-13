/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — RMB (controller rumble) public surface.
 *
 * Collection of controller rumble effects used across many games.
 * Extension: .rmb. Big-endian variants are auto-detected.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/RMB.cs
 */

#ifndef SOULS_FORMATS_SF_RMB_H
#define SOULS_FORMATS_SF_RMB_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_rmb sf_rmb_t;

typedef struct sf_rmb_state {
    float power;
    float duration;
} sf_rmb_state_t;

SF_API sf_result_t sf_rmb_create(sf_rmb_t **out, const sf_allocator_t *alloc);
SF_API void sf_rmb_destroy(sf_rmb_t *rmb);

SF_API sf_result_t sf_rmb_read_from_memory(sf_rmb_t **out, const void *bytes, size_t size,
                                           const sf_allocator_t *alloc);
SF_API sf_result_t sf_rmb_write_to_memory(const sf_rmb_t *rmb, void **out_bytes,
                                          size_t *out_size, const sf_allocator_t *alloc);

SF_API bool sf_rmb_big_endian(const sf_rmb_t *rmb);
SF_API void sf_rmb_set_big_endian(sf_rmb_t *rmb, bool big_endian);

SF_API size_t sf_rmb_rumble_count(const sf_rmb_t *rmb);
SF_API sf_result_t sf_rmb_add_rumble(sf_rmb_t *rmb, size_t *out_rumble_index);

SF_API sf_result_t sf_rmb_get_heavy_state_count(const sf_rmb_t *rmb, size_t rumble_index,
                                                size_t *out_count);
SF_API sf_result_t sf_rmb_get_light_state_count(const sf_rmb_t *rmb, size_t rumble_index,
                                                size_t *out_count);
SF_API sf_result_t sf_rmb_get_heavy_state(const sf_rmb_t *rmb, size_t rumble_index,
                                          size_t state_index, sf_rmb_state_t *out_state);
SF_API sf_result_t sf_rmb_get_light_state(const sf_rmb_t *rmb, size_t rumble_index,
                                          size_t state_index, sf_rmb_state_t *out_state);

SF_API sf_result_t sf_rmb_add_heavy_state(sf_rmb_t *rmb, size_t rumble_index,
                                          float power, float duration);
SF_API sf_result_t sf_rmb_add_light_state(sf_rmb_t *rmb, size_t rumble_index,
                                          float power, float duration);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_RMB_H */
