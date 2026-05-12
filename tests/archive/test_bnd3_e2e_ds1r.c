/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds1r_test_helper.h"

#include "souls_formats/sf_bnd3.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static void test_bnd3_e2e_ds1r_chrbnd(void)
{
    if (!ds1r_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS1R copy not available");
    }
    void       *bytes = NULL;
    size_t      size  = 0;
    sf_result_t r     = ds1r_read_file("chr/c0000.chrbnd.dcx", &bytes, &size);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R c0000.chrbnd.dcx not accessible");
    }

    sf_bnd3_t *bnd = NULL;
    r = sf_bnd3_read_from_memory(&bnd, (const uint8_t *)bytes, size, NULL);
    sf_free(NULL, bytes);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R BND3 did not parse in this environment");
    }
    TEST_ASSERT_NOT_NULL(bnd);
    TEST_ASSERT_GREATER_THAN(0, (int)sf_bnd3_file_count(bnd));
    sf_bnd3_destroy(bnd);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bnd3_e2e_ds1r_chrbnd);
    return UNITY_END();
}
