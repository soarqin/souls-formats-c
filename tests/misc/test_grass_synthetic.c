/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_grass.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_grass_create_destroy(void) {
    sf_grass_t *g = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_create(&g, NULL));
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_size_t(0, sf_grass_volume_count(g));
    TEST_ASSERT_EQUAL_size_t(0, sf_grass_vertex_count(g));
    TEST_ASSERT_EQUAL_size_t(0, sf_grass_face_count(g));
    sf_grass_destroy(g);
}

static void test_grass_is_function(void) {
    uint8_t valid[0x28];
    memset(valid, 0, sizeof(valid));
    int32_t v;
    v = 1;       memcpy(valid + 0x00, &v, 4);
    v = 0x28;    memcpy(valid + 0x04, &v, 4);
    v = 0x14;    memcpy(valid + 0x08, &v, 4);
    v = 0x24;    memcpy(valid + 0x10, &v, 4);
    v = 0x18;    memcpy(valid + 0x18, &v, 4);
    v = 0x18;    memcpy(valid + 0x20, &v, 4);
    TEST_ASSERT_TRUE(sf_grass_is(valid, sizeof(valid)));

    uint8_t bad_version[0x28];
    memcpy(bad_version, valid, sizeof(valid));
    v = 2; memcpy(bad_version + 0x00, &v, 4);
    TEST_ASSERT_FALSE(sf_grass_is(bad_version, sizeof(bad_version)));

    uint8_t too_short[8] = { 1, 0, 0, 0, 0x28, 0, 0, 0 };
    TEST_ASSERT_FALSE(sf_grass_is(too_short, sizeof(too_short)));

    TEST_ASSERT_FALSE(sf_grass_is(NULL, 0));
}

static void test_grass_round_trip(void) {
    sf_grass_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_create(&a, NULL));

    sf_grass_volume_t v0 = { 0, 2, 0, 1, 100,
                             -1.0f, -2.0f, -3.0f, 1.0f, 2.0f, 3.0f };
    sf_grass_volume_t v1 = { 2, 2, 1, 2, 200,
                             -4.0f, -5.0f, -6.0f, 4.0f, 5.0f, 6.0f };
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_add_volume(a, v0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_add_volume(a, v1));

    sf_grass_vertex_t vert0 = { 1.5f, 2.5f, 3.5f, { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f } };
    sf_grass_vertex_t vert1 = { -7.0f, 0.0f, 0.25f, { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f } };
    sf_grass_vertex_t vert2 = { 100.0f, 200.0f, 300.0f, { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f } };
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_add_vertex(a, vert0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_add_vertex(a, vert1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_add_vertex(a, vert2));

    sf_grass_face_t f0 = { 0.0f, 1.0f, 0.0f, 0, 1, 2 };
    sf_grass_face_t f1 = { 0.0f, 0.0f, 1.0f, 2, 1, 0 };
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_add_face(a, f0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_add_face(a, f1));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_write_to_memory(a, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    /* Header(40) + 2 volumes(2*20=40) + 3 vertices(3*36=108)
     * + 2 faces(2*24=48) + 2 bounding boxes(2*24=48) = 284 bytes. */
    TEST_ASSERT_EQUAL_size_t(284u, size);
    TEST_ASSERT_TRUE(sf_grass_is(bytes, size));

    sf_grass_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_EQUAL_size_t(2, sf_grass_volume_count(b));
    TEST_ASSERT_EQUAL_size_t(3, sf_grass_vertex_count(b));
    TEST_ASSERT_EQUAL_size_t(2, sf_grass_face_count(b));

    sf_grass_volume_t r_v0, r_v1;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_get_volume(b, 0, &r_v0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_get_volume(b, 1, &r_v1));
    TEST_ASSERT_EQUAL_INT32(v0.start_child_index, r_v0.start_child_index);
    TEST_ASSERT_EQUAL_INT32(v0.end_child_index,   r_v0.end_child_index);
    TEST_ASSERT_EQUAL_INT32(v0.start_face_index,  r_v0.start_face_index);
    TEST_ASSERT_EQUAL_INT32(v0.end_face_index,    r_v0.end_face_index);
    TEST_ASSERT_EQUAL_INT32(v0.unk10,             r_v0.unk10);
    TEST_ASSERT_EQUAL_FLOAT(v0.bb_min_x, r_v0.bb_min_x);
    TEST_ASSERT_EQUAL_FLOAT(v0.bb_min_y, r_v0.bb_min_y);
    TEST_ASSERT_EQUAL_FLOAT(v0.bb_min_z, r_v0.bb_min_z);
    TEST_ASSERT_EQUAL_FLOAT(v0.bb_max_x, r_v0.bb_max_x);
    TEST_ASSERT_EQUAL_FLOAT(v0.bb_max_y, r_v0.bb_max_y);
    TEST_ASSERT_EQUAL_FLOAT(v0.bb_max_z, r_v0.bb_max_z);
    TEST_ASSERT_EQUAL_INT32(v1.unk10, r_v1.unk10);
    TEST_ASSERT_EQUAL_FLOAT(v1.bb_max_z, r_v1.bb_max_z);

    sf_grass_vertex_t r_vert0, r_vert2;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_get_vertex(b, 0, &r_vert0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_get_vertex(b, 2, &r_vert2));
    TEST_ASSERT_EQUAL_FLOAT(vert0.x, r_vert0.x);
    TEST_ASSERT_EQUAL_FLOAT(vert0.y, r_vert0.y);
    TEST_ASSERT_EQUAL_FLOAT(vert0.z, r_vert0.z);
    for (size_t i = 0; i < 6; i++) {
        TEST_ASSERT_EQUAL_FLOAT(vert0.grass_densities[i], r_vert0.grass_densities[i]);
        TEST_ASSERT_EQUAL_FLOAT(vert2.grass_densities[i], r_vert2.grass_densities[i]);
    }
    TEST_ASSERT_EQUAL_FLOAT(vert2.x, r_vert2.x);

    sf_grass_face_t r_f0, r_f1;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_get_face(b, 0, &r_f0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_grass_get_face(b, 1, &r_f1));
    TEST_ASSERT_EQUAL_FLOAT(f0.normal_x, r_f0.normal_x);
    TEST_ASSERT_EQUAL_FLOAT(f0.normal_y, r_f0.normal_y);
    TEST_ASSERT_EQUAL_FLOAT(f0.normal_z, r_f0.normal_z);
    TEST_ASSERT_EQUAL_INT32(f0.vertex_index_a, r_f0.vertex_index_a);
    TEST_ASSERT_EQUAL_INT32(f0.vertex_index_b, r_f0.vertex_index_b);
    TEST_ASSERT_EQUAL_INT32(f0.vertex_index_c, r_f0.vertex_index_c);
    TEST_ASSERT_EQUAL_INT32(f1.vertex_index_a, r_f1.vertex_index_a);
    TEST_ASSERT_EQUAL_INT32(f1.vertex_index_c, r_f1.vertex_index_c);

    sf_free(NULL, bytes);
    sf_grass_destroy(b);
    sf_grass_destroy(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_grass_create_destroy);
    RUN_TEST(test_grass_is_function);
    RUN_TEST(test_grass_round_trip);
    return UNITY_END();
}
