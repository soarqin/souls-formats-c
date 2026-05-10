/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 3 QA — BXF4 split-archive round-trip + streaming reader synthetic checks.
 */

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bxf4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_hash.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned int UINT;

__declspec(dllimport) DWORD GetTempPathW(DWORD nBufferLength, wchar_t *lpBuffer);
__declspec(dllimport) DWORD GetCurrentProcessId(void);
__declspec(dllimport) BOOL  DeleteFileW(const wchar_t *lpFileName);
__declspec(dllimport) UINT  GetTempFileNameW(const wchar_t *lpPathName,
                                             const wchar_t *lpPrefixString,
                                             UINT uUnique, wchar_t *lpTempFileName);

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

static void populate_three_files(sf_bxf4_t *b) {
    sf_binder_file_t f1 = make_file(100, "a.txt", k_payload_a, sizeof k_payload_a);
    sf_binder_file_t f2 = make_file(200, "b.bin", k_payload_b, sizeof k_payload_b);
    sf_binder_file_t f3 = make_file(300, "c.dat", k_payload_c, sizeof k_payload_c);
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf4_add_file(b, &f1));
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf4_add_file(b, &f2));
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf4_add_file(b, &f3));
}

static void roundtrip_assert(const sf_bxf4_t *b1) {
    uint8_t *bhd_first = NULL;
    size_t   bhd_first_size = 0;
    uint8_t *bdt_first = NULL;
    size_t   bdt_first_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf4_write_to_memory(b1,
        &bhd_first, &bhd_first_size, &bdt_first, &bdt_first_size, NULL));

    sf_bxf4_t *b2 = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf4_read_from_memory(&b2,
        bhd_first, bhd_first_size, bdt_first, bdt_first_size, NULL));

    TEST_ASSERT_EQUAL_size_t(sf_bxf4_file_count(b1), sf_bxf4_file_count(b2));
    TEST_ASSERT_EQUAL_HEX8(sf_bxf4_get_format(b1),         sf_bxf4_get_format(b2));
    TEST_ASSERT_EQUAL    (sf_bxf4_get_big_endian(b1),     sf_bxf4_get_big_endian(b2));
    TEST_ASSERT_EQUAL    (sf_bxf4_get_bit_big_endian(b1), sf_bxf4_get_bit_big_endian(b2));
    TEST_ASSERT_EQUAL    (sf_bxf4_get_unicode(b1),        sf_bxf4_get_unicode(b2));
    TEST_ASSERT_EQUAL_HEX8(sf_bxf4_get_extended(b1),       sf_bxf4_get_extended(b2));
    TEST_ASSERT_EQUAL    (sf_bxf4_get_unk04(b1),          sf_bxf4_get_unk04(b2));
    TEST_ASSERT_EQUAL    (sf_bxf4_get_unk05(b1),          sf_bxf4_get_unk05(b2));
    TEST_ASSERT_EQUAL_STRING(sf_bxf4_get_version(b1), sf_bxf4_get_version(b2));

    for (size_t i = 0; i < sf_bxf4_file_count(b1); i++) {
        const sf_binder_file_t *e1 = sf_bxf4_get_file(b1, i);
        const sf_binder_file_t *e2 = sf_bxf4_get_file(b2, i);
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
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf4_write_to_memory(b2,
        &bhd_second, &bhd_second_size, &bdt_second, &bdt_second_size, NULL));

    TEST_ASSERT_EQUAL_size_t(bhd_first_size, bhd_second_size);
    TEST_ASSERT_EQUAL_MEMORY(bhd_first, bhd_second, bhd_first_size);
    TEST_ASSERT_EQUAL_size_t(bdt_first_size, bdt_second_size);
    TEST_ASSERT_EQUAL_MEMORY(bdt_first, bdt_second, bdt_first_size);

    sf_free(NULL, bhd_first);
    sf_free(NULL, bdt_first);
    sf_free(NULL, bhd_second);
    sf_free(NULL, bdt_second);
    sf_bxf4_destroy(b2);
}

static void test_bxf4_default(void) {
    sf_bxf4_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf4_create(&b, NULL));
    sf_bxf4_set_version(b, "11A1A0");
    populate_three_files(b);
    TEST_ASSERT_EQUAL_HEX8(4, sf_bxf4_get_extended(b));
    roundtrip_assert(b);
    sf_bxf4_destroy(b);
}

static void test_bxf4_unicode_names(void) {
    static const uint8_t payload[] = { 1, 2, 3, 4 };
    sf_bxf4_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf4_create(&b, NULL));
    sf_bxf4_set_version(b, "11A1A0");
    sf_bxf4_set_unicode(b, true);
    sf_binder_file_t f = make_file(20,
        "test_\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E.dds",
        payload, sizeof payload);
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf4_add_file(b, &f));
    roundtrip_assert(b);
    sf_bxf4_destroy(b);
}

static void test_bxf4_no_hashtable(void) {
    sf_bxf4_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf4_create(&b, NULL));
    sf_bxf4_set_version(b, "11A1A0");
    sf_bxf4_set_extended(b, 0);
    populate_three_files(b);
    TEST_ASSERT_EQUAL_HEX8(0, sf_bxf4_get_extended(b));
    roundtrip_assert(b);
    sf_bxf4_destroy(b);
}

static void test_bxf4_reader_pattern(void) {
    sf_bxf4_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf4_create(&b, NULL));
    sf_bxf4_set_version(b, "11A1A0");
    populate_three_files(b);

    wchar_t tempDir[MAX_PATH];
    DWORD got = GetTempPathW(MAX_PATH, tempDir);
    TEST_ASSERT_TRUE(got > 0 && got < MAX_PATH);

    wchar_t bhdFile[MAX_PATH];
    wchar_t bdtFile[MAX_PATH];
    UINT made_h = GetTempFileNameW(tempDir, L"bh4", 0, bhdFile);
    UINT made_d = GetTempFileNameW(tempDir, L"bd4", 0, bdtFile);
    TEST_ASSERT_TRUE(made_h != 0);
    TEST_ASSERT_TRUE(made_d != 0);

    TEST_ASSERT_EQUAL(SF_OK, sf_bxf4_write_to_paths(b, bhdFile, bdtFile));

    sf_bxf4_reader_t *r = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bxf4_reader_open(&r, bhdFile, bdtFile, NULL));
    TEST_ASSERT_EQUAL_size_t(3, sf_bxf4_reader_file_count(r));

    static const uint8_t *expected_data[3] = { k_payload_a, k_payload_b, k_payload_c };
    static const size_t   expected_size[3] = {
        sizeof k_payload_a, sizeof k_payload_b, sizeof k_payload_c
    };
    static const int32_t  expected_id[3]   = { 100, 200, 300 };
    static const char    *expected_name[3] = { "a.txt", "b.bin", "c.dat" };

    for (size_t i = 0; i < 3; i++) {
        const sf_binder_file_t *f = sf_bxf4_reader_get_file(r, i);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT32(expected_id[i], f->id);
        TEST_ASSERT_EQUAL_STRING(expected_name[i], f->name_utf8);
        TEST_ASSERT_EQUAL_size_t(expected_size[i], f->size);

        uint8_t *got_buf = NULL;
        size_t   got_n   = 0;
        TEST_ASSERT_EQUAL(SF_OK,
            sf_bxf4_reader_read_file_by_index(r, i, &got_buf, &got_n, NULL));
        TEST_ASSERT_EQUAL_size_t(expected_size[i], got_n);
        TEST_ASSERT_EQUAL_MEMORY(expected_data[i], got_buf, got_n);
        sf_free(NULL, got_buf);
    }

    uint8_t *by_hash_buf = NULL;
    size_t   by_hash_n   = 0;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_bxf4_reader_read_file_by_path_hash(r, sf_path_hash("b.bin"),
                                              &by_hash_buf, &by_hash_n, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof k_payload_b, by_hash_n);
    TEST_ASSERT_EQUAL_MEMORY(k_payload_b, by_hash_buf, by_hash_n);
    sf_free(NULL, by_hash_buf);

    uint8_t *missing_buf = NULL;
    size_t   missing_n   = 0;
    TEST_ASSERT_EQUAL(SF_ERR_NOT_FOUND,
        sf_bxf4_reader_read_file_by_id(r, 9999, &missing_buf, &missing_n, NULL));

    sf_bxf4_reader_close(r);
    sf_bxf4_destroy(b);

    DeleteFileW(bhdFile);
    DeleteFileW(bdtFile);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bxf4_default);
    RUN_TEST(test_bxf4_unicode_names);
    RUN_TEST(test_bxf4_no_hashtable);
    RUN_TEST(test_bxf4_reader_pattern);
    return UNITY_END();
}
