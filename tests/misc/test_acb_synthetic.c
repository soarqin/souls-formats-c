/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_acb.h"
#include "souls_formats/sf_common.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void put_i32(uint8_t *buf, size_t off, int32_t v) {
    memcpy(buf + off, &v, sizeof(v));
}

static void put_u16(uint8_t *buf, size_t off, uint16_t v) {
    memcpy(buf + off, &v, sizeof(v));
}

static void test_acb_create_destroy(void) {
    sf_acb_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acb_create(&a, false, NULL));
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_FALSE(sf_acb_big_endian(a));
    TEST_ASSERT_EQUAL_size_t(0, sf_acb_asset_count(a));
    sf_acb_destroy(a);
}

static void test_acb_is_function(void) {
    const uint8_t valid[] = {'A', 'C', 'B', 0};
    const uint8_t bad[]   = {'A', 'C', 'B', '!'};
    const uint8_t short_buf[] = {'A', 'C'};
    TEST_ASSERT_TRUE(sf_acb_is(valid, sizeof(valid)));
    TEST_ASSERT_FALSE(sf_acb_is(bad, sizeof(bad)));
    TEST_ASSERT_FALSE(sf_acb_is(short_buf, sizeof(short_buf)));
    TEST_ASSERT_FALSE(sf_acb_is(NULL, 0));
}

static void test_acb_read_minimal_pwv(void) {
    enum {
        FILE_SIZE = 60,
        HEADER_END = 16,
        ASSET_OFFSETS_END = 20,
        PWV_ASSET_OFFSET = 20,
        PWV_ASSET_SIZE = 16,
        ABS_PATH_OFFSET = 36,
        REL_PATH_OFFSET = 38,
        OFFSET_INDEX_OFFSET = 40,
    };

    uint8_t buf[FILE_SIZE];
    memset(buf, 0, sizeof(buf));

    memcpy(buf + 0, "ACB\0", 4);
    buf[4] = 2;
    buf[5] = 1;
    buf[6] = 0;
    buf[7] = 0;
    put_i32(buf, 8, 1);
    put_i32(buf, 12, OFFSET_INDEX_OFFSET);

    put_i32(buf, 16, PWV_ASSET_OFFSET);

    put_i32(buf, 20, ABS_PATH_OFFSET);
    put_i32(buf, 24, REL_PATH_OFFSET);
    put_u16(buf, 28, 0);
    put_u16(buf, 30, 0);
    put_i32(buf, 32, 0);

    buf[36] = 0; buf[37] = 0;
    buf[38] = 0; buf[39] = 0;

    sf_acb_t *acb = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acb_read_from_memory(&acb, buf, sizeof(buf), NULL));
    TEST_ASSERT_NOT_NULL(acb);
    TEST_ASSERT_FALSE(sf_acb_big_endian(acb));
    TEST_ASSERT_EQUAL_size_t(1, sf_acb_asset_count(acb));

    sf_acb_asset_type_t type = SF_ACB_ASSET_MOTION;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acb_get_asset_type(acb, 0, &type));
    TEST_ASSERT_EQUAL_INT(SF_ACB_ASSET_PWV, type);

    const char *abs_p = NULL, *rel_p = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_acb_get_asset_paths(acb, 0, &abs_p, &rel_p));
    TEST_ASSERT_NOT_NULL(abs_p);
    TEST_ASSERT_NOT_NULL(rel_p);
    TEST_ASSERT_EQUAL_STRING("", abs_p);
    TEST_ASSERT_EQUAL_STRING("", rel_p);

    sf_acb_destroy(acb);
}

static void test_acb_read_bad_magic(void) {
    uint8_t buf[60];
    memset(buf, 0, sizeof(buf));
    memcpy(buf + 0, "BAD\0", 4);
    sf_acb_t *acb = NULL;
    TEST_ASSERT_NOT_EQUAL(SF_OK, sf_acb_read_from_memory(&acb, buf, sizeof(buf), NULL));
    TEST_ASSERT_NULL(acb);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_acb_create_destroy);
    RUN_TEST(test_acb_is_function);
    RUN_TEST(test_acb_read_minimal_pwv);
    RUN_TEST(test_acb_read_bad_magic);
    return UNITY_END();
}
