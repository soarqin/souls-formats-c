/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_luagnl.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_luagnl_create_destroy(void) {
    sf_luagnl_t *gnl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_luagnl_create(&gnl, false, false, NULL));
    TEST_ASSERT_NOT_NULL(gnl);
    TEST_ASSERT_FALSE(sf_luagnl_big_endian(gnl));
    TEST_ASSERT_FALSE(sf_luagnl_long_format(gnl));
    TEST_ASSERT_EQUAL_size_t(0, sf_luagnl_global_count(gnl));
    sf_luagnl_destroy(gnl);
}

static void test_luagnl_is_function(void) {
    const uint8_t any4[]   = {0x01, 0x02, 0x03, 0x04};
    const uint8_t zeros4[] = {0x00, 0x00, 0x00, 0x00};
    const uint8_t three[]  = {0x00, 0x00, 0x00};
    TEST_ASSERT_TRUE (sf_luagnl_is(any4,   sizeof(any4)));
    TEST_ASSERT_TRUE (sf_luagnl_is(zeros4, sizeof(zeros4)));
    TEST_ASSERT_FALSE(sf_luagnl_is(three,  sizeof(three)));
    TEST_ASSERT_FALSE(sf_luagnl_is(NULL,   0));
}

static void test_luagnl_add_global(void) {
    sf_luagnl_t *gnl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_luagnl_create(&gnl, false, false, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_luagnl_add_global(gnl, "alpha"));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_luagnl_add_global(gnl, "beta"));
    TEST_ASSERT_EQUAL_size_t(2, sf_luagnl_global_count(gnl));

    const char *s = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_luagnl_get_global(gnl, 0, &s));
    TEST_ASSERT_EQUAL_STRING("alpha", s);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_luagnl_get_global(gnl, 1, &s));
    TEST_ASSERT_EQUAL_STRING("beta", s);

    TEST_ASSERT_EQUAL_INT(SF_ERR_OUT_OF_RANGE, sf_luagnl_get_global(gnl, 2, &s));
    sf_luagnl_destroy(gnl);
}

static void assert_round_trip(bool big_endian, bool long_format) {
    sf_luagnl_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_luagnl_create(&src, big_endian, long_format, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_luagnl_add_global(src, "g_HelloWorld"));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_luagnl_add_global(src, "g_PlayerName"));

    uint8_t *bytes = NULL;
    size_t   size  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_luagnl_write_to_memory(src, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN_UINT64(0, size);
    TEST_ASSERT_EQUAL_UINT64(0, size % 0x10u);

    sf_luagnl_t *dst = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_luagnl_read_from_memory(&dst, bytes, size, NULL));
    TEST_ASSERT_NOT_NULL(dst);
    TEST_ASSERT_EQUAL_INT(big_endian,  sf_luagnl_big_endian(dst));
    TEST_ASSERT_EQUAL_INT(long_format, sf_luagnl_long_format(dst));
    TEST_ASSERT_EQUAL_size_t(2, sf_luagnl_global_count(dst));

    const char *s = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_luagnl_get_global(dst, 0, &s));
    TEST_ASSERT_EQUAL_STRING("g_HelloWorld", s);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_luagnl_get_global(dst, 1, &s));
    TEST_ASSERT_EQUAL_STRING("g_PlayerName", s);

    uint8_t *bytes2 = NULL;
    size_t   size2  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_luagnl_write_to_memory(dst, &bytes2, &size2, NULL));
    TEST_ASSERT_EQUAL_UINT64(size, size2);
    TEST_ASSERT_EQUAL_MEMORY(bytes, bytes2, size);

    sf_free(NULL, bytes);
    sf_free(NULL, bytes2);
    sf_luagnl_destroy(src);
    sf_luagnl_destroy(dst);
}

static void test_luagnl_round_trip_short_le(void) { assert_round_trip(false, false); }
static void test_luagnl_round_trip_short_be(void) { assert_round_trip(true,  false); }
static void test_luagnl_round_trip_long_le (void) { assert_round_trip(false, true);  }
static void test_luagnl_round_trip_long_be (void) { assert_round_trip(true,  true);  }

static void test_luagnl_round_trip_empty(void) {
    sf_luagnl_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_luagnl_create(&src, false, true, NULL));

    uint8_t *bytes = NULL;
    size_t   size  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_luagnl_write_to_memory(src, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(8, size);

    sf_luagnl_t *dst = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_luagnl_read_from_memory(&dst, bytes, size, NULL));
    TEST_ASSERT_EQUAL_size_t(0, sf_luagnl_global_count(dst));
    TEST_ASSERT_TRUE(sf_luagnl_long_format(dst));

    sf_free(NULL, bytes);
    sf_luagnl_destroy(src);
    sf_luagnl_destroy(dst);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_luagnl_create_destroy);
    RUN_TEST(test_luagnl_is_function);
    RUN_TEST(test_luagnl_add_global);
    RUN_TEST(test_luagnl_round_trip_short_le);
    RUN_TEST(test_luagnl_round_trip_short_be);
    RUN_TEST(test_luagnl_round_trip_long_le);
    RUN_TEST(test_luagnl_round_trip_long_be);
    RUN_TEST(test_luagnl_round_trip_empty);
    return UNITY_END();
}
