/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file sf_flver.h
 * @brief FLVER common types: Dummy, Node, LayoutMember enums, half-float helpers.
 *
 * Shared by FLVER2 (and future FLVER0). All values mirror upstream
 * SoulsFormatsNEXT at commit 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a.
 */
#ifndef SF_FLVER_H
#define SF_FLVER_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_math.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── POD value types ─────────────────────────────────────────────────────── */

/** Mirrors upstream Dummy.cs public fields (field order matches binary layout). */
typedef struct sf_flver_dummy {
    sf_vec3_t  position;
    uint32_t   color;               /**< ARGB packed u32 */
    sf_vec3_t  forward;
    int16_t    reference_id;
    int16_t    parent_bone_index;
    sf_vec3_t  upward;
    int16_t    attach_bone_index;
    bool       flag1;
    bool       use_upward_vector;
    int32_t    unk30;
    int32_t    unk34;
} sf_flver_dummy_t;

/** Mirrors upstream Node.NodeFlags [Flags] enum. */
typedef enum sf_flver_node_flags {
    SF_FLVER_NODE_FLAG_DISABLED    = 1,
    SF_FLVER_NODE_FLAG_DUMMY_OWNER = 2,
    SF_FLVER_NODE_FLAG_MESH        = 4,
    SF_FLVER_NODE_FLAG_BONE        = 8,
} sf_flver_node_flags_t;

/**
 * Mirrors upstream Node.cs public fields.
 * `name` is UTF-8, heap-owned by the allocator passed to the reader.
 * Index fields default to -1 (no link).
 */
typedef struct sf_flver_node {
    char                  *name;
    int16_t                parent_index;
    int16_t                first_child_index;
    int16_t                next_sibling_index;
    int16_t                previous_sibling_index;
    sf_vec3_t              translation;
    sf_vec3_t              rotation;    /**< XZY Euler radians */
    sf_vec3_t              scale;       /**< default (1,1,1) */
    sf_vec3_t              bbox_min;
    sf_vec3_t              bbox_max;
    sf_flver_node_flags_t  flags;
} sf_flver_node_t;

/** Mirrors upstream VertexColor.cs — four f32 channels. */
typedef struct sf_flver_vertex_color {
    float a, r, g, b;
} sf_flver_vertex_color_t;

/** Mirrors upstream VertexBoneIndices.cs — four i32 bone indices. */
typedef struct sf_flver_vertex_bone_indices {
    int32_t v[4];
} sf_flver_vertex_bone_indices_t;

/** Mirrors upstream VertexBoneWeights.cs — four f32 weights. */
typedef struct sf_flver_vertex_bone_weights {
    float v[4];
} sf_flver_vertex_bone_weights_t;

/* ── LayoutType enum ─────────────────────────────────────────────────────── */
/**
 * Mirrors upstream LayoutMember.cs:LayoutType (uint).
 * Values are FILE-FORMAT-DEFINED and must not be changed.
 */
typedef enum sf_flver_layout_type {
    SF_FLVER_LAYOUT_TYPE_FLOAT1          = 0,
    SF_FLVER_LAYOUT_TYPE_FLOAT2          = 1,
    SF_FLVER_LAYOUT_TYPE_FLOAT3          = 2,
    SF_FLVER_LAYOUT_TYPE_FLOAT4          = 3,
    SF_FLVER_LAYOUT_TYPE_COLOR           = 16,  /**< 0x10 — 4-byte ARGB */
    SF_FLVER_LAYOUT_TYPE_UBYTE4          = 17,
    SF_FLVER_LAYOUT_TYPE_BYTE4           = 18,
    SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM     = 19,
    SF_FLVER_LAYOUT_TYPE_BYTE4_NORM      = 20,
    SF_FLVER_LAYOUT_TYPE_SHORT2          = 21,
    SF_FLVER_LAYOUT_TYPE_SHORT4          = 22,
    SF_FLVER_LAYOUT_TYPE_USHORT2         = 23,
    SF_FLVER_LAYOUT_TYPE_USHORT4         = 24,
    SF_FLVER_LAYOUT_TYPE_SHORT4_NORM     = 26,
    SF_FLVER_LAYOUT_TYPE_HALF2           = 45,
    SF_FLVER_LAYOUT_TYPE_HALF4           = 46,
    SF_FLVER_LAYOUT_TYPE_BYTE4E          = 47,  /**< upstream: Unknown */
    SF_FLVER_LAYOUT_TYPE_EDGE_COMPRESSED = 240, /**< v1 OUT-of-scope; triggers SF_ERR_UNSUPPORTED_VERSION */
} sf_flver_layout_type_t;

_Static_assert(SF_FLVER_LAYOUT_TYPE_EDGE_COMPRESSED == 240, "LayoutType drift");

/* ── LayoutSemantic enum ─────────────────────────────────────────────────── */
/**
 * Mirrors upstream LayoutMember.cs:LayoutSemantic (uint).
 * Note: values 4, 8, 9 are not used upstream (intentional gaps).
 */
typedef enum sf_flver_layout_semantic {
    SF_FLVER_LAYOUT_SEMANTIC_POSITION     = 0,
    SF_FLVER_LAYOUT_SEMANTIC_BONE_WEIGHTS = 1,
    SF_FLVER_LAYOUT_SEMANTIC_BONE_INDICES = 2,
    SF_FLVER_LAYOUT_SEMANTIC_NORMAL       = 3,
    /* 4 unused */
    SF_FLVER_LAYOUT_SEMANTIC_UV           = 5,
    SF_FLVER_LAYOUT_SEMANTIC_TANGENT      = 6,
    SF_FLVER_LAYOUT_SEMANTIC_BITANGENT    = 7,
    /* 8, 9 unused */
    SF_FLVER_LAYOUT_SEMANTIC_VERTEX_COLOR = 10,
} sf_flver_layout_semantic_t;

_Static_assert(SF_FLVER_LAYOUT_SEMANTIC_VERTEX_COLOR == 10, "LayoutSemantic drift");

/* ── Helper function declarations ────────────────────────────────────────── */
/** Convert IEEE 754 half-precision to single-precision. */
SF_API float    sf_half_to_float(uint16_t half);
/** Convert single-precision to IEEE 754 half-precision. */
SF_API uint16_t sf_float_to_half(float f);
/** Unpack a 11_11_10 packed normal/tangent to three floats in [-1, 1]. */
SF_API void     sf_unpack_11_11_10(uint32_t packed, float *out_x, float *out_y, float *out_z);
/** Pack three floats in [-1, 1] to 11_11_10 format. */
SF_API uint32_t sf_pack_11_11_10(float x, float y, float z);
/**
 * Return the byte size of a LayoutType in the vertex stream.
 * If special_modifier == -32768 (SpeedTree sentinel), returns 0.
 * If type == SF_FLVER_LAYOUT_TYPE_EDGE_COMPRESSED, returns UINT32_MAX (sentinel for error).
 */
SF_API uint32_t sf_flver_layout_type_size(sf_flver_layout_type_t t, int32_t special_modifier);

#ifdef __cplusplus
}
#endif

#endif /* SF_FLVER_H */
