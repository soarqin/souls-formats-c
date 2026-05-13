/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_nsa.h"
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

static void make_nsa(uint8_t out[0xC0]) {
    memset(out, 0, 0xC0);
    put_u32_le(out + 0x08, 0xA0u); /* pDynamicSegment */
    put_u32_le(out + 0x0C, 0x10u); /* alignment */
    put_u32_le(out + 0x10, 0xC0u); /* size */
    put_u32_le(out + 0x3C, 9u);    /* animBoneCount */
    put_u32_le(out + 0x78, 0x90u); /* pStaticSegment */

    put_u32_le(out + 0x90, 2u); /* StaticSegment.translationBoneCount */
    put_u32_le(out + 0x94, 1u); /* StaticSegment.rotationBoneCount */

    put_u32_le(out + 0xA0, 5u); /* DynamicSegment.sampleCount */
    put_u32_le(out + 0xA4, 3u); /* DynamicSegment.translationBoneCount */
    put_u32_le(out + 0xA8, 4u); /* DynamicSegment.rotationBoneCount */
}

static void test_nsa_create_destroy_empty(void) {
    sf_nsa_t *nsa = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nsa_create(&nsa, NULL));
    TEST_ASSERT_NOT_NULL(nsa);
    TEST_ASSERT_EQUAL_UINT32(0, sf_nsa_frame_count(nsa));
    TEST_ASSERT_EQUAL_UINT32(0, sf_nsa_static_translation_count(nsa));
    TEST_ASSERT_EQUAL_UINT32(0, sf_nsa_static_rotation_count(nsa));
    TEST_ASSERT_EQUAL_UINT32(0, sf_nsa_dynamic_translation_count(nsa));
    TEST_ASSERT_EQUAL_UINT32(0, sf_nsa_dynamic_rotation_count(nsa));

    void *out = (void *)0x1;
    size_t out_size = 99;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nsa_write_to_memory(nsa, &out, &out_size, NULL));
    TEST_ASSERT_NULL(out);
    TEST_ASSERT_EQUAL_size_t(0, out_size);

    sf_nsa_destroy(nsa);
}

static void test_nsa_is_function(void) {
    uint8_t bytes[0xC0];
    make_nsa(bytes);
    uint8_t bad[0x88] = { 0 };

    TEST_ASSERT_TRUE(sf_nsa_is(bytes, sizeof(bytes)));
    TEST_ASSERT_FALSE(sf_nsa_is(bad, sizeof(bad)));
    TEST_ASSERT_FALSE(sf_nsa_is(bytes, 0x40));
    TEST_ASSERT_FALSE(sf_nsa_is(NULL, 0));
}

static void test_nsa_roundtrip_and_counts(void) {
    uint8_t bytes[0xC0];
    make_nsa(bytes);

    sf_nsa_t *nsa = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nsa_read_from_memory(&nsa, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(nsa);
    TEST_ASSERT_EQUAL_UINT32(5, sf_nsa_frame_count(nsa));
    TEST_ASSERT_EQUAL_UINT32(2, sf_nsa_static_translation_count(nsa));
    TEST_ASSERT_EQUAL_UINT32(1, sf_nsa_static_rotation_count(nsa));
    TEST_ASSERT_EQUAL_UINT32(3, sf_nsa_dynamic_translation_count(nsa));
    TEST_ASSERT_EQUAL_UINT32(4, sf_nsa_dynamic_rotation_count(nsa));

    void *out = NULL;
    size_t out_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nsa_write_to_memory(nsa, &out, &out_size, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof(bytes), out_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, out, sizeof(bytes));

    sf_free(NULL, out);
    sf_nsa_destroy(nsa);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nsa_create_destroy_empty);
    RUN_TEST(test_nsa_is_function);
    RUN_TEST(test_nsa_roundtrip_and_counts);
    return UNITY_END();
}
