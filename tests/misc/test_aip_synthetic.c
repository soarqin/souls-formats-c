/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_aip.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_aip_create_destroy(void) {
    sf_aip_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_aip_create(&a, NULL));
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_EQUAL_UINT32(0, sf_aip_version(a));
    TEST_ASSERT_EQUAL_size_t(0, sf_aip_point_count(a));
    sf_aip_destroy(a);
}

static void test_aip_is_function(void) {
    const uint8_t valid[] = {'F', 'P', 'I', 'A', 0, 0, 0, 0};
    const uint8_t bad_magic[] = {'X', 'X', 'X', 'X'};
    const uint8_t too_short[] = {'F', 'P'};
    TEST_ASSERT_TRUE(sf_aip_is(valid, sizeof(valid)));
    TEST_ASSERT_FALSE(sf_aip_is(bad_magic, sizeof(bad_magic)));
    TEST_ASSERT_FALSE(sf_aip_is(too_short, sizeof(too_short)));
    TEST_ASSERT_FALSE(sf_aip_is(NULL, 0));
}

static void test_aip_round_trip(void) {
    sf_aip_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_aip_create(&a, NULL));

    sf_aip_set_version(a, 0x12345678u);
    sf_aip_block_id_t id = { 10, 20, 30, 40 };
    sf_aip_set_block_id(a, id);

    sf_aip_point_t p0 = { 1.0f, 2.0f, 3.0f, 0.5f };
    sf_aip_point_t p1 = { -4.5f, 100.0f, 99.25f, -1.5f };
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_aip_add_point(a, p0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_aip_add_point(a, p1));
    TEST_ASSERT_EQUAL_size_t(2, sf_aip_point_count(a));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_aip_write_to_memory(a, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_size_t(4 + 4 + 4 + 4 + 2 * 16, size);
    TEST_ASSERT_TRUE(sf_aip_is(bytes, size));

    sf_aip_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_aip_read_from_memory(&b, bytes, size, NULL));

    TEST_ASSERT_EQUAL_UINT32(0x12345678u, sf_aip_version(b));
    sf_aip_block_id_t rid = sf_aip_block_id(b);
    TEST_ASSERT_EQUAL_UINT8(10, rid.index);
    TEST_ASSERT_EQUAL_UINT8(20, rid.region);
    TEST_ASSERT_EQUAL_UINT8(30, rid.block);
    TEST_ASSERT_EQUAL_UINT8(40, rid.area);
    TEST_ASSERT_EQUAL_size_t(2, sf_aip_point_count(b));

    sf_aip_point_t r0, r1;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_aip_get_point(b, 0, &r0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_aip_get_point(b, 1, &r1));
    TEST_ASSERT_EQUAL_FLOAT(p0.x, r0.x);
    TEST_ASSERT_EQUAL_FLOAT(p0.y, r0.y);
    TEST_ASSERT_EQUAL_FLOAT(p0.z, r0.z);
    TEST_ASSERT_EQUAL_FLOAT(p0.rotation, r0.rotation);
    TEST_ASSERT_EQUAL_FLOAT(p1.x, r1.x);
    TEST_ASSERT_EQUAL_FLOAT(p1.y, r1.y);
    TEST_ASSERT_EQUAL_FLOAT(p1.z, r1.z);
    TEST_ASSERT_EQUAL_FLOAT(p1.rotation, r1.rotation);

    sf_free(NULL, bytes);
    sf_aip_destroy(b);
    sf_aip_destroy(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aip_create_destroy);
    RUN_TEST(test_aip_is_function);
    RUN_TEST(test_aip_round_trip);
    return UNITY_END();
}
