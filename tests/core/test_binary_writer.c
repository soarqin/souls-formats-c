/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 1 QA — sf_binary_writer_t covers BinaryWriterEx semantics, with
 * particular focus on Reserve/Fill placeholder backfill (the hardest API).
 */

#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*===========================================================================
 * Helpers
 *===========================================================================*/

static void make_writer(sf_ostream_t **out_s, sf_binary_writer_t **out_w,
                       bool big_endian) {
    sf_ostream_t *s = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&s, NULL));
    sf_binary_writer_t *w = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&w, s, big_endian, NULL));
    *out_s = s;
    *out_w = w;
}

static void destroy_writer(sf_ostream_t *s, sf_binary_writer_t *w) {
    sf_binary_writer_destroy(w);
    sf_ostream_close(s);
}

static void expect_finish_bytes(sf_ostream_t *s, sf_binary_writer_t *w,
                                const uint8_t *expected, size_t expected_n) {
    uint8_t *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish_bytes(w, &bytes, &n));
    TEST_ASSERT_EQUAL_size_t(expected_n, n);
    if (expected_n > 0) TEST_ASSERT_EQUAL_MEMORY(expected, bytes, n);
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

/*===========================================================================
 * Primitives + endianness
 *===========================================================================*/

static void test_write_primitives_le(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u8 (w, 0x12));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u16(w, 0x3456));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u32(w, 0xDEADBEEF));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u64(w, 0x0123456789ABCDEFull));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    static const uint8_t expected[] = {
        0x12,
        0x56, 0x34,
        0xEF, 0xBE, 0xAD, 0xDE,
        0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01,
    };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes, n);
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

static void test_write_primitives_be(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, true);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u32(w, 0x12345678u));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u64(w, 0x0102030405060708ull));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    static const uint8_t expected[] = {
        0x12, 0x34, 0x56, 0x78,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes, n);
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

static void test_write_bools_plural(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    const bool values[] = { true, false, true };
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_bools(w, 3, values));
    static const uint8_t expected[] = { 1, 0, 1 };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_write_i8s_plural(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    const int8_t values[] = { -1, 0x12 };
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_i8s(w, 2, values));
    static const uint8_t expected[] = { 0xFF, 0x12 };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_write_u8s_plural(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    const uint8_t values[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u8s(w, 4, values));
    expect_finish_bytes(s, w, values, sizeof(values));
}

static void test_write_i16s_plural(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    const int16_t values[] = { (int16_t)0x1122, (int16_t)-2 };
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_i16s(w, 2, values));
    static const uint8_t expected[] = { 0x22, 0x11, 0xFE, 0xFF };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_write_u16s_plural_be(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, true);
    const uint16_t values[] = { 0x1234, 0xABCD };
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u16s(w, 2, values));
    static const uint8_t expected[] = { 0x12, 0x34, 0xAB, 0xCD };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_write_i32s_plural(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    const int32_t values[] = { 0x01020304, -2 };
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_i32s(w, 2, values));
    static const uint8_t expected[] = { 0x04, 0x03, 0x02, 0x01, 0xFE, 0xFF, 0xFF, 0xFF };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_write_u32s_plural_be(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, true);
    const uint32_t values[] = { 0x01020304u, 0xA0B0C0D0u };
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u32s(w, 2, values));
    static const uint8_t expected[] = { 0x01, 0x02, 0x03, 0x04, 0xA0, 0xB0, 0xC0, 0xD0 };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_write_i64s_plural(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    const int64_t values[] = { 1, -1 };
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_i64s(w, 2, values));
    static const uint8_t expected[] = {
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_write_u64s_plural_be(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, true);
    const uint64_t values[] = { 0x0102030405060708ull };
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u64s(w, 1, values));
    static const uint8_t expected[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_write_f32s_plural(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    const float values[] = { 1.0f, -2.0f };
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_f32s(w, 2, values));
    static const uint8_t expected[] = { 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0xC0 };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_write_f64s_plural(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    const double values[] = { 1.5 };
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_f64s(w, 1, values));
    static const uint8_t expected[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x3F };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_write_varints_plural_32(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    const int64_t values[] = { 0x01020304, -1 };
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_varints(w, 2, values));
    static const uint8_t expected[] = { 0x04, 0x03, 0x02, 0x01, 0xFF, 0xFF, 0xFF, 0xFF };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_write_varints_plural_64(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    sf_binary_writer_set_varint_long(w, true);
    const int64_t values[] = { 0x0102030405060708ll };
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_varints(w, 1, values));
    static const uint8_t expected[] = { 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

/*===========================================================================
 * Reservations: u32 / u64 / varint
 *===========================================================================*/

static void test_reserve_fill_u32(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    /*  Header layout: [reserved u32 "size" at off 0] [body 'A'B' at off 4]
     *  → fill in actual size after writing body. */
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_u32(w, "size"));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u8(w, 'A'));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u8(w, 'B'));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_u32(w, "size",
        (uint32_t)sf_binary_writer_position(w)));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    /*  Final stream: [06 00 00 00] [41] [42] */
    static const uint8_t expected[] = { 0x06, 0x00, 0x00, 0x00, 'A', 'B' };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes, n);
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

static void test_reserve_fill_u64(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_u64(w, "off"));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u8 (w, 0x42));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_u64(w, "off", 0xDEADBEEFCAFEBABEull));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    static const uint8_t expected[] = {
        0xBE, 0xBA, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE,
        0x42,
    };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes, n);
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

static void test_reserve_fill_varint_short(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    /*  varint_long = false → 4-byte fill */
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_varint(w, "v"));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_varint(w, "v", 0x12345678));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    static const uint8_t expected[] = { 0x78, 0x56, 0x34, 0x12 };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes, n);
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

static void test_reserve_fill_varint_long(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    sf_binary_writer_set_varint_long(w, true);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_varint(w, "v"));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_varint(w, "v", 0x0123456789ABCDEFll));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    static const uint8_t expected[] = { 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01 };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes, n);
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

static void test_reserve_unfilled_blocks_finish(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK,           sf_binary_writer_reserve_u32(w, "leak"));
    TEST_ASSERT_EQUAL(SF_ERR_INTERNAL, sf_binary_writer_finish(w));
    /*  Force-destroy without finish; sf_binary_writer_destroy must be tolerant. */
    destroy_writer(s, w);
}

static void test_reserve_duplicate(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_u32(w, "x"));
    TEST_ASSERT_EQUAL(SF_ERR_ALREADY_EXISTS,
                      sf_binary_writer_reserve_u32(w, "x"));
    /*  Different kind under the same name is allowed (matches upstream
     *  scoping by name+typeName). */
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_u64(w, "x"));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_u32(w, "x", 0));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_u64(w, "x", 0));
    static const uint8_t expected[12] = { 0 };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_reserve_fill_bool(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_bool(w, "b"));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u8(w, 0xAA));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_bool(w, "b", true));
    static const uint8_t expected[] = { 0x01, 0xAA };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_reserve_fill_i8(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_i8(w, "i8"));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_i8(w, "i8", (int8_t)-3));
    static const uint8_t expected[] = { 0xFD };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_reserve_fill_u8(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_u8(w, "u8"));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_u8(w, "u8", 0x7F));
    static const uint8_t expected[] = { 0x7F };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_reserve_fill_i16(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_i16(w, "i16"));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_i16(w, "i16", (int16_t)0x1234));
    static const uint8_t expected[] = { 0x34, 0x12 };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_reserve_fill_u16_be(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, true);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_u16(w, "u16"));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_u16(w, "u16", 0xABCD));
    static const uint8_t expected[] = { 0xAB, 0xCD };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_reserve_fill_f32_roundtrips_float(void) {
    sf_ostream_t *os; sf_binary_writer_t *w;
    make_writer(&os, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_f32(w, "single"));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u8(w, 0x42));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_f32(w, "single", 12.5f));

    uint8_t *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish_bytes(w, &bytes, &n));
    TEST_ASSERT_EQUAL_size_t(5, n);
    sf_binary_writer_destroy(w);
    sf_ostream_close(os);

    sf_istream_t *is = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_istream_open_memory(&is, bytes, n, NULL));
    sf_binary_reader_t *r = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_create(&r, is, false, NULL));
    float f = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_f32(r, &f));
    TEST_ASSERT_EQUAL_FLOAT(12.5f, f);
    uint8_t tail = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u8(r, &tail));
    TEST_ASSERT_EQUAL_HEX8(0x42, tail);
    sf_binary_reader_destroy(r);
    sf_istream_close(is);
    sf_free(NULL, bytes);
}

static void test_reserve_fill_f64(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_f64(w, "double"));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_f64(w, "double", 1.5));
    static const uint8_t expected[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x3F };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_reservation_mixing_types_errors_without_removing_reservation(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_f32(w, "x"));
    TEST_ASSERT_EQUAL(SF_ERR_NOT_FOUND, sf_binary_writer_fill_u32(w, "x", 0));
    TEST_ASSERT_EQUAL(SF_ERR_INTERNAL, sf_binary_writer_finish(w));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_f32(w, "x", 1.0f));
    static const uint8_t expected[] = { 0x00, 0x00, 0x80, 0x3F };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_reservation_same_name_different_primitive_types(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_bool(w, "same"));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_u8(w, "same"));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_u8(w, "same", 0xCC));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_bool(w, "same", false));
    static const uint8_t expected[] = { 0x00, 0xCC };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

/*  Stress: 10k reserve+fill pairs with snprintf-built names. With the
 *  legacy O(N²) reservation table this loop took seconds; with the hash
 *  table it should complete in milliseconds. Functional check: every
 *  back-patched byte is the index modulo 256. */
static void test_reservation_many_back_patches(void) {
    enum { N = 10000 };
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);

    /*  Reserve N u8 slots with distinct names, leaving room for back-patch. */
    for (int i = 0; i < N; i++) {
        char name[32];
        (void)snprintf(name, sizeof(name), "Slot%d", i);
        TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_u8(w, name));
    }
    /*  Fill out of order (reverse) to stress probe walks past tombstones. */
    for (int i = N - 1; i >= 0; i--) {
        char name[32];
        (void)snprintf(name, sizeof(name), "Slot%d", i);
        TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_u8(w, name, (uint8_t)(i & 0xFF)));
    }
    /*  finish requires every reservation to be filled. */
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    TEST_ASSERT_EQUAL_size_t(N, n);
    const uint8_t *b = (const uint8_t *)bytes;
    for (int i = 0; i < N; i++) {
        TEST_ASSERT_EQUAL_HEX8((uint8_t)(i & 0xFF), b[i]);
    }
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

/*  After popping a reservation, the slot becomes a tombstone. Re-inserting
 *  the same (name, kind) must succeed and the new pos must be back-patched
 *  correctly even after many other inserts collide through that slot. */
static void test_reservation_reuse_after_pop(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);

    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_u8(w, "x"));      /* off 0 */
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_u8 (w, "x", 0xAA));  /* pop */
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_u8(w, "x"));      /* off 1, reuse-after-tomb */
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_u8 (w, "x", 0xBB));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));

    static const uint8_t expected[] = { 0xAA, 0xBB };
    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes, n);
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

/*===========================================================================
 * Step + pad
 *===========================================================================*/

static void test_step_in_out(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    /*  Write 8 bytes of zero, step back, overwrite first 4 with 0x42. */
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_pattern(w, 8, 0));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_step_in(w, 0));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u32(w, 0x42424242u));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_step_out(w));
    TEST_ASSERT_EQUAL_INT64(8, sf_binary_writer_position(w));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    static const uint8_t expected[] = {
        0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00,
    };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes, n);
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

static void test_pad_zero_and_ff(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u8(w, 0x42));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_pad(w, 4));   /* pad to 4 with 0 */
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_pad_byte(w, 8, 0xFF));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    static const uint8_t expected[8] = {
        0x42, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes, n);
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

static void test_pad_ff_shorthand(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u8(w, 0x11));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_pad_ff(w, 4));
    static const uint8_t expected[] = { 0x11, 0xFF, 0xFF, 0xFF };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_pad_relative_rejects_future_start(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u8(w, 0x42));
    TEST_ASSERT_EQUAL(SF_ERR_INVALID_ARG, sf_binary_writer_pad_relative(w, 2, 4));
    TEST_ASSERT_EQUAL_INT64(1, sf_binary_writer_position(w));
    static const uint8_t expected[] = { 0x42 };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_write_pattern_renamed(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_pattern(w, 5, 0xAB));
    static const uint8_t expected[] = { 0xAB, 0xAB, 0xAB, 0xAB, 0xAB };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_stream_getter_returns_borrowed_stream(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL_PTR(s, sf_binary_writer_stream(w));
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_write(sf_binary_writer_stream(w), "A", 1));
    TEST_ASSERT_EQUAL_INT64(1, sf_binary_writer_position(w));
    static const uint8_t expected[] = { 'A' };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_to_array_keeps_writer_open(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u8(w, 0x10));
    uint8_t *snapshot = NULL;
    size_t snapshot_n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_to_array(w, &snapshot, &snapshot_n));
    static const uint8_t first[] = { 0x10 };
    TEST_ASSERT_EQUAL_size_t(sizeof(first), snapshot_n);
    TEST_ASSERT_EQUAL_MEMORY(first, snapshot, snapshot_n);
    sf_free(NULL, snapshot);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u8(w, 0x20));
    static const uint8_t expected[] = { 0x10, 0x20 };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

static void test_finish_bytes_closes_writer(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u8(w, 0x55));
    uint8_t *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish_bytes(w, &bytes, &n));
    static const uint8_t expected[] = { 0x55 };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes, n);
    TEST_ASSERT_EQUAL(SF_ERR_INVALID_ARG, sf_binary_writer_write_u8(w, 0x66));
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

static void test_finish_bytes_snapshots_borrowed_stream(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u8(w, 0x11));

    uint8_t *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish_bytes(w, &bytes, &n));
    static const uint8_t expected[] = { 0x11 };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes, n);
    sf_free(NULL, bytes);

    void *stream_bytes = NULL;
    size_t stream_n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &stream_bytes, &stream_n));
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), stream_n);
    TEST_ASSERT_EQUAL_MEMORY(expected, stream_bytes, stream_n);
    sf_free(NULL, stream_bytes);
    destroy_writer(s, w);
}

static void test_finish_bytes_unfilled_keeps_writer_open(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_reserve_u8(w, "later"));
    uint8_t *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_ERR_INTERNAL, sf_binary_writer_finish_bytes(w, &bytes, &n));
    TEST_ASSERT_NULL(bytes);
    TEST_ASSERT_EQUAL_size_t(0, n);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_fill_u8(w, "later", 0x33));
    static const uint8_t expected[] = { 0x33 };
    expect_finish_bytes(s, w, expected, sizeof(expected));
}

/*===========================================================================
 * Strings
 *===========================================================================*/

static void test_write_ascii_terminated(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_ascii(w, "BND4", true));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    static const uint8_t expected[] = { 'B','N','D','4', 0 };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes, n);
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

static void test_write_shift_jis_japanese(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_shift_jis(w,
        "\xE3\x82\xA8\xE3\x83\xAB\xE3\x83\x87\xE3\x83\xB3"
        "\xE3\x83\xAA\xE3\x83\xB3\xE3\x82\xB0", false));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    static const uint8_t expected[] = {
        0x83, 0x47, 0x83, 0x8B, 0x83, 0x66, 0x83, 0x93,
        0x83, 0x8A, 0x83, 0x93, 0x83, 0x4F,
    };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, bytes, n);
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

static void test_write_fix_str(void) {
    sf_ostream_t *s; sf_binary_writer_t *w;
    make_writer(&s, &w, false);
    /*  16-byte fixed field, content "ABC", pad 0x00. */
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_fix_str(w, "ABC", 16, 0));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    TEST_ASSERT_EQUAL_size_t(16, n);
    const uint8_t *b = (const uint8_t *)bytes;
    TEST_ASSERT_EQUAL_HEX8('A', b[0]);
    TEST_ASSERT_EQUAL_HEX8('B', b[1]);
    TEST_ASSERT_EQUAL_HEX8('C', b[2]);
    /*  Trailing bytes must be 0. */
    for (size_t i = 3; i < 16; i++) TEST_ASSERT_EQUAL_HEX8(0x00, b[i]);
    sf_free(NULL, bytes);
    destroy_writer(s, w);
}

/*===========================================================================
 * Round-trip via writer → reader
 *===========================================================================*/

static void test_full_roundtrip_via_reader(void) {
    sf_ostream_t *os; sf_binary_writer_t *w;
    make_writer(&os, &w, false);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u32(w, 0xDEADBEEFu));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u64(w, 0x0102030405060708ull));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_f32(w, 3.14159f));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_ascii(w, "BND4", true));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(os, &bytes, &n));
    sf_binary_writer_destroy(w);
    sf_ostream_close(os);

    /*  Read back. */
    sf_istream_t *is = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_istream_open_memory(&is, bytes, n, NULL));
    sf_binary_reader_t *r = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_create(&r, is, false, NULL));

    uint32_t v32; uint64_t v64; float f;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u32(r, &v32));
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, v32);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_u64(r, &v64));
    TEST_ASSERT_EQUAL_HEX64(0x0102030405060708ull, v64);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_f32(r, &f));
    TEST_ASSERT_EQUAL_FLOAT(3.14159f, f);
    char *str = NULL; size_t slen = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_reader_read_ascii(r, &str, &slen));
    TEST_ASSERT_EQUAL_STRING("BND4", str);
    sf_free(NULL, str);

    sf_binary_reader_destroy(r);
    sf_istream_close(is);
    sf_free(NULL, bytes);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_write_primitives_le);
    RUN_TEST(test_write_primitives_be);
    RUN_TEST(test_write_bools_plural);
    RUN_TEST(test_write_i8s_plural);
    RUN_TEST(test_write_u8s_plural);
    RUN_TEST(test_write_i16s_plural);
    RUN_TEST(test_write_u16s_plural_be);
    RUN_TEST(test_write_i32s_plural);
    RUN_TEST(test_write_u32s_plural_be);
    RUN_TEST(test_write_i64s_plural);
    RUN_TEST(test_write_u64s_plural_be);
    RUN_TEST(test_write_f32s_plural);
    RUN_TEST(test_write_f64s_plural);
    RUN_TEST(test_write_varints_plural_32);
    RUN_TEST(test_write_varints_plural_64);
    RUN_TEST(test_reserve_fill_u32);
    RUN_TEST(test_reserve_fill_u64);
    RUN_TEST(test_reserve_fill_varint_short);
    RUN_TEST(test_reserve_fill_varint_long);
    RUN_TEST(test_reserve_unfilled_blocks_finish);
    RUN_TEST(test_reserve_duplicate);
    RUN_TEST(test_reserve_fill_bool);
    RUN_TEST(test_reserve_fill_i8);
    RUN_TEST(test_reserve_fill_u8);
    RUN_TEST(test_reserve_fill_i16);
    RUN_TEST(test_reserve_fill_u16_be);
    RUN_TEST(test_reserve_fill_f32_roundtrips_float);
    RUN_TEST(test_reserve_fill_f64);
    RUN_TEST(test_reservation_mixing_types_errors_without_removing_reservation);
    RUN_TEST(test_reservation_same_name_different_primitive_types);
    RUN_TEST(test_reservation_many_back_patches);
    RUN_TEST(test_reservation_reuse_after_pop);
    RUN_TEST(test_step_in_out);
    RUN_TEST(test_pad_zero_and_ff);
    RUN_TEST(test_pad_ff_shorthand);
    RUN_TEST(test_pad_relative_rejects_future_start);
    RUN_TEST(test_write_pattern_renamed);
    RUN_TEST(test_stream_getter_returns_borrowed_stream);
    RUN_TEST(test_to_array_keeps_writer_open);
    RUN_TEST(test_finish_bytes_closes_writer);
    RUN_TEST(test_finish_bytes_snapshots_borrowed_stream);
    RUN_TEST(test_finish_bytes_unfilled_keeps_writer_open);
    RUN_TEST(test_write_ascii_terminated);
    RUN_TEST(test_write_shift_jis_japanese);
    RUN_TEST(test_write_fix_str);
    RUN_TEST(test_full_roundtrip_via_reader);
    return UNITY_END();
}
