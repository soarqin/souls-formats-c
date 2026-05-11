/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "nightreign_test_helper.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_nightreign_helper_init_or_skip(void)
{
    if (!nightreign_helper_is_available()) {
        TEST_IGNORE_MESSAGE("Nightreign data not available in this environment");
    }
    sf_result_t r = nightreign_helper_init();
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("nightreign_helper_init failed");
    }
    TEST_PASS();
}

static void test_nightreign_helper_is_idempotent(void)
{
    if (!nightreign_helper_is_available()) {
        TEST_IGNORE_MESSAGE("Nightreign data not available in this environment");
    }
    sf_result_t r1 = nightreign_helper_init();
    sf_result_t r2 = nightreign_helper_init();
    TEST_ASSERT_EQUAL_INT(r1, r2);
}

static void test_nightreign_helper_shutdown_is_idempotent(void)
{
    nightreign_helper_shutdown();
    nightreign_helper_shutdown();
    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nightreign_helper_init_or_skip);
    RUN_TEST(test_nightreign_helper_is_idempotent);
    RUN_TEST(test_nightreign_helper_shutdown_is_idempotent);
    return UNITY_END();
}
