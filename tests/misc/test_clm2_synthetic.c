/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_clm2.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_clm2_create_destroy(void) {
    sf_clm2_t *c = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_create(&c, NULL));
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQUAL_size_t(0, sf_clm2_mesh_count(c));
    sf_clm2_destroy(c);
}

static void test_clm2_is_function(void) {
    const uint8_t valid[] = {'C', 'L', 'M', '2'};
    const uint8_t bad[] = {'C', 'L', 'M', '1'};
    const uint8_t too_short[] = {'C', 'L'};
    TEST_ASSERT_TRUE(sf_clm2_is(valid, sizeof(valid)));
    TEST_ASSERT_FALSE(sf_clm2_is(bad, sizeof(bad)));
    TEST_ASSERT_FALSE(sf_clm2_is(too_short, sizeof(too_short)));
    TEST_ASSERT_FALSE(sf_clm2_is(NULL, 0));
}

static void test_clm2_round_trip(void) {
    sf_clm2_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_create(&a, NULL));

    size_t m0 = 0, m1 = 0, m2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_add_mesh(a, &m0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_add_mesh(a, &m1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_add_mesh(a, &m2));
    TEST_ASSERT_EQUAL_size_t(0, m0);
    TEST_ASSERT_EQUAL_size_t(1, m1);
    TEST_ASSERT_EQUAL_size_t(2, m2);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_add_mesh_entry(a, m0, 11, 22));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_add_mesh_entry(a, m0, 33, 44));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_add_mesh_entry(a, m0, -1, 32000));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_add_mesh_entry(a, m1, 100, 200));

    TEST_ASSERT_EQUAL_size_t(3, sf_clm2_mesh_count(a));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_write_to_memory(a, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_TRUE(size > 0);
    TEST_ASSERT_TRUE(sf_clm2_is(bytes, size));

    sf_clm2_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_EQUAL_size_t(3, sf_clm2_mesh_count(b));

    size_t c0 = 0, c1 = 0, c2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_get_mesh_entry_count(b, 0, &c0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_get_mesh_entry_count(b, 1, &c1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_get_mesh_entry_count(b, 2, &c2));
    TEST_ASSERT_EQUAL_size_t(3, c0);
    TEST_ASSERT_EQUAL_size_t(1, c1);
    TEST_ASSERT_EQUAL_size_t(0, c2);

    sf_clm2_entry_t got;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_get_mesh_entry(b, 0, 0, &got));
    TEST_ASSERT_EQUAL_INT16(11, got.unk00);
    TEST_ASSERT_EQUAL_INT16(22, got.unk02);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_get_mesh_entry(b, 0, 1, &got));
    TEST_ASSERT_EQUAL_INT16(33, got.unk00);
    TEST_ASSERT_EQUAL_INT16(44, got.unk02);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_get_mesh_entry(b, 0, 2, &got));
    TEST_ASSERT_EQUAL_INT16(-1, got.unk00);
    TEST_ASSERT_EQUAL_INT16(32000, got.unk02);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_clm2_get_mesh_entry(b, 1, 0, &got));
    TEST_ASSERT_EQUAL_INT16(100, got.unk00);
    TEST_ASSERT_EQUAL_INT16(200, got.unk02);

    sf_free(NULL, bytes);
    sf_clm2_destroy(b);
    sf_clm2_destroy(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clm2_create_destroy);
    RUN_TEST(test_clm2_is_function);
    RUN_TEST(test_clm2_round_trip);
    return UNITY_END();
}
