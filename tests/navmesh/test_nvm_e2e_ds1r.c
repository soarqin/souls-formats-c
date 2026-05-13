/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds1r_test_helper.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_nvm.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_nvm_e2e_ds1r(void) {
    if (!ds1r_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS1R copy not available");
    }
    void *bytes = NULL;
    size_t size = 0;
    sf_result_t r = ds1r_extract_bnd3_entry(
        "map/m10_00_00_00/m10_00_00_00.nvmbnd.dcx", ".nvm", &bytes, &size);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R NVM bundle not accessible");
    }
    sf_nvm_t *nvm = NULL;
    r = sf_nvm_read_from_memory(&nvm, bytes, size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(nvm);
    TEST_ASSERT_GREATER_THAN(0, (int)sf_nvm_vertex_count(nvm));
    TEST_ASSERT_GREATER_THAN(0, (int)sf_nvm_triangle_count(nvm));
    TEST_ASSERT_NOT_NULL(sf_nvm_root_box(nvm));

    void *bytes2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_write_to_memory(nvm, &bytes2, &size2, NULL));
    TEST_ASSERT_NOT_NULL(bytes2);
    TEST_ASSERT_EQUAL_size_t(size, size2);

    sf_nvm_t *reparsed = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_nvm_read_from_memory(&reparsed, bytes2, size2, NULL));
    TEST_ASSERT_NOT_NULL(reparsed);
    TEST_ASSERT_EQUAL_size_t(sf_nvm_vertex_count(nvm), sf_nvm_vertex_count(reparsed));
    TEST_ASSERT_EQUAL_size_t(sf_nvm_triangle_count(nvm), sf_nvm_triangle_count(reparsed));
    TEST_ASSERT_EQUAL_size_t(sf_nvm_entity_count(nvm), sf_nvm_entity_count(reparsed));

    sf_free(NULL, bytes);
    sf_free(NULL, bytes2);
    sf_nvm_destroy(nvm);
    sf_nvm_destroy(reparsed);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nvm_e2e_ds1r);
    return UNITY_END();
}
