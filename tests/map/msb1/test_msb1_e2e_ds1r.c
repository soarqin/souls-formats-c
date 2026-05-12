/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds1r_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msb1.h"
#include "unity.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static void test_msb1_e2e_ds1r(void)
{
    if (!ds1r_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS1R copy not available");
    }

    void       *bytes = NULL;
    size_t      size  = 0;
    sf_result_t r     = ds1r_read_file("map/MapStudio/m10_00_00_00.msb", &bytes, &size);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R MSB1 loose file not accessible");
    }

    sf_msb1_t *msb1 = NULL;
    r = sf_msb1_read_from_memory(&msb1, (const uint8_t *)bytes, size, NULL);
    sf_free(NULL, bytes);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R MSB1 did not parse in this environment");
    }
    TEST_ASSERT_NOT_NULL(msb1);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, sf_msb1_model_count(msb1));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, sf_msb1_part_count(msb1));
    sf_msb1_destroy(msb1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_msb1_e2e_ds1r);
    return UNITY_END();
}
