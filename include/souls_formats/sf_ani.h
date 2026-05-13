/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — ANI (Armored Core For Answer animations) public surface.
 *
 * Big-endian only. Magic: 0x20051014 at offset 0. The format encodes a tree
 * of nodes (bones / dummies) with optional per-node animation data composed
 * of keyframes that index into shared translation and rotation buffers.
 *
 * Upstream describes support as "extremely poor and incomplete". Several
 * fields remain unknown (UnkIndex12, NodeAnimation.Unk10 / Unk20, etc.) but
 * are preserved verbatim for round-trip fidelity.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/ANI.cs
 */

#ifndef SOULS_FORMATS_SF_ANI_H
#define SOULS_FORMATS_SF_ANI_H

#include "sf_common.h"
#include "sf_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Node type — wire int32 at node offset +4
 *
 * Mirrors upstream ANI.Node.NodeType (ANI.cs:200-213).
 *===========================================================================*/
typedef enum sf_ani_node_type {
    SF_ANI_NODE_TYPE_GEOM  = 1,
    SF_ANI_NODE_TYPE_DUMMY = 2,
} sf_ani_node_type_t;

_Static_assert(SF_ANI_NODE_TYPE_GEOM  == 1, "ANI NodeType drift (Geom)");
_Static_assert(SF_ANI_NODE_TYPE_DUMMY == 2, "ANI NodeType drift (Dummy)");

/*===========================================================================
 * Frame format — wire int32 in NodeAnimation header
 *
 * Mirrors upstream ANI.Node.NodeAnimation.FrameFormat (ANI.cs:357-375).
 * Note RotShorts is encoded as 4, not 3 — the enum is sparse.
 *===========================================================================*/
typedef enum sf_ani_frame_format {
    SF_ANI_FRAME_FORMAT_POS_ROT_BYTES  = 1, /* indices stored as bytes  (8 bytes/frame) */
    SF_ANI_FRAME_FORMAT_POS_ROT_SHORTS = 2, /* indices stored as shorts (16 bytes/frame) */
    SF_ANI_FRAME_FORMAT_ROT_SHORTS     = 4, /* only rotation indices    (8 bytes/frame) */
} sf_ani_frame_format_t;

_Static_assert(SF_ANI_FRAME_FORMAT_POS_ROT_BYTES  == 1, "ANI FrameFormat drift (PosRotBytes)");
_Static_assert(SF_ANI_FRAME_FORMAT_POS_ROT_SHORTS == 2, "ANI FrameFormat drift (PosRotShorts)");
_Static_assert(SF_ANI_FRAME_FORMAT_ROT_SHORTS     == 4, "ANI FrameFormat drift (RotShorts)");

/*===========================================================================
 * sf_ani_frame_t — POD value for a single keyframe.
 *
 * Mirrors upstream ANI.Node.NodeAnimation.Frame (ANI.cs:461-633). All seven
 * index fields are normalised to int16 regardless of the parent animation's
 * FrameFormat. Index fields not present in a given format are defaulted by
 * the reader (untouched defaults: translation_*_index = -1 for RotShorts,
 * unk_index = 1 for both PosRotBytes and RotShorts) — see upstream.
 *
 * key_frame is stored as int16 on the wire (this matches upstream's read
 * path; upstream's write path mistakenly emits int32 — see ani.c).
 *===========================================================================*/
typedef struct sf_ani_frame {
    int16_t key_frame;
    int16_t translation_index;
    int16_t translation_in_tangent_index;
    int16_t translation_out_tangent_index;
    int16_t rotation_index;
    int16_t rotation_in_tangent_index;
    int16_t rotation_out_tangent_index;
    int16_t unk_index;
} sf_ani_frame_t;

/*===========================================================================
 * Opaque forward declarations
 *===========================================================================*/
typedef struct sf_ani                 sf_ani_t;
typedef struct sf_ani_node            sf_ani_node_t;
typedef struct sf_ani_node_animation  sf_ani_node_animation_t;

/*===========================================================================
 * Lifecycle and I/O
 *===========================================================================*/

/* Allocate an empty ANI. */
SF_API sf_result_t sf_ani_create(sf_ani_t **out, const sf_allocator_t *alloc);

/* Free an ANI and every nested object it owns. NULL-safe. */
SF_API void sf_ani_destroy(sf_ani_t *a);

/* Parse an ANI from an in-memory buffer. */
SF_API sf_result_t sf_ani_read_from_memory(sf_ani_t **out, const void *bytes,
                                           size_t size, const sf_allocator_t *alloc);

/* Serialise an ANI to a fresh heap-allocated buffer. Caller frees with
 * sf_free(alloc, *out_bytes). */
SF_API sf_result_t sf_ani_write_to_memory(const sf_ani_t *a, void **out_bytes,
                                          size_t *out_size, const sf_allocator_t *alloc);

/* Returns true if @bytes starts with the ANI magic (0x20051014 big-endian
 * int32). At least 64 bytes are required, mirroring upstream's Is() guard. */
SF_API bool sf_ani_is(const void *bytes, size_t size);

/*===========================================================================
 * Translation buffer accessors
 *
 * Stored as raw float32 vec3 on the wire.
 *===========================================================================*/
SF_API size_t      sf_ani_translation_count(const sf_ani_t *a);
SF_API sf_result_t sf_ani_get_translation(const sf_ani_t *a, size_t index,
                                          sf_vec3_t *out);
SF_API sf_result_t sf_ani_add_translation(sf_ani_t *a, sf_vec3_t v);

/*===========================================================================
 * Rotation buffer accessors
 *
 * Stored as int16[3] / 1000.0f on the wire. Reading recovers the float
 * representation; writing applies the inverse quantisation.
 *===========================================================================*/
SF_API size_t      sf_ani_rotation_count(const sf_ani_t *a);
SF_API sf_result_t sf_ani_get_rotation(const sf_ani_t *a, size_t index,
                                       sf_vec3_t *out);
SF_API sf_result_t sf_ani_add_rotation(sf_ani_t *a, sf_vec3_t v);

/*===========================================================================
 * Node accessors
 *
 * Nodes are heap-allocated and owned by the parent ANI. Pointers returned
 * by sf_ani_node_at remain valid until sf_ani_destroy is called (no
 * pointer-invalidating reallocation occurs across add_node calls).
 *===========================================================================*/
SF_API size_t          sf_ani_node_count(const sf_ani_t *a);
SF_API sf_ani_node_t  *sf_ani_node_at   (const sf_ani_t *a, size_t index);

/* Append a fresh node (default-initialised per upstream Node() ctor:
 * type=Geom, indices=-1, translation/rotation=0, scale=(1,1,1), name=""). */
SF_API sf_result_t sf_ani_add_node(sf_ani_t *a, sf_ani_node_t **out_node);

/* Per-node field getters / setters. */
SF_API sf_ani_node_type_t sf_ani_node_type             (const sf_ani_node_t *n);
SF_API void               sf_ani_node_set_type         (sf_ani_node_t *n, sf_ani_node_type_t t);
SF_API int16_t            sf_ani_node_geom_index       (const sf_ani_node_t *n);
SF_API void               sf_ani_node_set_geom_index   (sf_ani_node_t *n, int16_t v);
SF_API int16_t            sf_ani_node_parent_index     (const sf_ani_node_t *n);
SF_API void               sf_ani_node_set_parent_index (sf_ani_node_t *n, int16_t v);
SF_API int16_t            sf_ani_node_first_child_index(const sf_ani_node_t *n);
SF_API void               sf_ani_node_set_first_child_index(sf_ani_node_t *n, int16_t v);
SF_API int16_t            sf_ani_node_next_sibling_index   (const sf_ani_node_t *n);
SF_API void               sf_ani_node_set_next_sibling_index(sf_ani_node_t *n, int16_t v);
SF_API int16_t            sf_ani_node_unk_index_12     (const sf_ani_node_t *n);
SF_API void               sf_ani_node_set_unk_index_12 (sf_ani_node_t *n, int16_t v);
SF_API sf_vec3_t          sf_ani_node_translation      (const sf_ani_node_t *n);
SF_API void               sf_ani_node_set_translation  (sf_ani_node_t *n, sf_vec3_t v);
SF_API sf_vec3_t          sf_ani_node_rotation         (const sf_ani_node_t *n);
SF_API void               sf_ani_node_set_rotation     (sf_ani_node_t *n, sf_vec3_t v);
SF_API sf_vec3_t          sf_ani_node_scale            (const sf_ani_node_t *n);
SF_API void               sf_ani_node_set_scale        (sf_ani_node_t *n, sf_vec3_t v);

/* Returns a borrowed UTF-8 pointer to the node name. Never NULL — empty
 * nodes have an empty string. Lifetime: until sf_ani_destroy or until the
 * next sf_ani_node_set_name on this node. */
SF_API const char *sf_ani_node_name(const sf_ani_node_t *n);

/* Replaces the node name. The ANI deep-copies @utf8. Pass an empty string
 * (or NULL) to clear. */
SF_API sf_result_t sf_ani_node_set_name(sf_ani_node_t *n, const char *utf8);

/*===========================================================================
 * Per-node animation accessors
 *
 * Each node optionally owns at most one NodeAnimation. The opaque pointer
 * returned by sf_ani_node_animation is borrowed from the node and lives
 * until sf_ani_destroy or sf_ani_node_clear_animation.
 *===========================================================================*/

/* Returns the node's animation, or NULL if there is none. */
SF_API sf_ani_node_animation_t *sf_ani_node_animation(const sf_ani_node_t *n);

/* Create-and-attach a fresh animation (default: format=PosRotShorts, empty
 * frames). If the node already has an animation, returns SF_ERR_ALREADY_EXISTS
 * and leaves the existing animation untouched. */
SF_API sf_result_t sf_ani_node_create_animation(sf_ani_node_t *n,
                                                sf_ani_node_animation_t **out_anim);

/* Detach and free the node's animation. NULL-safe; no-op if absent. */
SF_API void sf_ani_node_clear_animation(sf_ani_node_t *n);

/*===========================================================================
 * NodeAnimation accessors
 *===========================================================================*/
SF_API sf_ani_frame_format_t sf_ani_animation_format    (const sf_ani_node_animation_t *anim);
SF_API void                  sf_ani_animation_set_format(sf_ani_node_animation_t *anim,
                                                         sf_ani_frame_format_t fmt);
SF_API sf_vec3_t             sf_ani_animation_unk10     (const sf_ani_node_animation_t *anim);
SF_API void                  sf_ani_animation_set_unk10 (sf_ani_node_animation_t *anim, sf_vec3_t v);
SF_API sf_vec3_t             sf_ani_animation_unk20     (const sf_ani_node_animation_t *anim);
SF_API void                  sf_ani_animation_set_unk20 (sf_ani_node_animation_t *anim, sf_vec3_t v);

/* Frame collection. */
SF_API size_t      sf_ani_animation_frame_count(const sf_ani_node_animation_t *anim);
SF_API sf_result_t sf_ani_animation_get_frame  (const sf_ani_node_animation_t *anim,
                                                size_t index, sf_ani_frame_t *out);
SF_API sf_result_t sf_ani_animation_add_frame  (sf_ani_node_animation_t *anim,
                                                sf_ani_frame_t frame);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_ANI_H */
