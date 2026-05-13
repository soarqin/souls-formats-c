/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_ccm.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_ccm_create_destroy(void) {
    sf_ccm_t *c = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_create(&c, SF_CCM_VERSION_DARK_SOULS_1, NULL));
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQUAL_INT(SF_CCM_VERSION_DARK_SOULS_1, sf_ccm_version(c));
    TEST_ASSERT_EQUAL_size_t(0, sf_ccm_glyph_count(c));
    sf_ccm_destroy(c);
}

static void test_ccm_invalid_version_rejected(void) {
    sf_ccm_t *c = NULL;
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG, sf_ccm_create(&c, (sf_ccm_version_t)0xdead, NULL));
    TEST_ASSERT_NULL(c);
}

static sf_ccm_glyph_t make_glyph(int32_t code, float u1, float v1, float u2, float v2,
                                  int16_t pre, int16_t w, int16_t adv, int16_t tex) {
    sf_ccm_glyph_t g;
    g.code = code;
    g.uv1_x = u1; g.uv1_y = v1;
    g.uv2_x = u2; g.uv2_y = v2;
    g.pre_space = pre; g.width = w; g.advance = adv; g.tex_index = tex;
    return g;
}

static void test_ccm_round_trip_des(void) {
    sf_ccm_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_create(&a, SF_CCM_VERSION_DEMONS_SOULS, NULL));
    sf_ccm_set_full_width(a, 32);
    sf_ccm_set_tex_width(a, 256);
    sf_ccm_set_tex_height(a, 512);
    sf_ccm_set_unk0e(a, 0x20);
    sf_ccm_set_unk1c(a, 1);
    sf_ccm_set_unk1d(a, 1);
    sf_ccm_set_tex_count(a, 2);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_set_glyph(a, make_glyph(0x41, 0.0f, 0.0f,
                                                                 0.125f, 0.25f, 1, 16, 18, 0)));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_set_glyph(a, make_glyph(0x42, 0.125f, 0.0f,
                                                                 0.25f, 0.25f, 0, 14, 16, 1)));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_set_glyph(a, make_glyph(0x44, 0.25f, 0.25f,
                                                                 0.5f, 0.5f, 2, 20, 22, 1)));
    TEST_ASSERT_EQUAL_size_t(3, sf_ccm_glyph_count(a));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_write_to_memory(a, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_TRUE(size > 0);

    sf_ccm_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_CCM_VERSION_DEMONS_SOULS, sf_ccm_version(b));
    TEST_ASSERT_EQUAL_INT16(32, sf_ccm_full_width(b));
    TEST_ASSERT_EQUAL_INT16(256, sf_ccm_tex_width(b));
    TEST_ASSERT_EQUAL_INT16(512, sf_ccm_tex_height(b));
    TEST_ASSERT_EQUAL_INT16(0x20, sf_ccm_unk0e(b));
    TEST_ASSERT_EQUAL_UINT8(1, sf_ccm_unk1c(b));
    TEST_ASSERT_EQUAL_UINT8(1, sf_ccm_unk1d(b));
    TEST_ASSERT_EQUAL_UINT8(2, sf_ccm_tex_count(b));
    TEST_ASSERT_EQUAL_size_t(3, sf_ccm_glyph_count(b));

    sf_ccm_glyph_t g;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_find_glyph(b, 0x41, &g));
    TEST_ASSERT_EQUAL_INT32(0x41, g.code);
    TEST_ASSERT_EQUAL_FLOAT(0.0f,   g.uv1_x);
    TEST_ASSERT_EQUAL_FLOAT(0.125f, g.uv2_x);
    TEST_ASSERT_EQUAL_INT16(1,  g.pre_space);
    TEST_ASSERT_EQUAL_INT16(16, g.width);
    TEST_ASSERT_EQUAL_INT16(18, g.advance);
    TEST_ASSERT_EQUAL_INT16(0,  g.tex_index);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_find_glyph(b, 0x44, &g));
    TEST_ASSERT_EQUAL_INT16(2,  g.pre_space);
    TEST_ASSERT_EQUAL_INT16(20, g.width);
    TEST_ASSERT_EQUAL_INT16(1,  g.tex_index);
    TEST_ASSERT_EQUAL_INT(SF_ERR_NOT_FOUND, sf_ccm_find_glyph(b, 0x99, &g));

    sf_free(NULL, bytes);
    sf_ccm_destroy(b);
    sf_ccm_destroy(a);
}

static void test_ccm_round_trip_ds1(void) {
    sf_ccm_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_create(&a, SF_CCM_VERSION_DARK_SOULS_1, NULL));
    sf_ccm_set_full_width(a, 24);
    sf_ccm_set_tex_width(a, 1024);
    sf_ccm_set_tex_height(a, 1024);
    sf_ccm_set_unk0e(a, 0);
    sf_ccm_set_unk1c(a, 4);
    sf_ccm_set_unk1d(a, 0);
    sf_ccm_set_tex_count(a, 1);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_set_glyph(a, make_glyph(100, 0.1f, 0.2f, 0.3f, 0.4f,
                                                                 1, 8, 10, 0)));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_set_glyph(a, make_glyph(101, 0.3f, 0.4f, 0.5f, 0.6f,
                                                                 0, 12, 14, 0)));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_write_to_memory(a, &bytes, &size, NULL));

    sf_ccm_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_CCM_VERSION_DARK_SOULS_1, sf_ccm_version(b));
    TEST_ASSERT_EQUAL_size_t(2, sf_ccm_glyph_count(b));

    sf_ccm_glyph_t g;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_find_glyph(b, 100, &g));
    TEST_ASSERT_EQUAL_FLOAT(0.1f, g.uv1_x);
    TEST_ASSERT_EQUAL_FLOAT(0.4f, g.uv2_y);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_find_glyph(b, 101, &g));
    TEST_ASSERT_EQUAL_INT16(12, g.width);

    sf_free(NULL, bytes);
    sf_ccm_destroy(b);
    sf_ccm_destroy(a);
}

static void test_ccm_round_trip_ds2(void) {
    sf_ccm_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_create(&a, SF_CCM_VERSION_DARK_SOULS_2, NULL));
    sf_ccm_set_full_width(a, 32);
    sf_ccm_set_tex_width(a, 256);
    sf_ccm_set_tex_height(a, 128);
    sf_ccm_set_unk1c(a, 4);
    sf_ccm_set_tex_count(a, 1);

    /* UVs chosen so that uv * tex_size yields exact integers, avoiding rounding loss. */
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_set_glyph(a, make_glyph(0x30,
        0.0f, 0.0f, 16.0f / 256.0f, 32.0f / 128.0f, 1, 16, 18, 0)));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_set_glyph(a, make_glyph(0x31,
        16.0f / 256.0f, 0.0f, 32.0f / 256.0f, 32.0f / 128.0f, 0, 14, 16, 0)));
    /* Same UV as code 0x30: tests TexRegion deduplication. */
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_set_glyph(a, make_glyph(0x32,
        0.0f, 0.0f, 16.0f / 256.0f, 32.0f / 128.0f, 2, 8, 10, 0)));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_write_to_memory(a, &bytes, &size, NULL));

    sf_ccm_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_read_from_memory(&b, bytes, size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_CCM_VERSION_DARK_SOULS_2, sf_ccm_version(b));
    TEST_ASSERT_EQUAL_size_t(3, sf_ccm_glyph_count(b));

    sf_ccm_glyph_t g;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_find_glyph(b, 0x30, &g));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g.uv1_x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g.uv1_y);
    TEST_ASSERT_EQUAL_FLOAT(16.0f / 256.0f, g.uv2_x);
    TEST_ASSERT_EQUAL_FLOAT(32.0f / 128.0f, g.uv2_y);
    TEST_ASSERT_EQUAL_INT16(16, g.width);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_find_glyph(b, 0x32, &g));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g.uv1_x);
    TEST_ASSERT_EQUAL_FLOAT(16.0f / 256.0f, g.uv2_x);
    TEST_ASSERT_EQUAL_INT16(2, g.pre_space);

    sf_free(NULL, bytes);
    sf_ccm_destroy(b);
    sf_ccm_destroy(a);
}

static void test_ccm_glyph_upsert(void) {
    sf_ccm_t *c = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_create(&c, SF_CCM_VERSION_DARK_SOULS_1, NULL));
    sf_ccm_set_tex_width(c, 256);
    sf_ccm_set_tex_height(c, 256);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_set_glyph(c, make_glyph(5, 0.1f, 0.1f, 0.2f, 0.2f,
                                                                 0, 8, 10, 0)));
    TEST_ASSERT_EQUAL_size_t(1, sf_ccm_glyph_count(c));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_set_glyph(c, make_glyph(5, 0.3f, 0.3f, 0.4f, 0.4f,
                                                                 1, 16, 18, 1)));
    TEST_ASSERT_EQUAL_size_t(1, sf_ccm_glyph_count(c));

    sf_ccm_glyph_t g;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ccm_find_glyph(c, 5, &g));
    TEST_ASSERT_EQUAL_FLOAT(0.3f, g.uv1_x);
    TEST_ASSERT_EQUAL_INT16(16, g.width);
    TEST_ASSERT_EQUAL_INT16(1, g.tex_index);

    sf_ccm_destroy(c);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ccm_create_destroy);
    RUN_TEST(test_ccm_invalid_version_rejected);
    RUN_TEST(test_ccm_round_trip_des);
    RUN_TEST(test_ccm_round_trip_ds1);
    RUN_TEST(test_ccm_round_trip_ds2);
    RUN_TEST(test_ccm_glyph_upsert);
    return UNITY_END();
}
