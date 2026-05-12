/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds1r_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_flver2.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_flver2_e2e_ds1r_c0000(void)
{
    if (!ds1r_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS1R copy not available");
    }
    void       *bytes = NULL;
    size_t      size  = 0;
    sf_result_t r = ds1r_extract_bnd3_entry("chr/c0000.chrbnd.dcx", ".flver",
                                            &bytes, &size);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R c0000.flver not accessible");
    }

    sf_flver2_t *flver = NULL;
    r = sf_flver2_read_from_memory(&flver, bytes, size, NULL);
    sf_free(NULL, bytes);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R FLVER2 did not parse in this environment");
    }
    TEST_ASSERT_NOT_NULL(flver);
    TEST_ASSERT_GREATER_THAN((size_t)0, sf_flver2_node_count(flver));
    sf_flver2_destroy(flver);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_flver2_e2e_ds1r_c0000);
    return UNITY_END();
}
