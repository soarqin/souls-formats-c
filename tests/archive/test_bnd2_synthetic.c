/* SPDX-License-Identifier: GPL-3.0-or-later */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "souls_formats/sf_bnd2.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const uint8_t k_payload_a[] = { 0x10, 0x20, 0x30, 0x40, 0x50 };
static const uint8_t k_payload_b[] = { 0xAA, 0xBB, 0xCC, 0xDD };
static const uint8_t k_payload_c[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x11 };

static sf_bnd2_file_t make_file(int32_t id, const char *name,
                                const uint8_t *data, size_t size) {
    sf_bnd2_file_t f;
    memset(&f, 0, sizeof f);
    f.id = id;
    f.name_utf8 = name;
    f.data = data;
    f.size = size;
    return f;
}

static void populate_three_files(sf_bnd2_t *b) {
    sf_bnd2_file_t f1 = make_file(100, "a.txt", k_payload_a, sizeof k_payload_a);
    sf_bnd2_file_t f2 = make_file(200, "dir/b.bin", k_payload_b, sizeof k_payload_b);
    sf_bnd2_file_t f3 = make_file(300, "c.dat", k_payload_c, sizeof k_payload_c);
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_add_file(b, &f1));
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_add_file(b, &f2));
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_add_file(b, &f3));
}

static void assert_bnd2_roundtrip(const sf_bnd2_t *b1) {
    uint8_t *bytes_first = NULL;
    size_t size_first = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_write_to_memory(b1, &bytes_first, &size_first, NULL));
    TEST_ASSERT_TRUE(sf_bnd2_is_format(bytes_first, size_first));

    sf_bnd2_t *b2 = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_read_from_memory(&b2, bytes_first, size_first, NULL));
    TEST_ASSERT_EQUAL_HEX8(sf_bnd2_get_header_info_flags(b1), sf_bnd2_get_header_info_flags(b2));
    TEST_ASSERT_EQUAL_HEX8(sf_bnd2_get_file_info_flags(b1), sf_bnd2_get_file_info_flags(b2));
    TEST_ASSERT_EQUAL_UINT8(sf_bnd2_get_unk06(b1), sf_bnd2_get_unk06(b2));
    TEST_ASSERT_EQUAL_UINT8(sf_bnd2_get_unk07(b1), sf_bnd2_get_unk07(b2));
    TEST_ASSERT_EQUAL_INT32(sf_bnd2_get_file_version(b1), sf_bnd2_get_file_version(b2));
    TEST_ASSERT_EQUAL_HEX16(sf_bnd2_get_alignment_size(b1), sf_bnd2_get_alignment_size(b2));
    TEST_ASSERT_EQUAL_UINT8(sf_bnd2_get_file_path_mode(b1), sf_bnd2_get_file_path_mode(b2));
    TEST_ASSERT_EQUAL_UINT8(sf_bnd2_get_unk1b(b1), sf_bnd2_get_unk1b(b2));
    TEST_ASSERT_EQUAL_STRING(sf_bnd2_get_base_directory(b1), sf_bnd2_get_base_directory(b2));
    TEST_ASSERT_EQUAL_size_t(sf_bnd2_file_count(b1), sf_bnd2_file_count(b2));

    for (size_t i = 0; i < sf_bnd2_file_count(b1); i++) {
        const sf_bnd2_file_t *e1 = sf_bnd2_get_file(b1, i);
        const sf_bnd2_file_t *e2 = sf_bnd2_get_file(b2, i);
        TEST_ASSERT_NOT_NULL(e1);
        TEST_ASSERT_NOT_NULL(e2);
        TEST_ASSERT_EQUAL_INT32(e1->id, e2->id);
        if (sf_bnd2_get_file_path_mode(b1) == SF_BND2_FILE_PATH_MODE_NAMELESS) {
            char expected[16];
            wsprintfA(expected, "%d", e1->id);
            TEST_ASSERT_EQUAL_STRING(expected, e2->name_utf8);
        } else if (sf_bnd2_get_file_path_mode(b1) == SF_BND2_FILE_PATH_MODE_FULL_PATH &&
                   !(e1->name_utf8 && strlen(e1->name_utf8) > 1 && e1->name_utf8[1] == ':')) {
            char expected[64];
            wsprintfA(expected, "K:\\%s", e1->name_utf8);
            TEST_ASSERT_EQUAL_STRING(expected, e2->name_utf8);
        } else {
            TEST_ASSERT_EQUAL_STRING(e1->name_utf8, e2->name_utf8);
        }
        TEST_ASSERT_EQUAL_size_t(e1->size, e2->size);
        TEST_ASSERT_EQUAL_MEMORY(e1->data, e2->data, e1->size);
    }

    uint8_t *bytes_second = NULL;
    size_t size_second = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_write_to_memory(b2, &bytes_second, &size_second, NULL));
    TEST_ASSERT_EQUAL_size_t(size_first, size_second);
    TEST_ASSERT_EQUAL_MEMORY(bytes_first, bytes_second, size_first);

    sf_free(NULL, bytes_first);
    sf_free(NULL, bytes_second);
    sf_bnd2_destroy(b2);
}

static void configure_common(sf_bnd2_t *b, sf_bnd2_file_path_mode_t mode) {
    sf_bnd2_set_alignment_size(b, 16);
    sf_bnd2_set_file_path_mode(b, mode);
    sf_bnd2_set_file_version(b, 211);
}

static void test_bnd2_roundtrip_filename_mode(void) {
    sf_bnd2_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_create(&b, NULL));
    configure_common(b, SF_BND2_FILE_PATH_MODE_FILE_NAME);
    populate_three_files(b);
    assert_bnd2_roundtrip(b);
    sf_bnd2_destroy(b);
}

static void test_bnd2_roundtrip_nameless_mode(void) {
    sf_bnd2_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_create(&b, NULL));
    configure_common(b, SF_BND2_FILE_PATH_MODE_NAMELESS);
    populate_three_files(b);
    assert_bnd2_roundtrip(b);
    sf_bnd2_destroy(b);
}

static void test_bnd2_roundtrip_base_directory_mode(void) {
    sf_bnd2_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_create(&b, NULL));
    configure_common(b, SF_BND2_FILE_PATH_MODE_BASE_DIRECTORY);
    sf_bnd2_set_base_directory(b, "N:\\base");
    populate_three_files(b);
    assert_bnd2_roundtrip(b);
    sf_bnd2_destroy(b);
}

static void test_bnd2_reader_pattern(void) {
    sf_bnd2_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_create(&b, NULL));
    configure_common(b, SF_BND2_FILE_PATH_MODE_FILE_NAME);
    populate_three_files(b);

    wchar_t temp_dir[MAX_PATH];
    DWORD got = GetTempPathW(MAX_PATH, temp_dir);
    TEST_ASSERT_TRUE(got > 0 && got < MAX_PATH);
    wchar_t temp_file[MAX_PATH];
    UINT made = GetTempFileNameW(temp_dir, L"bn2", 0, temp_file);
    TEST_ASSERT_TRUE(made != 0);
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_write_to_path(b, temp_file));

    sf_bnd2_reader_t *r = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_reader_open(&r, temp_file, NULL));
    TEST_ASSERT_EQUAL_size_t(3, sf_bnd2_reader_file_count(r));
    static const uint8_t *expected_data[3] = { k_payload_a, k_payload_b, k_payload_c };
    static const size_t expected_size[3] = { sizeof k_payload_a, sizeof k_payload_b, sizeof k_payload_c };
    static const int32_t expected_id[3] = { 100, 200, 300 };
    static const char *expected_name[3] = { "a.txt", "dir/b.bin", "c.dat" };
    for (size_t i = 0; i < 3; i++) {
        const sf_bnd2_file_header_t *h = sf_bnd2_reader_get_file_header(r, i);
        TEST_ASSERT_NOT_NULL(h);
        TEST_ASSERT_EQUAL_INT32(expected_id[i], h->id);
        TEST_ASSERT_EQUAL_STRING(expected_name[i], h->name_utf8);
        TEST_ASSERT_EQUAL_INT32((int32_t)expected_size[i], h->size);
        uint8_t *buf = NULL;
        size_t n = 0;
        TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_reader_read_file_by_index(r, i, &buf, &n, NULL));
        TEST_ASSERT_EQUAL_size_t(expected_size[i], n);
        TEST_ASSERT_EQUAL_MEMORY(expected_data[i], buf, n);
        sf_free(NULL, buf);
    }
    uint8_t *by_id = NULL;
    size_t by_id_n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_reader_read_file_by_id(r, 200, &by_id, &by_id_n, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof k_payload_b, by_id_n);
    TEST_ASSERT_EQUAL_MEMORY(k_payload_b, by_id, by_id_n);
    sf_free(NULL, by_id);
    TEST_ASSERT_EQUAL(SF_ERR_NOT_FOUND, sf_bnd2_reader_read_file_by_id(r, 999, &by_id, &by_id_n, NULL));
    sf_bnd2_reader_close(r);
    sf_bnd2_destroy(b);
    DeleteFileW(temp_file);
}

static void test_bnd2_roundtrip_full_path_mode(void) {
    sf_bnd2_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd2_create(&b, NULL));
    configure_common(b, SF_BND2_FILE_PATH_MODE_FULL_PATH);
    populate_three_files(b);
    assert_bnd2_roundtrip(b);
    sf_bnd2_destroy(b);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bnd2_roundtrip_filename_mode);
    RUN_TEST(test_bnd2_roundtrip_nameless_mode);
    RUN_TEST(test_bnd2_roundtrip_base_directory_mode);
    RUN_TEST(test_bnd2_reader_pattern);
    RUN_TEST(test_bnd2_roundtrip_full_path_mode);
    return UNITY_END();
}
