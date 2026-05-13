/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_mcg.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_mcg_synthetic_roundtrip(void) {
    sf_mcg_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_create_empty(&src, NULL));
    sf_mcg_set_unk04(src, 0xAAAA);
    sf_mcg_set_unk18(src, 0xBBBB);
    sf_mcg_set_unk1c(src, 0xCCCC);

    sf_mcg_node_t *n0 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_append_node(src, &n0));
    sf_vec3_t p0 = {1.0f, 2.0f, 3.0f};
    sf_mcg_node_set_position(n0, p0);
    sf_mcg_node_set_unk18(n0, -1);
    sf_mcg_node_set_unk1c(n0, 42);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_node_append_connected_node_index(n0, 1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_node_append_connected_edge_index(n0, 0));

    sf_mcg_node_t *n1 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_append_node(src, &n1));
    sf_vec3_t p1 = {4.0f, 5.0f, 6.0f};
    sf_mcg_node_set_position(n1, p1);
    sf_mcg_node_set_unk18(n1, 0);
    sf_mcg_node_set_unk1c(n1, 7);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_node_append_connected_node_index(n1, 0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_node_append_connected_edge_index(n1, 0));

    sf_mcg_edge_t *e0 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_append_edge(src, &e0));
    sf_mcg_edge_set_node_index_a(e0, 0);
    sf_mcg_edge_set_node_index_b(e0, 1);
    sf_mcg_edge_set_mcp_room_index(e0, 5);
    sf_mcg_edge_set_map_id(e0, 0x10010000u);
    sf_mcg_edge_set_unk20(e0, 2.5f);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_edge_append_unk_indices_a(e0, 100));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_edge_append_unk_indices_b(e0, 200));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_edge_append_unk_indices_b(e0, 201));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_write_to_memory(src, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);

    sf_mcg_t *parsed = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_read_from_memory(&parsed, bytes, size, NULL));
    TEST_ASSERT_NOT_NULL(parsed);
    TEST_ASSERT_EQUAL_INT(0xAAAA, sf_mcg_unk04(parsed));
    TEST_ASSERT_EQUAL_INT(0xBBBB, sf_mcg_unk18(parsed));
    TEST_ASSERT_EQUAL_INT(0xCCCC, sf_mcg_unk1c(parsed));
    TEST_ASSERT_EQUAL_size_t(2, sf_mcg_node_count(parsed));
    TEST_ASSERT_EQUAL_size_t(1, sf_mcg_edge_count(parsed));

    const sf_mcg_node_t *pn0 = sf_mcg_node(parsed, 0);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sf_mcg_node_position(pn0).x);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, sf_mcg_node_position(pn0).z);
    TEST_ASSERT_EQUAL_INT(-1, sf_mcg_node_unk18(pn0));
    TEST_ASSERT_EQUAL_INT(42, sf_mcg_node_unk1c(pn0));
    TEST_ASSERT_EQUAL_size_t(1, sf_mcg_node_connected_node_count(pn0));
    TEST_ASSERT_EQUAL_INT(1, sf_mcg_node_connected_node_index(pn0, 0));
    TEST_ASSERT_EQUAL_INT(0, sf_mcg_node_connected_edge_index(pn0, 0));

    const sf_mcg_edge_t *pe0 = sf_mcg_edge(parsed, 0);
    TEST_ASSERT_EQUAL_INT(0, sf_mcg_edge_node_index_a(pe0));
    TEST_ASSERT_EQUAL_INT(1, sf_mcg_edge_node_index_b(pe0));
    TEST_ASSERT_EQUAL_INT(5, sf_mcg_edge_mcp_room_index(pe0));
    TEST_ASSERT_EQUAL_UINT32(0x10010000u, sf_mcg_edge_map_id(pe0));
    TEST_ASSERT_EQUAL_FLOAT(2.5f, sf_mcg_edge_unk20(pe0));
    TEST_ASSERT_EQUAL_size_t(1, sf_mcg_edge_unk_indices_a_count(pe0));
    TEST_ASSERT_EQUAL_INT(100, sf_mcg_edge_unk_indices_a_index(pe0, 0));
    TEST_ASSERT_EQUAL_size_t(2, sf_mcg_edge_unk_indices_b_count(pe0));
    TEST_ASSERT_EQUAL_INT(200, sf_mcg_edge_unk_indices_b_index(pe0, 0));
    TEST_ASSERT_EQUAL_INT(201, sf_mcg_edge_unk_indices_b_index(pe0, 1));

    void *bytes2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_write_to_memory(parsed, &bytes2, &size2, NULL));
    TEST_ASSERT_EQUAL_size_t(size, size2);
    TEST_ASSERT_EQUAL_MEMORY(bytes, bytes2, size);

    sf_free(NULL, bytes);
    sf_free(NULL, bytes2);
    sf_mcg_destroy(parsed);
    sf_mcg_destroy(src);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mcg_synthetic_roundtrip);
    return UNITY_END();
}
