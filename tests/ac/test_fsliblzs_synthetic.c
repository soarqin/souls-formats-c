/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_fsliblzs.h"
#include "souls_formats/sf_common.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_fsliblzs_is_false_on_short_buffer(void) {
    uint8_t buf[4] = {0};
    TEST_ASSERT_FALSE(sf_fsliblzs_is(buf, sizeof(buf)));
}

static void test_fsliblzs_is_false_on_wrong_magic(void) {
    uint8_t buf[8] = {'f','s','l','i','b','l','z','X'};
    TEST_ASSERT_FALSE(sf_fsliblzs_is(buf, sizeof(buf)));
}

static void test_fsliblzs_is_true_on_correct_magic(void) {
    uint8_t buf[44];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "fsliblzs", 8);
    TEST_ASSERT_TRUE(sf_fsliblzs_is(buf, sizeof(buf)));
}

static void test_fsliblzs_read_header(void) {
    uint8_t buf[44];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "fsliblzs", 8);

    int32_t compressed_size = 1234;
    memcpy(buf + 16, &compressed_size, 4);

    int32_t decompressed_size_be = 0x00004567;
    buf[36] = (uint8_t)(decompressed_size_be >> 24);
    buf[37] = (uint8_t)(decompressed_size_be >> 16);
    buf[38] = (uint8_t)(decompressed_size_be >> 8);
    buf[39] = (uint8_t)(decompressed_size_be);

    int32_t out_compressed = 0, out_decompressed = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fsliblzs_read_header(buf, sizeof(buf),
                                                         &out_compressed, &out_decompressed));
    TEST_ASSERT_EQUAL_INT32(1234, out_compressed);
    TEST_ASSERT_EQUAL_INT32(0x4567, out_decompressed);
}

static void test_fsliblzs_decompress_returns_unsupported(void) {
    uint8_t buf[44];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "fsliblzs", 8);
    void *out = NULL;
    size_t out_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_ERR_UNSUPPORTED_VERSION,
                          sf_fsliblzs_decompress(buf, sizeof(buf), &out, &out_size, NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fsliblzs_is_false_on_short_buffer);
    RUN_TEST(test_fsliblzs_is_false_on_wrong_magic);
    RUN_TEST(test_fsliblzs_is_true_on_correct_magic);
    RUN_TEST(test_fsliblzs_read_header);
    RUN_TEST(test_fsliblzs_decompress_returns_unsupported);
    return UNITY_END();
}
