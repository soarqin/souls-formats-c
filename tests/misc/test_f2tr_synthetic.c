/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_f2tr.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_f2tr_create_destroy(void) {
    sf_f2tr_t *f = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_create(&f, NULL));
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_FALSE(sf_f2tr_big_endian(f));
    TEST_ASSERT_EQUAL_size_t(0, sf_f2tr_entry_count(f));
    sf_f2tr_destroy(f);
}

static void test_f2tr_is_function(void) {
    const uint8_t valid[] = {'F', '2', 'T', 'R'};
    const uint8_t bad_magic[] = {'X', '2', 'T', 'R'};
    const uint8_t too_short[] = {'F', '2'};
    TEST_ASSERT_TRUE(sf_f2tr_is(valid, sizeof(valid)));
    TEST_ASSERT_FALSE(sf_f2tr_is(bad_magic, sizeof(bad_magic)));
    TEST_ASSERT_FALSE(sf_f2tr_is(too_short, sizeof(too_short)));
    TEST_ASSERT_FALSE(sf_f2tr_is(NULL, 0));
}

static void test_f2tr_round_trip(void) {
    sf_f2tr_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_create(&a, NULL));

    int16_t idx0[] = {10, 20, 30, 40};
    int16_t idx1[] = {100, 200};
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_add_entry(a, "mesh_a", idx0, 4));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_add_entry(a, "mesh_beta", idx1, 2));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_add_entry(a, "empty", NULL, 0));
    TEST_ASSERT_EQUAL_size_t(3, sf_f2tr_entry_count(a));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_write_to_memory(a, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_TRUE(size > 0);
    TEST_ASSERT_TRUE(sf_f2tr_is(bytes, size));

    sf_f2tr_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_FALSE(sf_f2tr_big_endian(b));
    TEST_ASSERT_EQUAL_size_t(3, sf_f2tr_entry_count(b));

    TEST_ASSERT_EQUAL_STRING("mesh_a", sf_f2tr_get_entry_name(b, 0));
    TEST_ASSERT_EQUAL_STRING("mesh_beta", sf_f2tr_get_entry_name(b, 1));
    TEST_ASSERT_EQUAL_STRING("empty", sf_f2tr_get_entry_name(b, 2));

    const int16_t *got_idx = NULL;
    size_t got_count = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_get_entry_indices(b, 0, &got_idx, &got_count));
    TEST_ASSERT_EQUAL_size_t(4, got_count);
    TEST_ASSERT_EQUAL_INT16_ARRAY(idx0, got_idx, 4);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_get_entry_indices(b, 1, &got_idx, &got_count));
    TEST_ASSERT_EQUAL_size_t(2, got_count);
    TEST_ASSERT_EQUAL_INT16_ARRAY(idx1, got_idx, 2);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_get_entry_indices(b, 2, &got_idx, &got_count));
    TEST_ASSERT_EQUAL_size_t(0, got_count);

    sf_free(NULL, bytes);
    sf_f2tr_destroy(b);
    sf_f2tr_destroy(a);
}

static void test_f2tr_round_trip_big_endian(void) {
    sf_f2tr_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_create(&a, NULL));
    sf_f2tr_set_big_endian(a, true);

    int16_t idx[] = {0x1234, 0x5678};
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_add_entry(a, "be_entry", idx, 2));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_write_to_memory(a, &bytes, &size, NULL));

    sf_f2tr_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_TRUE(sf_f2tr_big_endian(b));
    TEST_ASSERT_EQUAL_STRING("be_entry", sf_f2tr_get_entry_name(b, 0));

    const int16_t *got_idx = NULL;
    size_t got_count = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_f2tr_get_entry_indices(b, 0, &got_idx, &got_count));
    TEST_ASSERT_EQUAL_INT16_ARRAY(idx, got_idx, 2);

    sf_free(NULL, bytes);
    sf_f2tr_destroy(b);
    sf_f2tr_destroy(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_f2tr_create_destroy);
    RUN_TEST(test_f2tr_is_function);
    RUN_TEST(test_f2tr_round_trip);
    RUN_TEST(test_f2tr_round_trip_big_endian);
    return UNITY_END();
}
