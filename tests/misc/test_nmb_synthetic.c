/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_nmb.h"
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

static void put_bundle_header(uint8_t *p, sf_nmb_bundle_type_t type, uint32_t data_size) {
    put_u32_le(p + 0, 0x18u);
    put_u32_le(p + 4, 0xAu);
    put_u32_le(p + 8, (uint32_t)type);
    put_u32_le(p + 12, 0x12345678u);
    put_u32_le(p + 16, 0u);
    put_u32_le(p + 20, 0u);
    put_u32_le(p + 24, 0u);
    put_u32_le(p + 28, 0u);
    put_u32_le(p + 32, data_size);
    put_u32_le(p + 36, 4u);
}

static void make_nmb(uint8_t out[87]) {
    memset(out, 0, 87);
    put_bundle_header(out, SF_NMB_BUNDLE_FILE_HEADER, 4u);
    out[40] = 0xDEu;
    out[41] = 0xADu;
    out[42] = 0xBEu;
    out[43] = 0xEFu;

    put_bundle_header(out + 44, SF_NMB_BUNDLE_NETWORK, 3u);
    out[84] = 0xAAu;
    out[85] = 0xBBu;
    out[86] = 0xCCu;
}

static void test_nmb_create_destroy_empty(void) {
    sf_nmb_t *nmb = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nmb_create(&nmb, NULL));
    TEST_ASSERT_NOT_NULL(nmb);
    TEST_ASSERT_EQUAL_size_t(0, sf_nmb_bundle_count(nmb));

    void *out = (void *)0x1;
    size_t out_size = 99;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nmb_write_to_memory(nmb, &out, &out_size, NULL));
    TEST_ASSERT_NULL(out);
    TEST_ASSERT_EQUAL_size_t(0, out_size);

    sf_nmb_destroy(nmb);
}

static void test_nmb_is_function(void) {
    uint8_t bytes[87];
    make_nmb(bytes);
    uint8_t bad0[8] = { 0 };
    uint8_t bad1[8] = { 0x18, 0, 0, 0, 0x09, 0, 0, 0 };

    TEST_ASSERT_TRUE(sf_nmb_is(bytes, sizeof(bytes)));
    TEST_ASSERT_FALSE(sf_nmb_is(bad0, sizeof(bad0)));
    TEST_ASSERT_FALSE(sf_nmb_is(bad1, sizeof(bad1)));
    TEST_ASSERT_FALSE(sf_nmb_is(NULL, 0));
}

static void test_nmb_roundtrip_and_bundle_accessors(void) {
    uint8_t bytes[87];
    make_nmb(bytes);

    sf_nmb_t *nmb = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nmb_read_from_memory(&nmb, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(nmb);
    TEST_ASSERT_EQUAL_size_t(2, sf_nmb_bundle_count(nmb));

    sf_nmb_bundle_t *first = sf_nmb_bundle_at(nmb, 0);
    sf_nmb_bundle_t *second = sf_nmb_bundle_at(nmb, 1);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_UINT32(SF_NMB_BUNDLE_FILE_HEADER, sf_nmb_bundle_type(first));
    TEST_ASSERT_EQUAL_UINT32(SF_NMB_BUNDLE_NETWORK, sf_nmb_bundle_type(second));

    size_t data_size = 0;
    const uint8_t *data = (const uint8_t *)sf_nmb_bundle_data(first, &data_size);
    TEST_ASSERT_EQUAL_size_t(4, data_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes + 40, data, 4);
    data = (const uint8_t *)sf_nmb_bundle_data(second, &data_size);
    TEST_ASSERT_EQUAL_size_t(3, data_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes + 84, data, 3);
    TEST_ASSERT_NULL(sf_nmb_bundle_at(nmb, 2));

    void *out = NULL;
    size_t out_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nmb_write_to_memory(nmb, &out, &out_size, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof(bytes), out_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, out, sizeof(bytes));

    sf_free(NULL, out);
    sf_nmb_destroy(nmb);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nmb_create_destroy_empty);
    RUN_TEST(test_nmb_is_function);
    RUN_TEST(test_nmb_roundtrip_and_bundle_accessors);
    return UNITY_END();
}
