/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_fxr1.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void put_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_u32_be(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void make_little_narrow(uint8_t out[56]) {
    memset(out, 0, 56);
    memcpy(out, "FXR\0", 4);
    put_u32_le(out + 4, 0x10000u);
    put_u32_le(out + 8, 32u);
    put_u32_le(out + 12, 48u);
    put_u32_le(out + 16, 0u);
    put_u32_le(out + 20, 0u);
    put_u32_le(out + 24, 0x11223344u);
    put_u32_le(out + 28, 0x55667788u);
    for (uint8_t i = 0; i < 16; i++) out[32 + i] = (uint8_t)(0xA0u + i);
    for (uint8_t i = 0; i < 8; i++) out[48 + i] = (uint8_t)(0xC0u + i);
}

static void make_big_wide(uint8_t out[72]) {
    memset(out, 0, 72);
    memcpy(out, "FXR\0", 4);
    put_u32_le(out + 4, 0x100u);
    put_u32_be(out + 8, 48u);
    put_u32_be(out + 12, 0u);
    put_u32_be(out + 16, 64u);
    put_u32_be(out + 20, 0u);
    put_u32_be(out + 24, 0u);
    put_u32_be(out + 28, 0x01020304u);
    put_u32_be(out + 32, 0x05060708u);
    for (uint8_t i = 0; i < 16; i++) out[48 + i] = (uint8_t)(0x10u + i);
    for (uint8_t i = 0; i < 8; i++) out[64 + i] = (uint8_t)(0x30u + i);
}

static void test_fxr1_create_destroy_and_accessors(void) {
    sf_fxr1_t *fxr = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fxr1_create(&fxr, NULL));
    TEST_ASSERT_NOT_NULL(fxr);

    TEST_ASSERT_FALSE(sf_fxr1_big_endian(fxr));
    TEST_ASSERT_FALSE(sf_fxr1_wide(fxr));
    TEST_ASSERT_EQUAL_INT32(0, sf_fxr1_unk1(fxr));
    TEST_ASSERT_EQUAL_INT32(0, sf_fxr1_unk2(fxr));

    sf_fxr1_set_big_endian(fxr, true);
    sf_fxr1_set_wide(fxr, true);
    sf_fxr1_set_unk1(fxr, 1234);
    sf_fxr1_set_unk2(fxr, -5678);

    TEST_ASSERT_TRUE(sf_fxr1_big_endian(fxr));
    TEST_ASSERT_TRUE(sf_fxr1_wide(fxr));
    TEST_ASSERT_EQUAL_INT32(1234, sf_fxr1_unk1(fxr));
    TEST_ASSERT_EQUAL_INT32(-5678, sf_fxr1_unk2(fxr));

    sf_fxr1_destroy(fxr);
}

static void test_fxr1_is_function(void) {
    const uint8_t valid[] = { 'F', 'X', 'R', 0, 0, 0, 1, 0 };
    const uint8_t big[] = { 'F', 'X', 'R', 0, 0, 1, 0, 0 };
    const uint8_t bad_magic[] = { 'F', 'X', 'R', '1', 0, 0, 1, 0 };
    const uint8_t bad_check[] = { 'F', 'X', 'R', 0, 1, 2, 3, 4 };

    TEST_ASSERT_TRUE(sf_fxr1_is(valid, sizeof(valid)));
    TEST_ASSERT_TRUE(sf_fxr1_is(big, sizeof(big)));
    TEST_ASSERT_FALSE(sf_fxr1_is(bad_magic, sizeof(bad_magic)));
    TEST_ASSERT_FALSE(sf_fxr1_is(bad_check, sizeof(bad_check)));
    TEST_ASSERT_FALSE(sf_fxr1_is(valid, 7));
    TEST_ASSERT_FALSE(sf_fxr1_is(NULL, 0));
}

static void test_fxr1_roundtrip_little_narrow_blob(void) {
    uint8_t bytes[56];
    make_little_narrow(bytes);

    sf_fxr1_t *fxr = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fxr1_read_from_memory(bytes, sizeof(bytes), &fxr, NULL));
    TEST_ASSERT_NOT_NULL(fxr);
    TEST_ASSERT_FALSE(sf_fxr1_big_endian(fxr));
    TEST_ASSERT_FALSE(sf_fxr1_wide(fxr));
    TEST_ASSERT_EQUAL_INT32(0x11223344, sf_fxr1_unk1(fxr));
    TEST_ASSERT_EQUAL_INT32(0x55667788, sf_fxr1_unk2(fxr));

    void *out = NULL;
    size_t out_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fxr1_write_to_memory(fxr, &out, &out_size, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof(bytes), out_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, out, sizeof(bytes));

    sf_free(NULL, out);
    sf_fxr1_destroy(fxr);
}

static void test_fxr1_roundtrip_big_wide_blob(void) {
    uint8_t bytes[72];
    make_big_wide(bytes);

    sf_fxr1_t *fxr = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fxr1_read_from_memory(bytes, sizeof(bytes), &fxr, NULL));
    TEST_ASSERT_NOT_NULL(fxr);
    TEST_ASSERT_TRUE(sf_fxr1_big_endian(fxr));
    TEST_ASSERT_TRUE(sf_fxr1_wide(fxr));
    TEST_ASSERT_EQUAL_INT32(0x01020304, sf_fxr1_unk1(fxr));
    TEST_ASSERT_EQUAL_INT32(0x05060708, sf_fxr1_unk2(fxr));

    void *out = NULL;
    size_t out_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fxr1_write_to_memory(fxr, &out, &out_size, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof(bytes), out_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, out, sizeof(bytes));

    sf_free(NULL, out);
    sf_fxr1_destroy(fxr);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fxr1_create_destroy_and_accessors);
    RUN_TEST(test_fxr1_is_function);
    RUN_TEST(test_fxr1_roundtrip_little_narrow_blob);
    RUN_TEST(test_fxr1_roundtrip_big_wide_blob);
    return UNITY_END();
}
