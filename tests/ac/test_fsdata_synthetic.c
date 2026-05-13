/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_fsdata.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_fsdata_create_destroy(void) {
    sf_fsdata_t *f = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fsdata_create(&f, 8192, false, NULL));
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_FALSE(sf_fsdata_is_compressed(f));
    TEST_ASSERT_EQUAL_INT(8192, sf_fsdata_entry_count(f));
    TEST_ASSERT_EQUAL_size_t(0, sf_fsdata_file_count(f));
    sf_fsdata_destroy(f);
}

static void test_fsdata_add_and_get_file(void) {
    sf_fsdata_t *f = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fsdata_create(&f, 8192, false, NULL));

    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fsdata_add_file(f, 42, data, sizeof(data)));
    TEST_ASSERT_EQUAL_size_t(1, sf_fsdata_file_count(f));

    int id = 0;
    const uint8_t *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fsdata_get_file(f, 0, &id, &bytes, &size));
    TEST_ASSERT_EQUAL_INT(42, id);
    TEST_ASSERT_EQUAL_size_t(sizeof(data), size);
    TEST_ASSERT_EQUAL_MEMORY(data, bytes, sizeof(data));

    sf_fsdata_destroy(f);
}

static void test_fsdata_uncompressed_round_trip(void) {
    sf_fsdata_t *a = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fsdata_create(&a, 8192, false, NULL));

    uint8_t file0[0x800];
    memset(file0, 0xAB, sizeof(file0));
    uint8_t file1[0x1000];
    memset(file1, 0xCD, sizeof(file1));

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fsdata_add_file(a, 0, file0, sizeof(file0)));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fsdata_add_file(a, 5, file1, sizeof(file1)));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fsdata_write_to_memory(a, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_TRUE(size > 0);

    sf_fsdata_t *b = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fsdata_read_from_memory(&b, bytes, size, 8192, false, NULL));
    TEST_ASSERT_EQUAL_size_t(2, sf_fsdata_file_count(b));

    int id0 = 0;
    const uint8_t *rb0 = NULL;
    size_t rs0 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fsdata_get_file(b, 0, &id0, &rb0, &rs0));
    TEST_ASSERT_EQUAL_INT(0, id0);
    TEST_ASSERT_EQUAL_size_t(sizeof(file0), rs0);
    TEST_ASSERT_EQUAL_MEMORY(file0, rb0, sizeof(file0));

    sf_free(NULL, bytes);
    sf_fsdata_destroy(b);
    sf_fsdata_destroy(a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fsdata_create_destroy);
    RUN_TEST(test_fsdata_add_and_get_file);
    RUN_TEST(test_fsdata_uncompressed_round_trip);
    return UNITY_END();
}
