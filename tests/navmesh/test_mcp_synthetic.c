/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_mcp.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_mcp_synthetic_roundtrip(void) {
    sf_mcp_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcp_create_empty(&src, NULL));
    sf_mcp_set_unk04(src, 0x12345);
    sf_mcp_set_big_endian(src, false);

    sf_mcp_room_t *r0 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcp_append_room(src, &r0));
    sf_mcp_room_set_map_id(r0, 0x10010000u);
    sf_mcp_room_set_local_index(r0, 0);
    sf_vec3_t mn = {-1, -2, -3};
    sf_vec3_t mx = { 1,  2,  3};
    sf_mcp_room_set_bbox_min(r0, mn);
    sf_mcp_room_set_bbox_max(r0, mx);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcp_room_append_connected_index(r0, 1));

    sf_mcp_room_t *r1 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcp_append_room(src, &r1));
    sf_mcp_room_set_map_id(r1, 0x10010100u);
    sf_mcp_room_set_local_index(r1, 1);
    sf_vec3_t mn2 = {10, 20, 30};
    sf_vec3_t mx2 = {11, 21, 31};
    sf_mcp_room_set_bbox_min(r1, mn2);
    sf_mcp_room_set_bbox_max(r1, mx2);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcp_room_append_connected_index(r1, 0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcp_room_append_connected_index(r1, 0));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcp_write_to_memory(src, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);

    sf_mcp_t *parsed = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcp_read_from_memory(&parsed, bytes, size, NULL));
    TEST_ASSERT_NOT_NULL(parsed);
    TEST_ASSERT_FALSE(sf_mcp_big_endian(parsed));
    TEST_ASSERT_EQUAL_INT(0x12345, sf_mcp_unk04(parsed));
    TEST_ASSERT_EQUAL_size_t(2, sf_mcp_room_count(parsed));

    const sf_mcp_room_t *p0 = sf_mcp_room(parsed, 0);
    TEST_ASSERT_EQUAL_UINT32(0x10010000u, sf_mcp_room_map_id(p0));
    TEST_ASSERT_EQUAL_INT(0, sf_mcp_room_local_index(p0));
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, sf_mcp_room_bbox_min(p0).x);
    TEST_ASSERT_EQUAL_FLOAT( 3.0f, sf_mcp_room_bbox_max(p0).z);
    TEST_ASSERT_EQUAL_size_t(1, sf_mcp_room_connected_count(p0));
    TEST_ASSERT_EQUAL_INT(1, sf_mcp_room_connected_index(p0, 0));

    const sf_mcp_room_t *p1 = sf_mcp_room(parsed, 1);
    TEST_ASSERT_EQUAL_UINT32(0x10010100u, sf_mcp_room_map_id(p1));
    TEST_ASSERT_EQUAL_size_t(2, sf_mcp_room_connected_count(p1));
    TEST_ASSERT_EQUAL_INT(0, sf_mcp_room_connected_index(p1, 0));
    TEST_ASSERT_EQUAL_INT(0, sf_mcp_room_connected_index(p1, 1));

    void *bytes2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcp_write_to_memory(parsed, &bytes2, &size2, NULL));
    TEST_ASSERT_EQUAL_size_t(size, size2);
    TEST_ASSERT_EQUAL_MEMORY(bytes, bytes2, size);

    sf_free(NULL, bytes);
    sf_free(NULL, bytes2);
    sf_mcp_destroy(parsed);
    sf_mcp_destroy(src);
}

static void test_mcp_big_endian_roundtrip(void) {
    sf_mcp_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcp_create_empty(&src, NULL));
    sf_mcp_set_big_endian(src, true);

    sf_mcp_room_t *r0 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcp_append_room(src, &r0));
    sf_mcp_room_set_map_id(r0, 1);

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcp_write_to_memory(src, &bytes, &size, NULL));

    sf_mcp_t *parsed = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcp_read_from_memory(&parsed, bytes, size, NULL));
    TEST_ASSERT_TRUE(sf_mcp_big_endian(parsed));
    TEST_ASSERT_EQUAL_size_t(1, sf_mcp_room_count(parsed));

    sf_free(NULL, bytes);
    sf_mcp_destroy(parsed);
    sf_mcp_destroy(src);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mcp_synthetic_roundtrip);
    RUN_TEST(test_mcp_big_endian_roundtrip);
    return UNITY_END();
}
