/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 3 QA — BND3 round-trip + streaming reader synthetic checks.
 *
 * Goals:
 *   1. write_to_memory(read_from_memory(write_to_memory(b))) is byte-equal
 *      to the first write_to_memory(b). Permutes Unk18, BigEndian/BitBigEndian,
 *      and WriteFileHeadersEnd to exercise every bit of the on-disk header.
 *   2. The streaming reader (sf_bnd3_reader_*) returns the same payload
 *      bytes that were written, and exposes the same per-entry headers
 *      (id, name, flags, size).
 *
 * No DCX wrapper is used; entries are stored uncompressed (no FileFlag.Compressed)
 * so the test is hermetic and does not depend on the Oodle DLL.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd3.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*===========================================================================
 * Synthetic file factory
 *===========================================================================*/

static const uint8_t k_payload_a[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80 };
static const uint8_t k_payload_b[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11,
                                       0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 };
static const uint8_t k_payload_c[] = { 0xDE, 0xAD, 0xBE, 0xEF };

static sf_dcx_compression_info_t zlib_info(void) {
    sf_dcx_compression_info_t i;
    memset(&i, 0, sizeof i);
    i.type = SF_DCX_TYPE_ZLIB;
    return i;
}

static sf_binder_file_t make_file(int32_t id, const char *name,
                                  const uint8_t *data, size_t size) {
    sf_binder_file_t f;
    memset(&f, 0, sizeof f);
    f.id               = id;
    f.name_utf8        = name;
    f.data             = data;
    f.size             = size;
    f.flags            = SF_BINDER_FILE_FLAG_FLAG1;
    f.compression_info = zlib_info();
    return f;
}

static void populate_three_files(sf_bnd3_t *b) {
    sf_binder_file_t f1 = make_file(100, "a.txt", k_payload_a, sizeof k_payload_a);
    sf_binder_file_t f2 = make_file(200, "b.bin", k_payload_b, sizeof k_payload_b);
    sf_binder_file_t f3 = make_file(300, "c.dat", k_payload_c, sizeof k_payload_c);
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd3_add_file(b, &f1));
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd3_add_file(b, &f2));
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd3_add_file(b, &f3));
}

static void roundtrip_assert(const sf_bnd3_t *b1) {
    uint8_t *bytes_first = NULL;
    size_t   size_first  = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd3_write_to_memory(b1, &bytes_first, &size_first, NULL));

    sf_bnd3_t *b2 = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd3_read_from_memory(&b2, bytes_first, size_first, NULL));

    TEST_ASSERT_EQUAL_size_t(sf_bnd3_file_count(b1), sf_bnd3_file_count(b2));
    TEST_ASSERT_EQUAL_HEX8(sf_bnd3_get_format(b1),         sf_bnd3_get_format(b2));
    TEST_ASSERT_EQUAL    (sf_bnd3_get_big_endian(b1),     sf_bnd3_get_big_endian(b2));
    TEST_ASSERT_EQUAL    (sf_bnd3_get_bit_big_endian(b1), sf_bnd3_get_bit_big_endian(b2));
    TEST_ASSERT_EQUAL_INT32(sf_bnd3_get_unk18(b1),        sf_bnd3_get_unk18(b2));
    TEST_ASSERT_EQUAL    (sf_bnd3_get_write_file_headers_end(b1),
                          sf_bnd3_get_write_file_headers_end(b2));
    TEST_ASSERT_EQUAL_STRING(sf_bnd3_get_version(b1), sf_bnd3_get_version(b2));

    for (size_t i = 0; i < sf_bnd3_file_count(b1); i++) {
        const sf_binder_file_t *e1 = sf_bnd3_get_file(b1, i);
        const sf_binder_file_t *e2 = sf_bnd3_get_file(b2, i);
        TEST_ASSERT_NOT_NULL(e1);
        TEST_ASSERT_NOT_NULL(e2);
        TEST_ASSERT_EQUAL_INT32(e1->id, e2->id);
        TEST_ASSERT_EQUAL_STRING(e1->name_utf8 ? e1->name_utf8 : "",
                                 e2->name_utf8 ? e2->name_utf8 : "");
        TEST_ASSERT_EQUAL_size_t(e1->size, e2->size);
        TEST_ASSERT_EQUAL_HEX8(e1->flags, e2->flags);
        TEST_ASSERT_EQUAL_MEMORY(e1->data, e2->data, e1->size);
    }

    uint8_t *bytes_second = NULL;
    size_t   size_second  = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd3_write_to_memory(b2, &bytes_second, &size_second, NULL));

    TEST_ASSERT_EQUAL_size_t(size_first, size_second);
    TEST_ASSERT_EQUAL_MEMORY(bytes_first, bytes_second, size_first);

    sf_free(NULL, bytes_first);
    sf_free(NULL, bytes_second);
    sf_bnd3_destroy(b2);
}

/*===========================================================================
 * Test 1 — basic round-trip with default settings
 *===========================================================================*/

static void test_bnd3_roundtrip_basic(void) {
    sf_bnd3_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd3_create(&b, NULL));
    sf_bnd3_set_unk18(b, 0);
    sf_bnd3_set_write_file_headers_end(b, true);
    sf_bnd3_set_big_endian(b, false);
    sf_bnd3_set_bit_big_endian(b, false);
    sf_bnd3_set_version(b, "11A1A0");
    populate_three_files(b);

    roundtrip_assert(b);
    sf_bnd3_destroy(b);
}

/*===========================================================================
 * Test 2 — WriteFileHeadersEnd = false
 *===========================================================================*/

static void test_bnd3_write_file_headers_end_false(void) {
    sf_bnd3_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd3_create(&b, NULL));
    sf_bnd3_set_write_file_headers_end(b, false);
    sf_bnd3_set_version(b, "11A1A0");
    populate_three_files(b);

    roundtrip_assert(b);
    sf_bnd3_destroy(b);
}

/*===========================================================================
 * Test 3 — Unk18 = 0x80000000 (the DeS variant)
 *===========================================================================*/

static void test_bnd3_unk18_variant(void) {
    sf_bnd3_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd3_create(&b, NULL));
    sf_bnd3_set_unk18(b, INT32_MIN);
    sf_bnd3_set_version(b, "11A1A0");
    populate_three_files(b);

    roundtrip_assert(b);
    sf_bnd3_destroy(b);
}

/*===========================================================================
 * Test 4 — BigEndian + BitBigEndian
 *===========================================================================*/

static void test_bnd3_big_endian(void) {
    sf_bnd3_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd3_create(&b, NULL));
    sf_bnd3_set_big_endian(b, true);
    sf_bnd3_set_bit_big_endian(b, true);
    sf_bnd3_set_version(b, "11A1A0");
    populate_three_files(b);

    roundtrip_assert(b);
    sf_bnd3_destroy(b);
}

/*===========================================================================
 * Test 5 — sf_bnd3_reader_t streaming API
 *
 * Round-trips through the wide-path API: writes a BND3 to a temp file,
 * opens it via sf_bnd3_reader_open, materialises every entry by index
 * and verifies content matches. Also re-reads by ID to exercise that
 * code path.
 *===========================================================================*/

static void test_bnd3_reader_pattern(void) {
    sf_bnd3_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd3_create(&b, NULL));
    sf_bnd3_set_version(b, "11A1A0");
    populate_three_files(b);

    wchar_t tempDir[MAX_PATH];
    DWORD got = GetTempPathW(MAX_PATH, tempDir);
    TEST_ASSERT_TRUE(got > 0 && got < MAX_PATH);

    wchar_t tempFile[MAX_PATH];
    UINT made = GetTempFileNameW(tempDir, L"bnd", 0, tempFile);
    TEST_ASSERT_TRUE(made != 0);

    TEST_ASSERT_EQUAL(SF_OK, sf_bnd3_write_to_path(b, tempFile));

    sf_bnd3_reader_t *r = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd3_reader_open(&r, tempFile, NULL));
    TEST_ASSERT_EQUAL_size_t(3, sf_bnd3_reader_file_count(r));

    /* By-index read */
    static const uint8_t *expected_data[3] = { k_payload_a, k_payload_b, k_payload_c };
    static const size_t   expected_size[3] = {
        sizeof k_payload_a, sizeof k_payload_b, sizeof k_payload_c
    };
    static const int32_t  expected_id[3]   = { 100, 200, 300 };
    static const char    *expected_name[3] = { "a.txt", "b.bin", "c.dat" };

    for (size_t i = 0; i < 3; i++) {
        const sf_binder_file_t *f = sf_bnd3_reader_get_file(r, i);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT32(expected_id[i], f->id);
        TEST_ASSERT_EQUAL_STRING(expected_name[i], f->name_utf8);
        TEST_ASSERT_EQUAL_size_t(expected_size[i], f->size);

        uint8_t *got_buf = NULL;
        size_t   got_n   = 0;
        TEST_ASSERT_EQUAL(SF_OK,
            sf_bnd3_reader_read_file_by_index(r, i, &got_buf, &got_n, NULL));
        TEST_ASSERT_EQUAL_size_t(expected_size[i], got_n);
        TEST_ASSERT_EQUAL_MEMORY(expected_data[i], got_buf, got_n);
        sf_free(NULL, got_buf);
    }

    /* By-ID read */
    uint8_t *by_id_buf = NULL;
    size_t   by_id_n   = 0;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_bnd3_reader_read_file_by_id(r, 200, &by_id_buf, &by_id_n, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof k_payload_b, by_id_n);
    TEST_ASSERT_EQUAL_MEMORY(k_payload_b, by_id_buf, by_id_n);
    sf_free(NULL, by_id_buf);

    /* Missing ID */
    uint8_t *missing_buf = NULL;
    size_t   missing_n   = 0;
    TEST_ASSERT_EQUAL(SF_ERR_NOT_FOUND,
        sf_bnd3_reader_read_file_by_id(r, 9999, &missing_buf, &missing_n, NULL));

    sf_bnd3_reader_close(r);
    sf_bnd3_destroy(b);

    DeleteFileW(tempFile);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bnd3_roundtrip_basic);
    RUN_TEST(test_bnd3_write_file_headers_end_false);
    RUN_TEST(test_bnd3_unk18_variant);
    RUN_TEST(test_bnd3_big_endian);
    RUN_TEST(test_bnd3_reader_pattern);
    return UNITY_END();
}
