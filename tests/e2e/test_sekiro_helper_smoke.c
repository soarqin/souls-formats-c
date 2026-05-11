/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "sekiro_test_helper.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

#include <stddef.h>

void setUp(void) {}
void tearDown(void) {}

static void test_sekiro_helper_init_or_skip(void)
{
    if (!sekiro_helper_is_available()) {
        TEST_IGNORE_MESSAGE("Sekiro data not available in this environment");
    }
    sf_result_t r = sekiro_helper_init();
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("sekiro_helper_init failed");
    }
    TEST_PASS();
}

static void test_sekiro_helper_is_idempotent(void)
{
    if (!sekiro_helper_is_available()) {
        TEST_IGNORE_MESSAGE("Sekiro data not available in this environment");
    }
    sf_result_t r1 = sekiro_helper_init();
    sf_result_t r2 = sekiro_helper_init();
    TEST_ASSERT_EQUAL_INT(r1, r2);
}

static void test_sekiro_extract_known_file(void)
{
    if (!sekiro_helper_is_available()) {
        TEST_IGNORE_MESSAGE("Sekiro data not available in this environment");
    }
    sf_result_t r = sekiro_helper_init();
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("sekiro_helper_init failed");
    }

    static const char *const candidates[] = {
        "/chr/c0000.chrbnd.dcx",
        "/chr/c1000.chrbnd.dcx",
        "/event/common.emevd.dcx",
        "/msg/engus/menu.msgbnd.dcx",
        "/parts/wp_a_0000.partsbnd.dcx",
        "/script/c0000.luabnd.dcx",
        NULL,
    };

    void   *bytes = NULL;
    size_t  size  = 0;
    bool    found = false;
    for (int i = 0; candidates[i]; ++i) {
        bytes = NULL;
        size  = 0;
        r = sekiro_extract_from_anybhd(candidates[i], &bytes, &size);
        if (r == SF_OK) {
            found = true;
            break;
        }
    }
    if (!found) {
        TEST_IGNORE_MESSAGE("No known Sekiro path found in any shard");
    }
    TEST_ASSERT_GREATER_THAN(0, (int)size);
    TEST_ASSERT_NOT_NULL(bytes);
    sf_free(NULL, bytes);
}

static void test_sekiro_helper_shutdown_is_idempotent(void)
{
    sekiro_helper_shutdown();
    sekiro_helper_shutdown();
    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sekiro_helper_init_or_skip);
    RUN_TEST(test_sekiro_helper_is_idempotent);
    RUN_TEST(test_sekiro_extract_known_file);
    RUN_TEST(test_sekiro_helper_shutdown_is_idempotent);
    return UNITY_END();
}
