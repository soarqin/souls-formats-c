/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_mlb.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_mlb_ac4_create_destroy(void) {
    sf_mlb_ac4_t *m = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mlb_ac4_create(&m, SF_MLB_AC4_RESOURCE_MODEL, false, NULL));
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_INT(SF_MLB_AC4_RESOURCE_MODEL, sf_mlb_ac4_resource_type(m));
    TEST_ASSERT_FALSE(sf_mlb_ac4_is_animation(m));
    TEST_ASSERT_EQUAL_size_t(0, sf_mlb_ac4_resource_count(m));
    sf_mlb_ac4_destroy(m);
}

static void test_mlb_ac4_write_read_round_trip(void) {
    sf_mlb_ac4_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mlb_ac4_create(&a, SF_MLB_AC4_RESOURCE_TEXTURE, false, NULL));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mlb_ac4_write_to_memory(a, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_TRUE(size >= 16u);

    sf_mlb_ac4_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mlb_ac4_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_MLB_AC4_RESOURCE_TEXTURE, sf_mlb_ac4_resource_type(b));
    TEST_ASSERT_EQUAL_size_t(0, sf_mlb_ac4_resource_count(b));

    sf_free(NULL, bytes);
    sf_mlb_ac4_destroy(b);
    sf_mlb_ac4_destroy(a);
}

static void test_mlb_ac5_create_destroy(void) {
    sf_mlb_ac5_t *m = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mlb_ac5_create(&m, SF_MLB_AC5_RESOURCE_MODEL, NULL));
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_INT(SF_MLB_AC5_RESOURCE_MODEL, sf_mlb_ac5_resource_type(m));
    TEST_ASSERT_EQUAL_size_t(0, sf_mlb_ac5_resource_count(m));
    sf_mlb_ac5_destroy(m);
}

static void test_mlb_ac5_write_read_round_trip(void) {
    sf_mlb_ac5_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mlb_ac5_create(&a, SF_MLB_AC5_RESOURCE_MODEL, NULL));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mlb_ac5_write_to_memory(a, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_TRUE(size >= 16u);

    sf_mlb_ac5_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mlb_ac5_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_MLB_AC5_RESOURCE_MODEL, sf_mlb_ac5_resource_type(b));
    TEST_ASSERT_EQUAL_size_t(0, sf_mlb_ac5_resource_count(b));

    sf_free(NULL, bytes);
    sf_mlb_ac5_destroy(b);
    sf_mlb_ac5_destroy(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mlb_ac4_create_destroy);
    RUN_TEST(test_mlb_ac4_write_read_round_trip);
    RUN_TEST(test_mlb_ac5_create_destroy);
    RUN_TEST(test_mlb_ac5_write_read_round_trip);
    return UNITY_END();
}
