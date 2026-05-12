/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file sf_flver2.h
 * @brief FLVER2 — modern FromSoftware mesh format (Sekiro / ER / Nightreign / AC6).
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/FLVER/FLVER2/FLVER2.cs
 *   SoulsFormats/Formats/FLVER/FLVER2/FaceSet.cs
 *   SoulsFormats/Formats/FLVER/FLVER2/Texture.cs
 *   SoulsFormats/Formats/FLVER/FLVER2/GXList.cs
 *   SoulsFormats/Formats/FLVER/FLVER2/SkeletonSet.cs
 *   SoulsFormats/Formats/FLVER/FLVER2/Mesh.cs
 *   SoulsFormats/Formats/FLVER/FLVER2/BufferLayout.cs
 *
 * Design adaptations from upstream (see docs/api-mapping/extensions.md):
 *   - BufferLayout / VertexBuffer / FaceSet are MESH-SHARED via global index.
 *     The upstream `Mesh.FaceSets` etc. lists store references that map back
 *     into the FLVER2-level pools; this header exposes the global pools as
 *     `sf_flver2_buffer_layout(f, idx)` / `sf_flver2_vertex_buffer(f, idx)` /
 *     `sf_flver2_face_set(f, idx)` and asks meshes for their list of
 *     indices. This avoids accidental aliasing in C-land.
 *   - Big-endian files are rejected at read time (FromSoftware PS3-era only).
 *   - `sf_flver2_decode_mesh()` is a C-side extension: it lays vertex
 *     attributes out into typed arrays so callers do not have to walk the
 *     buffer-layout / vertex-buffer indirection themselves.
 */
#ifndef SOULS_FORMATS_SF_FLVER2_H
#define SOULS_FORMATS_SF_FLVER2_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_flver.h"
#include "souls_formats/sf_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Opaque forward declarations
 *
 * All FLVER2 aggregate types are opaque to public API consumers; access is
 * via the `sf_flver2_*` accessor functions further down this header.
 *===========================================================================*/
typedef struct sf_flver2               sf_flver2_t;
typedef struct sf_flver2_mesh          sf_flver2_mesh_t;
typedef struct sf_flver2_material      sf_flver2_material_t;
typedef struct sf_flver2_texture       sf_flver2_texture_t;
typedef struct sf_flver2_face_set      sf_flver2_face_set_t;
typedef struct sf_flver2_vertex_buffer sf_flver2_vertex_buffer_t;
typedef struct sf_flver2_buffer_layout sf_flver2_buffer_layout_t;
typedef struct sf_flver2_skeleton_set  sf_flver2_skeleton_set_t;
typedef struct sf_flver2_bone          sf_flver2_bone_t;
typedef struct sf_flver2_gx_list       sf_flver2_gx_list_t;
typedef struct sf_flver2_gx_item       sf_flver2_gx_item_t;

/*===========================================================================
 * Texture.TilingType — wrap mode per UV component
 *
 * Mirrors upstream Texture.cs:13-21 (`public enum TilingType : byte`).
 * Stored as a single byte in the file; values are file-format-defined.
 *===========================================================================*/
typedef enum sf_flver2_tiling_type {
    SF_FLVER2_TILING_TYPE_NONE          = 0,
    SF_FLVER2_TILING_TYPE_REPEAT        = 1,
    SF_FLVER2_TILING_TYPE_MIRROR_REPEAT = 2,
    SF_FLVER2_TILING_TYPE_CLAMP         = 3,
    SF_FLVER2_TILING_TYPE_BORDER        = 4,
    SF_FLVER2_TILING_TYPE_MIRROR_ONCE   = 5,
} sf_flver2_tiling_type_t;

_Static_assert(SF_FLVER2_TILING_TYPE_NONE        == 0, "TilingType drift (NONE)");
_Static_assert(SF_FLVER2_TILING_TYPE_MIRROR_ONCE == 5, "TilingType drift (MIRROR_ONCE)");

/*===========================================================================
 * FaceSet.FSFlags — bit flags on a face set
 *
 * Mirrors upstream FaceSet.cs:19-50 (`[Flags] enum FSFlags : uint`). Wire
 * format is a 32-bit unsigned int, so values 0x40000000 and 0x80000000 do
 * not fit in `int`. Use a `uint32_t` typedef + `#define` constants so the
 * representation is byte-stable regardless of toolchain (same pattern as
 * Binder.Format in sf_binder.h).
 *===========================================================================*/
typedef uint32_t sf_flver2_fs_flags_t;

#define SF_FLVER2_FS_FLAGS_NONE            ((sf_flver2_fs_flags_t)0x00000000U)
#define SF_FLVER2_FS_FLAGS_LOD_LEVEL_1     ((sf_flver2_fs_flags_t)0x01000000U) /* low detail */
#define SF_FLVER2_FS_FLAGS_LOD_LEVEL_2     ((sf_flver2_fs_flags_t)0x02000000U) /* very low detail */
#define SF_FLVER2_FS_FLAGS_LOD_LEVEL_EX    ((sf_flver2_fs_flags_t)0x04000000U) /* extreme low detail */
#define SF_FLVER2_FS_FLAGS_EDGE_COMPRESSED ((sf_flver2_fs_flags_t)0x40000000U) /* v1 OUT-of-scope; read returns SF_ERR_UNSUPPORTED_VERSION */
#define SF_FLVER2_FS_FLAGS_MOTION_BLUR     ((sf_flver2_fs_flags_t)0x80000000U) /* duplicate faceset hint for motion blur */

_Static_assert(SF_FLVER2_FS_FLAGS_NONE         == 0x00000000U, "FSFlags drift (NONE)");
_Static_assert(SF_FLVER2_FS_FLAGS_LOD_LEVEL_1  == 0x01000000U, "FSFlags drift (LOD1)");
_Static_assert(SF_FLVER2_FS_FLAGS_LOD_LEVEL_2  == 0x02000000U, "FSFlags drift (LOD2)");
_Static_assert(SF_FLVER2_FS_FLAGS_LOD_LEVEL_EX == 0x04000000U, "FSFlags drift (LODEX)");
_Static_assert(SF_FLVER2_FS_FLAGS_MOTION_BLUR  == 0x80000000U, "FSFlags drift (MOTION_BLUR)");

/*===========================================================================
 * Read / Write / Destroy
 *===========================================================================*/

/**
 * Parse a FLVER2 from an in-memory buffer. The buffer is not retained;
 * heap copies are made into the returned handle's allocator pool.
 *
 * Big-endian files (`'B','\0'` byte-order tag at offset 0x06) are rejected
 * with `SF_ERR_UNSUPPORTED_VERSION` — v1 is x86_64 LE-only.
 */
SF_API sf_result_t sf_flver2_read_from_memory(sf_flver2_t **out, const void *bytes,
                                              size_t size, const sf_allocator_t *a);

/** Parse a FLVER2 from a UTF-16 path. */
SF_API sf_result_t sf_flver2_read_from_path(sf_flver2_t **out, const wchar_t *path,
                                            const sf_allocator_t *a);

/** Serialise to a freshly-allocated heap buffer. Always emits little-endian. */
SF_API sf_result_t sf_flver2_write_to_memory(const sf_flver2_t *f, void **out_bytes,
                                             size_t *out_size, const sf_allocator_t *a);

/** Serialise to a UTF-16 path. Uses the FLVER2's internal allocator. */
SF_API sf_result_t sf_flver2_write_to_path(const sf_flver2_t *f, const wchar_t *path);

/** Release a FLVER2 and every buffer it owns. NULL-safe. */
SF_API void sf_flver2_destroy(sf_flver2_t *f);

/*===========================================================================
 * Header accessors (FLVERHeader fields)
 *===========================================================================*/

/** Version word from the header (e.g. 0x20013 for Sekiro, 0x20021 Nightreign). */
SF_API uint32_t  sf_flver2_header_version(const sf_flver2_t *f);
/** `true` if the file uses UTF-16 strings (newer games); `false` for Shift-JIS. */
SF_API bool      sf_flver2_header_unicode(const sf_flver2_t *f);
/** Lower bound of the model-level bounding box. */
SF_API sf_vec3_t sf_flver2_header_bounding_box_min(const sf_flver2_t *f);
/** Upper bound of the model-level bounding box. */
SF_API sf_vec3_t sf_flver2_header_bounding_box_max(const sf_flver2_t *f);

/*===========================================================================
 * Top-level count accessors
 *===========================================================================*/

SF_API size_t sf_flver2_dummy_count        (const sf_flver2_t *f);
SF_API size_t sf_flver2_node_count         (const sf_flver2_t *f);
SF_API size_t sf_flver2_material_count     (const sf_flver2_t *f);
SF_API size_t sf_flver2_mesh_count         (const sf_flver2_t *f);
SF_API size_t sf_flver2_buffer_layout_count(const sf_flver2_t *f);
SF_API size_t sf_flver2_vertex_buffer_count(const sf_flver2_t *f);
SF_API size_t sf_flver2_face_set_count     (const sf_flver2_t *f);
SF_API size_t sf_flver2_texture_count      (const sf_flver2_t *f);

/*===========================================================================
 * Top-level index accessors
 *
 * NOTE: `buffer_layout`, `vertex_buffer`, and `face_set` are GLOBAL pools
 * shared by every mesh. To enumerate a mesh's face sets, ask the mesh for
 * its face-set-index count and then look the index up in the global pool:
 *
 *   size_t n = sf_flver2_mesh_face_set_index_count(mesh);
 *   for (size_t i = 0; i < n; ++i) {
 *       int32_t idx = sf_flver2_mesh_face_set_index(mesh, i);
 *       const sf_flver2_face_set_t *fs = sf_flver2_face_set(f, (size_t)idx);
 *       ...
 *   }
 *
 * All accessors return NULL when the index is out of range.
 *===========================================================================*/
SF_API const sf_flver_dummy_t           *sf_flver2_dummy        (const sf_flver2_t *f, size_t i);
SF_API const sf_flver_node_t            *sf_flver2_node         (const sf_flver2_t *f, size_t i);
SF_API const sf_flver2_material_t       *sf_flver2_material     (const sf_flver2_t *f, size_t i);
SF_API const sf_flver2_mesh_t           *sf_flver2_mesh         (const sf_flver2_t *f, size_t i);
SF_API const sf_flver2_buffer_layout_t  *sf_flver2_buffer_layout(const sf_flver2_t *f, size_t i);
SF_API const sf_flver2_vertex_buffer_t  *sf_flver2_vertex_buffer(const sf_flver2_t *f, size_t i);
SF_API const sf_flver2_face_set_t       *sf_flver2_face_set     (const sf_flver2_t *f, size_t i);
SF_API const sf_flver2_texture_t        *sf_flver2_texture      (const sf_flver2_t *f, size_t i);

/*===========================================================================
 * Mesh field accessors
 *
 * Meshes carry indices into the global pools above, not direct pointers.
 *===========================================================================*/

/** `true` if vertices are skinned via weights+indices; `false` if rigidly
 *  attached to a single node via `NormalW`. Upstream `Mesh.UseBoneWeights`. */
SF_API bool    sf_flver2_mesh_use_bone_weights(const sf_flver2_mesh_t *m);
/** Index into the material pool. */
SF_API int32_t sf_flver2_mesh_material_index(const sf_flver2_mesh_t *m);
/** Index into the node pool (rigid-bind node when not skinned). */
SF_API int32_t sf_flver2_mesh_node_index(const sf_flver2_mesh_t *m);
/** Number of node indices referenced by this mesh's vertices. */
SF_API size_t  sf_flver2_mesh_bone_index_count(const sf_flver2_mesh_t *m);
/** Node index referenced by this mesh at slot `i`. -1 on out-of-range. */
SF_API int32_t sf_flver2_mesh_bone_index(const sf_flver2_mesh_t *m, size_t i);
/** Number of face-set indices owned by this mesh. */
SF_API size_t  sf_flver2_mesh_face_set_index_count(const sf_flver2_mesh_t *m);
/** Global face-set index for this mesh's `i`-th face set. -1 on OOR. */
SF_API int32_t sf_flver2_mesh_face_set_index(const sf_flver2_mesh_t *m, size_t i);
/** Number of vertex-buffer indices owned by this mesh. */
SF_API size_t  sf_flver2_mesh_vertex_buffer_index_count(const sf_flver2_mesh_t *m);
/** Global vertex-buffer index for this mesh's `i`-th buffer. -1 on OOR. */
SF_API int32_t sf_flver2_mesh_vertex_buffer_index(const sf_flver2_mesh_t *m, size_t i);

/*===========================================================================
 * FaceSet field accessors
 *===========================================================================*/

SF_API sf_flver2_fs_flags_t sf_flver2_face_set_flags(const sf_flver2_face_set_t *fs);
SF_API bool                 sf_flver2_face_set_triangle_strip(const sf_flver2_face_set_t *fs);
SF_API bool                 sf_flver2_face_set_cull_backfaces(const sf_flver2_face_set_t *fs);
SF_API uint8_t              sf_flver2_face_set_unk06(const sf_flver2_face_set_t *fs);
SF_API uint8_t              sf_flver2_face_set_unk07(const sf_flver2_face_set_t *fs);
SF_API uint8_t              sf_flver2_face_set_index_size(const sf_flver2_face_set_t *fs);
SF_API size_t               sf_flver2_face_set_index_count(const sf_flver2_face_set_t *fs);
SF_API uint32_t             sf_flver2_face_set_index(const sf_flver2_face_set_t *fs,
                                                     size_t i);

/*===========================================================================
 * GXList — opaque transit
 *
 * Upstream `GXItem.Data` is an opaque `byte[]`; we mirror that with a
 * pointer+size pair. ID is exposed as the raw 4 wire-format bytes packed
 * as a `uint32_t` (DS2/older versions store an int; newer versions store a
 * 4-char fixstr — both fit). Callers wanting the string form can `memcpy`
 * the four bytes through a `char[5]` buffer themselves.
 *===========================================================================*/

/**
 * Return the GXList referenced by mesh `mesh_index`'s material. Returns
 * NULL if `mesh_index` is out of range or the material has no GX list.
 */
SF_API const sf_flver2_gx_list_t *sf_flver2_gx_list(const sf_flver2_t *f, size_t mesh_index);

SF_API size_t                     sf_flver2_gx_list_item_count(const sf_flver2_gx_list_t *gx);
SF_API const sf_flver2_gx_item_t *sf_flver2_gx_item(const sf_flver2_gx_list_t *gx, size_t i);

/** Raw 4-byte ID as a uint32_t (LE byte order of the original ASCII fixstr). */
SF_API uint32_t                   sf_flver2_gx_item_id    (const sf_flver2_gx_item_t *item);
/** Upstream `Unk04` (typically 100). */
SF_API uint32_t                   sf_flver2_gx_item_unk04 (const sf_flver2_gx_item_t *item);
/** Opaque parameter blob. `out_size` receives the byte count. */
SF_API const uint8_t             *sf_flver2_gx_item_data  (const sf_flver2_gx_item_t *item, size_t *out_size);

/*===========================================================================
 * SkeletonSet — Sekiro and forward (version >= 0x2001A)
 *
 * `sf_flver2_skeleton_set()` returns NULL for older versions (DS3 / BB and
 * earlier do not write a skeleton block).
 *===========================================================================*/
SF_API const sf_flver2_skeleton_set_t *sf_flver2_skeleton_set(const sf_flver2_t *f);

SF_API size_t                  sf_flver2_skeleton_set_base_count(const sf_flver2_skeleton_set_t *set);
SF_API const sf_flver2_bone_t *sf_flver2_skeleton_set_base_bone (const sf_flver2_skeleton_set_t *set, size_t i);
SF_API size_t                  sf_flver2_skeleton_set_all_count (const sf_flver2_skeleton_set_t *set);
SF_API const sf_flver2_bone_t *sf_flver2_skeleton_set_all_bone  (const sf_flver2_skeleton_set_t *set, size_t i);

SF_API int16_t sf_flver2_bone_parent_index          (const sf_flver2_bone_t *b);
SF_API int16_t sf_flver2_bone_first_child_index     (const sf_flver2_bone_t *b);
SF_API int16_t sf_flver2_bone_next_sibling_index    (const sf_flver2_bone_t *b);
SF_API int16_t sf_flver2_bone_previous_sibling_index(const sf_flver2_bone_t *b);
SF_API int32_t sf_flver2_bone_node_index            (const sf_flver2_bone_t *b);

/*===========================================================================
 * Decoded-mesh extension
 *
 * Lays a single mesh's vertex/index data out into typed arrays so callers
 * never have to walk the BufferLayout / VertexBuffer indirection. This is
 * the high-level "I want a position+normal+uv+indices array" entry point.
 *
 * All array pointers are NULL if the mesh has no data for that attribute.
 * Heap-allocate arrays are owned by the caller; free with
 * `sf_flver2_decoded_mesh_free(mesh, allocator)`.
 *
 * Documented as extension in docs/api-mapping/extensions.md.
 *===========================================================================*/
typedef struct sf_flver2_decoded_mesh {
    uint32_t                        vertex_count;
    sf_vec3_t                      *positions;     /**< always present after decode  */
    sf_vec3_t                      *normals;       /**< NULL if absent               */
    sf_vec4_t                      *tangents;      /**< NULL if absent               */
    sf_vec3_t                      *bitangents;    /**< NULL if absent               */
    sf_vec2_t                      *uvs[8];        /**< up to 8 channels             */
    sf_flver_vertex_color_t        *colors[4];     /**< up to 4 channels             */
    sf_flver_vertex_bone_indices_t *bone_indices;  /**< 1 per vertex if skinned     */
    sf_flver_vertex_bone_weights_t *bone_weights;  /**< 1 per vertex if skinned     */
    uint32_t                       *indices;       /**< concatenated triangulated   */
    uint32_t                        index_count;
} sf_flver2_decoded_mesh_t;

/**
 * Decode mesh `mesh_index` into `*out`. Existing fields of `*out` are
 * overwritten. On error, `*out` is left as-is.
 */
SF_API sf_result_t sf_flver2_decode_mesh(const sf_flver2_t *f, size_t mesh_index,
                                         sf_flver2_decoded_mesh_t *out,
                                         const sf_allocator_t *a);

/** Free every heap array inside `*m` using the same allocator as decode.
 *  Zeroes the struct after freeing. NULL-safe. */
SF_API void sf_flver2_decoded_mesh_free(sf_flver2_decoded_mesh_t *m,
                                        const sf_allocator_t *a);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_FLVER2_H */
