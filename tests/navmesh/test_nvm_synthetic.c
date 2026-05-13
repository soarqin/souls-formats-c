/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_nvm.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_nvm_synthetic_roundtrip(void) {
    sf_nvm_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_create_empty(&src, NULL));

    sf_vec3_t v0 = {0, 0, 0};
    sf_vec3_t v1 = {1, 0, 0};
    sf_vec3_t v2 = {0, 1, 0};
    sf_vec3_t v3 = {1, 1, 0};
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_append_vertex(src, v0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_append_vertex(src, v1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_append_vertex(src, v2));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_append_vertex(src, v3));

    sf_nvm_triangle_t *t0 = NULL, *t1 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_append_triangle(src, &t0));
    sf_nvm_triangle_set_vertex_indices(t0, 0, 1, 2);
    sf_nvm_triangle_set_edge_indices  (t0, -1, 1, -1);
    sf_nvm_triangle_set_obstacle_count(t0, 3);
    sf_nvm_triangle_set_flags         (t0, SF_NVM_TRI_FLAG_LARGE_SPACE);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_append_triangle(src, &t1));
    sf_nvm_triangle_set_vertex_indices(t1, 1, 3, 2);
    sf_nvm_triangle_set_edge_indices  (t1, 0, -1, -1);
    sf_nvm_triangle_set_obstacle_count(t1, 0);
    sf_nvm_triangle_set_flags         (t1, SF_NVM_TRI_FLAG_WALL);

    sf_nvm_box_t *root = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_create_box(src, &root));
    sf_vec3_t mn = {0, 0, 0};
    sf_vec3_t mx = {1, 1, 0};
    sf_nvm_box_set_min_corner(root, mn);
    sf_nvm_box_set_max_corner(root, mx);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_box_append_triangle_index(root, 0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_box_append_triangle_index(root, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_set_root_box(src, root));

    sf_nvm_entity_t *e0 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_append_entity(src, &e0));
    sf_nvm_entity_set_entity_id(e0, 12345);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_entity_append_triangle_index(e0, 0));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_write_to_memory(src, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);

    sf_nvm_t *parsed = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_read_from_memory(&parsed, bytes, size, NULL));
    TEST_ASSERT_NOT_NULL(parsed);
    TEST_ASSERT_FALSE(sf_nvm_big_endian(parsed));
    TEST_ASSERT_EQUAL_size_t(4, sf_nvm_vertex_count(parsed));
    TEST_ASSERT_EQUAL_size_t(2, sf_nvm_triangle_count(parsed));
    TEST_ASSERT_EQUAL_size_t(1, sf_nvm_entity_count(parsed));

    TEST_ASSERT_EQUAL_FLOAT(0.0f, sf_nvm_vertex(parsed, 0).x);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sf_nvm_vertex(parsed, 3).y);

    const sf_nvm_triangle_t *pt0 = sf_nvm_triangle(parsed, 0);
    TEST_ASSERT_EQUAL_INT(0, sf_nvm_triangle_vertex_index_1(pt0));
    TEST_ASSERT_EQUAL_INT(1, sf_nvm_triangle_vertex_index_2(pt0));
    TEST_ASSERT_EQUAL_INT(2, sf_nvm_triangle_vertex_index_3(pt0));
    TEST_ASSERT_EQUAL_INT(-1, sf_nvm_triangle_edge_index_1(pt0));
    TEST_ASSERT_EQUAL_INT(3, sf_nvm_triangle_obstacle_count(pt0));
    TEST_ASSERT_EQUAL_INT(SF_NVM_TRI_FLAG_LARGE_SPACE, sf_nvm_triangle_flags(pt0));

    const sf_nvm_box_t *pbox = sf_nvm_root_box(parsed);
    TEST_ASSERT_NOT_NULL(pbox);
    TEST_ASSERT_EQUAL_size_t(2, sf_nvm_box_triangle_index_count(pbox));
    TEST_ASSERT_EQUAL_INT(0, sf_nvm_box_triangle_index(pbox, 0));
    TEST_ASSERT_EQUAL_INT(1, sf_nvm_box_triangle_index(pbox, 1));

    const sf_nvm_entity_t *pe0 = sf_nvm_entity(parsed, 0);
    TEST_ASSERT_EQUAL_INT(12345, sf_nvm_entity_entity_id(pe0));

    void *bytes2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_write_to_memory(parsed, &bytes2, &size2, NULL));
    TEST_ASSERT_EQUAL_size_t(size, size2);
    TEST_ASSERT_EQUAL_MEMORY(bytes, bytes2, size);

    sf_free(NULL, bytes);
    sf_free(NULL, bytes2);
    sf_nvm_destroy(parsed);
    sf_nvm_destroy(src);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nvm_synthetic_roundtrip);
    return UNITY_END();
}
