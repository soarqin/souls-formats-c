/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SF_FLVER0_INTERNAL_H
#define SF_FLVER0_INTERNAL_H

#include "souls_formats/sf_flver0.h"
#include "souls_formats/sf_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SF_FLVER0_MAX_BONE_COUNT 28

typedef struct sf_flver0_header {
    bool big_endian;
    uint32_t version;
    int32_t data_offset;
    int32_t data_length;
    int32_t dummy_count;
    int32_t material_count;
    int32_t bone_count;
    int32_t mesh_count;
    int32_t vertex_buffer_count;
    sf_vec3_t bbox_min;
    sf_vec3_t bbox_max;
    int32_t face_count;
    int32_t total_face_count;
    uint8_t vertex_index_size;
    uint8_t unicode;
    uint8_t unk4a;
    uint8_t unk4b;
    int32_t unk4c;
    uint8_t unk5c;
} sf_flver0_header_t;

typedef struct sf_flver0_layout_member {
    int32_t stream;
    int32_t struct_offset;
    sf_flver_layout_type_t type;
    sf_flver_layout_semantic_t semantic;
    int32_t index;
    int16_t special_modifier;
} sf_flver0_layout_member_t;

struct sf_flver0_buffer_layout {
    sf_flver0_layout_member_t *members;
    size_t member_count;
    uint32_t size;
};

struct sf_flver0_texture {
    char *path;
    char *param_name;
};

struct sf_flver0_material {
    char *name;
    char *mtd;
    sf_flver0_texture_t *textures;
    size_t texture_count;
    sf_flver0_buffer_layout_t *layouts;
    size_t layout_count;
};

struct sf_flver0_mesh {
    uint8_t dynamic;
    uint8_t material_index;
    bool cull_backfaces;
    bool triangle_strip;
    int16_t node_index;
    int16_t bone_indices[SF_FLVER0_MAX_BONE_COUNT];
    int16_t used_bone_count;
    uint32_t *indices;
    size_t index_count;
    uint8_t index_size;
    size_t vertex_count;
    int32_t layout_index;
    uint8_t *vertex_bytes;
    size_t vertex_bytes_size;
};

struct sf_flver0 {
    const sf_allocator_t *alloc;
    sf_flver0_header_t header;
    sf_flver_dummy_t *dummies;
    sf_flver0_material_t *materials;
    sf_flver_node_t *nodes;
    sf_flver0_mesh_t *meshes;
};

#endif /* SF_FLVER0_INTERNAL_H */
