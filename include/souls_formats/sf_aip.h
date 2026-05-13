/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — AIP (Auto Invasion Points) public surface.
 *
 * AutoInvadePoint format used for invasion spawn points in the
 * overworld in Elden Ring. Little-endian only.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/AIP.cs
 */

#ifndef SOULS_FORMATS_SF_AIP_H
#define SOULS_FORMATS_SF_AIP_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_aip sf_aip_t;

/* Block ID (also known as map ID) for the invasion points. */
typedef struct sf_aip_block_id {
    uint8_t index;
    uint8_t region;
    uint8_t block;
    uint8_t area;
} sf_aip_block_id_t;

/* A single auto-invade point: position + Y-axis rotation. */
typedef struct sf_aip_point {
    float x;
    float y;
    float z;
    float rotation;
} sf_aip_point_t;

SF_API sf_result_t sf_aip_create(sf_aip_t **out, const sf_allocator_t *alloc);
SF_API void sf_aip_destroy(sf_aip_t *aip);

SF_API sf_result_t sf_aip_read_from_memory(sf_aip_t **out, const void *bytes, size_t size,
                                           const sf_allocator_t *alloc);
SF_API sf_result_t sf_aip_write_to_memory(const sf_aip_t *aip, void **out_bytes,
                                          size_t *out_size, const sf_allocator_t *alloc);

SF_API bool sf_aip_is(const void *bytes, size_t size);

SF_API uint32_t sf_aip_version(const sf_aip_t *aip);
SF_API void sf_aip_set_version(sf_aip_t *aip, uint32_t version);

SF_API sf_aip_block_id_t sf_aip_block_id(const sf_aip_t *aip);
SF_API void sf_aip_set_block_id(sf_aip_t *aip, sf_aip_block_id_t block_id);

SF_API size_t sf_aip_point_count(const sf_aip_t *aip);
SF_API sf_result_t sf_aip_get_point(const sf_aip_t *aip, size_t index, sf_aip_point_t *out);
SF_API sf_result_t sf_aip_add_point(sf_aip_t *aip, sf_aip_point_t point);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_AIP_H */
