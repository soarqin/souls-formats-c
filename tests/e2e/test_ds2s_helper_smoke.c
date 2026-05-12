/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds2s_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_ds2s_helper_available_or_skip(void)
{
    if (!ds2s_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS2S data not available in this environment");
    }
    TEST_PASS();
}

static void test_ds2s_read_keyconfig_param(void)
{
    if (!ds2s_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS2S data not available in this environment");
    }
    void       *bytes = NULL;
    size_t      size  = 0;
    sf_result_t r     = ds2s_read_loose_param("Param/KeyConfigParam.param", &bytes, &size);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS2S KeyConfigParam.param not accessible");
    }
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN(0, (int)size);
    sf_free(NULL, bytes);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ds2s_helper_available_or_skip);
    RUN_TEST(test_ds2s_read_keyconfig_param);
    return UNITY_END();
}
