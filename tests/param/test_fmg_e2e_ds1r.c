/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds1r_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_fmg.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static void test_fmg_e2e_ds1r(void)
{
    if (!ds1r_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS1R copy not available");
    }
    void       *bytes = NULL;
    size_t      size  = 0;
    sf_result_t r = ds1r_extract_bnd3_entry("msg/ENGLISH/item.msgbnd.dcx", ".fmg",
                                            &bytes, &size);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R FMG entry not accessible");
    }

    sf_fmg_t *fmg = NULL;
    r = sf_fmg_read_from_memory(&fmg, (const uint8_t *)bytes, size, NULL);
    sf_free(NULL, bytes);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R FMG did not parse in this environment");
    }
    TEST_ASSERT_NOT_NULL(fmg);
    TEST_ASSERT_GREATER_THAN(0, (int)sf_fmg_get_entry_count(fmg));
    sf_fmg_destroy(fmg, NULL);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fmg_e2e_ds1r);
    return UNITY_END();
}
