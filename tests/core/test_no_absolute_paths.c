/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Hygiene QA — project tree must not contain absolute include paths.
 */

#include "unity.h"

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

static void test_no_absolute_include_paths(void) {
    char command[1024];
    const char *needle = "\"/" "home/";
    int  exit_code;

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, _chdir("C:\\"), "failed to move to local drive before system()");
    snprintf(command, sizeof(command),
             "wsl.exe bash -lc \"cd \\\"%s\\\" && grep -rln '%s' include/ src/ tests/ 2>/dev/null\"",
             SOULS_FORMATS_ROOT_DIR, needle);

    exit_code = system(command);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, exit_code, "failed to execute hygiene grep");
    {
        const int status = (exit_code > 255) ? (exit_code / 256) : exit_code;
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, status,
                                      "absolute path detected: grep found include path matches");
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_no_absolute_include_paths);
    return UNITY_END();
}
