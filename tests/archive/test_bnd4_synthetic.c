/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 3 QA — BND4 synthetic round-trip + streaming reader checks.
 */

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd4.h"
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

__declspec(dllimport) DWORD GetTempPathW(DWORD nBufferLength, wchar_t *lpBuffer);
__declspec(dllimport) DWORD GetCurrentProcessId(void);
__declspec(dllimport) BOOL DeleteFileW(const wchar_t *lpFileName);

void setUp(void) {}
void tearDown(void) {}

static const uint8_t k_payload_a[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
static const uint8_t k_payload_b[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11 };
static const uint8_t k_payload_c[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x42 };

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
    f.id = id;
    f.name_utf8 = name;
    f.data = data;
    f.size = size;
    f.flags = SF_BINDER_FILE_FLAG_FLAG1;
    f.compression_info = zlib_info();
    return f;
}

static void populate_three_files(sf_bnd4_t *b) {
    sf_binder_file_t f1 = make_file(100, "a.txt", k_payload_a, sizeof k_payload_a);
    sf_binder_file_t f2 = make_file(200, "b.bin", k_payload_b, sizeof k_payload_b);
    sf_binder_file_t f3 = make_file(300, "c.dat", k_payload_c, sizeof k_payload_c);
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_add_file(b, &f1));
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_add_file(b, &f2));
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_add_file(b, &f3));
}

static void roundtrip_assert(const sf_bnd4_t *b1) {
    uint8_t *bytes_first = NULL;
    size_t size_first = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_write_to_memory(b1, &bytes_first, &size_first, NULL));

    sf_bnd4_t *b2 = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_read_from_memory(&b2, bytes_first, size_first, NULL));

    TEST_ASSERT_EQUAL_size_t(sf_bnd4_file_count(b1), sf_bnd4_file_count(b2));
    TEST_ASSERT_EQUAL_HEX8(sf_bnd4_get_format(b1), sf_bnd4_get_format(b2));
    TEST_ASSERT_EQUAL(sf_bnd4_get_big_endian(b1), sf_bnd4_get_big_endian(b2));
    TEST_ASSERT_EQUAL(sf_bnd4_get_bit_big_endian(b1), sf_bnd4_get_bit_big_endian(b2));
    TEST_ASSERT_EQUAL(sf_bnd4_get_unicode(b1), sf_bnd4_get_unicode(b2));
    TEST_ASSERT_EQUAL_HEX8(sf_bnd4_get_extended(b1), sf_bnd4_get_extended(b2));
    TEST_ASSERT_EQUAL(sf_bnd4_get_unk04(b1), sf_bnd4_get_unk04(b2));
    TEST_ASSERT_EQUAL(sf_bnd4_get_unk05(b1), sf_bnd4_get_unk05(b2));
    TEST_ASSERT_EQUAL_STRING(sf_bnd4_get_version(b1), sf_bnd4_get_version(b2));

    for (size_t i = 0; i < sf_bnd4_file_count(b1); i++) {
        const sf_binder_file_t *e1 = sf_bnd4_get_file(b1, i);
        const sf_binder_file_t *e2 = sf_bnd4_get_file(b2, i);
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
    size_t size_second = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_write_to_memory(b2, &bytes_second, &size_second, NULL));
    TEST_ASSERT_EQUAL_size_t(size_first, size_second);
    TEST_ASSERT_EQUAL_MEMORY(bytes_first, bytes_second, size_first);

    sf_free(NULL, bytes_first);
    sf_free(NULL, bytes_second);
    sf_bnd4_destroy(b2);
}

static void test_bnd4_names1_pcsave(void) {
    sf_bnd4_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_create(&b, NULL));
    sf_bnd4_set_version(b, "11A1A0");
    sf_bnd4_set_format(b, SF_BINDER_FORMAT_NAMES1);
    sf_bnd4_set_unicode(b, false);
    sf_bnd4_set_extended(b, 0);
    populate_three_files(b);
    roundtrip_assert(b);
    sf_bnd4_destroy(b);
}

static void test_bnd4_names2_sekiro(void) {
    sf_bnd4_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_create(&b, NULL));
    sf_bnd4_set_version(b, "11A1A0");
    sf_bnd4_set_format(b, (sf_binder_format_t)(SF_BINDER_FORMAT_IDS | SF_BINDER_FORMAT_NAMES2));
    sf_bnd4_set_extended(b, 1);
    populate_three_files(b);
    roundtrip_assert(b);
    sf_bnd4_destroy(b);
}

static void test_bnd4_ds3_default(void) {
    sf_bnd4_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_create(&b, NULL));
    sf_bnd4_set_version(b, "11A1A0");
    populate_three_files(b);
    roundtrip_assert(b);
    sf_bnd4_destroy(b);
}

static void test_bnd4_shiftjis_names(void) {
    static const uint8_t payload[] = { 1, 2, 3 };
    sf_bnd4_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_create(&b, NULL));
    sf_bnd4_set_version(b, "11A1A0");
    sf_bnd4_set_unicode(b, false);
    sf_bnd4_set_extended(b, 0);
    sf_binder_file_t f = make_file(10, "abc_123.bin", payload, sizeof payload);
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_add_file(b, &f));
    roundtrip_assert(b);
    sf_bnd4_destroy(b);
}

static void test_bnd4_unicode_names(void) {
    static const uint8_t payload[] = { 4, 5, 6, 7 };
    sf_bnd4_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_create(&b, NULL));
    sf_bnd4_set_version(b, "11A1A0");
    sf_bnd4_set_unicode(b, true);
    sf_bnd4_set_extended(b, 0);
    sf_binder_file_t f = make_file(20, "\xE6\x97\xA5\xE6\x9C\xAC.bin", payload,
                                   sizeof payload);
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_add_file(b, &f));
    roundtrip_assert(b);
    sf_bnd4_destroy(b);
}

static void test_bnd4_extended_variants(void) {
    sf_bnd4_t *b0 = NULL;
    sf_bnd4_t *b4 = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_create(&b0, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_create(&b4, NULL));
    sf_bnd4_set_version(b0, "11A1A0");
    sf_bnd4_set_version(b4, "11A1A0");
    sf_bnd4_set_extended(b0, 0);
    sf_bnd4_set_extended(b4, 4);
    populate_three_files(b0);
    populate_three_files(b4);
    roundtrip_assert(b0);
    roundtrip_assert(b4);
    sf_bnd4_destroy(b0);
    sf_bnd4_destroy(b4);
}

static void test_bnd4_unk04_unk05(void) {
    sf_bnd4_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_create(&b, NULL));
    sf_bnd4_set_version(b, "11A1A0");
    sf_bnd4_set_unk04(b, true);
    sf_bnd4_set_unk05(b, true);
    populate_three_files(b);
    roundtrip_assert(b);
    sf_bnd4_destroy(b);
}

static void test_bnd4_reader_pattern(void) {
    sf_bnd4_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_create(&b, NULL));
    sf_bnd4_set_version(b, "11A1A0");
    populate_three_files(b);

    wchar_t tmpdir[MAX_PATH];
    DWORD got = GetTempPathW(MAX_PATH, tmpdir);
    TEST_ASSERT_TRUE(got > 0 && got < MAX_PATH);

    wchar_t tmppath[MAX_PATH];
    int wrote = swprintf(tmppath, MAX_PATH, L"%stemp_bnd4_%d.bnd", tmpdir,
                         (int)GetCurrentProcessId());
    TEST_ASSERT_TRUE(wrote > 0 && wrote < MAX_PATH);

    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_write_to_path(b, tmppath));

    sf_bnd4_reader_t *r = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd4_reader_open(&r, tmppath, NULL));
    TEST_ASSERT_EQUAL_size_t(3, sf_bnd4_reader_file_count(r));
    TEST_ASSERT_EQUAL(SF_DCX_TYPE_NONE, sf_bnd4_reader_get_outer_compression(r).type);

    static const uint8_t *expected_data[3] = { k_payload_a, k_payload_b, k_payload_c };
    static const size_t expected_size[3] = {
        sizeof k_payload_a, sizeof k_payload_b, sizeof k_payload_c
    };
    static const int32_t expected_id[3] = { 100, 200, 300 };
    static const char *expected_name[3] = { "a.txt", "b.bin", "c.dat" };

    for (size_t i = 0; i < 3; i++) {
        const sf_binder_file_t *f = sf_bnd4_reader_get_file(r, i);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT32(expected_id[i], f->id);
        TEST_ASSERT_EQUAL_STRING(expected_name[i], f->name_utf8);
        TEST_ASSERT_EQUAL_size_t(expected_size[i], f->size);

        uint8_t *got_buf = NULL;
        size_t got_n = 0;
        TEST_ASSERT_EQUAL(SF_OK,
            sf_bnd4_reader_read_file_by_index(r, i, &got_buf, &got_n, NULL));
        TEST_ASSERT_EQUAL_size_t(expected_size[i], got_n);
        TEST_ASSERT_EQUAL_MEMORY(expected_data[i], got_buf, got_n);
        sf_free(NULL, got_buf);
    }

    uint8_t *by_id_buf = NULL;
    size_t by_id_n = 0;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_bnd4_reader_read_file_by_id(r, 200, &by_id_buf, &by_id_n, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof k_payload_b, by_id_n);
    TEST_ASSERT_EQUAL_MEMORY(k_payload_b, by_id_buf, by_id_n);
    sf_free(NULL, by_id_buf);

    uint8_t *by_hash_buf = NULL;
    size_t by_hash_n = 0;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_bnd4_reader_read_file_by_path_hash(r, sf_path_hash("b.bin"),
                                              &by_hash_buf, &by_hash_n, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof k_payload_b, by_hash_n);
    TEST_ASSERT_EQUAL_MEMORY(k_payload_b, by_hash_buf, by_hash_n);
    sf_free(NULL, by_hash_buf);

    sf_bnd4_reader_close(r);
    sf_bnd4_destroy(b);
    DeleteFileW(tmppath);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bnd4_names1_pcsave);
    RUN_TEST(test_bnd4_names2_sekiro);
    RUN_TEST(test_bnd4_ds3_default);
    RUN_TEST(test_bnd4_shiftjis_names);
    RUN_TEST(test_bnd4_unicode_names);
    RUN_TEST(test_bnd4_extended_variants);
    RUN_TEST(test_bnd4_unk04_unk05);
    RUN_TEST(test_bnd4_reader_pattern);
    return UNITY_END();
}
