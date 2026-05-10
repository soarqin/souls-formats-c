/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "er_test_helper.h"
#include "souls_formats/sf_bhd5.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_er_helper_init_or_skip(void)
{
    sf_result_t r = er_helper_init();
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("ER copy or BHD5 not available in this environment");
    }
    TEST_ASSERT_NOT_NULL(er_helper_get_bhd5_for_testing());
    TEST_ASSERT_GREATER_THAN(
        0, (int)sf_bhd5_total_file_count(er_helper_get_bhd5_for_testing()));
}

static void test_er_helper_is_idempotent(void)
{
    sf_result_t r1 = er_helper_init();
    sf_result_t r2 = er_helper_init();
    TEST_ASSERT_EQUAL_INT(r1, r2);
}

static void test_er_helper_shutdown_is_idempotent(void)
{
    er_helper_shutdown();
    er_helper_shutdown();
    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_er_helper_init_or_skip);
    RUN_TEST(test_er_helper_is_idempotent);
    RUN_TEST(test_er_helper_shutdown_is_idempotent);
    return UNITY_END();
}
