/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "internal/mdl4_internal.h"
#include "souls_formats/sf_mdl4.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void build_mdl4(sf_mdl4_t *m, sf_mdl4_material_t *materials,
                       sf_mdl4_node_t *nodes, sf_mdl4_mesh_t *meshes,
                       uint16_t *indices, uint8_t *vertex_bytes) {
    memset(m, 0, sizeof(*m));
    m->header.version = 0x40001u;
    m->header.material_count = 1;
    m->header.bone_count = 1;
    m->header.mesh_count = 1;
    m->header.vertex_buffer_count = 1;
    materials[0].name = (char *)"mdl4_mat";
    materials[0].shader = (char *)"shader";
    m->materials = materials;
    nodes[0].name = (char *)"root";
    nodes[0].scale.x = 1.0f;
    nodes[0].scale.y = 1.0f;
    nodes[0].scale.z = 1.0f;
    nodes[0].parent_index = -1;
    nodes[0].first_child_index = -1;
    nodes[0].next_sibling_index = -1;
    nodes[0].previous_sibling_index = -1;
    for (size_t i = 0; i < 16; i++) nodes[0].unk_indices[i] = -1;
    m->nodes = nodes;
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 0;
    for (size_t i = 0; i < 0x80; i++) vertex_bytes[i] = (uint8_t)i;
    meshes[0].vertex_format = 0;
    meshes[0].material_index = 0;
    for (size_t i = 0; i < SF_MDL4_MAX_BONE_COUNT; i++) meshes[0].bone_indices[i] = -1;
    meshes[0].indices = indices;
    meshes[0].index_count = 3;
    meshes[0].vertex_bytes = vertex_bytes;
    meshes[0].vertex_bytes_size = 0x80;
    meshes[0].vertex_count = 2;
    m->meshes = meshes;
}

static void test_mdl4_round_trip(void) {
    sf_mdl4_t m;
    sf_mdl4_material_t materials[1];
    sf_mdl4_node_t nodes[1];
    sf_mdl4_mesh_t meshes[1];
    uint16_t indices[3];
    uint8_t vertex_bytes[0x80];
    memset(&materials, 0, sizeof(materials));
    memset(&nodes, 0, sizeof(nodes));
    memset(&meshes, 0, sizeof(meshes));
    build_mdl4(&m, materials, nodes, meshes, indices, vertex_bytes);

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mdl4_write_to_memory(&m, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_MEMORY("MDL4", bytes, 4);

    sf_mdl4_t *read_m = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mdl4_read_from_memory(&read_m, bytes, size, NULL));
    TEST_ASSERT_EQUAL_HEX32(0x40001u, sf_mdl4_header_version(read_m));
    TEST_ASSERT_EQUAL_size_t(1u, sf_mdl4_material_count(read_m));
    TEST_ASSERT_EQUAL_STRING("mdl4_mat", sf_mdl4_material_name(sf_mdl4_material(read_m, 0)));
    TEST_ASSERT_EQUAL_STRING("root", sf_mdl4_node_name(sf_mdl4_node(read_m, 0)));
    const sf_mdl4_mesh_t *mesh = sf_mdl4_mesh(read_m, 0);
    TEST_ASSERT_EQUAL_UINT8(0u, sf_mdl4_mesh_vertex_format(mesh));
    TEST_ASSERT_EQUAL_size_t(3u, sf_mdl4_mesh_index_count(mesh));
    TEST_ASSERT_EQUAL_UINT16(1u, sf_mdl4_mesh_index(mesh, 1));
    TEST_ASSERT_EQUAL_size_t(2u, sf_mdl4_mesh_vertex_count(mesh));

    sf_mdl4_destroy(read_m);
    sf_free(NULL, bytes);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mdl4_round_trip);
    return UNITY_END();
}
