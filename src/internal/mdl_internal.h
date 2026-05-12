/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SF_MDL_INTERNAL_H
#define SF_MDL_INTERNAL_H

#include "souls_formats/sf_mdl.h"

#include <stddef.h>
#include <stdint.h>

typedef struct sf_mdl_blob {
    uint8_t *bytes;
    size_t size;
    size_t count;
} sf_mdl_blob_t;

struct sf_mdl {
    const sf_allocator_t *alloc;
    int32_t unk0c;
    int32_t unk10;
    int32_t unk14;
    int32_t node_count;
    int32_t index_count;
    int32_t vertex_count_a;
    int32_t vertex_count_b;
    int32_t vertex_count_c;
    int32_t vertex_count_d;
    int32_t count7;
    int32_t material_count;
    int32_t texture_count;
    uint16_t *indices;
    sf_mdl_blob_t nodes;
    sf_mdl_blob_t vertices_a;
    sf_mdl_blob_t vertices_b;
    sf_mdl_blob_t vertices_c;
    sf_mdl_blob_t vertices_d;
    sf_mdl_blob_t dummies;
    sf_mdl_blob_t materials;
    char **textures;
};

struct sf_mdl0 {
    const sf_allocator_t *alloc;
    int32_t unk04;
    int32_t unk08;
    int32_t face_count;
    int32_t node_count;
    int32_t index_count;
    int32_t vertex_count_a;
    int32_t vertex_count_b;
    int32_t vertex_count_c;
    int32_t count6;
    int32_t material_count;
    int32_t texture_count;
    uint16_t *indices;
    sf_mdl_blob_t nodes;
    sf_mdl_blob_t vertices_a;
    sf_mdl_blob_t vertices_b;
    sf_mdl_blob_t vertices_c;
    sf_mdl_blob_t struct6s;
    sf_mdl_blob_t materials;
    char **textures;
};

#endif /* SF_MDL_INTERNAL_H */
