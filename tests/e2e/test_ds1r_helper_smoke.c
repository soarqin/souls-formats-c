/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds1r_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_ds1r_helper_available_or_skip(void)
{
    if (!ds1r_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS1R data not available in this environment");
    }
    TEST_PASS();
}

static void test_ds1r_read_known_loose_msb(void)
{
    if (!ds1r_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS1R data not available in this environment");
    }
    void       *bytes = NULL;
    size_t      size  = 0;
    sf_result_t r     = ds1r_read_file("map/MapStudio/m10_00_00_00.msb", &bytes, &size);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R known loose MSB not accessible");
    }
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN(0, (int)size);
    sf_free(NULL, bytes);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ds1r_helper_available_or_skip);
    RUN_TEST(test_ds1r_read_known_loose_msb);
    return UNITY_END();
}
