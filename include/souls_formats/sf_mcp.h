/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — MCP navigation volumes (DeS/DS1).
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormatsNEXT @ 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a
 *   SoulsFormats/Formats/MCP.cs
 *
 * A navigation format used in DeS and DS1 that defines a basic graph of
 * connected volumes (rooms). Each room has a map ID, bounding box, and a
 * list of indices into the same MCP for connected rooms.
 *
 * Endianness is auto-detected on read; preserved verbatim on write.
 */

#ifndef SOULS_FORMATS_SF_MCP_H
#define SOULS_FORMATS_SF_MCP_H

#include "sf_common.h"
#include "sf_io.h"
#include "sf_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_mcp      sf_mcp_t;
typedef struct sf_mcp_room sf_mcp_room_t;

SF_API sf_result_t sf_mcp_read_from_memory(sf_mcp_t **out, const void *data,
                                           size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_mcp_write_to_memory(const sf_mcp_t *mcp,
                                          void **out_data, size_t *out_size,
                                          const sf_allocator_t *alloc);
SF_API void sf_mcp_destroy(sf_mcp_t *mcp);

SF_API sf_result_t sf_mcp_create_empty(sf_mcp_t **out, const sf_allocator_t *alloc);
SF_API sf_result_t sf_mcp_append_room (sf_mcp_t *mcp, sf_mcp_room_t **out_room);

SF_API bool    sf_mcp_big_endian(const sf_mcp_t *mcp);
SF_API void    sf_mcp_set_big_endian(sf_mcp_t *mcp, bool be);
SF_API int32_t sf_mcp_unk04(const sf_mcp_t *mcp);
SF_API void    sf_mcp_set_unk04(sf_mcp_t *mcp, int32_t v);

SF_API size_t               sf_mcp_room_count(const sf_mcp_t *mcp);
SF_API const sf_mcp_room_t *sf_mcp_room      (const sf_mcp_t *mcp, size_t i);
SF_API sf_mcp_room_t       *sf_mcp_room_mut  (sf_mcp_t *mcp, size_t i);

SF_API uint32_t  sf_mcp_room_map_id           (const sf_mcp_room_t *room);
SF_API int32_t   sf_mcp_room_local_index      (const sf_mcp_room_t *room);
SF_API sf_vec3_t sf_mcp_room_bbox_min         (const sf_mcp_room_t *room);
SF_API sf_vec3_t sf_mcp_room_bbox_max         (const sf_mcp_room_t *room);
SF_API size_t    sf_mcp_room_connected_count  (const sf_mcp_room_t *room);
SF_API int32_t   sf_mcp_room_connected_index  (const sf_mcp_room_t *room, size_t i);

SF_API void sf_mcp_room_set_map_id     (sf_mcp_room_t *room, uint32_t v);
SF_API void sf_mcp_room_set_local_index(sf_mcp_room_t *room, int32_t  v);
SF_API void sf_mcp_room_set_bbox_min   (sf_mcp_room_t *room, sf_vec3_t v);
SF_API void sf_mcp_room_set_bbox_max   (sf_mcp_room_t *room, sf_vec3_t v);
SF_API sf_result_t sf_mcp_room_append_connected_index(sf_mcp_room_t *room, int32_t idx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_MCP_H */
