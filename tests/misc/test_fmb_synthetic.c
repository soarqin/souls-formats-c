/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_fmb.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_fmb_is_function(void) {
    const uint8_t valid[]     = {'F', 'M', 'B', ' '};
    const uint8_t bad[]       = {'F', 'M', 'B', '!'};
    const uint8_t short_buf[] = {'F', 'M'};
    TEST_ASSERT_TRUE(sf_fmb_is(valid, sizeof(valid)));
    TEST_ASSERT_FALSE(sf_fmb_is(bad, sizeof(bad)));
    TEST_ASSERT_FALSE(sf_fmb_is(short_buf, sizeof(short_buf)));
    TEST_ASSERT_FALSE(sf_fmb_is(NULL, 0));
}

static void test_fmb_create_destroy(void) {
    sf_fmb_t *f = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_create(&f, NULL));
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(0, sf_fmb_get_unk20(f));
    TEST_ASSERT_EQUAL_size_t(0, sf_fmb_entry_count(f));
    sf_fmb_set_unk20(f, 0x1234);
    TEST_ASSERT_EQUAL_INT(0x1234, sf_fmb_get_unk20(f));
    sf_fmb_destroy(f);
}

static void round_trip(sf_fmb_t *src, sf_fmb_t **out) {
    uint8_t *buf = NULL;
    size_t   sz  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_write_to_memory(src, &buf, &sz, NULL));
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_TRUE(sz >= 0x40u);
    TEST_ASSERT_TRUE(sf_fmb_is(buf, sz));

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_read_from_memory(out, buf, sz, NULL));
    sf_free(NULL, buf);
}

static void test_fmb_round_trip_plain(void) {
    sf_fmb_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_create(&src, NULL));
    sf_fmb_set_unk20(src, 7);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_add_plain_entry(src, 2));

    sf_fmb_t *dst = NULL;
    round_trip(src, &dst);
    TEST_ASSERT_NOT_NULL(dst);
    TEST_ASSERT_EQUAL_INT(7, sf_fmb_get_unk20(dst));
    TEST_ASSERT_EQUAL_size_t(1, sf_fmb_entry_count(dst));

    const sf_fmb_entry_t *e = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_get_entry(dst, 0, &e));
    TEST_ASSERT_EQUAL_INT(2, e->type);
    TEST_ASSERT_EQUAL_INT(SF_FMB_ENTRY_KIND_PLAIN, e->kind);
    TEST_ASSERT_NULL(e->string_value);

    sf_fmb_destroy(src);
    sf_fmb_destroy(dst);
}

static void test_fmb_round_trip_string(void) {
    sf_fmb_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_create(&src, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_add_string_entry(src, 7, "hello"));

    sf_fmb_t *dst = NULL;
    round_trip(src, &dst);
    TEST_ASSERT_EQUAL_size_t(1, sf_fmb_entry_count(dst));

    const sf_fmb_entry_t *e = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_get_entry(dst, 0, &e));
    TEST_ASSERT_EQUAL_INT(7, e->type);
    TEST_ASSERT_EQUAL_INT(SF_FMB_ENTRY_KIND_STRING, e->kind);
    TEST_ASSERT_NOT_NULL(e->string_value);
    TEST_ASSERT_EQUAL_STRING("hello", e->string_value);

    sf_fmb_destroy(src);
    sf_fmb_destroy(dst);
}

static void test_fmb_round_trip_double(void) {
    sf_fmb_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_create(&src, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_add_double_entry(src, 1, 3.14159));

    sf_fmb_t *dst = NULL;
    round_trip(src, &dst);
    TEST_ASSERT_EQUAL_size_t(1, sf_fmb_entry_count(dst));

    const sf_fmb_entry_t *e = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_get_entry(dst, 0, &e));
    TEST_ASSERT_EQUAL_INT(1, e->type);
    TEST_ASSERT_EQUAL_INT(SF_FMB_ENTRY_KIND_DOUBLE, e->kind);
    TEST_ASSERT_EQUAL_DOUBLE(3.14159, e->double_value);

    sf_fmb_destroy(src);
    sf_fmb_destroy(dst);
}

static void test_fmb_round_trip_double2(void) {
    sf_fmb_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_create(&src, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_add_double2_entry(src, 52, 1.5, -2.5));

    sf_fmb_t *dst = NULL;
    round_trip(src, &dst);
    TEST_ASSERT_EQUAL_size_t(1, sf_fmb_entry_count(dst));

    const sf_fmb_entry_t *e = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fmb_get_entry(dst, 0, &e));
    TEST_ASSERT_EQUAL_INT(52, e->type);
    TEST_ASSERT_EQUAL_INT(SF_FMB_ENTRY_KIND_DOUBLE2, e->kind);
    TEST_ASSERT_EQUAL_DOUBLE(1.5, e->double_value);
    TEST_ASSERT_EQUAL_DOUBLE(-2.5, e->double_value2);

    sf_fmb_destroy(src);
    sf_fmb_destroy(dst);
}

static void test_fmb_bad_magic(void) {
    uint8_t buf[0x80];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "BAD ", 4);
    sf_fmb_t *f = NULL;
    TEST_ASSERT_NOT_EQUAL(SF_OK, sf_fmb_read_from_memory(&f, buf, sizeof(buf), NULL));
    TEST_ASSERT_NULL(f);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fmb_is_function);
    RUN_TEST(test_fmb_create_destroy);
    RUN_TEST(test_fmb_round_trip_plain);
    RUN_TEST(test_fmb_round_trip_string);
    RUN_TEST(test_fmb_round_trip_double);
    RUN_TEST(test_fmb_round_trip_double2);
    RUN_TEST(test_fmb_bad_magic);
    return UNITY_END();
}
