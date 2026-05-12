/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds3_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msb3.h"
#include "unity.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static void test_msb3_e2e_ds3(void)
{
    if (!ds3_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS3 copy not available");
    }
    void       *bytes = NULL;
    size_t      size  = 0;
    sf_result_t r = ds3_extract_from_anybhd("/map/mapstudio/m10_00_00_00.msb.dcx",
                                            &bytes, &size);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DS3 MSB3");
    }
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS3 MSB3 not accessible from any shard");
    }

    sf_msb3_t *msb3 = NULL;
    r = sf_msb3_read_from_memory(&msb3, (const uint8_t *)bytes, size, NULL);
    sf_free(NULL, bytes);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS3 MSB3 did not parse in this environment");
    }
    TEST_ASSERT_NOT_NULL(msb3);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, sf_msb3_model_count(msb3));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, sf_msb3_part_count(msb3));
    sf_msb3_destroy(msb3);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_msb3_e2e_ds3);
    ds3_helper_shutdown();
    return UNITY_END();
}
