/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_btl.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void put_i32_le(uint8_t *p, int32_t v) {
    uint32_t u;
    memcpy(&u, &v, 4);
    p[0] = (uint8_t)(u);
    p[1] = (uint8_t)(u >> 8);
    p[2] = (uint8_t)(u >> 16);
    p[3] = (uint8_t)(u >> 24);
}

static void put_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_i64_le(uint8_t *p, int64_t v) {
    uint64_t u;
    memcpy(&u, &v, 8);
    for (int i = 0; i < 8; i++) p[i] = (uint8_t)(u >> (8 * i));
}

static void put_f32_le(uint8_t *p, float v) {
    uint32_t u;
    memcpy(&u, &v, 4);
    p[0] = (uint8_t)(u);
    p[1] = (uint8_t)(u >> 8);
    p[2] = (uint8_t)(u >> 16);
    p[3] = (uint8_t)(u >> 24);
}

static void put_vec3_le(uint8_t *p, float x, float y, float z) {
    put_f32_le(p + 0, x);
    put_f32_le(p + 4, y);
    put_f32_le(p + 8, z);
}

/*
 * Per upstream BTL.cs Write() lines 79-86, the names section pads relative to
 * the name's START offset within the section, not its end position. Therefore
 * for N two-byte-name + null sequences (6 bytes each), names land at offsets
 * 0, 6, 22, 38, ... and the total names_length = 6, 22, 38, 54 ...
 */
#define BTL_HEADER_SIZE 0x3Cu

#define BTL_V16_LIGHT_SIZE 0xE8u
#define BTL_V6_LIGHT_SIZE  0xC8u

static void put_name2_utf16le(uint8_t *p, char c0, char c1) {
    p[0] = (uint8_t)c0; p[1] = 0;
    p[2] = (uint8_t)c1; p[3] = 0;
    p[4] = 0;           p[5] = 0;
}

/*
 * V16 (Sekiro/ER+) two-light BTL.
 *
 * Layout:
 *   header[0x3C] | names[0x16] | light0[0xE8] | light1[0xE8]   = 0x222 = 546 bytes
 *
 * Names (mirrors upstream's quirky padding):
 *   offset 0:  "L1\0" (6 bytes)        — 0%0x10==0, no pad
 *   offset 6:  "L2\0" (6 bytes)        — 6%0x10!=0, pad 10 bytes
 *   total names_length = 22
 */
#define BTL_V16_NAMES_SIZE 22u
#define BTL_V16_2L_SIZE \
    (BTL_HEADER_SIZE + BTL_V16_NAMES_SIZE + 2u * BTL_V16_LIGHT_SIZE)

static void fill_v16_light_full(uint8_t *L, int64_t name_offset,
                                uint32_t type) {
    /* Unk00: distinguishable byte pattern */
    for (int i = 0; i < 16; i++) L[i] = (uint8_t)(0x11 + i);

    put_i64_le(L + 0x10, name_offset);
    put_u32_le(L + 0x18, type);
    L[0x1C] = 1;                                   /* Unk1C */
    L[0x1D] = 0xAA; L[0x1E] = 0xBB; L[0x1F] = 0xCC; /* DiffuseColor RGB */
    put_f32_le(L + 0x20, 1.5f);                    /* DiffusePower */
    L[0x24] = 0x11; L[0x25] = 0x22; L[0x26] = 0x33; /* SpecularColor RGB */
    L[0x27] = 1;                                   /* CastShadows */
    put_f32_le(L + 0x28, 2.5f);                    /* SpecularPower */
    put_f32_le(L + 0x2C, 30.0f);                   /* ConeAngle */
    put_f32_le(L + 0x30, 1.0f);                    /* Unk30 */
    put_f32_le(L + 0x34, 2.0f);                    /* Unk34 */
    put_vec3_le(L + 0x38, 10.0f, 20.0f, 30.0f);    /* Position */
    put_vec3_le(L + 0x44, 0.1f, 0.2f, 0.3f);       /* Rotation */
    put_i32_le(L + 0x50, 4);                       /* Unk50 */
    put_f32_le(L + 0x54, 5.0f);                    /* Unk54 */
    put_f32_le(L + 0x58, 15.0f);                   /* Radius */
    put_i32_le(L + 0x5C, -1);                      /* Unk5C */
    put_i32_le(L + 0x60, 0);                       /* zero */
    L[0x64] = 0; L[0x65] = 0; L[0x66] = 0; L[0x67] = 1; /* Unk64 */
    put_f32_le(L + 0x68, 6.5f);                    /* Unk68 */
    L[0x6C] = 50; L[0x6D] = 60; L[0x6E] = 70; L[0x6F] = 80; /* ShadowColor RGBA */
    put_f32_le(L + 0x70, 7.0f);                    /* Unk70 */
    put_f32_le(L + 0x74, 0.5f);                    /* FlickerIntervalMin */
    put_f32_le(L + 0x78, 1.5f);                    /* FlickerIntervalMax */
    put_f32_le(L + 0x7C, 1.0f);                    /* FlickerBrightnessMult */
    put_i32_le(L + 0x80, -1);                      /* Unk80 */
    L[0x84] = 0; L[0x85] = 0; L[0x86] = 0; L[0x87] = 0; /* Unk84 */
    put_f32_le(L + 0x88, 8.5f);                    /* Unk88 */
    put_i32_le(L + 0x8C, 0);                       /* zero */
    put_f32_le(L + 0x90, 9.0f);                    /* Unk90 */
    put_i32_le(L + 0x94, 0);                       /* zero */
    put_f32_le(L + 0x98, 1.0f);                    /* Unk98 */
    put_f32_le(L + 0x9C, 1.0f);                    /* NearClip */
    L[0xA0] = 1; L[0xA1] = 0; L[0xA2] = 2; L[0xA3] = 1; /* UnkA0 */
    put_f32_le(L + 0xA4, 1.0f);                    /* Sharpness */
    put_i32_le(L + 0xA8, 0);                       /* zero */
    put_f32_le(L + 0xAC, 0.75f);                   /* UnkAC */
    put_i64_le(L + 0xB0, 0);                       /* AssertVarint(0) long mode */
    put_f32_le(L + 0xB8, 4.0f);                    /* Width */
    put_f32_le(L + 0xBC, 5.0f);                    /* UnkBC */
    L[0xC0] = 0; L[0xC1] = 0; L[0xC2] = 0; L[0xC3] = 0; /* UnkC0 */
    put_f32_le(L + 0xC4, 6.0f);                    /* UnkC4 */
    /* Sekiro+ tail */
    put_f32_le(L + 0xC8, 0.1f);
    put_f32_le(L + 0xCC, 0.2f);
    put_f32_le(L + 0xD0, 0.3f);
    put_f32_le(L + 0xD4, 0.4f);
    put_f32_le(L + 0xD8, 0.5f);
    put_i32_le(L + 0xDC, 42);
    put_f32_le(L + 0xE0, 0.6f);
    put_i32_le(L + 0xE4, 99);
}

static void make_btl_v16_2lights(uint8_t out[BTL_V16_2L_SIZE]) {
    memset(out, 0, BTL_V16_2L_SIZE);

    put_i32_le(out + 0x00, 2);                          /* magic */
    put_i32_le(out + 0x04, 16);                         /* version */
    put_i32_le(out + 0x08, 2);                          /* lightCount */
    put_i32_le(out + 0x0C, (int32_t)BTL_V16_NAMES_SIZE); /* namesLength */
    put_i32_le(out + 0x10, 0);
    put_i32_le(out + 0x14, (int32_t)BTL_V16_LIGHT_SIZE);
    /* 0x18..0x3B: 36-byte zero pattern (already zeroed) */

    /* Names */
    uint8_t *names = out + BTL_HEADER_SIZE;
    put_name2_utf16le(names + 0, 'L', '1');             /* offset 0 */
    put_name2_utf16le(names + 6, 'L', '2');             /* offset 6 */
    /* 12..21: 10 bytes pad (already zeroed) */

    /* Lights */
    uint8_t *L0 = out + BTL_HEADER_SIZE + BTL_V16_NAMES_SIZE;
    uint8_t *L1 = L0 + BTL_V16_LIGHT_SIZE;
    fill_v16_light_full(L0, 0, /* Spot */ 1);
    fill_v16_light_full(L1, 6, /* Point */ 0);

    /* Differentiate L1's diffuse color so we can verify it round-trips. */
    L1[0x1D] = 0x10; L1[0x1E] = 0x20; L1[0x1F] = 0x30;
    put_f32_le(L1 + 0x58, 9.5f); /* override Radius */
}

/*
 * V6 (DS3) single-light BTL.
 *
 *   header[0x3C] | names[0x06] | light[0xC8]   = 0x10A = 266 bytes
 *
 * Names:
 *   offset 0: "X1\0" (6 bytes) — no pad
 *   total = 6 bytes
 */
#define BTL_V6_NAMES_SIZE 6u
#define BTL_V6_1L_SIZE \
    (BTL_HEADER_SIZE + BTL_V6_NAMES_SIZE + BTL_V6_LIGHT_SIZE)

static void make_btl_v6_1light(uint8_t out[BTL_V6_1L_SIZE]) {
    memset(out, 0, BTL_V6_1L_SIZE);

    put_i32_le(out + 0x00, 2);
    put_i32_le(out + 0x04, 6);                          /* version = 6 */
    put_i32_le(out + 0x08, 1);                          /* lightCount */
    put_i32_le(out + 0x0C, (int32_t)BTL_V6_NAMES_SIZE);
    put_i32_le(out + 0x10, 0);
    put_i32_le(out + 0x14, (int32_t)BTL_V6_LIGHT_SIZE);

    uint8_t *names = out + BTL_HEADER_SIZE;
    put_name2_utf16le(names + 0, 'X', '1');

    /* V6 light layout is identical to V16 except no Sekiro tail (32 bytes
     * shorter). Reuse the fill helper but only first 0xC8 bytes are kept. */
    uint8_t scratch[BTL_V16_LIGHT_SIZE];
    memset(scratch, 0, sizeof(scratch));
    fill_v16_light_full(scratch, 0, /* Directional */ 2);
    memcpy(out + BTL_HEADER_SIZE + BTL_V6_NAMES_SIZE, scratch, BTL_V6_LIGHT_SIZE);
}

/*
 * V16 zero-lights edge case.
 *   header[0x3C] only = 60 bytes
 */
#define BTL_V16_0L_SIZE BTL_HEADER_SIZE

static void make_btl_v16_0lights(uint8_t out[BTL_V16_0L_SIZE]) {
    memset(out, 0, BTL_V16_0L_SIZE);
    put_i32_le(out + 0x00, 2);
    put_i32_le(out + 0x04, 16);
    put_i32_le(out + 0x08, 0);
    put_i32_le(out + 0x0C, 0);
    put_i32_le(out + 0x10, 0);
    put_i32_le(out + 0x14, (int32_t)BTL_V16_LIGHT_SIZE);
}

/*
 * V16 three-light fixture exercising all three LightType values.
 *
 * Names: 3 × "Ln\0" with upstream-style padding.
 *   offset 0:  "L1\0" (6 bytes), 0%16==0, no pad
 *   offset 6:  "L2\0" (6 bytes), 6%16==6, pad 10 → 22
 *   offset 22: "L3\0" (6 bytes), 22%16==6, pad 10 → 38
 *   total names_length = 38
 */
#define BTL_V16_NAMES3_SIZE 38u
#define BTL_V16_3L_SIZE \
    (BTL_HEADER_SIZE + BTL_V16_NAMES3_SIZE + 3u * BTL_V16_LIGHT_SIZE)

static void make_btl_v16_3types(uint8_t out[BTL_V16_3L_SIZE]) {
    memset(out, 0, BTL_V16_3L_SIZE);
    put_i32_le(out + 0x00, 2);
    put_i32_le(out + 0x04, 16);
    put_i32_le(out + 0x08, 3);
    put_i32_le(out + 0x0C, (int32_t)BTL_V16_NAMES3_SIZE);
    put_i32_le(out + 0x10, 0);
    put_i32_le(out + 0x14, (int32_t)BTL_V16_LIGHT_SIZE);

    uint8_t *names = out + BTL_HEADER_SIZE;
    put_name2_utf16le(names + 0,  'L', '1');
    put_name2_utf16le(names + 6,  'L', '2');
    put_name2_utf16le(names + 22, 'L', '3');

    uint8_t *L0 = out + BTL_HEADER_SIZE + BTL_V16_NAMES3_SIZE;
    uint8_t *L1 = L0 + BTL_V16_LIGHT_SIZE;
    uint8_t *L2 = L1 + BTL_V16_LIGHT_SIZE;
    fill_v16_light_full(L0,  0, 0); /* Point */
    fill_v16_light_full(L1,  6, 1); /* Spot */
    fill_v16_light_full(L2, 22, 2); /* Directional */
}

/*===========================================================================
 * Tests
 *===========================================================================*/

static void test_btl_v16_2lights_roundtrip(void) {
    uint8_t bytes[BTL_V16_2L_SIZE];
    make_btl_v16_2lights(bytes);

    sf_btl_t *btl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btl_read_from_memory(&btl, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(btl);
    TEST_ASSERT_EQUAL_INT32(16, sf_btl_version(btl));
    TEST_ASSERT_EQUAL_size_t(2, sf_btl_light_count(btl));

    const sf_btl_light_t *L0 = sf_btl_get_light(btl, 0);
    TEST_ASSERT_NOT_NULL(L0);
    TEST_ASSERT_EQUAL_STRING("L1", sf_btl_light_name(L0));
    TEST_ASSERT_EQUAL_INT(SF_BTL_LIGHT_TYPE_SPOT, sf_btl_light_type(L0));
    sf_color_t diff0 = sf_btl_light_diffuse_color(L0);
    TEST_ASSERT_EQUAL_UINT8(255,  diff0.a);
    TEST_ASSERT_EQUAL_UINT8(0xAA, diff0.r);
    TEST_ASSERT_EQUAL_UINT8(0xBB, diff0.g);
    TEST_ASSERT_EQUAL_UINT8(0xCC, diff0.b);
    TEST_ASSERT_EQUAL_FLOAT(1.5f,  sf_btl_light_diffuse_power(L0));
    TEST_ASSERT_EQUAL_FLOAT(10.0f, sf_btl_light_position(L0).x);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, sf_btl_light_position(L0).y);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, sf_btl_light_position(L0).z);
    TEST_ASSERT_EQUAL_FLOAT(15.0f, sf_btl_light_radius(L0));

    const sf_btl_light_t *L1 = sf_btl_get_light(btl, 1);
    TEST_ASSERT_NOT_NULL(L1);
    TEST_ASSERT_EQUAL_STRING("L2", sf_btl_light_name(L1));
    TEST_ASSERT_EQUAL_INT(SF_BTL_LIGHT_TYPE_POINT, sf_btl_light_type(L1));
    TEST_ASSERT_EQUAL_FLOAT(9.5f, sf_btl_light_radius(L1));

    /* First write */
    void  *buf1 = NULL;
    size_t sz1  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btl_write_to_buffer(btl, &buf1, &sz1, NULL));
    TEST_ASSERT_NOT_NULL(buf1);
    TEST_ASSERT_EQUAL_size_t(sizeof(bytes), sz1);
    TEST_ASSERT_EQUAL_MEMORY(bytes, buf1, sz1);

    /* Read back */
    sf_btl_t *btl2 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btl_read_from_memory(&btl2, buf1, sz1, NULL));
    TEST_ASSERT_NOT_NULL(btl2);

    /* Second write */
    void  *buf2 = NULL;
    size_t sz2  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btl_write_to_buffer(btl2, &buf2, &sz2, NULL));
    TEST_ASSERT_NOT_NULL(buf2);
    TEST_ASSERT_EQUAL_size_t(sz1, sz2);
    TEST_ASSERT_EQUAL_MEMORY(buf1, buf2, sz1);

    sf_free(NULL, buf1);
    sf_free(NULL, buf2);
    sf_btl_destroy(btl);
    sf_btl_destroy(btl2);
}

static void test_btl_v6_1light_roundtrip(void) {
    uint8_t bytes[BTL_V6_1L_SIZE];
    make_btl_v6_1light(bytes);

    sf_btl_t *btl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btl_read_from_memory(&btl, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(btl);
    TEST_ASSERT_EQUAL_INT32(6, sf_btl_version(btl));
    TEST_ASSERT_EQUAL_size_t(1, sf_btl_light_count(btl));

    const sf_btl_light_t *L0 = sf_btl_get_light(btl, 0);
    TEST_ASSERT_NOT_NULL(L0);
    TEST_ASSERT_EQUAL_STRING("X1", sf_btl_light_name(L0));
    TEST_ASSERT_EQUAL_INT(SF_BTL_LIGHT_TYPE_DIRECTIONAL, sf_btl_light_type(L0));

    void  *buf1 = NULL;
    size_t sz1  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btl_write_to_buffer(btl, &buf1, &sz1, NULL));
    TEST_ASSERT_NOT_NULL(buf1);
    TEST_ASSERT_EQUAL_size_t(sizeof(bytes), sz1);
    TEST_ASSERT_EQUAL_MEMORY(bytes, buf1, sz1);

    sf_btl_t *btl2 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btl_read_from_memory(&btl2, buf1, sz1, NULL));
    TEST_ASSERT_NOT_NULL(btl2);

    void  *buf2 = NULL;
    size_t sz2  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btl_write_to_buffer(btl2, &buf2, &sz2, NULL));
    TEST_ASSERT_NOT_NULL(buf2);
    TEST_ASSERT_EQUAL_size_t(sz1, sz2);
    TEST_ASSERT_EQUAL_MEMORY(buf1, buf2, sz1);

    sf_free(NULL, buf1);
    sf_free(NULL, buf2);
    sf_btl_destroy(btl);
    sf_btl_destroy(btl2);
}

static void test_btl_zero_lights_roundtrip(void) {
    uint8_t bytes[BTL_V16_0L_SIZE];
    make_btl_v16_0lights(bytes);

    sf_btl_t *btl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btl_read_from_memory(&btl, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(btl);
    TEST_ASSERT_EQUAL_INT32(16, sf_btl_version(btl));
    TEST_ASSERT_EQUAL_size_t(0, sf_btl_light_count(btl));
    TEST_ASSERT_NULL(sf_btl_get_light(btl, 0));

    void  *buf = NULL;
    size_t sz  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btl_write_to_buffer(btl, &buf, &sz, NULL));
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_size_t(sizeof(bytes), sz);
    TEST_ASSERT_EQUAL_MEMORY(bytes, buf, sz);

    sf_free(NULL, buf);
    sf_btl_destroy(btl);
}

static void test_btl_all_three_light_types(void) {
    uint8_t bytes[BTL_V16_3L_SIZE];
    make_btl_v16_3types(bytes);

    sf_btl_t *btl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btl_read_from_memory(&btl, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(btl);
    TEST_ASSERT_EQUAL_size_t(3, sf_btl_light_count(btl));

    TEST_ASSERT_EQUAL_INT(SF_BTL_LIGHT_TYPE_POINT,
                          sf_btl_light_type(sf_btl_get_light(btl, 0)));
    TEST_ASSERT_EQUAL_INT(SF_BTL_LIGHT_TYPE_SPOT,
                          sf_btl_light_type(sf_btl_get_light(btl, 1)));
    TEST_ASSERT_EQUAL_INT(SF_BTL_LIGHT_TYPE_DIRECTIONAL,
                          sf_btl_light_type(sf_btl_get_light(btl, 2)));

    TEST_ASSERT_EQUAL_STRING("L1", sf_btl_light_name(sf_btl_get_light(btl, 0)));
    TEST_ASSERT_EQUAL_STRING("L2", sf_btl_light_name(sf_btl_get_light(btl, 1)));
    TEST_ASSERT_EQUAL_STRING("L3", sf_btl_light_name(sf_btl_get_light(btl, 2)));

    void  *buf1 = NULL;
    size_t sz1  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btl_write_to_buffer(btl, &buf1, &sz1, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof(bytes), sz1);
    TEST_ASSERT_EQUAL_MEMORY(bytes, buf1, sz1);

    sf_free(NULL, buf1);
    sf_btl_destroy(btl);
}

static void test_btl_bad_version_rejected(void) {
    uint8_t bytes[BTL_V16_0L_SIZE];
    make_btl_v16_0lights(bytes);
    put_i32_le(bytes + 0x04, 99);    /* invalid version */

    sf_btl_t *btl = NULL;
    sf_result_t r = sf_btl_read_from_memory(&btl, bytes, sizeof(bytes), NULL);
    TEST_ASSERT_EQUAL_INT(SF_ERR_UNSUPPORTED_VERSION, r);
    TEST_ASSERT_NULL(btl);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_btl_v16_2lights_roundtrip);
    RUN_TEST(test_btl_v6_1light_roundtrip);
    RUN_TEST(test_btl_zero_lights_roundtrip);
    RUN_TEST(test_btl_all_three_light_types);
    RUN_TEST(test_btl_bad_version_rejected);
    return UNITY_END();
}
