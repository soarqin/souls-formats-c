/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 1 QA — sf_binary_reader_t covers BinaryReaderEx semantics.
 */

#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*  Helper: open a memory stream + reader on a static byte array. */
static sf_binary_reader_t *open_reader(const void *data, size_t n,
                                        sf_istream_t **out_stream,
                                        bool big_endian) {
    sf_istream_t *s = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_istream_open_memory(&s, data, n, NULL));
    sf_binary_reader_t *r = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_create(&r, s, big_endian, NULL));
    *out_stream = s;
    return r;
}

#define FREE_READER(r, s) do { sf_binary_reader_destroy(r); sf_istream_close(s); } while (0)

static void put_i16_le(uint8_t *p, int16_t v) {
    uint16_t u = (uint16_t)v;
    p[0] = (uint8_t)(u & 0xFFu);
    p[1] = (uint8_t)(u >> 8);
}

static void put_u16_le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void put_i32_le(uint8_t *p, int32_t v) {
    uint32_t u = (uint32_t)v;
    p[0] = (uint8_t)(u & 0xFFu);
    p[1] = (uint8_t)((u >> 8) & 0xFFu);
    p[2] = (uint8_t)((u >> 16) & 0xFFu);
    p[3] = (uint8_t)(u >> 24);
}

static void put_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)(v >> 24);
}

static void put_i64_le(uint8_t *p, int64_t v) {
    uint64_t u = (uint64_t)v;
    for (size_t i = 0; i < 8; i++) p[i] = (uint8_t)(u >> (i * 8));
}

static void put_u64_le(uint8_t *p, uint64_t v) {
    for (size_t i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (i * 8));
}

static void put_f32_le(uint8_t *p, float v) {
    uint32_t u;
    memcpy(&u, &v, sizeof(u));
    put_u32_le(p, u);
}

static void put_f64_le(uint8_t *p, double v) {
    uint64_t u;
    memcpy(&u, &v, sizeof(u));
    put_u64_le(p, u);
}

/*===========================================================================
 * Primitive types — little-endian
 *===========================================================================*/

static void test_read_primitives_le(void) {
    static const uint8_t buf[] = {
        0x01,                                                /* bool true */
        0xFF,                                                /* i8 = -1 */
        0x80,                                                /* u8 = 128 */
        0x34, 0x12,                                          /* i16 = 0x1234 */
        0xFF, 0xFF,                                          /* u16 = 0xFFFF */
        0x78, 0x56, 0x34, 0x12,                              /* i32 = 0x12345678 */
        0xEF, 0xBE, 0xAD, 0xDE,                              /* u32 = 0xDEADBEEF */
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* i64 = 1 */
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,      /* u64 = max */
        0x00, 0x00, 0x80, 0x3F,                              /* f32 = 1.0 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F,      /* f64 = 1.0 */
    };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);

    bool b; int8_t i8; uint8_t u8;
    int16_t i16; uint16_t u16;
    int32_t i32; uint32_t u32;
    int64_t i64; uint64_t u64;
    float f; double d;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_bool(r, &b));    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_i8 (r, &i8));    TEST_ASSERT_EQUAL_INT8(-1, i8);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u8 (r, &u8));    TEST_ASSERT_EQUAL_UINT8(128, u8);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_i16(r, &i16));   TEST_ASSERT_EQUAL_HEX16(0x1234, i16);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u16(r, &u16));   TEST_ASSERT_EQUAL_UINT16(0xFFFF, u16);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_i32(r, &i32));   TEST_ASSERT_EQUAL_HEX32(0x12345678, i32);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u32(r, &u32));   TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, u32);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_i64(r, &i64));   TEST_ASSERT_EQUAL_INT64(1, i64);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u64(r, &u64));   TEST_ASSERT_EQUAL_HEX64((uint64_t)~0ull, u64);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_f32(r, &f));     TEST_ASSERT_EQUAL_FLOAT (1.0f, f);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_f64(r, &d));     TEST_ASSERT_EQUAL_DOUBLE(1.0,  d);
    TEST_ASSERT_EQUAL_INT64(0, sf_binary_reader_remaining(r));

    FREE_READER(r, s);
}

/*===========================================================================
 * Primitive types — big-endian
 *===========================================================================*/

static void test_read_primitives_be(void) {
    static const uint8_t buf[] = {
        0x12, 0x34,                                          /* u16 = 0x1234 */
        0xDE, 0xAD, 0xBE, 0xEF,                              /* u32 = 0xDEADBEEF */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,      /* u64 = 1 */
    };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, true);

    uint16_t u16; uint32_t u32; uint64_t u64;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u16(r, &u16));   TEST_ASSERT_EQUAL_HEX16(0x1234, u16);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u32(r, &u32));   TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, u32);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u64(r, &u64));   TEST_ASSERT_EQUAL_HEX64(1ull, u64);

    FREE_READER(r, s);
}

/*===========================================================================
 * Endian flip mid-stream
 *===========================================================================*/

static void test_endian_flip(void) {
    /*  Same u32 0x11223344 written as LE then BE consecutively. */
    static const uint8_t buf[] = { 0x44, 0x33, 0x22, 0x11,   /* LE */
                                   0x11, 0x22, 0x33, 0x44 }; /* BE */
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    uint32_t a, b;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u32(r, &a));
    TEST_ASSERT_EQUAL_HEX32(0x11223344, a);
    sf_binary_reader_set_big_endian(r, true);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u32(r, &b));
    TEST_ASSERT_EQUAL_HEX32(0x11223344, b);
    FREE_READER(r, s);
}

/*===========================================================================
 * Varint
 *===========================================================================*/

static void test_varint_short_then_long(void) {
    static const uint8_t buf[] = {
        0x78, 0x56, 0x34, 0x12,                              /* varint32 */
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      /* varint64 */
    };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    int64_t v;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_varint(r, &v));
    TEST_ASSERT_EQUAL_INT64(0x12345678, v);
    sf_binary_reader_set_varint_long(r, true);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_varint(r, &v));
    TEST_ASSERT_EQUAL_INT64(1, v);
    FREE_READER(r, s);
}

/*===========================================================================
 * StepIn / StepOut nested 3 deep
 *===========================================================================*/

static void test_step_nested(void) {
    static const uint8_t buf[16] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    uint8_t v;
    /*  cursor=0 → step to 4 */
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_step_in(r, 4));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u8(r, &v));   TEST_ASSERT_EQUAL_HEX8(0x05, v);
    /*  cursor=5 → step to 8 */
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_step_in(r, 8));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u8(r, &v));   TEST_ASSERT_EQUAL_HEX8(0x09, v);
    /*  cursor=9 → step to 12 */
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_step_in(r, 12));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u8(r, &v));   TEST_ASSERT_EQUAL_HEX8(0x0D, v);
    /*  back out to 9 → 5 → 0 */
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_step_out(r));
    TEST_ASSERT_EQUAL_INT64(9, sf_binary_reader_position(r));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_step_out(r));
    TEST_ASSERT_EQUAL_INT64(5, sf_binary_reader_position(r));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_step_out(r));
    TEST_ASSERT_EQUAL_INT64(0, sf_binary_reader_position(r));
    /*  Extra step_out is an error. */
    TEST_ASSERT_NOT_EQUAL(SF_OK, sf_binary_reader_step_out(r));
    FREE_READER(r, s);
}

/*===========================================================================
 * Pad / Skip
 *===========================================================================*/

static void test_pad_skip(void) {
    static const uint8_t buf[16] = {0};
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_skip(r, 3));
    TEST_ASSERT_EQUAL_INT64(3, sf_binary_reader_position(r));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_pad(r, 4));
    TEST_ASSERT_EQUAL_INT64(4, sf_binary_reader_position(r));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_pad(r, 8));
    TEST_ASSERT_EQUAL_INT64(8, sf_binary_reader_position(r));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_pad(r, 8));   /* already aligned */
    TEST_ASSERT_EQUAL_INT64(8, sf_binary_reader_position(r));
    FREE_READER(r, s);
}

static void test_pad_relative_rejects_future_start(void) {
    static const uint8_t buf[16] = {0};
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_skip(r, 1));
    TEST_ASSERT_EQUAL(SF_ERR_INVALID_ARG, sf_binary_reader_pad_relative(r, 2, 4));
    TEST_ASSERT_EQUAL_INT64(1, sf_binary_reader_position(r));
    FREE_READER(r, s);
}

/*===========================================================================
 * Get* (read at offset without moving cursor)
 *===========================================================================*/

static void test_get_value(void) {
    static const uint8_t buf[8] = { 0x00, 0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12 };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    uint32_t v;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_u32(r, 4, &v));
    TEST_ASSERT_EQUAL_HEX32(0x12345678, v);
    /*  Cursor must still be at 0 after a get. */
    TEST_ASSERT_EQUAL_INT64(0, sf_binary_reader_position(r));
    FREE_READER(r, s);
}

static void test_reader_state_extensions(void) {
    static const uint8_t buf[] = {0xAA};
    bool old_default = sf_binary_reader_flexible_default();
    sf_binary_reader_set_flexible_default(true);

    sf_istream_t *s;
    sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    TEST_ASSERT_TRUE(sf_binary_reader_flexible(r));
    TEST_ASSERT_EQUAL_PTR(s, sf_binary_reader_stream(r));
    sf_binary_reader_set_flexible(r, false);
    TEST_ASSERT_FALSE(sf_binary_reader_flexible(r));

    FREE_READER(r, s);
    sf_binary_reader_set_flexible_default(old_default);
}

static void test_read_plural_primitives(void) {
    uint8_t buf[95];
    size_t p = 0;
    buf[p++] = 0; buf[p++] = 1; buf[p++] = 0;
    buf[p++] = 0xFE; buf[p++] = 0xFF; buf[p++] = 0x01;
    buf[p++] = 0x10; buf[p++] = 0x20; buf[p++] = 0x30;
    put_i16_le(&buf[p], -1); p += 2; put_i16_le(&buf[p], 2); p += 2; put_i16_le(&buf[p], -3); p += 2;
    put_u16_le(&buf[p], 4); p += 2; put_u16_le(&buf[p], 5); p += 2; put_u16_le(&buf[p], 6); p += 2;
    put_i32_le(&buf[p], -7); p += 4; put_i32_le(&buf[p], 8); p += 4; put_i32_le(&buf[p], -9); p += 4;
    put_u32_le(&buf[p], 10); p += 4; put_u32_le(&buf[p], 11); p += 4; put_u32_le(&buf[p], 12); p += 4;
    put_i64_le(&buf[p], -13); p += 8; put_i64_le(&buf[p], 14); p += 8; put_i64_le(&buf[p], -15); p += 8;
    put_u64_le(&buf[p], 16); p += 8; put_u64_le(&buf[p], 17); p += 8; put_u64_le(&buf[p], 18); p += 8;

    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, p, &s, false);
    bool bs[3]; int8_t i8s[3]; uint8_t u8s[3]; int16_t i16s[3]; uint16_t u16s[3];
    int32_t i32s[3]; uint32_t u32s[3]; int64_t i64s[3]; uint64_t u64s[3];
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_bools(r, 3, bs));
    TEST_ASSERT_FALSE(bs[0]); TEST_ASSERT_TRUE(bs[1]); TEST_ASSERT_FALSE(bs[2]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_i8s(r, 3, i8s));
    TEST_ASSERT_EQUAL_INT8(-2, i8s[0]); TEST_ASSERT_EQUAL_INT8(-1, i8s[1]); TEST_ASSERT_EQUAL_INT8(1, i8s[2]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u8s(r, 3, u8s));
    TEST_ASSERT_EQUAL_HEX8(0x10, u8s[0]); TEST_ASSERT_EQUAL_HEX8(0x30, u8s[2]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_i16s(r, 3, i16s));
    TEST_ASSERT_EQUAL_INT16(-1, i16s[0]); TEST_ASSERT_EQUAL_INT16(-3, i16s[2]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u16s(r, 3, u16s));
    TEST_ASSERT_EQUAL_UINT16(4, u16s[0]); TEST_ASSERT_EQUAL_UINT16(6, u16s[2]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_i32s(r, 3, i32s));
    TEST_ASSERT_EQUAL_INT32(-7, i32s[0]); TEST_ASSERT_EQUAL_INT32(-9, i32s[2]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u32s(r, 3, u32s));
    TEST_ASSERT_EQUAL_UINT32(10, u32s[0]); TEST_ASSERT_EQUAL_UINT32(12, u32s[2]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_i64s(r, 3, i64s));
    TEST_ASSERT_EQUAL_INT64(-13, i64s[0]); TEST_ASSERT_EQUAL_INT64(-15, i64s[2]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u64s(r, 3, u64s));
    TEST_ASSERT_EQUAL_UINT64(16, u64s[0]); TEST_ASSERT_EQUAL_UINT64(18, u64s[2]);
    FREE_READER(r, s);
}

static void test_read_plural_primitives_be(void) {
    static const uint8_t buf[] = {
        0xFF, 0xFE, 0x00, 0x03,
        0x12, 0x34, 0x56, 0x78, 0xCA, 0xFE, 0xBA, 0xBE,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x3F, 0x80, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
    };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, true);

    int16_t i16s[2]; uint32_t u32s[2]; int64_t i64s[1]; uint64_t u64s[1]; float f32s[2];
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_i16s(r, 2, i16s));
    TEST_ASSERT_EQUAL_INT16(-2, i16s[0]); TEST_ASSERT_EQUAL_INT16(3, i16s[1]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u32s(r, 2, u32s));
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, u32s[0]); TEST_ASSERT_EQUAL_HEX32(0xCAFEBABEu, u32s[1]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_i64s(r, 1, i64s));
    TEST_ASSERT_EQUAL_INT64(1, i64s[0]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u64s(r, 1, u64s));
    TEST_ASSERT_EQUAL_HEX64(0x0102030405060708ull, u64s[0]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_f32s(r, 2, f32s));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, f32s[0]); TEST_ASSERT_EQUAL_FLOAT(2.0f, f32s[1]);
    FREE_READER(r, s);
}

static void test_read_plural_float_and_varint(void) {
    uint8_t buf[60];
    size_t p = 0;
    put_f32_le(&buf[p], 1.0f); p += 4; put_f32_le(&buf[p], 2.0f); p += 4; put_f32_le(&buf[p], 3.0f); p += 4;
    put_f64_le(&buf[p], 4.0); p += 8; put_f64_le(&buf[p], 5.0); p += 8; put_f64_le(&buf[p], 6.0); p += 8;
    put_i32_le(&buf[p], -7); p += 4; put_i32_le(&buf[p], 8); p += 4; put_i32_le(&buf[p], -9); p += 4;

    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, p, &s, false);
    float f32s[3]; double f64s[3]; int64_t vars[3];
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_f32s(r, 3, f32s));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, f32s[0]); TEST_ASSERT_EQUAL_FLOAT(3.0f, f32s[2]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_f64s(r, 3, f64s));
    TEST_ASSERT_EQUAL_DOUBLE(4.0, f64s[0]); TEST_ASSERT_EQUAL_DOUBLE(6.0, f64s[2]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_varints(r, 3, vars));
    TEST_ASSERT_EQUAL_INT64(-7, vars[0]); TEST_ASSERT_EQUAL_INT64(-9, vars[2]);
    FREE_READER(r, s);
}

static void test_get_single_primitives(void) {
    uint8_t buf[64] = {0};
    buf[1] = 1; buf[2] = 0xFE; buf[3] = 0xAB;
    put_i16_le(&buf[4], -12); put_u16_le(&buf[6], 0x1234);
    put_i32_le(&buf[8], -34); put_u32_le(&buf[12], 0x89ABCDEFu);
    put_i64_le(&buf[16], -56); put_u64_le(&buf[24], 0x1122334455667788ull);
    put_f32_le(&buf[32], 1.5f); put_f64_le(&buf[40], 2.25); put_i32_le(&buf[52], -77);

    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    bool b; int8_t i8; uint8_t u8; int16_t i16; uint16_t u16; int32_t i32; uint32_t u32;
    int64_t i64; uint64_t u64; float f32; double f64; int64_t var;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_bool(r, 1, &b)); TEST_ASSERT_TRUE(b);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_i8(r, 2, &i8)); TEST_ASSERT_EQUAL_INT8(-2, i8);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_u8(r, 3, &u8)); TEST_ASSERT_EQUAL_UINT8(0xAB, u8);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_i16(r, 4, &i16)); TEST_ASSERT_EQUAL_INT16(-12, i16);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_u16(r, 6, &u16)); TEST_ASSERT_EQUAL_HEX16(0x1234, u16);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_i32(r, 8, &i32)); TEST_ASSERT_EQUAL_INT32(-34, i32);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_u32(r, 12, &u32)); TEST_ASSERT_EQUAL_HEX32(0x89ABCDEFu, u32);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_i64(r, 16, &i64)); TEST_ASSERT_EQUAL_INT64(-56, i64);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_u64(r, 24, &u64)); TEST_ASSERT_EQUAL_HEX64(0x1122334455667788ull, u64);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_f32(r, 32, &f32)); TEST_ASSERT_EQUAL_FLOAT(1.5f, f32);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_f64(r, 40, &f64)); TEST_ASSERT_EQUAL_DOUBLE(2.25, f64);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_varint(r, 52, &var)); TEST_ASSERT_EQUAL_INT64(-77, var);
    TEST_ASSERT_EQUAL_INT64(0, sf_binary_reader_position(r));
    FREE_READER(r, s);
}

static void test_get_plural_primitives(void) {
    uint8_t buf[48];
    size_t p = 0;
    buf[p++] = 0; buf[p++] = 1;
    put_i16_le(&buf[p], -1); p += 2; put_i16_le(&buf[p], 2); p += 2;
    put_u32_le(&buf[p], 3); p += 4; put_u32_le(&buf[p], 4); p += 4;
    put_f32_le(&buf[p], 5.0f); p += 4; put_f32_le(&buf[p], 6.0f); p += 4;
    put_i32_le(&buf[p], -7); p += 4; put_i32_le(&buf[p], 8); p += 4;
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, p, &s, false);
    bool bs[2]; int16_t i16s[2]; uint32_t u32s[2]; float f32s[2]; int64_t vars[2];
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_bools(r, 0, 2, bs));
    TEST_ASSERT_FALSE(bs[0]); TEST_ASSERT_TRUE(bs[1]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_i16s(r, 2, 2, i16s));
    TEST_ASSERT_EQUAL_INT16(-1, i16s[0]); TEST_ASSERT_EQUAL_INT16(2, i16s[1]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_u32s(r, 6, 2, u32s));
    TEST_ASSERT_EQUAL_UINT32(3, u32s[0]); TEST_ASSERT_EQUAL_UINT32(4, u32s[1]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_f32s(r, 14, 2, f32s));
    TEST_ASSERT_EQUAL_FLOAT(5.0f, f32s[0]); TEST_ASSERT_EQUAL_FLOAT(6.0f, f32s[1]);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_varints(r, 22, 2, vars));
    TEST_ASSERT_EQUAL_INT64(-7, vars[0]); TEST_ASSERT_EQUAL_INT64(8, vars[1]);
    TEST_ASSERT_EQUAL_INT64(0, sf_binary_reader_position(r));
    FREE_READER(r, s);
}

/*===========================================================================
 * Asserts
 *===========================================================================*/

static void test_assert(void) {
    static const uint8_t buf[8] = { 0xEF, 0xBE, 0xAD, 0xDE, 0x42, 0x00, 0x00, 0x00 };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_u32_one(r, 0xDEADBEEFu));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_u32_one(r, 0x42));
    FREE_READER(r, s);
}

static void test_assert_multi_options_all_types(void) {
    uint8_t buf[126];
    size_t p = 0;
    for (int pass = 0; pass < 2; pass++) {
        (void)pass;
        buf[p++] = 1; buf[p++] = 0xFB; buf[p++] = 0x42;
        put_i16_le(&buf[p], -1234); p += 2; put_u16_le(&buf[p], 0x1234); p += 2;
        put_i32_le(&buf[p], -123456); p += 4; put_u32_le(&buf[p], 0xDEADBEEFu); p += 4;
        put_i64_le(&buf[p], -2); p += 8; put_u64_le(&buf[p], 0x1122334455667788ull); p += 8;
        put_f32_le(&buf[p], 1.5f); p += 4; put_f64_le(&buf[p], 2.25); p += 8;
        put_i32_le(&buf[p], -77); p += 4;
    }

    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, p, &s, false);
    bool b_opts[] = { false, true, false }; bool b_val = false;
    int8_t i8_opts[] = { -8, -5, 7 }; int8_t i8_val = 0;
    uint8_t u8_opts[] = { 0x10, 0x42, 0x99 }; uint8_t u8_val = 0;
    int16_t i16_opts[] = { -1, -1234, 2 }; int16_t i16_val = 0;
    uint16_t u16_opts[] = { 1, 0x1234, 2 }; uint16_t u16_val = 0;
    int32_t i32_opts[] = { -1, -123456, 2 }; int32_t i32_val = 0;
    uint32_t u32_opts[] = { 1, 0xDEADBEEFu, 2 }; uint32_t u32_val = 0;
    int64_t i64_opts[] = { -1, -2, -3 }; int64_t i64_val = 0;
    uint64_t u64_opts[] = { 1, 0x1122334455667788ull, 2 }; uint64_t u64_val = 0;
    float f32_opts[] = { 0.5f, 1.5f, 2.5f }; float f32_val = 0.0f;
    double f64_opts[] = { 1.25, 2.25, 3.25 }; double f64_val = 0.0;
    int64_t var_opts[] = { -1, -77, 2 }; int64_t var_val = 0;

    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_bool(r, 3, b_opts, &b_val)); TEST_ASSERT_TRUE(b_val);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_i8(r, 3, i8_opts, &i8_val)); TEST_ASSERT_EQUAL_INT8(-5, i8_val);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_u8(r, 3, u8_opts, &u8_val)); TEST_ASSERT_EQUAL_HEX8(0x42, u8_val);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_i16(r, 3, i16_opts, &i16_val)); TEST_ASSERT_EQUAL_INT16(-1234, i16_val);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_u16(r, 3, u16_opts, &u16_val)); TEST_ASSERT_EQUAL_HEX16(0x1234, u16_val);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_i32(r, 3, i32_opts, &i32_val)); TEST_ASSERT_EQUAL_INT32(-123456, i32_val);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_u32(r, 3, u32_opts, &u32_val)); TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, u32_val);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_i64(r, 3, i64_opts, &i64_val)); TEST_ASSERT_EQUAL_INT64(-2, i64_val);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_u64(r, 3, u64_opts, &u64_val)); TEST_ASSERT_EQUAL_HEX64(0x1122334455667788ull, u64_val);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_f32(r, 3, f32_opts, &f32_val)); TEST_ASSERT_EQUAL_FLOAT(1.5f, f32_val);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_f64(r, 3, f64_opts, &f64_val)); TEST_ASSERT_EQUAL_DOUBLE(2.25, f64_val);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_varint(r, 3, var_opts, &var_val)); TEST_ASSERT_EQUAL_INT64(-77, var_val);

    bool b_bad[] = { false, false, false }; int8_t i8_bad[] = { 1, 2, 3 }; uint8_t u8_bad[] = { 1, 2, 3 };
    int16_t i16_bad[] = { 1, 2, 3 }; uint16_t u16_bad[] = { 1, 2, 3 }; int32_t i32_bad[] = { 1, 2, 3 };
    uint32_t u32_bad[] = { 1, 2, 3 }; int64_t i64_bad[] = { 1, 2, 3 }; uint64_t u64_bad[] = { 1, 2, 3 };
    float f32_bad[] = { 2.0f, 3.0f, 4.0f }; double f64_bad[] = { 1.0, 3.0, 4.0 }; int64_t var_bad[] = { 1, 2, 3 };
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_assert_bool(r, 3, b_bad, NULL));
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_assert_i8(r, 3, i8_bad, NULL));
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_assert_u8(r, 3, u8_bad, NULL));
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_assert_i16(r, 3, i16_bad, NULL));
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_assert_u16(r, 3, u16_bad, NULL));
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_assert_i32(r, 3, i32_bad, NULL));
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_assert_u32(r, 3, u32_bad, NULL));
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_assert_i64(r, 3, i64_bad, NULL));
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_assert_u64(r, 3, u64_bad, NULL));
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_assert_f32(r, 3, f32_bad, NULL));
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_assert_f64(r, 3, f64_bad, NULL));
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_assert_varint(r, 3, var_bad, NULL));
    FREE_READER(r, s);
}

static void test_flexible_skips_assert_mismatch(void) {
    static const uint8_t buf[] = { 0x78, 0x56, 0x34, 0x12 };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    uint32_t options[] = { 1, 2, 3 };
    uint32_t value = 0;
    sf_binary_reader_set_flexible(r, true);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_u32(r, 3, options, &value));
    TEST_ASSERT_EQUAL_HEX32(0x12345678, value);
    FREE_READER(r, s);
}

static void test_enum_reads_and_gets(void) {
    uint8_t buf[30];
    size_t p = 0;
    buf[p++] = 2; put_u16_le(&buf[p], 3); p += 2; put_u32_le(&buf[p], 4); p += 4; put_u64_le(&buf[p], 5); p += 8;
    buf[p++] = 9; put_u16_le(&buf[p], 10); p += 2; put_u32_le(&buf[p], 11); p += 4; put_u64_le(&buf[p], 12); p += 8;
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, p, &s, false);
    uint8_t e8_opts[] = { 1, 2, 3 }; uint16_t e16_opts[] = { 2, 3, 4 };
    uint32_t e32_opts[] = { 3, 4, 5 }; uint64_t e64_opts[] = { 4, 5, 6 };
    uint8_t e8; uint16_t e16; uint32_t e32; uint64_t e64;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_enum_8(r, 3, e8_opts, &e8)); TEST_ASSERT_EQUAL_UINT8(2, e8);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_enum_16(r, 3, e16_opts, &e16)); TEST_ASSERT_EQUAL_UINT16(3, e16);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_enum_32(r, 3, e32_opts, &e32)); TEST_ASSERT_EQUAL_UINT32(4, e32);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_enum_64(r, 3, e64_opts, &e64)); TEST_ASSERT_EQUAL_UINT64(5, e64);
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_read_enum_8(r, 3, e8_opts, &e8));
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_read_enum_16(r, 3, e16_opts, &e16));
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_read_enum_32(r, 3, e32_opts, &e32));
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_read_enum_64(r, 3, e64_opts, &e64));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_enum_32(r, 3, 3, e32_opts, &e32));
    TEST_ASSERT_EQUAL_UINT32(4, e32);
    FREE_READER(r, s);
}

static void test_assert_pattern(void) {
    static uint8_t buf[400];
    memset(buf, 0xCC, sizeof(buf));
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_pattern(r, 400, 0xCC));
    FREE_READER(r, s);

    /*  Failure case. */
    static uint8_t bad[8] = { 0xCC, 0xCC, 0xCC, 0xCC, 0x99, 0xCC, 0xCC, 0xCC };
    r = open_reader(bad, sizeof(bad), &s, false);
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_assert_pattern(r, 8, 0xCC));
    FREE_READER(r, s);
}

static void test_assert_ascii(void) {
    static const uint8_t buf[] = { 'B', 'N', 'D', '4' };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_assert_ascii(r, "BND4"));
    FREE_READER(r, s);

    static const uint8_t bad[] = { 'B', 'N', 'D', '3' };
    r = open_reader(bad, sizeof(bad), &s, false);
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_assert_ascii(r, "BND4"));
    FREE_READER(r, s);
}

/*===========================================================================
 * Strings
 *===========================================================================*/

static void test_read_ascii_terminated(void) {
    /*  "Hello\0World\0" */
    static const uint8_t buf[] = { 'H','e','l','l','o', 0,
                                   'W','o','r','l','d', 0 };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    char *a = NULL, *b = NULL;
    size_t la = 0, lb = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_ascii(r, &a, &la));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_ascii(r, &b, &lb));
    TEST_ASSERT_EQUAL_STRING("Hello", a);
    TEST_ASSERT_EQUAL_STRING("World", b);
    sf_free(NULL, a);
    sf_free(NULL, b);
    FREE_READER(r, s);
}

static void test_read_shift_jis_terminated(void) {
    /*  "エルデンリング\0" */
    static const uint8_t buf[] = {
        0x83, 0x47, 0x83, 0x8B, 0x83, 0x66, 0x83, 0x93,
        0x83, 0x8A, 0x83, 0x93, 0x83, 0x4F, 0x00,
    };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    char *a = NULL;
    size_t la = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_shift_jis(r, &a, &la));
    TEST_ASSERT_EQUAL_STRING("\xE3\x82\xA8\xE3\x83\xAB\xE3\x83\x87\xE3\x83\xB3"
                             "\xE3\x83\xAA\xE3\x83\xB3\xE3\x82\xB0", a);
    sf_free(NULL, a);
    FREE_READER(r, s);
}

static void test_read_utf16_terminated_le(void) {
    /*  "黑暗之魂\0" UTF-16 LE */
    static const uint8_t buf[] = {
        0xD1, 0x9E, 0x97, 0x66, 0x4B, 0x4E, 0x42, 0x9B, 0x00, 0x00,
    };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    char *a = NULL;
    size_t la = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_utf16(r, &a, &la));
    TEST_ASSERT_EQUAL_STRING("\xE9\xBB\x91\xE6\x9A\x97\xE4\xB9\x8B\xE9\xAD\x82", a);
    sf_free(NULL, a);
    FREE_READER(r, s);
}

static void test_read_utf16_terminated_be(void) {
    static const uint8_t buf[] = {
        0x9E, 0xD1, 0x66, 0x97, 0x4E, 0x4B, 0x9B, 0x42, 0x00, 0x00,
    };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, true);
    char *a = NULL;
    size_t la = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_utf16(r, &a, &la));
    TEST_ASSERT_EQUAL_STRING("\xE9\xBB\x91\xE6\x9A\x97\xE4\xB9\x8B\xE9\xAD\x82", a);
    sf_free(NULL, a);
    FREE_READER(r, s);
}

static void test_read_fix_str(void) {
    /*  16-byte field, "ABC\0" + padding garbage. */
    static const uint8_t buf[16] = { 'A','B','C', 0, 'X','X','X','X',
                                      'X','X','X','X','X','X','X','X' };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    char *a = NULL;
    size_t la = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_fix_str(r, sizeof(buf), &a, &la));
    TEST_ASSERT_EQUAL_STRING("ABC", a);
    /*  Cursor must have advanced the entire field. */
    TEST_ASSERT_EQUAL_INT64(16, sf_binary_reader_position(r));
    sf_free(NULL, a);
    FREE_READER(r, s);
}

static void test_get_strings(void) {
    static const uint8_t buf[] = {
        'A','S','C','I','I',0,
        'N','A','M','E','X','X',
        0x83, 0x47, 0x83, 0x8B, 0x00,
        0xD1, 0x9E, 0x97, 0x66, 0x00, 0x00,
    };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    char *str = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_ascii(r, 0, &str, &len));
    TEST_ASSERT_EQUAL_STRING("ASCII", str); sf_free(NULL, str); str = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_ascii_n(r, 6, 4, &str, &len));
    TEST_ASSERT_EQUAL_STRING("NAME", str); sf_free(NULL, str); str = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_shift_jis(r, 12, &str, &len));
    TEST_ASSERT_EQUAL_STRING("\xE3\x82\xA8\xE3\x83\xAB", str); sf_free(NULL, str); str = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_shift_jis_n(r, 12, 4, &str, &len));
    TEST_ASSERT_EQUAL_STRING("\xE3\x82\xA8\xE3\x83\xAB", str); sf_free(NULL, str); str = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_get_utf16(r, 17, &str, &len));
    TEST_ASSERT_EQUAL_STRING("\xE9\xBB\x91\xE6\x9A\x97", str); sf_free(NULL, str);
    TEST_ASSERT_EQUAL_INT64(0, sf_binary_reader_position(r));
    FREE_READER(r, s);
}

static void test_shift_jis_round_trip_identity(void) {
    static const uint8_t sjis[] = {
        0x83, 0x47, 0x83, 0x8B, 0x83, 0x66, 0x83, 0x93,
        0x83, 0x8A, 0x83, 0x93, 0x83, 0x4F, 0x00,
    };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(sjis, sizeof(sjis), &s, false);
    char *utf8 = NULL;
    size_t len = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_shift_jis(r, &utf8, &len));
    FREE_READER(r, s);

    sf_ostream_t *os = NULL;
    sf_binary_writer_t *w = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&os, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&w, os, false, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_shift_jis(w, utf8, true));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));
    sf_binary_writer_destroy(w);

    void *data = NULL;
    size_t data_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(os, &data, &data_size));
    TEST_ASSERT_EQUAL_UINT64(sizeof(sjis), data_size);
    TEST_ASSERT_EQUAL(0, memcmp(sjis, data, sizeof(sjis)));
    sf_free(NULL, data);
    sf_free(NULL, utf8);
    sf_ostream_close(os);
}

/*===========================================================================
 * Vectors / quat / 11_11_10 / colors
 *===========================================================================*/

static void test_vec_and_quat(void) {
    static const uint8_t buf[] = {
        /*  vec3 (1.0, 2.0, 3.0) */
        0x00,0x00,0x80,0x3F, 0x00,0x00,0x00,0x40, 0x00,0x00,0x40,0x40,
        /*  quat (0, 0, 0, 1) */
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x80,0x3F,
    };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    sf_vec3_t v;
    sf_quat_t q;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_vec3(r, &v));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, v.x);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, v.y);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, v.z);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_quat(r, &q));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, q.x);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, q.w);
    FREE_READER(r, s);
}

static void test_argb_and_friends(void) {
    static const uint8_t buf[16] = {
        0xFF, 0x10, 0x20, 0x30,   /* ARGB: A=FF R=10 G=20 B=30 */
        0xFF, 0x30, 0x20, 0x10,   /* ABGR: A=FF B=30 G=20 R=10 */
        0x10, 0x20, 0x30, 0xFF,   /* RGBA: R=10 G=20 B=30 A=FF */
        0x30, 0x20, 0x10, 0xFF,   /* BGRA: B=30 G=20 R=10 A=FF */
    };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    sf_color_t c;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_argb(r, &c));
    TEST_ASSERT_EQUAL_UINT8(0xFF, c.a); TEST_ASSERT_EQUAL_HEX8(0x10, c.r);
    TEST_ASSERT_EQUAL_HEX8(0x20, c.g); TEST_ASSERT_EQUAL_HEX8(0x30, c.b);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_abgr(r, &c));
    TEST_ASSERT_EQUAL_UINT8(0xFF, c.a); TEST_ASSERT_EQUAL_HEX8(0x10, c.r);
    TEST_ASSERT_EQUAL_HEX8(0x20, c.g); TEST_ASSERT_EQUAL_HEX8(0x30, c.b);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_rgba(r, &c));
    TEST_ASSERT_EQUAL_UINT8(0xFF, c.a); TEST_ASSERT_EQUAL_HEX8(0x10, c.r);
    TEST_ASSERT_EQUAL_HEX8(0x20, c.g); TEST_ASSERT_EQUAL_HEX8(0x30, c.b);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_bgra(r, &c));
    TEST_ASSERT_EQUAL_UINT8(0xFF, c.a); TEST_ASSERT_EQUAL_HEX8(0x10, c.r);
    TEST_ASSERT_EQUAL_HEX8(0x20, c.g); TEST_ASSERT_EQUAL_HEX8(0x30, c.b);
    FREE_READER(r, s);
}

static void test_11_11_10_zero(void) {
    /*  All-zero packed → (0, 0, 0). */
    static const uint8_t buf[4] = {0};
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    sf_vec3_t v;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_11_11_10_vec3(r, &v));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, v.x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, v.y);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, v.z);
    FREE_READER(r, s);
}

/*===========================================================================
 * Truncation handling
 *===========================================================================*/

static void test_truncation(void) {
    static const uint8_t buf[3] = { 0x01, 0x02, 0x03 };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    uint32_t v;
    TEST_ASSERT_EQUAL(SF_ERR_TRUNCATED, sf_binary_reader_read_u32(r, &v));
    FREE_READER(r, s);
}

static void test_bool_invalid(void) {
    static const uint8_t buf[] = { 0x42 };
    sf_istream_t *s; sf_binary_reader_t *r = open_reader(buf, sizeof(buf), &s, false);
    bool b;
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_binary_reader_read_bool(r, &b));
    FREE_READER(r, s);
}

static void test_sf_reverse_bits_u8_known_values(void) {
    TEST_ASSERT_EQUAL_UINT8(0x00, sf_reverse_bits_u8(0x00));
    TEST_ASSERT_EQUAL_UINT8(0xFF, sf_reverse_bits_u8(0xFF));
    TEST_ASSERT_EQUAL_UINT8(0x80, sf_reverse_bits_u8(0x01));
    TEST_ASSERT_EQUAL_UINT8(0xD5, sf_reverse_bits_u8(0xAB));
    TEST_ASSERT_EQUAL_UINT8(0x42, sf_reverse_bits_u8(0x42));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_read_primitives_le);
    RUN_TEST(test_read_primitives_be);
    RUN_TEST(test_endian_flip);
    RUN_TEST(test_varint_short_then_long);
    RUN_TEST(test_step_nested);
    RUN_TEST(test_pad_skip);
    RUN_TEST(test_pad_relative_rejects_future_start);
    RUN_TEST(test_get_value);
    RUN_TEST(test_reader_state_extensions);
    RUN_TEST(test_read_plural_primitives);
    RUN_TEST(test_read_plural_primitives_be);
    RUN_TEST(test_read_plural_float_and_varint);
    RUN_TEST(test_get_single_primitives);
    RUN_TEST(test_get_plural_primitives);
    RUN_TEST(test_assert);
    RUN_TEST(test_assert_multi_options_all_types);
    RUN_TEST(test_flexible_skips_assert_mismatch);
    RUN_TEST(test_enum_reads_and_gets);
    RUN_TEST(test_assert_pattern);
    RUN_TEST(test_assert_ascii);
    RUN_TEST(test_read_ascii_terminated);
    RUN_TEST(test_read_shift_jis_terminated);
    RUN_TEST(test_read_utf16_terminated_le);
    RUN_TEST(test_read_utf16_terminated_be);
    RUN_TEST(test_read_fix_str);
    RUN_TEST(test_get_strings);
    RUN_TEST(test_shift_jis_round_trip_identity);
    RUN_TEST(test_vec_and_quat);
    RUN_TEST(test_argb_and_friends);
    RUN_TEST(test_11_11_10_zero);
    RUN_TEST(test_truncation);
    RUN_TEST(test_bool_invalid);
    RUN_TEST(test_sf_reverse_bits_u8_known_values);
    return UNITY_END();
}
