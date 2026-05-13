/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_rmb.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_rmb_create_destroy(void) {
    sf_rmb_t *r = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_create(&r, NULL));
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_FALSE(sf_rmb_big_endian(r));
    TEST_ASSERT_EQUAL_size_t(0, sf_rmb_rumble_count(r));
    sf_rmb_destroy(r);
}

static void test_rmb_round_trip_little_endian(void) {
    sf_rmb_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_create(&a, NULL));

    size_t r0 = 0, r1 = 0, r2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_add_rumble(a, &r0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_add_rumble(a, &r1));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_add_rumble(a, &r2));

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_add_heavy_state(a, r0, 0.25f, 1.5f));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_add_heavy_state(a, r0, 0.75f, 2.5f));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_add_light_state(a, r0, 0.1f, 0.2f));

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_add_light_state(a, r1, 0.5f, 1.0f));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_write_to_memory(a, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_TRUE(size > 0);

    sf_rmb_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_FALSE(sf_rmb_big_endian(b));
    TEST_ASSERT_EQUAL_size_t(3, sf_rmb_rumble_count(b));

    size_t hc = 0, lc = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_get_heavy_state_count(b, 0, &hc));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_get_light_state_count(b, 0, &lc));
    TEST_ASSERT_EQUAL_size_t(2, hc);
    TEST_ASSERT_EQUAL_size_t(1, lc);

    sf_rmb_state_t st;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_get_heavy_state(b, 0, 0, &st));
    TEST_ASSERT_EQUAL_FLOAT(0.25f, st.power);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, st.duration);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_get_heavy_state(b, 0, 1, &st));
    TEST_ASSERT_EQUAL_FLOAT(0.75f, st.power);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, st.duration);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_get_light_state(b, 0, 0, &st));
    TEST_ASSERT_EQUAL_FLOAT(0.1f, st.power);
    TEST_ASSERT_EQUAL_FLOAT(0.2f, st.duration);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_get_heavy_state_count(b, 1, &hc));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_get_light_state_count(b, 1, &lc));
    TEST_ASSERT_EQUAL_size_t(0, hc);
    TEST_ASSERT_EQUAL_size_t(1, lc);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_get_light_state(b, 1, 0, &st));
    TEST_ASSERT_EQUAL_FLOAT(0.5f, st.power);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, st.duration);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_get_heavy_state_count(b, 2, &hc));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_get_light_state_count(b, 2, &lc));
    TEST_ASSERT_EQUAL_size_t(0, hc);
    TEST_ASSERT_EQUAL_size_t(0, lc);

    sf_free(NULL, bytes);
    sf_rmb_destroy(b);
    sf_rmb_destroy(a);
}

static void test_rmb_round_trip_big_endian(void) {
    sf_rmb_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_create(&a, NULL));
    sf_rmb_set_big_endian(a, true);

    size_t r0 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_add_rumble(a, &r0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_add_heavy_state(a, r0, 1.0f, 2.0f));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_write_to_memory(a, &bytes, &size, NULL));

    sf_rmb_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_TRUE(sf_rmb_big_endian(b));
    TEST_ASSERT_EQUAL_size_t(1, sf_rmb_rumble_count(b));

    sf_rmb_state_t st;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_rmb_get_heavy_state(b, 0, 0, &st));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, st.power);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, st.duration);

    sf_free(NULL, bytes);
    sf_rmb_destroy(b);
    sf_rmb_destroy(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rmb_create_destroy);
    RUN_TEST(test_rmb_round_trip_little_endian);
    RUN_TEST(test_rmb_round_trip_big_endian);
    return UNITY_END();
}
