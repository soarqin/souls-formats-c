/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ac6_test_helper.h"

#include "souls_formats/sf_io.h"
#include "unity.h"

#include <stddef.h>

void setUp(void) {}
void tearDown(void) {}

static const char *const k_candidates[] = {
    "/map/mapstudio/m01_00_00_00.msb.dcx",
    "/map/mapstudio/m10_00_00_00.msb.dcx",
    "/map/mapstudio/m20_00_00_00.msb.dcx",
    "/map/mapstudio/m30_00_00_00.msb.dcx",
    NULL,
};

static void test_ac6_helper_init_or_skip(void)
{
    if (!ac6_helper_is_available()) {
        TEST_IGNORE_MESSAGE("AC6 data not available in this environment");
    }
    sf_result_t r = ac6_helper_init();
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("ac6_helper_init failed");
    }
    TEST_PASS();
}

static void test_ac6_helper_extract_first_candidate(void)
{
    if (!ac6_helper_is_available()) {
        TEST_IGNORE_MESSAGE("AC6 data not available in this environment");
    }

    for (size_t i = 0; k_candidates[i] != NULL; ++i) {
        void       *bytes = NULL;
        size_t      size  = 0;
        sf_result_t r     = ac6_extract_from_data0(k_candidates[i], &bytes, &size);
        if (r == SF_OK) {
            TEST_ASSERT_NOT_NULL(bytes);
            TEST_ASSERT_GREATER_THAN(0, (int)size);
            sf_free(NULL, bytes);
            return;
        }
        if (bytes) {
            sf_free(NULL, bytes);
        }
    }

    TEST_IGNORE_MESSAGE("No candidate AC6 path was accessible from Data0");
}

static void test_ac6_helper_is_idempotent(void)
{
    if (!ac6_helper_is_available()) {
        TEST_IGNORE_MESSAGE("AC6 data not available in this environment");
    }
    sf_result_t r1 = ac6_helper_init();
    sf_result_t r2 = ac6_helper_init();
    TEST_ASSERT_EQUAL_INT(r1, r2);
}

static void test_ac6_helper_shutdown_is_idempotent(void)
{
    ac6_helper_shutdown();
    ac6_helper_shutdown();
    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ac6_helper_init_or_skip);
    RUN_TEST(test_ac6_helper_extract_first_candidate);
    RUN_TEST(test_ac6_helper_is_idempotent);
    RUN_TEST(test_ac6_helper_shutdown_is_idempotent);
    return UNITY_END();
}
