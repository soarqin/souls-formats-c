/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Phase 4 stub test — implementation in T2.x / T3.x / T4.x */
#include "unity.h"
void setUp(void) {}
void tearDown(void) {}
static void test_stub(void) { TEST_IGNORE_MESSAGE("not yet implemented"); }
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_stub);
    return UNITY_END();
}
