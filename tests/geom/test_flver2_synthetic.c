/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 6 T22 — FLVER2 synthetic round-trip: a minimal unit-cube FLVER2
 * (1 material, 1 mesh, 1 buffer layout with Float3 Position, 1 vertex
 * buffer with 8 vertices, 1 face set with 12 indices = 4 triangles).
 *
 * Strategy: construct an sf_flver2_t directly via the internal header,
 * serialize to canonical bytes via the public writer, then verify the
 * read path reconstructs every field and the second write reproduces the
 * first byte-for-byte.
 */

#include "internal/flver2_internal.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_flver2.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

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
                                   uint8_t  *vertex_bytes) {
    memset(flver, 0, sizeof(*flver));
    flver->alloc = NULL;
    flver->header.version             = 0x20014u;
    flver->header.material_count      = 1;
    flver->header.mesh_count          = 1;
    flver->header.face_set_count      = 1;
    flver->header.vertex_buffer_count = 1;
    flver->header.buffer_layout_count = 1;
    flver->header.face_count          = 4;
    flver->header.vertex_indices_size = 16;
    flver->header.unicode             = 1;

    materials[0].name = (char *)"test_mat";
    materials[0].mtd  = (char *)"test.mtd";
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

static void verify_read_flver2_fields(const sf_flver2_t *f) {
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_HEX32(0x20014u, sf_flver2_header_version(f));
    TEST_ASSERT_TRUE(sf_flver2_header_unicode(f));
    TEST_ASSERT_EQUAL_size_t(1u, sf_flver2_material_count(f));
    TEST_ASSERT_EQUAL_size_t(1u, sf_flver2_mesh_count(f));
    TEST_ASSERT_EQUAL_size_t(1u, sf_flver2_face_set_count(f));
    TEST_ASSERT_EQUAL_size_t(1u, sf_flver2_vertex_buffer_count(f));
    TEST_ASSERT_EQUAL_size_t(1u, sf_flver2_buffer_layout_count(f));
    TEST_ASSERT_EQUAL_size_t(0u, sf_flver2_texture_count(f));
    TEST_ASSERT_EQUAL_size_t(0u, sf_flver2_node_count(f));
    TEST_ASSERT_EQUAL_size_t(0u, sf_flver2_dummy_count(f));

    const sf_flver2_material_t *m = sf_flver2_material(f, 0);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_STRING("test_mat", sf_flver2_material_name(m));
    TEST_ASSERT_EQUAL_STRING("test.mtd", sf_flver2_material_mtd(m));
    TEST_ASSERT_EQUAL_size_t(0u, sf_flver2_material_texture_count(m));

    const sf_flver2_mesh_t *mesh = sf_flver2_mesh(f, 0);
    TEST_ASSERT_NOT_NULL(mesh);
    TEST_ASSERT_FALSE(sf_flver2_mesh_use_bone_weights(mesh));
    TEST_ASSERT_EQUAL_INT32(0, sf_flver2_mesh_material_index(mesh));
    TEST_ASSERT_EQUAL_INT32(0, sf_flver2_mesh_node_index(mesh));
    TEST_ASSERT_EQUAL_size_t(0u, sf_flver2_mesh_bone_index_count(mesh));
    TEST_ASSERT_EQUAL_size_t(1u, sf_flver2_mesh_face_set_index_count(mesh));
    TEST_ASSERT_EQUAL_INT32(0, sf_flver2_mesh_face_set_index(mesh, 0));
    TEST_ASSERT_EQUAL_size_t(1u, sf_flver2_mesh_vertex_buffer_index_count(mesh));
    TEST_ASSERT_EQUAL_INT32(0, sf_flver2_mesh_vertex_buffer_index(mesh, 0));

    const sf_flver2_face_set_t *fs = sf_flver2_face_set(f, 0);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL_HEX32(SF_FLVER2_FS_FLAGS_NONE, sf_flver2_face_set_flags(fs));
    TEST_ASSERT_FALSE(sf_flver2_face_set_triangle_strip(fs));
    TEST_ASSERT_TRUE(sf_flver2_face_set_cull_backfaces(fs));
    TEST_ASSERT_EQUAL_UINT8(16u, sf_flver2_face_set_index_size(fs));
    TEST_ASSERT_EQUAL_size_t(12u, sf_flver2_face_set_index_count(fs));
    for (size_t i = 0; i < 12; i++) {
        TEST_ASSERT_EQUAL_UINT32(k_cube_indices[i], sf_flver2_face_set_index(fs, i));
    }

    const sf_flver2_vertex_buffer_t *vb = sf_flver2_vertex_buffer(f, 0);
    TEST_ASSERT_NOT_NULL(vb);
    TEST_ASSERT_EQUAL_INT32(0, sf_flver2_vertex_buffer_buffer_index(vb));
    TEST_ASSERT_EQUAL_INT32(0, sf_flver2_vertex_buffer_layout_index(vb));
    TEST_ASSERT_EQUAL_INT32(12, sf_flver2_vertex_buffer_vertex_size(vb));
    TEST_ASSERT_EQUAL_INT32(8, sf_flver2_vertex_buffer_vertex_count(vb));
    size_t vb_size = 0;
    const uint8_t *vb_data = sf_flver2_vertex_buffer_bytes(vb, &vb_size);
    TEST_ASSERT_EQUAL_size_t(sizeof(k_cube_vertices), vb_size);
    TEST_ASSERT_EQUAL_MEMORY(k_cube_vertices, vb_data, sizeof(k_cube_vertices));

    const sf_flver2_buffer_layout_t *bl = sf_flver2_buffer_layout(f, 0);
    TEST_ASSERT_NOT_NULL(bl);
    TEST_ASSERT_EQUAL_size_t(1u, sf_flver2_buffer_layout_member_count(bl));
    TEST_ASSERT_EQUAL_UINT32(12u, sf_flver2_buffer_layout_size(bl));
    TEST_ASSERT_EQUAL_INT32(0, sf_flver2_buffer_layout_member_stream(bl, 0));
    TEST_ASSERT_EQUAL_INT32(0, sf_flver2_buffer_layout_member_struct_offset(bl, 0));
    TEST_ASSERT_EQUAL_UINT32(SF_FLVER_LAYOUT_TYPE_FLOAT3,
                             (uint32_t)sf_flver2_buffer_layout_member_type(bl, 0));
    TEST_ASSERT_EQUAL_UINT32(SF_FLVER_LAYOUT_SEMANTIC_POSITION,
                             (uint32_t)sf_flver2_buffer_layout_member_semantic(bl, 0));
    TEST_ASSERT_EQUAL_INT32(0, sf_flver2_buffer_layout_member_index(bl, 0));
}

static void test_flver2_unit_cube_round_trip_byte_identical(void) {
    sf_flver2_t                 flver;
    sf_flver2_material_t        materials[1];
    sf_flver2_mesh_t            meshes[1];
    sf_flver2_face_set_t        face_sets[1];
    sf_flver2_vertex_buffer_t   vertex_buffers[1];
    sf_flver2_buffer_layout_t   buffer_layouts[1];
    sf_flver2_layout_member_t   layout_members[1];
    int32_t                     mesh_face_set_idx[1];
    int32_t                     mesh_vertex_buffer_idx[1];
    uint32_t                    face_set_indices[12];
    uint8_t                     vertex_bytes[96];

    memset(&materials, 0, sizeof(materials));
    memset(&meshes, 0, sizeof(meshes));
    memset(&face_sets, 0, sizeof(face_sets));
    memset(&vertex_buffers, 0, sizeof(vertex_buffers));
    memset(&buffer_layouts, 0, sizeof(buffer_layouts));
    memset(&layout_members, 0, sizeof(layout_members));

    build_synthetic_flver2(&flver, materials, meshes, face_sets, vertex_buffers,
                           buffer_layouts, layout_members,
                           mesh_face_set_idx, mesh_vertex_buffer_idx,
                           face_set_indices, vertex_bytes);

    void  *bytes_a = NULL;
    size_t size_a  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_flver2_write_to_memory(&flver, &bytes_a, &size_a, NULL));
    TEST_ASSERT_NOT_NULL(bytes_a);
    TEST_ASSERT_TRUE(size_a >= 128u);
    TEST_ASSERT_EQUAL_MEMORY("FLVER\0L\0", bytes_a, 8);

    sf_flver2_t *read_flver = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_flver2_read_from_memory(&read_flver, bytes_a, size_a, NULL));
    verify_read_flver2_fields(read_flver);

    void  *bytes_b = NULL;
    size_t size_b  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_flver2_write_to_memory(read_flver, &bytes_b, &size_b, NULL));
    TEST_ASSERT_NOT_NULL(bytes_b);
    TEST_ASSERT_EQUAL_size_t(size_a, size_b);
    TEST_ASSERT_EQUAL_MEMORY(bytes_a, bytes_b, size_a);

    sf_free(NULL, bytes_b);
    sf_flver2_destroy(read_flver);
    sf_free(NULL, bytes_a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_flver2_unit_cube_round_trip_byte_identical);
    return UNITY_END();
}
