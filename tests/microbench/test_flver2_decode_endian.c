/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T0.4 — FLVER2 decode microbenchmark for binary-reader endian fast-path work.
 * Uses the same synthetic unit-cube shape as tests/geom/test_flver2_synthetic.c;
 * no game files are required.
 */

#include "internal/flver2_internal.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_flver2.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const float k_cube_vertices[24] = {
    0.f, 0.f, 0.f,
    1.f, 0.f, 0.f,
    1.f, 1.f, 0.f,
    0.f, 1.f, 0.f,
    0.f, 0.f, 1.f,
    1.f, 0.f, 1.f,
    1.f, 1.f, 1.f,
    0.f, 1.f, 1.f,
};

static const uint32_t k_cube_indices[12] = {
    0, 1, 2,  0, 2, 3,
    4, 5, 6,  4, 6, 7,
};

static void build_synthetic_flver2(sf_flver2_t *flver,
                                   sf_flver2_material_t *materials,
                                   sf_flver2_mesh_t *meshes,
                                   sf_flver2_face_set_t *face_sets,
                                   sf_flver2_vertex_buffer_t *vertex_buffers,
                                   sf_flver2_buffer_layout_t *buffer_layouts,
                                   sf_flver2_layout_member_t *layout_members,
                                   int32_t *mesh_face_set_indices,
                                   int32_t *mesh_vertex_buffer_indices,
                                   uint32_t *face_set_indices,
                                   uint8_t *vertex_bytes) {
    memset(flver, 0, sizeof(*flver));
    flver->alloc = NULL;
    flver->header.version = 0x20014u;
    flver->header.material_count = 1;
    flver->header.mesh_count = 1;
    flver->header.face_set_count = 1;
    flver->header.vertex_buffer_count = 1;
    flver->header.buffer_layout_count = 1;
    flver->header.face_count = 4;
    flver->header.vertex_indices_size = 16;
    flver->header.unicode = 1;

    materials[0].name = (char *)"bench_mat";
    materials[0].mtd = (char *)"bench.mtd";
    materials[0].textures = NULL;
    materials[0].texture_count = 0;
    materials[0].gx_index = -1;
    materials[0].index = 0;
    materials[0].pretake_texture_index = -1;
    materials[0].pretake_texture_count = -1;
    flver->materials = materials;

    mesh_face_set_indices[0] = 0;
    mesh_vertex_buffer_indices[0] = 0;
    meshes[0].use_bone_weights = false;
    meshes[0].material_index = 0;
    meshes[0].node_index = 0;
    meshes[0].bone_indices = NULL;
    meshes[0].bone_index_count = 0;
    meshes[0].face_set_indices = mesh_face_set_indices;
    meshes[0].face_set_index_count = 1;
    meshes[0].vertex_buffer_indices = mesh_vertex_buffer_indices;
    meshes[0].vertex_buffer_index_count = 1;
    meshes[0].has_bounding_box = false;
    flver->meshes = meshes;

    memcpy(face_set_indices, k_cube_indices, sizeof(k_cube_indices));
    face_sets[0].flags = SF_FLVER2_FS_FLAGS_NONE;
    face_sets[0].triangle_strip = false;
    face_sets[0].cull_backfaces = true;
    face_sets[0].unk06 = 0;
    face_sets[0].unk07 = 0;
    face_sets[0].indices = face_set_indices;
    face_sets[0].index_count = 12;
    face_sets[0].index_size = 16;
    flver->face_sets = face_sets;

    memcpy(vertex_bytes, k_cube_vertices, sizeof(k_cube_vertices));
    vertex_buffers[0].buffer_index = 0;
    vertex_buffers[0].layout_index = 0;
    vertex_buffers[0].vertex_size = 12;
    vertex_buffers[0].vertex_count = 8;
    vertex_buffers[0].vertex_bytes = vertex_bytes;
    vertex_buffers[0].vertex_bytes_size = sizeof(k_cube_vertices);
    flver->vertex_buffers = vertex_buffers;

    layout_members[0].stream = 0;
    layout_members[0].struct_offset = 0;
    layout_members[0].type = SF_FLVER_LAYOUT_TYPE_FLOAT3;
    layout_members[0].semantic = SF_FLVER_LAYOUT_SEMANTIC_POSITION;
    layout_members[0].index = 0;
    layout_members[0].special_modifier = 0;
    buffer_layouts[0].members = layout_members;
    buffer_layouts[0].member_count = 1;
    flver->buffer_layouts = buffer_layouts;
}

static sf_result_t make_synthetic_bytes(void **out_bytes, size_t *out_size) {
    sf_flver2_t flver;
    sf_flver2_material_t materials[1];
    sf_flver2_mesh_t meshes[1];
    sf_flver2_face_set_t face_sets[1];
    sf_flver2_vertex_buffer_t vertex_buffers[1];
    sf_flver2_buffer_layout_t buffer_layouts[1];
    sf_flver2_layout_member_t layout_members[1];
    int32_t mesh_face_set_idx[1];
    int32_t mesh_vertex_buffer_idx[1];
    uint32_t face_set_indices[12];
    uint8_t vertex_bytes[96];

    memset(&materials, 0, sizeof(materials));
    memset(&meshes, 0, sizeof(meshes));
    memset(&face_sets, 0, sizeof(face_sets));
    memset(&vertex_buffers, 0, sizeof(vertex_buffers));
    memset(&buffer_layouts, 0, sizeof(buffer_layouts));
    memset(&layout_members, 0, sizeof(layout_members));

    build_synthetic_flver2(&flver, materials, meshes, face_sets, vertex_buffers,
                           buffer_layouts, layout_members, mesh_face_set_idx,
                           mesh_vertex_buffer_idx, face_set_indices, vertex_bytes);
    return sf_flver2_write_to_memory(&flver, out_bytes, out_size, NULL);
}

int main(int argc, char **argv) {
    const int iterations = (argc > 1) ? atoi(argv[1]) : 1000;
    if (iterations <= 0) {
        fprintf(stderr, "usage: %s [positive-iterations]\n", argv[0]);
        return 2;
    }

    void *bytes = NULL;
    size_t size = 0;
    sf_result_t r = make_synthetic_bytes(&bytes, &size);
    if (r != SF_OK) {
        fprintf(stderr, "failed to build synthetic FLVER2: %d\n", (int)r);
        return 1;
    }

    clock_t start = clock();
    for (int i = 0; i < iterations; ++i) {
        sf_flver2_t *decoded = NULL;
        r = sf_flver2_read_from_memory(&decoded, bytes, size, NULL);
        if (r != SF_OK) {
            fprintf(stderr, "decode failed at iteration %d: %d\n", i, (int)r);
            sf_free(NULL, bytes);
            return 1;
        }
        sf_flver2_destroy(decoded);
    }
    clock_t end = clock();

    const double elapsed_sec = (double)(end - start) / (double)CLOCKS_PER_SEC;
    const double elapsed_ms = elapsed_sec * 1000.0;
    const double us_per_decode = (elapsed_sec * 1000000.0) / (double)iterations;

    printf("FLVER2 synthetic decode endian microbench\n");
    printf("iterations=%d\n", iterations);
    printf("bytes=%zu\n", size);
    printf("elapsed_ms=%.3f\n", elapsed_ms);
    printf("us_per_decode=%.3f\n", us_per_decode);

    sf_free(NULL, bytes);
    return 0;
}
