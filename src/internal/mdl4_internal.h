/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SF_MDL4_INTERNAL_H
#define SF_MDL4_INTERNAL_H

#include "souls_formats/sf_mdl4.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SF_MDL4_MAX_BONE_COUNT 28

typedef struct sf_mdl4_header {
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
} sf_mdl4_header_t;

struct sf_mdl4_dummy {
    sf_vec3_t position;
    sf_vec3_t forward;
    uint32_t color;
    int16_t reference_id;
    int16_t parent_bone_index;
    int16_t attach_bone_index;
    int16_t unk22;
};

struct sf_mdl4_material_param {
    sf_mdl4_param_type_t type;
    char *name;
    int32_t int_value;
    float float_value;
    float float4_value[4];
    char *string_value;
};

struct sf_mdl4_material {
    char *name;
    char *shader;
    uint8_t unk3c;
    uint8_t unk3d;
    uint8_t unk3e;
    sf_mdl4_material_param_t *params;
    size_t param_count;
};

struct sf_mdl4_node {
    char *name;
    sf_vec3_t translation;
    sf_vec3_t rotation;
    sf_vec3_t scale;
    sf_vec3_t bbox_min;
    sf_vec3_t bbox_max;
    int16_t parent_index;
    int16_t first_child_index;
    int16_t next_sibling_index;
    int16_t previous_sibling_index;
    int16_t unk_indices[16];
};

struct sf_mdl4_mesh {
    uint8_t vertex_format;
    uint8_t material_index;
    bool unk02;
    bool unk03;
    int16_t unk08;
    int16_t bone_indices[SF_MDL4_MAX_BONE_COUNT];
    uint16_t *indices;
    size_t index_count;
    uint8_t *vertex_bytes;
    size_t vertex_bytes_size;
    size_t vertex_count;
    uint8_t *unk_blocks[16];
    size_t unk_block_sizes[16];
};

struct sf_mdl4 {
    const sf_allocator_t *alloc;
    sf_mdl4_header_t header;
    sf_mdl4_dummy_t *dummies;
    sf_mdl4_material_t *materials;
    sf_mdl4_node_t *nodes;
    sf_mdl4_mesh_t *meshes;
};

#endif /* SF_MDL4_INTERNAL_H */
