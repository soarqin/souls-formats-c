/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_edd.h"
#include "souls_formats/sf_common.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void put_i32(uint8_t *buf, size_t off, int32_t v) {
    memcpy(buf + off, &v, sizeof(v));
}

static void test_edd_create_destroy(void) {
    sf_edd_t *e = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_edd_create(&e, false, NULL));
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_FALSE(sf_edd_long_format(e));
    TEST_ASSERT_EQUAL_size_t(0, sf_edd_function_spec_count(e));
    TEST_ASSERT_EQUAL_size_t(0, sf_edd_command_spec_count(e));
    sf_edd_destroy(e);
}

static void test_edd_create_long_format(void) {
    sf_edd_t *e = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_edd_create(&e, true, NULL));
    TEST_ASSERT_TRUE(sf_edd_long_format(e));
    sf_edd_destroy(e);
}

static void test_edd_read_minimal_32bit(void) {
    enum {
        DATA_START = 148,
        DATA_SIZE  = 52,
        FILE_SIZE  = DATA_START + DATA_SIZE,
        STRINGS_OFF = FILE_SIZE,
    };

    uint8_t buf[FILE_SIZE];
    memset(buf, 0, sizeof(buf));

    memcpy(buf + 0, "fSSL", 4);
    put_i32(buf, 4,  1);
    put_i32(buf, 8,  1);
    put_i32(buf, 12, 1);
    put_i32(buf, 16, 0x7C);
    put_i32(buf, 20, DATA_SIZE);
    put_i32(buf, 24, 11);

    size_t pos = 28;
    put_i32(buf, pos +  0, 0x34); put_i32(buf, pos +  4, 1);
    put_i32(buf, pos +  8, 8);    put_i32(buf, pos + 12, 0);
    put_i32(buf, pos + 16, 4);    put_i32(buf, pos + 20, 0);
    put_i32(buf, pos + 24, 8);    put_i32(buf, pos + 28, 0);
    put_i32(buf, pos + 32, 8);    put_i32(buf, pos + 36, 0);
    put_i32(buf, pos + 40, 8);    put_i32(buf, pos + 44, 0);
    put_i32(buf, pos + 48, 0x10); put_i32(buf, pos + 52, 0);
    put_i32(buf, pos + 56, 4);    put_i32(buf, pos + 60, 0);
    put_i32(buf, pos + 64, 8);    put_i32(buf, pos + 68, 0);
    put_i32(buf, pos + 72, 0x3C); put_i32(buf, pos + 76, 0);
    put_i32(buf, pos + 80, 0x30); put_i32(buf, pos + 84, 0);
    pos += 88;

    put_i32(buf, pos +  0, STRINGS_OFF);
    put_i32(buf, pos +  4, 0);
    put_i32(buf, pos +  8, STRINGS_OFF);
    put_i32(buf, pos + 12, 0);
    put_i32(buf, pos + 16, DATA_SIZE);
    put_i32(buf, pos + 20, 0);
    put_i32(buf, pos + 24, DATA_SIZE);
    put_i32(buf, pos + 28, 0);
    pos += 32;

    TEST_ASSERT_EQUAL_size_t(DATA_START, pos);

    put_i32(buf, pos +  0, 0);
    put_i32(buf, pos +  4, 0);
    put_i32(buf, pos +  8, 0);
    put_i32(buf, pos + 12, 0);
    put_i32(buf, pos + 16, 0);
    put_i32(buf, pos + 20, 0);
    put_i32(buf, pos + 24, 0);
    put_i32(buf, pos + 28, 0);
    put_i32(buf, pos + 32, 0);
    put_i32(buf, pos + 36, 0);
    put_i32(buf, pos + 40, 0);
    put_i32(buf, pos + 44, 0x34);
    put_i32(buf, pos + 48, 0);

    sf_edd_t *e = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_edd_read_from_memory(&e, buf, sizeof(buf), NULL));
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_FALSE(sf_edd_long_format(e));
    TEST_ASSERT_EQUAL_size_t(0, sf_edd_function_spec_count(e));
    TEST_ASSERT_EQUAL_size_t(0, sf_edd_command_spec_count(e));
    sf_edd_destroy(e);
}

static void test_edd_read_bad_magic(void) {
    uint8_t buf[16] = { 'B', 'A', 'D', '!', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    sf_edd_t *e = NULL;
    TEST_ASSERT_NOT_EQUAL(SF_OK, sf_edd_read_from_memory(&e, buf, sizeof(buf), NULL));
    TEST_ASSERT_NULL(e);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_edd_create_destroy);
    RUN_TEST(test_edd_create_long_format);
    RUN_TEST(test_edd_read_minimal_32bit);
    RUN_TEST(test_edd_read_bad_magic);
    return UNITY_END();
}
