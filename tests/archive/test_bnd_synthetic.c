/* SPDX-License-Identifier: GPL-3.0-or-later */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "souls_formats/sf_bnd.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const uint8_t k_payload_a[] = { 0x10, 0x20, 0x30, 0x40 };
static const uint8_t k_payload_b[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };
static const uint8_t k_payload_c[] = { 0x01, 0x02, 0x03 };

static sf_bnd_file_t make_file(int32_t id, const char *name,
                               const uint8_t *data, size_t size) {
    sf_bnd_file_t f;
    memset(&f, 0, sizeof f);
    f.id = id;
    f.name_utf8 = name;
    f.data = data;
    f.size = size;
    return f;
}

static void populate_three_files(sf_bnd_t *b) {
    sf_bnd_file_t f1 = make_file(10, "a.txt", k_payload_a, sizeof k_payload_a);
    sf_bnd_file_t f2 = make_file(20, "b.bin", k_payload_b, sizeof k_payload_b);
    sf_bnd_file_t f3 = make_file(30, "c.dat", k_payload_c, sizeof k_payload_c);
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd_add_file(b, &f1));
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd_add_file(b, &f2));
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd_add_file(b, &f3));
}

static void assert_bnd_roundtrip(const sf_bnd_t *b1) {
    uint8_t *bytes_first = NULL;
    size_t size_first = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd_write_to_memory(b1, &bytes_first, &size_first, NULL));
    TEST_ASSERT_TRUE(sf_bnd_is_format(bytes_first, size_first));

    sf_bnd_t *b2 = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd_read_from_memory(&b2, bytes_first, size_first, NULL));
    TEST_ASSERT_EQUAL_INT32(sf_bnd_get_internal_version(b1), sf_bnd_get_internal_version(b2));
    TEST_ASSERT_EQUAL_HEX16(sf_bnd_get_format0(b1), sf_bnd_get_format0(b2));
    TEST_ASSERT_EQUAL_HEX16(sf_bnd_get_format1(b1), sf_bnd_get_format1(b2));
    TEST_ASSERT_EQUAL_STRING(sf_bnd_get_root_file_path(b1), sf_bnd_get_root_file_path(b2));
    TEST_ASSERT_EQUAL_size_t(sf_bnd_file_count(b1), sf_bnd_file_count(b2));

    for (size_t i = 0; i < sf_bnd_file_count(b1); i++) {
        const sf_bnd_file_t *e1 = sf_bnd_get_file(b1, i);
        const sf_bnd_file_t *e2 = sf_bnd_get_file(b2, i);
        TEST_ASSERT_NOT_NULL(e1);
        TEST_ASSERT_NOT_NULL(e2);
        TEST_ASSERT_EQUAL_INT32(e1->id, e2->id);
        TEST_ASSERT_EQUAL_STRING(e1->name_utf8, e2->name_utf8);
        TEST_ASSERT_EQUAL_size_t(e1->size, e2->size);
        TEST_ASSERT_EQUAL_MEMORY(e1->data, e2->data, e1->size);
    }

    uint8_t *bytes_second = NULL;
    size_t size_second = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd_write_to_memory(b2, &bytes_second, &size_second, NULL));
    TEST_ASSERT_EQUAL_size_t(size_first, size_second);
    TEST_ASSERT_EQUAL_MEMORY(bytes_first, bytes_second, size_first);

    sf_free(NULL, bytes_first);
    sf_free(NULL, bytes_second);
    sf_bnd_destroy(b2);
}

static void test_bnd_roundtrip_basic(void) {
    sf_bnd_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd_create(&b, NULL));
    sf_bnd_set_internal_version(b, 1);
    sf_bnd_set_format0(b, 0x0102);
    sf_bnd_set_format1(b, 0x0003);
    sf_bnd_set_root_file_path(b, "root/path");
    populate_three_files(b);
    assert_bnd_roundtrip(b);
    sf_bnd_destroy(b);
}

static void test_bnd_no_root_file_path(void) {
    sf_bnd_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd_create(&b, NULL));
    sf_bnd_set_internal_version(b, 2);
    sf_bnd_set_format0(b, 0x0001);
    sf_bnd_set_format1(b, 0x0001);
    populate_three_files(b);
    assert_bnd_roundtrip(b);
    sf_bnd_destroy(b);
}

static void test_bnd_with_root_file_path(void) {
    sf_bnd_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd_create(&b, NULL));
    sf_bnd_set_internal_version(b, 7);
    sf_bnd_set_root_file_path(b, "N:\\legacy\\binder");
    populate_three_files(b);
    assert_bnd_roundtrip(b);
    sf_bnd_destroy(b);
}

static void test_bnd_empty(void) {
    sf_bnd_t *b = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bnd_create(&b, NULL));
    sf_bnd_set_internal_version(b, 9);
    assert_bnd_roundtrip(b);
    sf_bnd_destroy(b);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bnd_roundtrip_basic);
    RUN_TEST(test_bnd_no_root_file_path);
    RUN_TEST(test_bnd_with_root_file_path);
    RUN_TEST(test_bnd_empty);
    return UNITY_END();
}
