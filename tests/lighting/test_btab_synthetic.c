/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_btab.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void put_u8(uint8_t *p, uint8_t v) { p[0] = v; }

static void put_i32_le(uint8_t *p, int32_t v) {
    uint32_t u;
    memcpy(&u, &v, 4);
    p[0] = (uint8_t)(u);
    p[1] = (uint8_t)(u >> 8);
    p[2] = (uint8_t)(u >> 16);
    p[3] = (uint8_t)(u >> 24);
}

static void put_f32_le(uint8_t *p, float v) {
    uint32_t u;
    memcpy(&u, &v, 4);
    p[0] = (uint8_t)(u);
    p[1] = (uint8_t)(u >> 8);
    p[2] = (uint8_t)(u >> 16);
    p[3] = (uint8_t)(u >> 24);
}

/*
 * Build a minimal BTAB binary blob for testing.
 *
 * Header layout (0x3C bytes):
 *   0x00: int32 = 1
 *   0x04: int32 = 0
 *   0x08: int32 = entryCount
 *   0x0C: int32 = stringsLength
 *   0x10: uint8 = BigEndian (0)
 *   0x11: uint8 = 0
 *   0x12: uint8 = 0
 *   0x13: uint8 = 0
 *   0x14: int32 = entrySize (0x1C or 0x28)
 *   0x18: 0x24 bytes of zeros (AssertPattern)
 *
 * Strings section starts at 0x3C.
 * Each string is UTF-16LE null-terminated, padded to 8-byte alignment
 * relative to strings_start.
 *
 * "AB" in UTF-16LE = 41 00 42 00 00 00 (6 bytes) + 2 pad = 8 bytes
 * "CD" in UTF-16LE = 43 00 44 00 00 00 (6 bytes) + 2 pad = 8 bytes
 *
 * Short format (0x1C) entry:
 *   int32 part_off + int32 mat_off + int32 atlas_id + float[2] uv_offset + float[2] uv_scale
 *   = 4 + 4 + 4 + 8 + 8 = 28 = 0x1C
 *
 * Long format (0x28) entry:
 *   int64 part_off + int64 mat_off + int32 atlas_id + float[2] uv_offset + float[2] uv_scale + int32(0)
 *   = 8 + 8 + 4 + 8 + 8 + 4 = 40 = 0x28
 */

#define BTAB_HEADER_SIZE 0x3Cu
#define BTAB_STR_ENTRY_SIZE 8u  /* each string slot: 6 bytes UTF-16LE + 2 pad */

static void write_utf16le_str2(uint8_t *p, char c0, char c1) {
    p[0] = (uint8_t)c0; p[1] = 0;
    p[2] = (uint8_t)c1; p[3] = 0;
    p[4] = 0; p[5] = 0;  /* null terminator */
    p[6] = 0; p[7] = 0;  /* padding to 8 bytes */
}

/*
 * DS2-style: BigEndian=false, LongFormat=false (entrySize=0x1C), 2 entries.
 *
 * Strings section (4 strings × 8 bytes = 32 bytes):
 *   offset 0:  "AB" (part_name entry 0)
 *   offset 8:  "CD" (mat_name entry 0)
 *   offset 16: "EF" (part_name entry 1)
 *   offset 24: "GH" (mat_name entry 1)
 *
 * Entries section (2 × 0x1C = 56 bytes):
 *   entry 0: part_off=0, mat_off=8, atlas_id=42, uv_offset=(0.1,0.2), uv_scale=(1.0,2.0)
 *   entry 1: part_off=16, mat_off=24, atlas_id=99, uv_offset=(0.5,0.5), uv_scale=(0.5,0.5)
 *
 * Total = 0x3C + 32 + 56 = 60 + 32 + 56 = 148 bytes
 */
#define BTAB_DS2_SIZE (BTAB_HEADER_SIZE + 4u * BTAB_STR_ENTRY_SIZE + 2u * 0x1Cu)

static void make_btab_ds2(uint8_t out[BTAB_DS2_SIZE]) {
    memset(out, 0, BTAB_DS2_SIZE);

    put_i32_le(out + 0x00, 1);
    put_i32_le(out + 0x04, 0);
    put_i32_le(out + 0x08, 2);                    /* entryCount */
    put_i32_le(out + 0x0C, 32);                   /* stringsLength = 4 * 8 */
    put_u8(out + 0x10, 0);                        /* BigEndian = false */
    put_u8(out + 0x11, 0);
    put_u8(out + 0x12, 0);
    put_u8(out + 0x13, 0);
    put_i32_le(out + 0x14, 0x1C);                 /* entrySize = short */
    /* 0x18..0x3B: zeros (AssertPattern) */

    uint8_t *str = out + BTAB_HEADER_SIZE;
    write_utf16le_str2(str + 0,  'A', 'B');       /* part_name[0] at offset 0 */
    write_utf16le_str2(str + 8,  'C', 'D');       /* mat_name[0]  at offset 8 */
    write_utf16le_str2(str + 16, 'E', 'F');       /* part_name[1] at offset 16 */
    write_utf16le_str2(str + 24, 'G', 'H');       /* mat_name[1]  at offset 24 */

    uint8_t *e0 = out + BTAB_HEADER_SIZE + 32;
    put_i32_le(e0 + 0,  0);                       /* part_off */
    put_i32_le(e0 + 4,  8);                       /* mat_off */
    put_i32_le(e0 + 8,  42);                      /* atlas_id */
    put_f32_le(e0 + 12, 0.1f);                    /* uv_offset.x */
    put_f32_le(e0 + 16, 0.2f);                    /* uv_offset.y */
    put_f32_le(e0 + 20, 1.0f);                    /* uv_scale.x */
    put_f32_le(e0 + 24, 2.0f);                    /* uv_scale.y */

    uint8_t *e1 = e0 + 0x1C;
    put_i32_le(e1 + 0,  16);                      /* part_off */
    put_i32_le(e1 + 4,  24);                      /* mat_off */
    put_i32_le(e1 + 8,  99);                      /* atlas_id */
    put_f32_le(e1 + 12, 0.5f);                    /* uv_offset.x */
    put_f32_le(e1 + 16, 0.5f);                    /* uv_offset.y */
    put_f32_le(e1 + 20, 0.5f);                    /* uv_scale.x */
    put_f32_le(e1 + 24, 0.5f);                    /* uv_scale.y */
}

/*
 * DS3-style: BigEndian=false, LongFormat=true (entrySize=0x28), 2 entries.
 *
 * Strings section (4 strings × 8 bytes = 32 bytes):
 *   offset 0:  "PQ" (part_name entry 0)
 *   offset 8:  "RS" (mat_name entry 0)
 *   offset 16: "TU" (part_name entry 1)
 *   offset 24: "VW" (mat_name entry 1)
 *
 * Entries section (2 × 0x28 = 80 bytes):
 *   entry 0: part_off=0(i64), mat_off=8(i64), atlas_id=7, uv_offset=(0.0,0.0), uv_scale=(1.0,1.0), int32(0)
 *   entry 1: part_off=16(i64), mat_off=24(i64), atlas_id=3, uv_offset=(0.25,0.75), uv_scale=(2.0,3.0), int32(0)
 *
 * Total = 0x3C + 32 + 80 = 60 + 32 + 80 = 172 bytes
 */
#define BTAB_DS3_SIZE (BTAB_HEADER_SIZE + 4u * BTAB_STR_ENTRY_SIZE + 2u * 0x28u)

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

static void make_btab_ds3(uint8_t out[BTAB_DS3_SIZE]) {
    memset(out, 0, BTAB_DS3_SIZE);

    put_i32_le(out + 0x00, 1);
    put_i32_le(out + 0x04, 0);
    put_i32_le(out + 0x08, 2);                    /* entryCount */
    put_i32_le(out + 0x0C, 32);                   /* stringsLength */
    put_u8(out + 0x10, 0);                        /* BigEndian = false */
    put_u8(out + 0x11, 0);
    put_u8(out + 0x12, 0);
    put_u8(out + 0x13, 0);
    put_i32_le(out + 0x14, 0x28);                 /* entrySize = long */
    /* 0x18..0x3B: zeros */

    uint8_t *str = out + BTAB_HEADER_SIZE;
    write_utf16le_str2(str + 0,  'P', 'Q');
    write_utf16le_str2(str + 8,  'R', 'S');
    write_utf16le_str2(str + 16, 'T', 'U');
    write_utf16le_str2(str + 24, 'V', 'W');

    uint8_t *e0 = out + BTAB_HEADER_SIZE + 32;
    put_i64_le(e0 + 0,  0);                       /* part_off (i64) */
    put_i64_le(e0 + 8,  8);                       /* mat_off (i64) */
    put_i32_le(e0 + 16, 7);                       /* atlas_id */
    put_f32_le(e0 + 20, 0.0f);                    /* uv_offset.x */
    put_f32_le(e0 + 24, 0.0f);                    /* uv_offset.y */
    put_f32_le(e0 + 28, 1.0f);                    /* uv_scale.x */
    put_f32_le(e0 + 32, 1.0f);                    /* uv_scale.y */
    put_i32_le(e0 + 36, 0);                       /* LongFormat padding */

    uint8_t *e1 = e0 + 0x28;
    put_i64_le(e1 + 0,  16);                      /* part_off (i64) */
    put_i64_le(e1 + 8,  24);                      /* mat_off (i64) */
    put_i32_le(e1 + 16, 3);                       /* atlas_id */
    put_f32_le(e1 + 20, 0.25f);                   /* uv_offset.x */
    put_f32_le(e1 + 24, 0.75f);                   /* uv_offset.y */
    put_f32_le(e1 + 28, 2.0f);                    /* uv_scale.x */
    put_f32_le(e1 + 32, 3.0f);                    /* uv_scale.y */
    put_i32_le(e1 + 36, 0);                       /* LongFormat padding */
}

static void test_btab_ds2_roundtrip(void) {
    uint8_t bytes[BTAB_DS2_SIZE];
    make_btab_ds2(bytes);

    sf_btab_t *btab = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btab_read_from_memory(&btab, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(btab);
    TEST_ASSERT_FALSE(sf_btab_is_big_endian(btab));
    TEST_ASSERT_FALSE(sf_btab_is_long_format(btab));
    TEST_ASSERT_EQUAL_size_t(2, sf_btab_entry_count(btab));

    const sf_btab_entry_t *e0 = sf_btab_get_entry(btab, 0);
    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_EQUAL_STRING("AB", sf_btab_entry_part_name(e0));
    TEST_ASSERT_EQUAL_STRING("CD", sf_btab_entry_material_name(e0));
    TEST_ASSERT_EQUAL_INT32(42, sf_btab_entry_atlas_id(e0));
    TEST_ASSERT_EQUAL_FLOAT(0.1f, sf_btab_entry_uv_offset(e0).x);
    TEST_ASSERT_EQUAL_FLOAT(0.2f, sf_btab_entry_uv_offset(e0).y);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sf_btab_entry_uv_scale(e0).x);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, sf_btab_entry_uv_scale(e0).y);

    const sf_btab_entry_t *e1 = sf_btab_get_entry(btab, 1);
    TEST_ASSERT_NOT_NULL(e1);
    TEST_ASSERT_EQUAL_STRING("EF", sf_btab_entry_part_name(e1));
    TEST_ASSERT_EQUAL_STRING("GH", sf_btab_entry_material_name(e1));
    TEST_ASSERT_EQUAL_INT32(99, sf_btab_entry_atlas_id(e1));

    void *buf1 = NULL;
    size_t sz1 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btab_write_to_buffer(btab, &buf1, &sz1, NULL));
    TEST_ASSERT_NOT_NULL(buf1);
    TEST_ASSERT_EQUAL_size_t(sizeof(bytes), sz1);

    sf_btab_t *btab2 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btab_read_from_memory(&btab2, buf1, sz1, NULL));
    TEST_ASSERT_NOT_NULL(btab2);

    void *buf2 = NULL;
    size_t sz2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btab_write_to_buffer(btab2, &buf2, &sz2, NULL));
    TEST_ASSERT_NOT_NULL(buf2);
    TEST_ASSERT_EQUAL_size_t(sz1, sz2);
    TEST_ASSERT_EQUAL_MEMORY(buf1, buf2, sz1);

    sf_free(NULL, buf1);
    sf_free(NULL, buf2);
    sf_btab_destroy(btab);
    sf_btab_destroy(btab2);
}

static void test_btab_ds3_roundtrip(void) {
    uint8_t bytes[BTAB_DS3_SIZE];
    make_btab_ds3(bytes);

    sf_btab_t *btab = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btab_read_from_memory(&btab, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(btab);
    TEST_ASSERT_FALSE(sf_btab_is_big_endian(btab));
    TEST_ASSERT_TRUE(sf_btab_is_long_format(btab));
    TEST_ASSERT_EQUAL_size_t(2, sf_btab_entry_count(btab));

    const sf_btab_entry_t *e0 = sf_btab_get_entry(btab, 0);
    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_EQUAL_STRING("PQ", sf_btab_entry_part_name(e0));
    TEST_ASSERT_EQUAL_STRING("RS", sf_btab_entry_material_name(e0));
    TEST_ASSERT_EQUAL_INT32(7, sf_btab_entry_atlas_id(e0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sf_btab_entry_uv_offset(e0).x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sf_btab_entry_uv_offset(e0).y);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sf_btab_entry_uv_scale(e0).x);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sf_btab_entry_uv_scale(e0).y);

    const sf_btab_entry_t *e1 = sf_btab_get_entry(btab, 1);
    TEST_ASSERT_NOT_NULL(e1);
    TEST_ASSERT_EQUAL_STRING("TU", sf_btab_entry_part_name(e1));
    TEST_ASSERT_EQUAL_STRING("VW", sf_btab_entry_material_name(e1));
    TEST_ASSERT_EQUAL_INT32(3, sf_btab_entry_atlas_id(e1));
    TEST_ASSERT_EQUAL_FLOAT(0.25f, sf_btab_entry_uv_offset(e1).x);
    TEST_ASSERT_EQUAL_FLOAT(0.75f, sf_btab_entry_uv_offset(e1).y);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, sf_btab_entry_uv_scale(e1).x);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, sf_btab_entry_uv_scale(e1).y);

    void *buf1 = NULL;
    size_t sz1 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btab_write_to_buffer(btab, &buf1, &sz1, NULL));
    TEST_ASSERT_NOT_NULL(buf1);
    TEST_ASSERT_EQUAL_size_t(sizeof(bytes), sz1);

    sf_btab_t *btab2 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btab_read_from_memory(&btab2, buf1, sz1, NULL));
    TEST_ASSERT_NOT_NULL(btab2);

    void *buf2 = NULL;
    size_t sz2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_btab_write_to_buffer(btab2, &buf2, &sz2, NULL));
    TEST_ASSERT_NOT_NULL(buf2);
    TEST_ASSERT_EQUAL_size_t(sz1, sz2);
    TEST_ASSERT_EQUAL_MEMORY(buf1, buf2, sz1);

    sf_free(NULL, buf1);
    sf_free(NULL, buf2);
    sf_btab_destroy(btab);
    sf_btab_destroy(btab2);
}

static void test_btab_bigendian_rejected(void) {
    uint8_t bytes[BTAB_DS2_SIZE];
    make_btab_ds2(bytes);

    put_u8(bytes + 0x10, 1);

    sf_btab_t *btab = NULL;
    sf_result_t r = sf_btab_read_from_memory(&btab, bytes, sizeof(bytes), NULL);
    TEST_ASSERT_EQUAL_INT(SF_ERR_UNSUPPORTED_VERSION, r);
    TEST_ASSERT_NULL(btab);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_btab_ds2_roundtrip);
    RUN_TEST(test_btab_ds3_roundtrip);
    RUN_TEST(test_btab_bigendian_rejected);
    return UNITY_END();
}
