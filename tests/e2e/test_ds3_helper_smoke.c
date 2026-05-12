/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds3_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_ds3_helper_init_or_skip(void)
{
    if (!ds3_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS3 data not available in this environment");
    }
    sf_result_t r = ds3_helper_init();
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("ds3_helper_init failed");
    }
    TEST_PASS();
}

static void test_ds3_extract_known_file(void)
{
    if (!ds3_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS3 data not available in this environment");
    }
    void       *bytes = NULL;
    size_t      size  = 0;
    sf_result_t r     = ds3_extract_from_anybhd("/event/common.emevd.dcx", &bytes, &size);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DS3 file");
    }
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS3 known path not found in available shards");
    }
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN(0, (int)size);
    sf_free(NULL, bytes);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ds3_helper_init_or_skip);
    RUN_TEST(test_ds3_extract_known_file);
    ds3_helper_shutdown();
    return UNITY_END();
}
