/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_smd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_smd4_create_destroy(void) {
    sf_smd4_t *s = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_smd4_create(&s, NULL));
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(SF_SMD4_VERSION_0x40001, sf_smd4_version(s));
    TEST_ASSERT_EQUAL_size_t(0, sf_smd4_unk10_count(s));
    TEST_ASSERT_EQUAL_size_t(0, sf_smd4_node_count(s));
    TEST_ASSERT_EQUAL_size_t(0, sf_smd4_mesh_count(s));
    sf_smd4_destroy(s);
}

static void test_smd4_is_function(void) {
    uint8_t valid[128];
    memset(valid, 0, sizeof(valid));
    memcpy(valid, "SMD4", 4);
    TEST_ASSERT_TRUE(sf_smd4_is(valid, sizeof(valid)));

    uint8_t bad_magic[128];
    memset(bad_magic, 0, sizeof(bad_magic));
    memcpy(bad_magic, "FOO!", 4);
    TEST_ASSERT_FALSE(sf_smd4_is(bad_magic, sizeof(bad_magic)));

    uint8_t too_short[16];
    memset(too_short, 0, sizeof(too_short));
    memcpy(too_short, "SMD4", 4);
    TEST_ASSERT_FALSE(sf_smd4_is(too_short, sizeof(too_short)));

    TEST_ASSERT_FALSE(sf_smd4_is(NULL, 0));
}

static sf_smd4_node_t make_default_node(const char *name) {
    sf_smd4_node_t n;
    memset(&n, 0, sizeof(n));
    if (name) {
        size_t len = strlen(name);
        if (len > 32u) len = 32u;
        memcpy(n.name, name, len);
        n.name[len] = '\0';
    }
    n.scale_x = 1.0f; n.scale_y = 1.0f; n.scale_z = 1.0f;
    n.parent_index = -1;
    n.first_child_index = -1;
    n.next_sibling_index = -1;
    n.prev_sibling_index = -1;
    for (int i = 0; i < 8; i++) n.unk70[i] = -1;
    return n;
}

static void test_smd4_round_trip(void) {
    sf_smd4_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_smd4_create(&a, NULL));

    sf_smd4_set_bounding_box(a, -1.0f, -2.0f, -3.0f, 4.0f, 5.0f, 6.0f);

    sf_smd4_node_t root = make_default_node("root");
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_smd4_add_node(a, root));

    sf_smd4_mesh_t mesh;
    memset(&mesh, 0, sizeof(mesh));
    mesh.vertex_format = 0;
    mesh.unk01 = 0;
    mesh.unk02 = true;
    mesh.unk03 = false;
    mesh.unk06 = 0;
    for (int i = 0; i < 28; i++) mesh.bone_indices[i] = -1;
    mesh.bone_indices[0] = 0;

    uint16_t indices[3] = { 0, 1, 2 };
    mesh.indices = indices;
    mesh.index_count = 3;

    sf_smd4_vertex_t verts[3];
    memset(verts, 0, sizeof(verts));
    verts[0].x = 0.0f; verts[0].y = 0.0f; verts[0].z = 0.0f;
    verts[1].x = 1.0f; verts[1].y = 0.0f; verts[1].z = 0.0f;
    verts[2].x = 0.0f; verts[2].y = 1.0f; verts[2].z = 0.0f;
    for (int i = 0; i < 3; i++) {
        verts[i].bone_indices[0] = 0;
        verts[i].bone_indices[1] = -1;
        verts[i].bone_indices[2] = -1;
        verts[i].bone_indices[3] = -1;
        verts[i].bone_weights[0] = 1.0f;
    }
    mesh.vertices = verts;
    mesh.vertex_count = 3;

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_smd4_add_mesh(a, mesh));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_smd4_write_to_memory(a, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_TRUE(size > 0);
    TEST_ASSERT_TRUE(sf_smd4_is(bytes, size));

    sf_smd4_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_smd4_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_NOT_NULL(b);

    TEST_ASSERT_EQUAL_INT(SF_SMD4_VERSION_0x40001, sf_smd4_version(b));

    float min_x, min_y, min_z, max_x, max_y, max_z;
    sf_smd4_get_bounding_box(b, &min_x, &min_y, &min_z, &max_x, &max_y, &max_z);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, min_x);
    TEST_ASSERT_EQUAL_FLOAT(-2.0f, min_y);
    TEST_ASSERT_EQUAL_FLOAT(-3.0f, min_z);
    TEST_ASSERT_EQUAL_FLOAT( 4.0f, max_x);
    TEST_ASSERT_EQUAL_FLOAT( 5.0f, max_y);
    TEST_ASSERT_EQUAL_FLOAT( 6.0f, max_z);

    TEST_ASSERT_EQUAL_size_t(0, sf_smd4_unk10_count(b));
    TEST_ASSERT_EQUAL_size_t(1, sf_smd4_node_count(b));
    TEST_ASSERT_EQUAL_size_t(1, sf_smd4_mesh_count(b));

    sf_smd4_node_t got_node;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_smd4_get_node(b, 0, &got_node));
    TEST_ASSERT_EQUAL_STRING("root", got_node.name);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, got_node.scale_x);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, got_node.scale_y);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, got_node.scale_z);
    TEST_ASSERT_EQUAL_INT16(-1, got_node.parent_index);
    TEST_ASSERT_EQUAL_INT16(-1, got_node.first_child_index);
    TEST_ASSERT_EQUAL_INT16(-1, got_node.next_sibling_index);
    TEST_ASSERT_EQUAL_INT16(-1, got_node.prev_sibling_index);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_INT32(-1, got_node.unk70[i]);
    }

    const sf_smd4_mesh_t *got_mesh = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_smd4_get_mesh(b, 0, &got_mesh));
    TEST_ASSERT_NOT_NULL(got_mesh);
    TEST_ASSERT_EQUAL_UINT8(0, got_mesh->vertex_format);
    TEST_ASSERT_TRUE(got_mesh->unk02);
    TEST_ASSERT_FALSE(got_mesh->unk03);
    TEST_ASSERT_EQUAL_size_t(3, got_mesh->index_count);
    TEST_ASSERT_EQUAL_size_t(3, got_mesh->vertex_count);
    TEST_ASSERT_EQUAL_UINT16(0, got_mesh->indices[0]);
    TEST_ASSERT_EQUAL_UINT16(1, got_mesh->indices[1]);
    TEST_ASSERT_EQUAL_UINT16(2, got_mesh->indices[2]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, got_mesh->vertices[0].x);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, got_mesh->vertices[1].x);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, got_mesh->vertices[2].y);
    TEST_ASSERT_EQUAL_INT16(0,  got_mesh->vertices[0].bone_indices[0]);
    TEST_ASSERT_EQUAL_INT16(-1, got_mesh->vertices[0].bone_indices[1]);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, got_mesh->vertices[0].bone_weights[0]);
    TEST_ASSERT_EQUAL_INT16(-1, got_mesh->bone_indices[27]);
    TEST_ASSERT_EQUAL_INT16(0,  got_mesh->bone_indices[0]);

    sf_free(NULL, bytes);
    sf_smd4_destroy(b);
    sf_smd4_destroy(a);
}

static void test_smd4_round_trip_unk10(void) {
    sf_smd4_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_smd4_create(&a, NULL));

    sf_smd4_unk10_t u;
    memset(&u, 0, sizeof(u));
    u.unk00 = 0x11; u.unk01 = 0x22; u.unk02 = 0x33; u.unk03 = 0x44;
    memcpy(u.name, "hello", 5);
    u.name[5] = '\0';
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_smd4_add_unk10(a, u));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_smd4_write_to_memory(a, &bytes, &size, NULL));

    sf_smd4_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_smd4_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_EQUAL_size_t(1, sf_smd4_unk10_count(b));

    sf_smd4_unk10_t got;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_smd4_get_unk10(b, 0, &got));
    TEST_ASSERT_EQUAL_UINT8(0x11, got.unk00);
    TEST_ASSERT_EQUAL_UINT8(0x22, got.unk01);
    TEST_ASSERT_EQUAL_UINT8(0x33, got.unk02);
    TEST_ASSERT_EQUAL_UINT8(0x44, got.unk03);
    TEST_ASSERT_EQUAL_STRING("hello", got.name);

    sf_free(NULL, bytes);
    sf_smd4_destroy(b);
    sf_smd4_destroy(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_smd4_create_destroy);
    RUN_TEST(test_smd4_is_function);
    RUN_TEST(test_smd4_round_trip);
    RUN_TEST(test_smd4_round_trip_unk10);
    return UNITY_END();
}
