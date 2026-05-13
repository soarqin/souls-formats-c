/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds1r_test_helper.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_mcg.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_mcg_e2e_ds1r(void) {
    if (!ds1r_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS1R copy not available");
    }
    void *bytes = NULL;
    size_t size = 0;
    sf_result_t r = ds1r_read_file("map/m10_00_00_00/m10_00_00_00.mcg", &bytes, &size);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R MCG not accessible");
    }
    sf_mcg_t *mcg = NULL;
    r = sf_mcg_read_from_memory(&mcg, bytes, size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(mcg);
    TEST_ASSERT_FALSE(sf_mcg_big_endian(mcg));
    TEST_ASSERT_GREATER_THAN(0, (int)sf_mcg_node_count(mcg));
    TEST_ASSERT_GREATER_THAN(0, (int)sf_mcg_edge_count(mcg));

    void *bytes2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcg_write_to_memory(mcg, &bytes2, &size2, NULL));
    TEST_ASSERT_EQUAL_size_t(size, size2);
    TEST_ASSERT_EQUAL_MEMORY(bytes, bytes2, size);

    sf_free(NULL, bytes);
    sf_free(NULL, bytes2);
    sf_mcg_destroy(mcg);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mcg_e2e_ds1r);
    return UNITY_END();
}
