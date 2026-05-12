/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Internal FLVER2 declarations. Mirrors upstream:
 *   SoulsFormats/Formats/FLVER/FLVER2/FLVER2.cs
 *   SoulsFormats/Formats/FLVER/FLVER2/GXList.cs
 *
 * NEVER include this from a public header.
 */

#ifndef SF_FLVER2_INTERNAL_H
#define SF_FLVER2_INTERNAL_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_flver.h"
#include "souls_formats/sf_flver2.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* FLVERHeader — mirrors FLVER2.cs:FLVERHeader read/write layout. */
typedef struct sf_flver2_header {
    uint32_t  version;
    int32_t   data_offset;
    int32_t   data_length;
    int32_t   dummy_count;
    int32_t   material_count;
    int32_t   bone_count;
    int32_t   mesh_count;
    int32_t   vertex_buffer_count;
    sf_vec3_t bbox_min;
    sf_vec3_t bbox_max;
    int32_t   face_count;
    int32_t   unk_face_count;
    int32_t   vertex_indices_size;
    int32_t   face_set_count;
    int32_t   buffer_layout_count;
    int32_t   texture_count;
    uint8_t   unicode;
    uint8_t   unk4a;
    uint8_t   unk4b;
    int32_t   unk4c;
    uint8_t   unk5c;
    uint8_t   unk5d;
    int32_t   unk68;
    int16_t   special_modifier;
    int32_t   unk74;
} sf_flver2_header_t;

typedef struct sf_flver2_gx_item {
    uint32_t id;
    uint32_t unk04;
    uint8_t *data;
    size_t   data_size;
} sf_flver2_gx_item_t;

typedef struct sf_flver2_gx_list {
    sf_flver2_gx_item_t *items;
    size_t               count;
    int32_t              terminator_id;
    int32_t              terminator_length;
} sf_flver2_gx_list_t;

/* Texture — mirrors upstream Texture.cs.
 *
 * Upstream field names map to ours as:
 *   ParamName       -> param_name
 *   Path            -> path
 *   TilingScale     -> tiling_scale
 *   TilingTypeU/V   -> tiling_type_u / tiling_type_v
 *   Unk14/Unk18/Unk1C -> unk14 / unk18 / unk1c
 */
struct sf_flver2_texture {
    char *param_name; /* UTF-8 heap-owned, upstream ParamName */
    char *path;       /* UTF-8 heap-owned */
    sf_vec2_t tiling_scale;
    sf_flver2_tiling_type_t tiling_type_u;
    sf_flver2_tiling_type_t tiling_type_v;
    float unk14;
    float unk18;
    float unk1c;
};

/* Material — mirrors upstream Material.cs.
 *
 * Upstream field names map to ours as:
 *   Name      -> name
 *   MTD       -> mtd
 *   Textures  -> textures (post-TakeTextures); count in texture_count
 *   GXIndex   -> gx_index    (index into flver2->gx_lists; -1 if none)
 *   Index     -> index
 *
 * pretake_texture_index / pretake_texture_count are scratch fields used
 * between material_read() and take_textures() to remember the wire-format
 * texture-index / texture-count range. After take_textures runs, both are
 * set to -1 (mirrors upstream's `textureIndex = textureCount = -1` reset).
 */
struct sf_flver2_material {
    char *name;
    char *mtd;
    sf_flver2_texture_t *textures;
    size_t               texture_count;
    int32_t              gx_index;
    int32_t              index;

    /* Read-only scratch: cleared by take_textures. */
    int32_t pretake_texture_index;
    int32_t pretake_texture_count;
};

struct sf_flver2_face_set {
    sf_flver2_fs_flags_t flags;
    bool                 triangle_strip;
    bool                 cull_backfaces;
    uint8_t              unk06;
    uint8_t              unk07;
    uint32_t            *indices;
    size_t               index_count;
    uint8_t              index_size;
};

typedef struct sf_flver2_layout_member {
    int32_t                    stream;
    int32_t                    struct_offset;
    sf_flver_layout_type_t     type;
    sf_flver_layout_semantic_t semantic;
    int32_t                    index;
    int16_t                    special_modifier; /* -32768 = SpeedTree sentinel (zero bytes) */
} sf_flver2_layout_member_t;

struct sf_flver2_buffer_layout {
    sf_flver2_layout_member_t *members;
    size_t                     member_count;
};

struct sf_flver2_vertex_buffer {
    int32_t  buffer_index;
    int32_t  layout_index;
    int32_t  vertex_size;
    int32_t  vertex_count;
    uint8_t *vertex_bytes; /* raw bytes, opaque */
    size_t   vertex_bytes_size;
};

struct sf_flver2_mesh {
    bool      use_bone_weights;
    int32_t   material_index;
    int32_t   node_index;
    int32_t  *bone_indices;
    size_t    bone_index_count;
    int32_t  *face_set_indices;
    size_t    face_set_index_count;
    int32_t  *vertex_buffer_indices;
    size_t    vertex_buffer_index_count;
    /* Optional BoundingBoxes sub-struct (Mesh.cs:269). Only present when
     * the on-disk `boundingBoxOffset` is non-zero. `bbox_unk` is only
     * meaningful (and only present in the file) for version >= 0x2001A. */
    bool      has_bounding_box;
    sf_vec3_t bbox_min;
    sf_vec3_t bbox_max;
    sf_vec3_t bbox_unk;
};

struct sf_flver2_bone {
    int16_t parent_index;
    int16_t first_child_index;
    int16_t next_sibling_index;
    int16_t previous_sibling_index;
    int32_t node_index;
};

struct sf_flver2_skeleton_set {
    sf_flver2_bone_t *base_bones;
    size_t            base_bone_count;
    sf_flver2_bone_t *all_bones;
    size_t            all_bone_count;
};

typedef struct sf_flver2 {
    const sf_allocator_t       *alloc;
    sf_flver2_header_t          header;
    sf_flver_dummy_t           *dummies;
    sf_flver2_material_t       *materials;
    sf_flver_node_t            *nodes;
    sf_flver2_mesh_t           *meshes;
    sf_flver2_face_set_t       *face_sets;
    sf_flver2_vertex_buffer_t  *vertex_buffers;
    sf_flver2_buffer_layout_t  *buffer_layouts;
    sf_flver2_texture_t        *textures;
    sf_flver2_skeleton_set_t   *skeleton_set;
    sf_flver2_gx_list_t        *gx_lists;
    size_t                      gx_list_count;
    int32_t                    *gx_offsets_internal;
} sf_flver2_t;

/* Future submodule contracts. T13-T18 replace the current flver2.c stubs. */
sf_result_t sfi_flver2_material_read(sf_binary_reader_t *br,
                                     sf_flver2_t *flver,
                                     sf_flver2_material_t *out,
                                     const sf_allocator_t *a);
sf_result_t sfi_flver2_material_write(sf_binary_writer_t *bw,
                                      const sf_flver2_header_t *hdr,
                                      const sf_flver2_material_t *m,
                                      size_t index);
sf_result_t sfi_flver2_material_write_textures(sf_binary_writer_t *bw,
                                               const sf_flver2_header_t *hdr,
                                               const sf_flver2_material_t *m,
                                               size_t mat_index,
                                               size_t texture_index);
sf_result_t sfi_flver2_material_fill_gx_offset(sf_binary_writer_t *bw,
                                               size_t mat_index, int32_t gx_index,
                                               const int32_t *gx_offsets,
                                               size_t gx_offset_count);
sf_result_t sfi_flver2_material_write_strings(sf_binary_writer_t *bw,
                                              const sf_flver2_header_t *hdr,
                                              const sf_flver2_material_t *m,
                                              size_t mat_index,
                                              size_t texture_index);
void sfi_flver2_material_destroy_inplace(sf_flver2_material_t *m,
                                         const sf_allocator_t *a);

sf_result_t sfi_flver2_take_textures(sf_flver2_t *f);

sf_result_t sfi_flver2_mesh_read(sf_binary_reader_t *br, const sf_flver2_header_t *hdr,
                                 sf_flver2_mesh_t *out, const sf_allocator_t *a);
sf_result_t sfi_flver2_mesh_write(sf_binary_writer_t *bw, const sf_flver2_header_t *hdr,
                                  const sf_flver2_mesh_t *m, size_t index);
sf_result_t sfi_flver2_mesh_write_bounding_box(sf_binary_writer_t *bw,
                                               const sf_flver2_header_t *hdr,
                                               const sf_flver2_mesh_t *m, size_t index);
sf_result_t sfi_flver2_mesh_write_bone_indices(sf_binary_writer_t *bw,
                                               const sf_flver2_mesh_t *m, size_t index,
                                               int32_t bone_indices_start);
sf_result_t sfi_flver2_mesh_fill_face_set_indices(sf_binary_writer_t *bw,
                                                  const sf_flver2_mesh_t *m, size_t index);
sf_result_t sfi_flver2_mesh_fill_vertex_buffer_indices(sf_binary_writer_t *bw,
                                                       const sf_flver2_mesh_t *m, size_t index);
void sfi_flver2_mesh_destroy_inplace(sf_flver2_mesh_t *m, const sf_allocator_t *a);

sf_result_t sfi_flver2_face_set_read(sf_binary_reader_t *br,
                                     const sf_flver2_header_t *hdr,
                                     int32_t vertex_indices_size,
                                     int32_t data_offset,
                                     sf_flver2_face_set_t *out,
                                     const sf_allocator_t *a);
sf_result_t sfi_flver2_face_set_write(sf_binary_writer_t *bw,
                                       const sf_flver2_header_t *hdr,
                                       const sf_flver2_face_set_t *fs,
                                       int32_t vertex_indices_size,
                                       size_t index);
sf_result_t sfi_flver2_face_set_write_indices(sf_binary_writer_t *bw,
                                              const sf_flver2_face_set_t *fs,
                                              size_t index,
                                              int32_t data_start);
sf_result_t sfi_flver2_face_set_triangulate(const sf_flver2_face_set_t *fs,
                                            bool filter_degenerate,
                                            uint32_t **out_indices,
                                            size_t *out_count,
                                            const sf_allocator_t *a);
void sfi_flver2_face_set_destroy_inplace(sf_flver2_face_set_t *fs,
                                         const sf_allocator_t *a);

sf_result_t sfi_flver2_vertex_buffer_read(sf_binary_reader_t *br,
                                          const sf_flver2_header_t *hdr,
                                          sf_flver2_vertex_buffer_t *out,
                                          const sf_allocator_t *a);
sf_result_t sfi_flver2_vertex_buffer_write(sf_binary_writer_t *bw,
                                            const sf_flver2_header_t *hdr,
                                            const sf_flver2_vertex_buffer_t *vb,
                                            size_t index);
sf_result_t sfi_flver2_vertex_buffer_write_data(sf_binary_writer_t *bw,
                                                const sf_flver2_vertex_buffer_t *vb,
                                                size_t index,
                                                int32_t data_start);
void sfi_flver2_vertex_buffer_destroy_inplace(sf_flver2_vertex_buffer_t *vb,
                                              const sf_allocator_t *a);

sf_result_t sfi_flver2_buffer_layout_read(sf_binary_reader_t *br,
                                          const sf_flver2_header_t *hdr,
                                          sf_flver2_buffer_layout_t *out,
                                          const sf_allocator_t *a);
sf_result_t sfi_flver2_buffer_layout_write(sf_binary_writer_t *bw,
                                            const sf_flver2_header_t *hdr,
                                            const sf_flver2_buffer_layout_t *bl,
                                            size_t index);
sf_result_t sfi_flver2_buffer_layout_write_members(sf_binary_writer_t *bw,
                                                   const sf_flver2_header_t *hdr,
                                                   const sf_flver2_buffer_layout_t *bl,
                                                   size_t index);
void sfi_flver2_buffer_layout_destroy_inplace(sf_flver2_buffer_layout_t *bl,
                                              const sf_allocator_t *a);
uint32_t sfi_flver2_buffer_layout_size(const sf_flver2_buffer_layout_t *bl);

sf_result_t sfi_flver2_texture_read(sf_binary_reader_t *br,
                                    const sf_flver2_header_t *hdr,
                                    sf_flver2_texture_t *out,
                                    const sf_allocator_t *a);
sf_result_t sfi_flver2_texture_write(sf_binary_writer_t *bw,
                                     const sf_flver2_header_t *hdr,
                                     const sf_flver2_texture_t *t,
                                     size_t index);
sf_result_t sfi_flver2_texture_write_strings(sf_binary_writer_t *bw,
                                              const sf_flver2_header_t *hdr,
                                              const sf_flver2_texture_t *t,
                                              size_t index);
void sfi_flver2_texture_destroy_inplace(sf_flver2_texture_t *t,
                                        const sf_allocator_t *a);

sf_result_t sfi_flver2_skeleton_set_read(sf_binary_reader_t *br,
                                         const sf_flver2_header_t *hdr,
                                         sf_flver2_skeleton_set_t **out,
                                         const sf_allocator_t *a);
sf_result_t sfi_flver2_skeleton_set_write(sf_binary_writer_t *bw,
                                          const sf_flver2_header_t *hdr,
                                          const sf_flver2_skeleton_set_t *set);
void sfi_flver2_skeleton_set_destroy(sf_flver2_skeleton_set_t *set,
                                     const sf_allocator_t *a);

#endif /* SF_FLVER2_INTERNAL_H */

typedef struct sf_flver2_vertex_context {
    float    uv_factor;
    bool     is_ac6;
    uint32_t header_version;
} sf_flver2_vertex_context_t;

typedef struct sf_flver2_decoded_vertex {
    sf_vec3_t  position;
    sf_vec3_t  normal;
    sf_vec4_t  tangent;
    sf_vec3_t  bitangent;
    sf_vec2_t  uvs[8];
    sf_flver_vertex_color_t        colors[4];
    sf_flver_vertex_bone_indices_t bone_indices;
    sf_flver_vertex_bone_weights_t bone_weights;
    uint8_t    uv_count;
    uint8_t    color_count;
    int32_t    normal_w;
} sf_flver2_decoded_vertex_t;

sf_result_t sfi_flver2_vertex_decode_one(
    const sf_flver2_buffer_layout_t *layout,
    const uint8_t *vertex_bytes,
    const sf_flver2_vertex_context_t *ctx,
    sf_flver2_decoded_vertex_t *out);

sf_result_t sfi_flver2_vertex_encode_one(
    const sf_flver2_buffer_layout_t *layout,
    const sf_flver2_decoded_vertex_t *in,
    const sf_flver2_vertex_context_t *ctx,
    uint8_t *vertex_bytes);
