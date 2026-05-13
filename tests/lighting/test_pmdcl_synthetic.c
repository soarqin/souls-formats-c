/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_pmdcl.h"
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

static void put_i64_le(uint8_t *p, int64_t v) {
    uint64_t u;
    memcpy(&u, &v, 8);
    p[0] = (uint8_t)(u);
    p[1] = (uint8_t)(u >> 8);
    p[2] = (uint8_t)(u >> 16);
    p[3] = (uint8_t)(u >> 24);
    p[4] = (uint8_t)(u >> 32);
    p[5] = (uint8_t)(u >> 40);
    p[6] = (uint8_t)(u >> 48);
    p[7] = (uint8_t)(u >> 56);
}

static void put_i16_le(uint8_t *p, int16_t v) {
    uint16_t u;
    memcpy(&u, &v, 2);
    p[0] = (uint8_t)(u);
    p[1] = (uint8_t)(u >> 8);
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
 * Build a 3-decal PMDCL binary blob.
 *
 * Layout:
 *   0x00: header (32 bytes)
 *   0x20: offset table (3 × 8 = 24 bytes)
 *   0x38: pad to 0x40 (8 bytes of zeros)
 *   0x40: decal 0 (96 bytes)
 *   0xA0: decal 1 (96 bytes)
 *   0x100: decal 2 (96 bytes)
 *   total: 0x160 = 352 bytes
 */
#define PMDCL_3DECAL_SIZE 0x160u

static void make_pmdcl_3decal(uint8_t out[PMDCL_3DECAL_SIZE]) {
    memset(out, 0, PMDCL_3DECAL_SIZE);

    /* Header */
    put_i64_le(out + 0x00, 3);      /* count */
    put_i64_le(out + 0x08, 0x20);   /* header size */
    put_i64_le(out + 0x10, 0);      /* padding */
    put_i64_le(out + 0x18, 0);      /* padding */

    /* Offset table */
    put_i64_le(out + 0x20, 0x40);   /* decal 0 offset */
    put_i64_le(out + 0x28, 0xA0);   /* decal 1 offset */
    put_i64_le(out + 0x30, 0x100);  /* decal 2 offset */
    /* 0x38..0x3F: pad zeros (already zeroed) */

    /* Decal 0 at 0x40 */
    {
        uint8_t *d = out + 0x40;
        put_vec3_le(d + 0x00, 1.0f, 2.0f, 3.0f);   /* x_angles */
        put_i32_le(d + 0x0C, 0);
        put_vec3_le(d + 0x10, 4.0f, 5.0f, 6.0f);   /* y_angles */
        put_i32_le(d + 0x1C, 0);
        put_vec3_le(d + 0x20, 7.0f, 8.0f, 9.0f);   /* z_angles */
        put_i32_le(d + 0x2C, 0);
        put_vec3_le(d + 0x30, 10.0f, 20.0f, 30.0f); /* position */
        put_f32_le(d + 0x3C, 1.0f);                 /* unk3c */
        put_i32_le(d + 0x40, 100);                  /* decal_param_id */
        put_i16_le(d + 0x44, 10);                   /* size1 */
        put_i16_le(d + 0x46, 10);                   /* size2 */
        /* 0x48..0x5F: trailing int64 zeros (already zeroed) */
    }

    /* Decal 1 at 0xA0 */
    {
        uint8_t *d = out + 0xA0;
        put_vec3_le(d + 0x00, -1.0f, -2.0f, -3.0f); /* x_angles */
        put_i32_le(d + 0x0C, 0);
        put_vec3_le(d + 0x10, 0.0f, 0.0f, 0.0f);    /* y_angles */
        put_i32_le(d + 0x1C, 0);
        put_vec3_le(d + 0x20, 0.0f, 0.0f, 0.0f);    /* z_angles */
        put_i32_le(d + 0x2C, 0);
        put_vec3_le(d + 0x30, -5.0f, 0.0f, 5.0f);   /* position */
        put_f32_le(d + 0x3C, 0.0f);                  /* unk3c */
        put_i32_le(d + 0x40, 200);                   /* decal_param_id */
        put_i16_le(d + 0x44, 20);                    /* size1 */
        put_i16_le(d + 0x46, 30);                    /* size2 */
    }

    /* Decal 2 at 0x100 */
    {
        uint8_t *d = out + 0x100;
        put_vec3_le(d + 0x00, 0.5f, 0.5f, 0.5f);    /* x_angles */
        put_i32_le(d + 0x0C, 0);
        put_vec3_le(d + 0x10, 0.5f, 0.5f, 0.5f);    /* y_angles */
        put_i32_le(d + 0x1C, 0);
        put_vec3_le(d + 0x20, 0.5f, 0.5f, 0.5f);    /* z_angles */
        put_i32_le(d + 0x2C, 0);
        put_vec3_le(d + 0x30, 100.0f, 200.0f, 300.0f); /* position */
        put_f32_le(d + 0x3C, 1.0f);                  /* unk3c */
        put_i32_le(d + 0x40, 999);                   /* decal_param_id */
        put_i16_le(d + 0x44, 5);                     /* size1 */
        put_i16_le(d + 0x46, 15);                    /* size2 */
    }
}

/*
 * Build a zero-decal PMDCL binary blob.
 *
 * Layout: just the 32-byte header.
 */
#define PMDCL_0DECAL_SIZE 0x20u

static void make_pmdcl_0decal(uint8_t out[PMDCL_0DECAL_SIZE]) {
    memset(out, 0, PMDCL_0DECAL_SIZE);
    put_i64_le(out + 0x00, 0);      /* count */
    put_i64_le(out + 0x08, 0x20);   /* header size */
    put_i64_le(out + 0x10, 0);      /* padding */
    put_i64_le(out + 0x18, 0);      /* padding */
}

static void test_pmdcl_3decal_roundtrip(void) {
    uint8_t bytes[PMDCL_3DECAL_SIZE];
    make_pmdcl_3decal(bytes);

    /* First read */
    sf_pmdcl_t *pmdcl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_pmdcl_read_from_memory(&pmdcl, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(pmdcl);
    TEST_ASSERT_EQUAL_size_t(3, sf_pmdcl_decal_count(pmdcl));

    /* Verify decal 0 */
    const sf_pmdcl_decal_t *d0 = sf_pmdcl_get_decal(pmdcl, 0);
    TEST_ASSERT_NOT_NULL(d0);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sf_pmdcl_decal_x_angles(d0).x);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, sf_pmdcl_decal_x_angles(d0).y);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, sf_pmdcl_decal_x_angles(d0).z);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, sf_pmdcl_decal_position(d0).x);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, sf_pmdcl_decal_position(d0).y);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, sf_pmdcl_decal_position(d0).z);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sf_pmdcl_decal_unk3c(d0));
    TEST_ASSERT_EQUAL_INT32(100, sf_pmdcl_decal_param_id(d0));
    TEST_ASSERT_EQUAL_INT16(10, sf_pmdcl_decal_size1(d0));
    TEST_ASSERT_EQUAL_INT16(10, sf_pmdcl_decal_size2(d0));

    /* Verify decal 1 */
    const sf_pmdcl_decal_t *d1 = sf_pmdcl_get_decal(pmdcl, 1);
    TEST_ASSERT_NOT_NULL(d1);
    TEST_ASSERT_EQUAL_INT32(200, sf_pmdcl_decal_param_id(d1));
    TEST_ASSERT_EQUAL_INT16(20, sf_pmdcl_decal_size1(d1));
    TEST_ASSERT_EQUAL_INT16(30, sf_pmdcl_decal_size2(d1));

    /* Verify decal 2 */
    const sf_pmdcl_decal_t *d2 = sf_pmdcl_get_decal(pmdcl, 2);
    TEST_ASSERT_NOT_NULL(d2);
    TEST_ASSERT_EQUAL_INT32(999, sf_pmdcl_decal_param_id(d2));

    /* First write */
    void *buf1 = NULL;
    size_t sz1 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_pmdcl_write_to_buffer(pmdcl, &buf1, &sz1, NULL));
    TEST_ASSERT_NOT_NULL(buf1);
    TEST_ASSERT_EQUAL_size_t(sizeof(bytes), sz1);

    /* Read back from first write */
    sf_pmdcl_t *pmdcl2 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_pmdcl_read_from_memory(&pmdcl2, buf1, sz1, NULL));
    TEST_ASSERT_NOT_NULL(pmdcl2);

    /* Second write */
    void *buf2 = NULL;
    size_t sz2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_pmdcl_write_to_buffer(pmdcl2, &buf2, &sz2, NULL));
    TEST_ASSERT_NOT_NULL(buf2);
    TEST_ASSERT_EQUAL_size_t(sz1, sz2);

    /* Byte-compare both writes */
    TEST_ASSERT_EQUAL_MEMORY(buf1, buf2, sz1);

    sf_free(NULL, buf1);
    sf_free(NULL, buf2);
    sf_pmdcl_destroy(pmdcl);
    sf_pmdcl_destroy(pmdcl2);
}

static void test_pmdcl_zero_decal_roundtrip(void) {
    uint8_t bytes[PMDCL_0DECAL_SIZE];
    make_pmdcl_0decal(bytes);

    sf_pmdcl_t *pmdcl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_pmdcl_read_from_memory(&pmdcl, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(pmdcl);
    TEST_ASSERT_EQUAL_size_t(0, sf_pmdcl_decal_count(pmdcl));
    TEST_ASSERT_NULL(sf_pmdcl_get_decal(pmdcl, 0));

    void *buf = NULL;
    size_t sz = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_pmdcl_write_to_buffer(pmdcl, &buf, &sz, NULL));
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_size_t(sizeof(bytes), sz);
    TEST_ASSERT_EQUAL_MEMORY(bytes, buf, sz);

    sf_free(NULL, buf);
    sf_pmdcl_destroy(pmdcl);
}

static void test_pmdcl_bad_header_rejected(void) {
    uint8_t bytes[PMDCL_0DECAL_SIZE];
    make_pmdcl_0decal(bytes);

    /* Corrupt the header-size field (should be 0x20, set to 0x10) */
    put_i64_le(bytes + 0x08, 0x10);

    sf_pmdcl_t *pmdcl = NULL;
    sf_result_t r = sf_pmdcl_read_from_memory(&pmdcl, bytes, sizeof(bytes), NULL);
    TEST_ASSERT_NOT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NULL(pmdcl);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pmdcl_3decal_roundtrip);
    RUN_TEST(test_pmdcl_zero_decal_roundtrip);
    RUN_TEST(test_pmdcl_bad_header_rejected);
    return UNITY_END();
}
