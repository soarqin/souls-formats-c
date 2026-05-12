/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "internal/flver0_internal.h"
#include "souls_formats/sf_flver0.h"

#include "unity.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void build_flver0(sf_flver0_t *f, sf_flver0_material_t *materials,
                         sf_flver0_buffer_layout_t *layouts,
                         sf_flver0_layout_member_t *members, sf_flver_node_t *nodes,
                         sf_flver0_mesh_t *meshes, uint32_t *indices,
                         uint8_t *vertex_bytes) {
    memset(f, 0, sizeof(*f));
    f->header.version = 0x15;
    f->header.material_count = 1;
    f->header.bone_count = 1;
    f->header.mesh_count = 1;
    f->header.vertex_buffer_count = 1;
    f->header.vertex_index_size = 16;
    f->header.unicode = 1;
    f->header.unk4c = 0xFFFF;

    members[0].stream = 0;
    members[0].struct_offset = 0;
    members[0].type = SF_FLVER_LAYOUT_TYPE_FLOAT3;
    members[0].semantic = SF_FLVER_LAYOUT_SEMANTIC_POSITION;
    members[0].index = 0;
    layouts[0].members = members;
    layouts[0].member_count = 1;
    layouts[0].size = 12;
    materials[0].name = (char *)"legacy_mat";
    materials[0].mtd = (char *)"legacy.mtd";
    materials[0].layouts = layouts;
    materials[0].layout_count = 1;
    f->materials = materials;

    nodes[0].name = (char *)"root";
    nodes[0].parent_index = -1;
    nodes[0].first_child_index = -1;
    nodes[0].next_sibling_index = -1;
    nodes[0].previous_sibling_index = -1;
    nodes[0].scale.x = 1.0f;
    nodes[0].scale.y = 1.0f;
    nodes[0].scale.z = 1.0f;
    f->nodes = nodes;

    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 0;
    float verts[6] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    memcpy(vertex_bytes, verts, sizeof(verts));
    meshes[0].dynamic = 0;
    meshes[0].material_index = 0;
    meshes[0].cull_backfaces = true;
    meshes[0].triangle_strip = false;
    meshes[0].node_index = 0;
    for (size_t i = 0; i < SF_FLVER0_MAX_BONE_COUNT; i++) meshes[0].bone_indices[i] = -1;
    meshes[0].indices = indices;
    meshes[0].index_count = 3;
    meshes[0].index_size = 16;
    meshes[0].vertex_count = 2;
    meshes[0].layout_index = 0;
    meshes[0].vertex_bytes = vertex_bytes;
    meshes[0].vertex_bytes_size = sizeof(verts);
    f->meshes = meshes;
}

static void verify_flver0(const sf_flver0_t *f) {
    TEST_ASSERT_EQUAL_HEX32(0x15u, sf_flver0_header_version(f));
    TEST_ASSERT_FALSE(sf_flver0_header_big_endian(f));
    TEST_ASSERT_TRUE(sf_flver0_header_unicode(f));
    TEST_ASSERT_EQUAL_size_t(1u, sf_flver0_material_count(f));
    TEST_ASSERT_EQUAL_size_t(1u, sf_flver0_node_count(f));
    TEST_ASSERT_EQUAL_size_t(1u, sf_flver0_mesh_count(f));
    const sf_flver0_material_t *mat = sf_flver0_material(f, 0);
    TEST_ASSERT_EQUAL_STRING("legacy_mat", sf_flver0_material_name(mat));
    TEST_ASSERT_EQUAL_STRING("legacy.mtd", sf_flver0_material_mtd(mat));
    TEST_ASSERT_EQUAL_size_t(1u, sf_flver0_material_layout_count(mat));
    const sf_flver0_buffer_layout_t *layout = sf_flver0_material_layout(mat, 0);
    TEST_ASSERT_EQUAL_UINT32(12u, sf_flver0_buffer_layout_size(layout));
    TEST_ASSERT_EQUAL_UINT32(SF_FLVER_LAYOUT_TYPE_FLOAT3,
                             (uint32_t)sf_flver0_buffer_layout_member_type(layout, 0));
    const sf_flver0_mesh_t *mesh = sf_flver0_mesh(f, 0);
    TEST_ASSERT_EQUAL_UINT8(0u, sf_flver0_mesh_material_index(mesh));
    TEST_ASSERT_EQUAL_size_t(3u, sf_flver0_mesh_index_count(mesh));
    TEST_ASSERT_EQUAL_UINT32(1u, sf_flver0_mesh_index(mesh, 1));
    TEST_ASSERT_EQUAL_size_t(2u, sf_flver0_mesh_vertex_count(mesh));
    size_t n = 0;
    TEST_ASSERT_NOT_NULL(sf_flver0_mesh_vertex_bytes(mesh, &n));
    TEST_ASSERT_EQUAL_size_t(24u, n);
}

static void test_flver0_round_trip_byte_identical(void) {
    sf_flver0_t f;
    sf_flver0_material_t materials[1];
    sf_flver0_buffer_layout_t layouts[1];
    sf_flver0_layout_member_t members[1];
    sf_flver_node_t nodes[1];
    sf_flver0_mesh_t meshes[1];
    uint32_t indices[3];
    uint8_t vertex_bytes[24];
    memset(&materials, 0, sizeof(materials));
    memset(&layouts, 0, sizeof(layouts));
    memset(&members, 0, sizeof(members));
    memset(&nodes, 0, sizeof(nodes));
    memset(&meshes, 0, sizeof(meshes));
    build_flver0(&f, materials, layouts, members, nodes, meshes, indices, vertex_bytes);

    void *bytes_a = NULL;
    size_t size_a = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_flver0_write_to_memory(&f, &bytes_a, &size_a, NULL));
    TEST_ASSERT_NOT_NULL(bytes_a);
    TEST_ASSERT_EQUAL_MEMORY("FLVER\0L\0", bytes_a, 8);

    sf_flver0_t *read_f = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_flver0_read_from_memory(&read_f, bytes_a, size_a, NULL));
    verify_flver0(read_f);

    void *bytes_b = NULL;
    size_t size_b = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_flver0_write_to_memory(read_f, &bytes_b, &size_b, NULL));
    TEST_ASSERT_EQUAL_size_t(size_a, size_b);
    TEST_ASSERT_EQUAL_MEMORY(bytes_a, bytes_b, size_a);

    sf_free(NULL, bytes_b);
    sf_flver0_destroy(read_f);
    sf_free(NULL, bytes_a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_flver0_round_trip_byte_identical);
    return UNITY_END();
}
