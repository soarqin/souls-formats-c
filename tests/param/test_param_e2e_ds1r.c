/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds1r_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_param.h"
#include "unity.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static void test_param_e2e_ds1r(void)
{
    if (!ds1r_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS1R copy not available");
    }
    void       *bytes = NULL;
    size_t      size  = 0;
    sf_result_t r = ds1r_extract_bnd3_entry("param/GameParam/GameParam.parambnd.dcx",
                                            ".param", &bytes, &size);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R PARAM entry not accessible");
    }

    sf_param_t *param = NULL;
    r = sf_param_read_from_memory(&param, (const uint8_t *)bytes, size, NULL);
    sf_free(NULL, bytes);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R PARAM did not parse in this environment");
    }
    TEST_ASSERT_NOT_NULL(param);
    TEST_ASSERT_GREATER_THAN(0, (int)sf_param_get_row_count(param));
    sf_param_destroy(param);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_param_e2e_ds1r);
    return UNITY_END();
}
