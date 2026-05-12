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

struct sf_flver2_texture {
    char *type;
    char *path;
    sf_flver2_tiling_type_t scale_u;
    sf_flver2_tiling_type_t scale_v;
};

struct sf_flver2_material {
    int32_t texture_index;
    int32_t texture_count;
    int32_t gx_index;
};

struct sf_flver2_face_set {
    sf_flver2_fs_flags_t flags;
};

struct sf_flver2_vertex_buffer {
    int32_t buffer_index;
    int32_t layout_index;
};

struct sf_flver2_buffer_layout {
    size_t member_count;
};

struct sf_flver2_mesh {
    bool     use_bone_weights;
    int32_t  material_index;
    int32_t  node_index;
    int32_t *bone_indices;
    size_t   bone_index_count;
    int32_t *face_set_indices;
    size_t   face_set_index_count;
    int32_t *vertex_buffer_indices;
    size_t   vertex_buffer_index_count;
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
} sf_flver2_t;

/* Future submodule contracts. T13-T18 replace the current flver2.c stubs. */
sf_result_t sfi_flver2_material_read(sf_binary_reader_t *br,
                                     const sf_flver2_header_t *hdr,
                                     sf_flver2_material_t *out,
                                     const sf_allocator_t *a);
sf_result_t sfi_flver2_material_write(sf_binary_writer_t *bw,
                                      const sf_flver2_header_t *hdr,
                                      const sf_flver2_material_t *m,
                                      size_t index);
void sfi_flver2_material_destroy_inplace(sf_flver2_material_t *m,
                                         const sf_allocator_t *a);

sf_result_t sfi_flver2_mesh_read(sf_binary_reader_t *br, const sf_flver2_header_t *hdr,
                                 sf_flver2_mesh_t *out, const sf_allocator_t *a);
sf_result_t sfi_flver2_mesh_write(sf_binary_writer_t *bw, const sf_flver2_header_t *hdr,
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

sf_result_t sfi_flver2_vertex_buffer_read(sf_binary_reader_t *br,
                                          const sf_flver2_header_t *hdr,
                                          sf_flver2_vertex_buffer_t *out,
                                          const sf_allocator_t *a);
sf_result_t sfi_flver2_vertex_buffer_write(sf_binary_writer_t *bw,
                                           const sf_flver2_header_t *hdr,
                                           const sf_flver2_vertex_buffer_t *vb,
                                           size_t index);

sf_result_t sfi_flver2_buffer_layout_read(sf_binary_reader_t *br,
                                          const sf_flver2_header_t *hdr,
                                          sf_flver2_buffer_layout_t *out,
                                          const sf_allocator_t *a);
sf_result_t sfi_flver2_buffer_layout_write(sf_binary_writer_t *bw,
                                           const sf_flver2_header_t *hdr,
                                           const sf_flver2_buffer_layout_t *bl,
                                           size_t index);

sf_result_t sfi_flver2_texture_read(sf_binary_reader_t *br,
                                    const sf_flver2_header_t *hdr,
                                    sf_flver2_texture_t *out,
                                    const sf_allocator_t *a);
sf_result_t sfi_flver2_texture_write(sf_binary_writer_t *bw,
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
