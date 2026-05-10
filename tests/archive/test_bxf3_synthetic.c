/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 3 QA — BXF3 split-archive round-trip + streaming reader synthetic checks.
 *
 * Goals:
 *   1. write_to_memory(read_from_memory(write_to_memory(b))) returns BHD/BDT
 *      pairs that are byte-equal to the first write_to_memory(b). Permutes
 *      BigEndian/BitBigEndian to exercise both endian variants.
 *   2. The streaming reader (sf_bxf3_reader_*) returns the same payload
 *      bytes that were written, and exposes the same per-entry headers
 *      (id, name, flags, size).
 *
 * No DCX wrapper is used; entries are stored uncompressed (no FileFlag.Compressed)
 * so the test is hermetic and does not depend on the Oodle DLL.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bxf3.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

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

static void populate_three_files(sf_bxf3_t *b) {
    sf_binder_file_t f1 = make_file(100, "a.txt", k_payload_a, sizeof k_payload_a);
    sf_binder_file_t f2 = make_file(200, "b.bin", k_payload_b, sizeof k_payload_b);
    sf_binder_file_t f3 = make_file(300, "c.dat", k_payload_c, sizeof k_payload_c);
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf3_add_file(b, &f1));
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf3_add_file(b, &f2));
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf3_add_file(b, &f3));
}

static void roundtrip_assert(const sf_bxf3_t *b1) {
    uint8_t *bhd_first = NULL;
    size_t   bhd_first_size = 0;
    uint8_t *bdt_first = NULL;
    size_t   bdt_first_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf3_write_to_memory(b1,
        &bhd_first, &bhd_first_size, &bdt_first, &bdt_first_size, NULL));

    sf_bxf3_t *b2 = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf3_read_from_memory(&b2,
        bhd_first, bhd_first_size, bdt_first, bdt_first_size, NULL));

    TEST_ASSERT_EQUAL_size_t(sf_bxf3_file_count(b1), sf_bxf3_file_count(b2));
    TEST_ASSERT_EQUAL_HEX8(sf_bxf3_get_format(b1),         sf_bxf3_get_format(b2));
    TEST_ASSERT_EQUAL    (sf_bxf3_get_big_endian(b1),     sf_bxf3_get_big_endian(b2));
    TEST_ASSERT_EQUAL    (sf_bxf3_get_bit_big_endian(b1), sf_bxf3_get_bit_big_endian(b2));
    TEST_ASSERT_EQUAL_STRING(sf_bxf3_get_version(b1), sf_bxf3_get_version(b2));

    for (size_t i = 0; i < sf_bxf3_file_count(b1); i++) {
        const sf_binder_file_t *e1 = sf_bxf3_get_file(b1, i);
        const sf_binder_file_t *e2 = sf_bxf3_get_file(b2, i);
        TEST_ASSERT_NOT_NULL(e1);
        TEST_ASSERT_NOT_NULL(e2);
        TEST_ASSERT_EQUAL_INT32(e1->id, e2->id);
        TEST_ASSERT_EQUAL_STRING(e1->name_utf8 ? e1->name_utf8 : "",
                                 e2->name_utf8 ? e2->name_utf8 : "");
        TEST_ASSERT_EQUAL_size_t(e1->size, e2->size);
        TEST_ASSERT_EQUAL_HEX8(e1->flags, e2->flags);
        TEST_ASSERT_EQUAL_MEMORY(e1->data, e2->data, e1->size);
    }

    uint8_t *bhd_second = NULL;
    size_t   bhd_second_size = 0;
    uint8_t *bdt_second = NULL;
    size_t   bdt_second_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf3_write_to_memory(b2,
        &bhd_second, &bhd_second_size, &bdt_second, &bdt_second_size, NULL));

    TEST_ASSERT_EQUAL_size_t(bhd_first_size, bhd_second_size);
    TEST_ASSERT_EQUAL_MEMORY(bhd_first, bhd_second, bhd_first_size);
    TEST_ASSERT_EQUAL_size_t(bdt_first_size, bdt_second_size);
    TEST_ASSERT_EQUAL_MEMORY(bdt_first, bdt_second, bdt_first_size);

    sf_free(NULL, bhd_first);
    sf_free(NULL, bdt_first);
    sf_free(NULL, bhd_second);
    sf_free(NULL, bdt_second);
    sf_bxf3_destroy(b2);
}

static void test_bxf3_roundtrip_basic(void) {
    sf_bxf3_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf3_create(&b, NULL));
    sf_bxf3_set_big_endian(b, false);
    sf_bxf3_set_bit_big_endian(b, false);
    sf_bxf3_set_version(b, "11A1A0");
    populate_three_files(b);

    roundtrip_assert(b);
    sf_bxf3_destroy(b);
}

static void test_bxf3_big_endian(void) {
    sf_bxf3_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf3_create(&b, NULL));
    sf_bxf3_set_big_endian(b, true);
    sf_bxf3_set_bit_big_endian(b, true);
    sf_bxf3_set_version(b, "11A1A0");
    populate_three_files(b);

    roundtrip_assert(b);
    sf_bxf3_destroy(b);
}

static void test_bxf3_reader_pattern(void) {
    sf_bxf3_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf3_create(&b, NULL));
    sf_bxf3_set_version(b, "11A1A0");
    populate_three_files(b);

    wchar_t tempDir[MAX_PATH];
    DWORD got = GetTempPathW(MAX_PATH, tempDir);
    TEST_ASSERT_TRUE(got > 0 && got < MAX_PATH);

    wchar_t bhdFile[MAX_PATH];
    wchar_t bdtFile[MAX_PATH];
    UINT made_h = GetTempFileNameW(tempDir, L"bhd", 0, bhdFile);
    UINT made_d = GetTempFileNameW(tempDir, L"bdt", 0, bdtFile);
    TEST_ASSERT_TRUE(made_h != 0);
    TEST_ASSERT_TRUE(made_d != 0);

    TEST_ASSERT_EQUAL(SF_OK, sf_bxf3_write_to_paths(b, bhdFile, bdtFile));

    sf_bxf3_reader_t *r = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf3_reader_open(&r, bhdFile, bdtFile, NULL));
    TEST_ASSERT_EQUAL_size_t(3, sf_bxf3_reader_file_count(r));

    static const uint8_t *expected_data[3] = { k_payload_a, k_payload_b, k_payload_c };
    static const size_t   expected_size[3] = {
        sizeof k_payload_a, sizeof k_payload_b, sizeof k_payload_c
    };
    static const int32_t  expected_id[3]   = { 100, 200, 300 };
    static const char    *expected_name[3] = { "a.txt", "b.bin", "c.dat" };

    for (size_t i = 0; i < 3; i++) {
        const sf_binder_file_t *f = sf_bxf3_reader_get_file(r, i);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT32(expected_id[i], f->id);
        TEST_ASSERT_EQUAL_STRING(expected_name[i], f->name_utf8);
        TEST_ASSERT_EQUAL_size_t(expected_size[i], f->size);

        uint8_t *got_buf = NULL;
        size_t   got_n   = 0;
        TEST_ASSERT_EQUAL(SF_OK,
            sf_bxf3_reader_read_file_by_index(r, i, &got_buf, &got_n, NULL));
        TEST_ASSERT_EQUAL_size_t(expected_size[i], got_n);
        TEST_ASSERT_EQUAL_MEMORY(expected_data[i], got_buf, got_n);
        sf_free(NULL, got_buf);
    }

    uint8_t *by_id_buf = NULL;
    size_t   by_id_n   = 0;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_bxf3_reader_read_file_by_id(r, 200, &by_id_buf, &by_id_n, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof k_payload_b, by_id_n);
    TEST_ASSERT_EQUAL_MEMORY(k_payload_b, by_id_buf, by_id_n);
    sf_free(NULL, by_id_buf);

    uint8_t *missing_buf = NULL;
    size_t   missing_n   = 0;
    TEST_ASSERT_EQUAL(SF_ERR_NOT_FOUND,
        sf_bxf3_reader_read_file_by_id(r, 9999, &missing_buf, &missing_n, NULL));

    sf_bxf3_reader_close(r);
    sf_bxf3_destroy(b);

    DeleteFileW(bhdFile);
    DeleteFileW(bdtFile);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bxf3_roundtrip_basic);
    RUN_TEST(test_bxf3_big_endian);
    RUN_TEST(test_bxf3_reader_pattern);
    return UNITY_END();
}
