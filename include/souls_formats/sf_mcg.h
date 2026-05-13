/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — MCG navigation graph (DeS/DS1).
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormatsNEXT @ 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a
 *   SoulsFormats/Formats/MCG.cs
 *
 * A coarse navigation graph used in DeS and DS1 for moving around a map.
 * Nodes are vertices with positions; edges connect pairs of nodes and
 * carry references to the MCP room they belong to and the parent map.
 *
 * Endianness is auto-detected on read; preserved verbatim on write.
 */

#ifndef SOULS_FORMATS_SF_MCG_H
#define SOULS_FORMATS_SF_MCG_H

#include "sf_common.h"
#include "souls_formats/sf_io.h"
#include "sf_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_mcg      sf_mcg_t;
typedef struct sf_mcg_node sf_mcg_node_t;
typedef struct sf_mcg_edge sf_mcg_edge_t;

SF_API sf_result_t sf_mcg_read_from_memory(sf_mcg_t **out, const void *data,
                                           size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_mcg_write_to_memory(const sf_mcg_t *mcg,
                                          void **out_data, size_t *out_size,
                                          const sf_allocator_t *alloc);
SF_API void sf_mcg_destroy(sf_mcg_t *mcg);

SF_API sf_result_t sf_mcg_create_empty(sf_mcg_t **out, const sf_allocator_t *alloc);
SF_API sf_result_t sf_mcg_append_node (sf_mcg_t *mcg, sf_mcg_node_t **out_node);
SF_API sf_result_t sf_mcg_append_edge (sf_mcg_t *mcg, sf_mcg_edge_t **out_edge);

SF_API bool    sf_mcg_big_endian(const sf_mcg_t *mcg);
SF_API void    sf_mcg_set_big_endian(sf_mcg_t *mcg, bool be);
SF_API int32_t sf_mcg_unk04(const sf_mcg_t *mcg);
SF_API void    sf_mcg_set_unk04(sf_mcg_t *mcg, int32_t v);
SF_API int32_t sf_mcg_unk18(const sf_mcg_t *mcg);
SF_API void    sf_mcg_set_unk18(sf_mcg_t *mcg, int32_t v);
SF_API int32_t sf_mcg_unk1c(const sf_mcg_t *mcg);
SF_API void    sf_mcg_set_unk1c(sf_mcg_t *mcg, int32_t v);

SF_API size_t               sf_mcg_node_count(const sf_mcg_t *mcg);
SF_API const sf_mcg_node_t *sf_mcg_node      (const sf_mcg_t *mcg, size_t i);
SF_API sf_mcg_node_t       *sf_mcg_node_mut  (sf_mcg_t *mcg, size_t i);

SF_API size_t               sf_mcg_edge_count(const sf_mcg_t *mcg);
SF_API const sf_mcg_edge_t *sf_mcg_edge      (const sf_mcg_t *mcg, size_t i);
SF_API sf_mcg_edge_t       *sf_mcg_edge_mut  (sf_mcg_t *mcg, size_t i);

SF_API sf_vec3_t sf_mcg_node_position             (const sf_mcg_node_t *node);
SF_API size_t    sf_mcg_node_connected_node_count (const sf_mcg_node_t *node);
SF_API int32_t   sf_mcg_node_connected_node_index (const sf_mcg_node_t *node, size_t i);
SF_API size_t    sf_mcg_node_connected_edge_count (const sf_mcg_node_t *node);
SF_API int32_t   sf_mcg_node_connected_edge_index (const sf_mcg_node_t *node, size_t i);
SF_API int32_t   sf_mcg_node_unk18                (const sf_mcg_node_t *node);
SF_API int32_t   sf_mcg_node_unk1c                (const sf_mcg_node_t *node);

SF_API void sf_mcg_node_set_position(sf_mcg_node_t *node, sf_vec3_t v);
SF_API void sf_mcg_node_set_unk18   (sf_mcg_node_t *node, int32_t v);
SF_API void sf_mcg_node_set_unk1c   (sf_mcg_node_t *node, int32_t v);
SF_API sf_result_t sf_mcg_node_append_connected_node_index(sf_mcg_node_t *node, int32_t idx);
SF_API sf_result_t sf_mcg_node_append_connected_edge_index(sf_mcg_node_t *node, int32_t idx);

SF_API int32_t  sf_mcg_edge_node_index_a  (const sf_mcg_edge_t *edge);
SF_API int32_t  sf_mcg_edge_node_index_b  (const sf_mcg_edge_t *edge);
SF_API int32_t  sf_mcg_edge_mcp_room_index(const sf_mcg_edge_t *edge);
SF_API uint32_t sf_mcg_edge_map_id        (const sf_mcg_edge_t *edge);
SF_API float    sf_mcg_edge_unk20         (const sf_mcg_edge_t *edge);
SF_API size_t   sf_mcg_edge_unk_indices_a_count(const sf_mcg_edge_t *edge);
SF_API int32_t  sf_mcg_edge_unk_indices_a_index(const sf_mcg_edge_t *edge, size_t i);
SF_API size_t   sf_mcg_edge_unk_indices_b_count(const sf_mcg_edge_t *edge);
SF_API int32_t  sf_mcg_edge_unk_indices_b_index(const sf_mcg_edge_t *edge, size_t i);

SF_API void sf_mcg_edge_set_node_index_a  (sf_mcg_edge_t *edge, int32_t v);
SF_API void sf_mcg_edge_set_node_index_b  (sf_mcg_edge_t *edge, int32_t v);
SF_API void sf_mcg_edge_set_mcp_room_index(sf_mcg_edge_t *edge, int32_t v);
SF_API void sf_mcg_edge_set_map_id        (sf_mcg_edge_t *edge, uint32_t v);
SF_API void sf_mcg_edge_set_unk20         (sf_mcg_edge_t *edge, float v);
SF_API sf_result_t sf_mcg_edge_append_unk_indices_a(sf_mcg_edge_t *edge, int32_t v);
SF_API sf_result_t sf_mcg_edge_append_unk_indices_b(sf_mcg_edge_t *edge, int32_t v);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_MCG_H */
