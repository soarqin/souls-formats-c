/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_emeld.h"
#include "souls_formats/sf_emevd.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_emeld_is_function(void) {
    const uint8_t good[]    = {'E', 'L', 'D', 0x00};
    const uint8_t bad[]     = {'E', 'V', 'D', 0x00};
    const uint8_t short_b[] = {'E', 'L', 'D'};
    TEST_ASSERT_TRUE (sf_emeld_is(good,    sizeof(good)));
    TEST_ASSERT_FALSE(sf_emeld_is(bad,     sizeof(bad)));
    TEST_ASSERT_FALSE(sf_emeld_is(short_b, sizeof(short_b)));
    TEST_ASSERT_FALSE(sf_emeld_is(NULL,    0));
}

static void test_emeld_create_destroy(void) {
    sf_emeld_t *e = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_emeld_create(&e, SF_EMEVD_FORMAT_DARK_SOULS_1, NULL));
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_INT(SF_EMEVD_FORMAT_DARK_SOULS_1, sf_emeld_get_format(e));
    TEST_ASSERT_EQUAL_size_t(0, sf_emeld_event_count(e));
    sf_emeld_destroy(e);

    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_emeld_create(&e, SF_EMEVD_FORMAT_DARK_SOULS_3, NULL));
}

static void assert_round_trip(sf_emevd_format_t format) {
    sf_emeld_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emeld_create(&src, format, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emeld_add_event(src, 1100,   "EVENT_ONE"));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emeld_add_event(src, 999999, "EVENT_TWO"));
    TEST_ASSERT_EQUAL_size_t(2, sf_emeld_event_count(src));

    uint8_t *bytes = NULL;
    size_t   size  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_emeld_write_to_memory(src, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN_UINT64(0, size);
    TEST_ASSERT_TRUE(sf_emeld_is(bytes, size));

    sf_emeld_t *dst = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_emeld_read_from_memory(&dst, bytes, size, NULL));
    TEST_ASSERT_NOT_NULL(dst);
    TEST_ASSERT_EQUAL_INT(format, sf_emeld_get_format(dst));
    TEST_ASSERT_EQUAL_size_t(2, sf_emeld_event_count(dst));

    const sf_emeld_event_t *ev = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emeld_get_event(dst, 0, &ev));
    TEST_ASSERT_EQUAL_INT64(1100, ev->id);
    TEST_ASSERT_EQUAL_STRING("EVENT_ONE", ev->name);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emeld_get_event(dst, 1, &ev));
    TEST_ASSERT_EQUAL_INT64(999999, ev->id);
    TEST_ASSERT_EQUAL_STRING("EVENT_TWO", ev->name);
    TEST_ASSERT_EQUAL_INT(SF_ERR_OUT_OF_RANGE,
        sf_emeld_get_event(dst, 2, &ev));

    uint8_t *bytes2 = NULL;
    size_t   size2  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_emeld_write_to_memory(dst, &bytes2, &size2, NULL));
    TEST_ASSERT_EQUAL_UINT64(size, size2);
    TEST_ASSERT_EQUAL_MEMORY(bytes, bytes2, size);

    sf_free(NULL, bytes);
    sf_free(NULL, bytes2);
    sf_emeld_destroy(src);
    sf_emeld_destroy(dst);
}

static void test_emeld_round_trip_ds1(void) {
    assert_round_trip(SF_EMEVD_FORMAT_DARK_SOULS_1);
}

static void test_emeld_round_trip_bloodborne(void) {
    assert_round_trip(SF_EMEVD_FORMAT_BLOODBORNE);
}

static void test_emeld_bad_magic(void) {
    const uint8_t junk[16] = {'X', 'Y', 'Z', 0,
                              0, 0, 0, 0,
                              0, 0, 0, 0,
                              0, 0, 0, 0};
    sf_emeld_t *emeld = NULL;
    TEST_ASSERT_EQUAL_INT(SF_ERR_BAD_MAGIC,
        sf_emeld_read_from_memory(&emeld, junk, sizeof(junk), NULL));
    TEST_ASSERT_NULL(emeld);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_emeld_is_function);
    RUN_TEST(test_emeld_create_destroy);
    RUN_TEST(test_emeld_round_trip_ds1);
    RUN_TEST(test_emeld_round_trip_bloodborne);
    RUN_TEST(test_emeld_bad_magic);
    return UNITY_END();
}
