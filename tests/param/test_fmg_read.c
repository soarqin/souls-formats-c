/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Synthetic-fixture tests for sf_fmg_read_from_memory.
 * Mirrors the upstream FMG layout described in
 *   SoulsFormatsNEXT/SoulsFormats/Formats/FMG.cs:68-139
 *
 * We hand-build little-endian FMG buffers (header + group table + string
 * offset table + strings) so we can exercise both the narrow (v0/v1) and
 * wide (v2) variants, the MD5 prefix path, and the deleted-entry path.
 */

#include "souls_formats/sf_fmg.h"

#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*===========================================================================
 * Little-endian fixture builders
 *===========================================================================*/

typedef struct fixture {
    uint8_t  data[512];
    size_t   size;
} fixture_t;

static void put_u8(uint8_t *p, size_t off, uint8_t v) { p[off] = v; }

static void put_u16(uint8_t *p, size_t off, uint16_t v) {
    p[off + 0] = (uint8_t)(v & 0xFFu);
    p[off + 1] = (uint8_t)(v >> 8);
}

static void put_i32(uint8_t *p, size_t off, int32_t v) {
    uint32_t u = (uint32_t)v;
    p[off + 0] = (uint8_t)(u & 0xFFu);
    p[off + 1] = (uint8_t)((u >> 8) & 0xFFu);
    p[off + 2] = (uint8_t)((u >> 16) & 0xFFu);
    p[off + 3] = (uint8_t)((u >> 24) & 0xFFu);
}

static void put_i64(uint8_t *p, size_t off, int64_t v) {
    uint64_t u = (uint64_t)v;
    for (size_t i = 0; i < 8; i++) p[off + i] = (uint8_t)((u >> (i * 8)) & 0xFFu);
}

static size_t put_cstr(uint8_t *p, size_t off, const char *s) {
    size_t n = strlen(s) + 1;
    memcpy(&p[off], s, n);
    return n;
}

/*  Write a NUL-terminated UTF-16LE codepoint sequence and return bytes
 *  written (including the 2-byte NUL terminator). */
static size_t put_utf16le_codepoints(uint8_t *p, size_t off,
                                     const uint16_t *codepoints, size_t n) {
    for (size_t i = 0; i < n; i++) put_u16(p, off + i * 2, codepoints[i]);
    put_u16(p, off + n * 2, 0);
    return (n + 1) * 2;
}

/*===========================================================================
 * Fixture: v0 (Demon's Souls) — 2 entries, Shift-JIS, narrow
 *===========================================================================*/

static fixture_t make_fixture_v0_basic(void) {
    fixture_t fx;
    memset(&fx, 0, sizeof(fx));

    const int64_t string_offsets_offset = 40;
    const size_t  str0_off = 48;

    put_u8 (fx.data, 0,  0);       /* padding */
    put_u8 (fx.data, 1,  0);       /* big_endian = false */
    put_u8 (fx.data, 2,  0);       /* version = DemonsSouls */
    put_u8 (fx.data, 3,  0);       /* padding */
    put_i32(fx.data, 4,  0);       /* file_size (unread by impl) */
    put_u8 (fx.data, 8,  0);       /* unicode = false */
    put_u8 (fx.data, 9,  0xFF);    /* aux byte: DemonsSouls writes 0xFF */
    put_u8 (fx.data, 10, 0);
    put_u8 (fx.data, 11, 0);
    put_i32(fx.data, 12, 1);       /* group_count */
    put_i32(fx.data, 16, 2);       /* string_count */
    put_i32(fx.data, 20, (int32_t)string_offsets_offset);
    put_i32(fx.data, 24, 0);

    put_i32(fx.data, 28, 0);   /* group: offset_index */
    put_i32(fx.data, 32, 100); /* group: first_id   */
    put_i32(fx.data, 36, 101); /* group: last_id    */

    size_t cursor = str0_off;
    put_i32(fx.data, (size_t)string_offsets_offset + 0, (int32_t)cursor);
    cursor += put_cstr(fx.data, cursor, "Hello");
    put_i32(fx.data, (size_t)string_offsets_offset + 4, (int32_t)cursor);
    cursor += put_cstr(fx.data, cursor, "World");

    fx.size = cursor;
    put_i32(fx.data, 4, (int32_t)fx.size);
    return fx;
}

/*===========================================================================
 * Fixture: v1 (Dark Souls 1) — 3 entries, Shift-JIS, narrow
 *===========================================================================*/

static fixture_t make_fixture_v1_basic(void) {
    fixture_t fx;
    memset(&fx, 0, sizeof(fx));

    const int64_t string_offsets_offset = 40;
    const size_t  strings_start = (size_t)string_offsets_offset + 3 * 4;

    put_u8 (fx.data, 2,  1);  /* version = DS1 */
    put_i32(fx.data, 12, 1);
    put_i32(fx.data, 16, 3);
    put_i32(fx.data, 20, (int32_t)string_offsets_offset);
    put_i32(fx.data, 28, 0);
    put_i32(fx.data, 32, 1000);
    put_i32(fx.data, 36, 1002);

    size_t cursor = strings_start;
    put_i32(fx.data, (size_t)string_offsets_offset + 0, (int32_t)cursor);
    cursor += put_cstr(fx.data, cursor, "Foo");
    put_i32(fx.data, (size_t)string_offsets_offset + 4, (int32_t)cursor);
    cursor += put_cstr(fx.data, cursor, "Bar");
    put_i32(fx.data, (size_t)string_offsets_offset + 8, (int32_t)cursor);
    cursor += put_cstr(fx.data, cursor, "Baz");

    fx.size = cursor;
    put_i32(fx.data, 4, (int32_t)fx.size);
    return fx;
}

/*===========================================================================
 * Fixture: v2 (Dark Souls 3) — 3 entries, UTF-16LE, wide
 *===========================================================================*/

static fixture_t make_fixture_v2_wide(void) {
    fixture_t fx;
    memset(&fx, 0, sizeof(fx));

    const int64_t string_offsets_offset = 56;
    const size_t  strings_start = (size_t)string_offsets_offset + 3 * 8;

    put_u8 (fx.data, 2,  2);   /* version = DS3 */
    put_u8 (fx.data, 8,  1);   /* unicode = true */
    put_i32(fx.data, 12, 1);   /* group_count */
    put_i32(fx.data, 16, 3);   /* string_count */
    put_i32(fx.data, 20, (int32_t)0xFF); /* wide sentinel */
    put_i64(fx.data, 24, string_offsets_offset);
    put_i64(fx.data, 32, 0);

    put_i32(fx.data, 40, 0);    /* offset_index */
    put_i32(fx.data, 44, 5000); /* first_id */
    put_i32(fx.data, 48, 5002); /* last_id  */
    put_i32(fx.data, 52, 0);    /* wide group padding */

    size_t cursor = strings_start;
    /*  Greek letters α (U+03B1), β (U+03B2), γ (U+03B3). */
    static const uint16_t alpha[] = { 0x03B1 };
    static const uint16_t beta [] = { 0x03B2 };
    static const uint16_t gamma[] = { 0x03B3 };

    put_i64(fx.data, (size_t)string_offsets_offset + 0,  (int64_t)cursor);
    cursor += put_utf16le_codepoints(fx.data, cursor, alpha, 1);
    put_i64(fx.data, (size_t)string_offsets_offset + 8,  (int64_t)cursor);
    cursor += put_utf16le_codepoints(fx.data, cursor, beta,  1);
    put_i64(fx.data, (size_t)string_offsets_offset + 16, (int64_t)cursor);
    cursor += put_utf16le_codepoints(fx.data, cursor, gamma, 1);

    fx.size = cursor;
    put_i32(fx.data, 4, (int32_t)fx.size);
    return fx;
}

/*===========================================================================
 * Fixture: v1 with MD5 prefix — 1 entry
 *===========================================================================*/

static fixture_t make_fixture_md5_v1(void) {
    fixture_t fx;
    memset(&fx, 0, sizeof(fx));

    /*  Random non-zero 16-byte prefix: byte 0 must be != 0 to trigger the
     *  detection path. We deliberately do NOT compute an actual MD5 hash —
     *  upstream does not verify it, only detects + skips. */
    static const uint8_t md5_prefix[16] = {
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    };
    memcpy(fx.data, md5_prefix, 16);

    /*  Body coordinates start at buffer offset 16. We fill the body just
     *  like a no-MD5 v1 file; the impl handles the +16 shift. */
    uint8_t *body = fx.data + 16;
    const int64_t string_offsets_offset = 40; /* body coordinates */
    const size_t  strings_start = (size_t)string_offsets_offset + 1 * 4;

    put_u8 (body, 2,  1);
    put_i32(body, 12, 1);
    put_i32(body, 16, 1);
    put_i32(body, 20, (int32_t)string_offsets_offset);
    put_i32(body, 28, 0);
    put_i32(body, 32, 42);
    put_i32(body, 36, 42);

    size_t cursor = strings_start;
    put_i32(body, (size_t)string_offsets_offset + 0, (int32_t)cursor);
    cursor += put_cstr(body, cursor, "MD5_OK");

    size_t body_size = cursor;
    put_i32(body, 4, (int32_t)body_size);
    fx.size = 16 + body_size;
    return fx;
}

/*===========================================================================
 * Fixture: v1 with deleted (offset=0) entry inside a 2-entry group
 *===========================================================================*/

static fixture_t make_fixture_deleted_entry(void) {
    fixture_t fx;
    memset(&fx, 0, sizeof(fx));

    const int64_t string_offsets_offset = 40;
    const size_t  strings_start = (size_t)string_offsets_offset + 2 * 4;

    put_u8 (fx.data, 2,  1);
    put_i32(fx.data, 12, 1);   /* group_count */
    put_i32(fx.data, 16, 2);   /* string_count */
    put_i32(fx.data, 20, (int32_t)string_offsets_offset);
    put_i32(fx.data, 28, 0);
    put_i32(fx.data, 32, 1);
    put_i32(fx.data, 36, 2);

    size_t cursor = strings_start;
    /*  id=1 → "OK", id=2 → tombstone (offset 0). */
    put_i32(fx.data, (size_t)string_offsets_offset + 0, (int32_t)cursor);
    cursor += put_cstr(fx.data, cursor, "OK");
    put_i32(fx.data, (size_t)string_offsets_offset + 4, 0);

    fx.size = cursor;
    put_i32(fx.data, 4, (int32_t)fx.size);
    return fx;
}

/*===========================================================================
 * Tests
 *===========================================================================*/

static void test_v0_demons_souls_basic(void) {
    fixture_t fx = make_fixture_v0_basic();
    sf_fmg_t *fmg = NULL;

    TEST_ASSERT_EQUAL(SF_OK,
        sf_fmg_read_from_memory(&fmg, fx.data, fx.size, NULL));
    TEST_ASSERT_NOT_NULL(fmg);

    TEST_ASSERT_EQUAL(SF_FMG_VERSION_DEMONS_SOULS, sf_fmg_get_version(fmg));
    TEST_ASSERT_FALSE(sf_fmg_is_big_endian(fmg));
    TEST_ASSERT_FALSE(sf_fmg_is_unicode(fmg));
    TEST_ASSERT_FALSE(sf_fmg_has_md5(fmg));
    TEST_ASSERT_EQUAL_size_t(2, sf_fmg_get_entry_count(fmg));

    const sf_fmg_entry_t *e0 = sf_fmg_get_entry(fmg, 0);
    const sf_fmg_entry_t *e1 = sf_fmg_get_entry(fmg, 1);
    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_NOT_NULL(e1);
    TEST_ASSERT_EQUAL_INT32(100, sf_fmg_entry_get_id(e0));
    TEST_ASSERT_EQUAL_STRING("Hello", sf_fmg_entry_get_text(e0));
    TEST_ASSERT_EQUAL_INT32(101, sf_fmg_entry_get_id(e1));
    TEST_ASSERT_EQUAL_STRING("World", sf_fmg_entry_get_text(e1));

    sf_fmg_destroy(fmg);
}

static void test_v1_dark_souls_1_basic(void) {
    fixture_t fx = make_fixture_v1_basic();
    sf_fmg_t *fmg = NULL;

    TEST_ASSERT_EQUAL(SF_OK,
        sf_fmg_read_from_memory(&fmg, fx.data, fx.size, NULL));
    TEST_ASSERT_NOT_NULL(fmg);

    TEST_ASSERT_EQUAL(SF_FMG_VERSION_DARK_SOULS_1, sf_fmg_get_version(fmg));
    TEST_ASSERT_FALSE(sf_fmg_is_unicode(fmg));
    TEST_ASSERT_FALSE(sf_fmg_has_md5(fmg));
    TEST_ASSERT_EQUAL_size_t(3, sf_fmg_get_entry_count(fmg));

    const char *expected_texts[] = { "Foo", "Bar", "Baz" };
    for (size_t i = 0; i < 3; i++) {
        const sf_fmg_entry_t *e = sf_fmg_get_entry(fmg, i);
        TEST_ASSERT_NOT_NULL(e);
        TEST_ASSERT_EQUAL_INT32((int32_t)(1000 + i), sf_fmg_entry_get_id(e));
        TEST_ASSERT_EQUAL_STRING(expected_texts[i], sf_fmg_entry_get_text(e));
    }

    const sf_fmg_entry_t *found = sf_fmg_find_entry_by_id(fmg, 1001);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("Bar", sf_fmg_entry_get_text(found));
    TEST_ASSERT_NULL(sf_fmg_find_entry_by_id(fmg, 9999));

    sf_fmg_destroy(fmg);
}

static void test_v2_wide_utf16(void) {
    fixture_t fx = make_fixture_v2_wide();
    sf_fmg_t *fmg = NULL;

    TEST_ASSERT_EQUAL(SF_OK,
        sf_fmg_read_from_memory(&fmg, fx.data, fx.size, NULL));
    TEST_ASSERT_NOT_NULL(fmg);

    TEST_ASSERT_EQUAL(SF_FMG_VERSION_DARK_SOULS_3, sf_fmg_get_version(fmg));
    TEST_ASSERT_TRUE(sf_fmg_is_unicode(fmg));
    TEST_ASSERT_EQUAL_size_t(3, sf_fmg_get_entry_count(fmg));

    /*  α β γ in UTF-8 = CE B1 / CE B2 / CE B3. */
    const char *expected[] = { "\xCE\xB1", "\xCE\xB2", "\xCE\xB3" };
    for (size_t i = 0; i < 3; i++) {
        const sf_fmg_entry_t *e = sf_fmg_get_entry(fmg, i);
        TEST_ASSERT_NOT_NULL(e);
        TEST_ASSERT_EQUAL_INT32((int32_t)(5000 + i), sf_fmg_entry_get_id(e));
        TEST_ASSERT_EQUAL_STRING(expected[i], sf_fmg_entry_get_text(e));
    }

    sf_fmg_destroy(fmg);
}

static void test_md5_prefix_detected_and_skipped(void) {
    fixture_t fx = make_fixture_md5_v1();
    sf_fmg_t *fmg = NULL;

    TEST_ASSERT_EQUAL(SF_OK,
        sf_fmg_read_from_memory(&fmg, fx.data, fx.size, NULL));
    TEST_ASSERT_NOT_NULL(fmg);

    TEST_ASSERT_TRUE(sf_fmg_has_md5(fmg));
    TEST_ASSERT_EQUAL(SF_FMG_VERSION_DARK_SOULS_1, sf_fmg_get_version(fmg));
    TEST_ASSERT_EQUAL_size_t(1, sf_fmg_get_entry_count(fmg));

    const sf_fmg_entry_t *e = sf_fmg_get_entry(fmg, 0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_INT32(42, sf_fmg_entry_get_id(e));
    TEST_ASSERT_EQUAL_STRING("MD5_OK", sf_fmg_entry_get_text(e));

    sf_fmg_destroy(fmg);
}

static void test_deleted_entry_offset_zero_yields_null_text(void) {
    fixture_t fx = make_fixture_deleted_entry();
    sf_fmg_t *fmg = NULL;

    TEST_ASSERT_EQUAL(SF_OK,
        sf_fmg_read_from_memory(&fmg, fx.data, fx.size, NULL));
    TEST_ASSERT_NOT_NULL(fmg);
    TEST_ASSERT_EQUAL_size_t(2, sf_fmg_get_entry_count(fmg));

    const sf_fmg_entry_t *e0 = sf_fmg_get_entry(fmg, 0);
    const sf_fmg_entry_t *e1 = sf_fmg_get_entry(fmg, 1);
    TEST_ASSERT_EQUAL_INT32(1, sf_fmg_entry_get_id(e0));
    TEST_ASSERT_EQUAL_STRING("OK", sf_fmg_entry_get_text(e0));
    TEST_ASSERT_EQUAL_INT32(2, sf_fmg_entry_get_id(e1));
    TEST_ASSERT_NULL(sf_fmg_entry_get_text(e1));

    sf_fmg_destroy(fmg);
}

static void test_truncated_input_returns_error(void) {
    sf_fmg_t *fmg = NULL;
    uint8_t  one_byte = 0x00;

    sf_result_t r = sf_fmg_read_from_memory(&fmg, &one_byte, 1, NULL);
    TEST_ASSERT_NOT_EQUAL(SF_OK, r);
    TEST_ASSERT_NULL(fmg);
}

static void test_empty_input_returns_error(void) {
    sf_fmg_t *fmg = NULL;
    sf_result_t r = sf_fmg_read_from_memory(&fmg, NULL, 0, NULL);
    TEST_ASSERT_NOT_EQUAL(SF_OK, r);
    TEST_ASSERT_NULL(fmg);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_v0_demons_souls_basic);
    RUN_TEST(test_v1_dark_souls_1_basic);
    RUN_TEST(test_v2_wide_utf16);
    RUN_TEST(test_md5_prefix_detected_and_skipped);
    RUN_TEST(test_deleted_entry_offset_zero_yields_null_text);
    RUN_TEST(test_truncated_input_returns_error);
    RUN_TEST(test_empty_input_returns_error);
    return UNITY_END();
}
