/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_nva.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_nva_empty_roundtrip(void) {
    sf_nva_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_nva_create_empty(&src, SF_NVA_VERSION_DARK_SOULS_3, NULL));
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL_INT(SF_NVA_VERSION_DARK_SOULS_3, sf_nva_version(src));
    TEST_ASSERT_EQUAL_size_t(9, sf_nva_section_count(src));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nva_write_to_memory(src, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);

    sf_nva_t *parsed = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nva_read_from_memory(&parsed, bytes, size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_NVA_VERSION_DARK_SOULS_3, sf_nva_version(parsed));
    TEST_ASSERT_EQUAL_size_t(9, sf_nva_section_count(parsed));

    void *bytes2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nva_write_to_memory(parsed, &bytes2, &size2, NULL));
    TEST_ASSERT_EQUAL_size_t(size, size2);
    TEST_ASSERT_EQUAL_MEMORY(bytes, bytes2, size);

    sf_free(NULL, bytes);
    sf_free(NULL, bytes2);
    sf_nva_destroy(parsed);
    sf_nva_destroy(src);
}

static void test_nva_with_entries_roundtrip(void) {
    sf_nva_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_nva_create_empty(&src, SF_NVA_VERSION_DARK_SOULS_3, NULL));

    /* Section 1 (index 1): 16-byte entries. */
    sf_nva_section_t *sec1 = sf_nva_section_mut(src, 1);
    TEST_ASSERT_NOT_NULL(sec1);
    uint8_t entry1[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nva_section_append_entry(sec1, entry1, 16));
    uint8_t entry2[16] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
                          0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00};
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nva_section_append_entry(sec1, entry2, 16));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nva_write_to_memory(src, &bytes, &size, NULL));

    sf_nva_t *parsed = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nva_read_from_memory(&parsed, bytes, size, NULL));
    const sf_nva_section_t *psec1 = sf_nva_section(parsed, 1);
    TEST_ASSERT_NOT_NULL(psec1);
    TEST_ASSERT_EQUAL_size_t(2, sf_nva_section_entry_count(psec1));
    TEST_ASSERT_EQUAL_size_t(16, sf_nva_section_entry_size(psec1));
    TEST_ASSERT_EQUAL_MEMORY(entry1, sf_nva_section_entries(psec1), 16);
    TEST_ASSERT_EQUAL_MEMORY(entry2, sf_nva_section_entries(psec1) + 16, 16);

    void *bytes2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nva_write_to_memory(parsed, &bytes2, &size2, NULL));
    TEST_ASSERT_EQUAL_size_t(size, size2);
    TEST_ASSERT_EQUAL_MEMORY(bytes, bytes2, size);

    sf_free(NULL, bytes);
    sf_free(NULL, bytes2);
    sf_nva_destroy(parsed);
    sf_nva_destroy(src);
}

static void test_nva_old_bloodborne_roundtrip(void) {
    sf_nva_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_nva_create_empty(&src, SF_NVA_VERSION_OLD_BLOODBORNE, NULL));
    TEST_ASSERT_EQUAL_size_t(8, sf_nva_section_count(src));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nva_write_to_memory(src, &bytes, &size, NULL));

    sf_nva_t *parsed = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nva_read_from_memory(&parsed, bytes, size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_NVA_VERSION_OLD_BLOODBORNE, sf_nva_version(parsed));
    TEST_ASSERT_EQUAL_size_t(8, sf_nva_section_count(parsed));

    sf_free(NULL, bytes);
    sf_nva_destroy(parsed);
    sf_nva_destroy(src);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nva_empty_roundtrip);
    RUN_TEST(test_nva_with_entries_roundtrip);
    RUN_TEST(test_nva_old_bloodborne_roundtrip);
    return UNITY_END();
}
