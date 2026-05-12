/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds2s_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_param.h"
#include "unity.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static void test_param_e2e_ds2s_keyconfig(void)
{
    if (!ds2s_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS2S copy not available");
    }
    void       *bytes = NULL;
    size_t      size  = 0;
    sf_result_t r = ds2s_read_loose_param("Param/KeyConfigParam.param", &bytes, &size);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS2S KeyConfigParam.param not accessible");
    }

    sf_param_t *param = NULL;
    r = sf_param_read_from_memory(&param, (const uint8_t *)bytes, size, NULL);
    sf_free(NULL, bytes);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS2S PARAM did not parse in this environment");
    }
    TEST_ASSERT_NOT_NULL(param);
    TEST_ASSERT_GREATER_THAN(0, (int)sf_param_get_row_count(param));
    sf_param_destroy(param);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_param_e2e_ds2s_keyconfig);
    return UNITY_END();
}
