/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 1 QA — sf_path_* mirrors PathHelper behavior.
 */

#include "souls_formats/sf_path.h"
#include "souls_formats/sf_io.h"  /* sf_free */
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"  /* sf_free */

#include "unity.h"

#include <string.h>
#include <windows.h>

void setUp(void) {}
void tearDown(void) {}

static void test_get_real_extension_dcx(void) {
    char *ext = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_path_get_real_extension("bar.flver.dcx", &ext, NULL));
    TEST_ASSERT_EQUAL_STRING(".flver", ext);
    sf_free(NULL, ext);
}

static void test_get_real_extension_plain(void) {
    char *ext = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_path_get_real_extension("bar.txt", &ext, NULL));
    TEST_ASSERT_EQUAL_STRING(".txt", ext);
    sf_free(NULL, ext);
}

static void test_get_real_extension_no_ext(void) {
    char *ext = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_path_get_real_extension("bar", &ext, NULL));
    TEST_ASSERT_EQUAL_STRING("", ext);
    sf_free(NULL, ext);
}

static void test_get_real_extension_path(void) {
    char *ext = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_path_get_real_extension("/path/bar.flver.dcx", &ext, NULL));
    TEST_ASSERT_EQUAL_STRING(".flver", ext);
    sf_free(NULL, ext);
}

static void test_get_real_file_name_dcx(void) {
    char *name = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_path_get_real_file_name("bar.flver.dcx", &name, NULL));
    TEST_ASSERT_EQUAL_STRING("bar", name);
    sf_free(NULL, name);
}

static void test_get_real_file_name_plain(void) {
    char *name = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_path_get_real_file_name("bar.txt", &name, NULL));
    TEST_ASSERT_EQUAL_STRING("bar", name);
    sf_free(NULL, name);
}

static void test_get_real_file_name_path(void) {
    char *name = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_path_get_real_file_name("/path/bar.flver.dcx", &name, NULL));
    TEST_ASSERT_EQUAL_STRING("bar", name);
    sf_free(NULL, name);
}

static void test_backup_creates_bak(void) {
    /* Create a temp file */
    char tmp_path[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp_path);
    strcat(tmp_path, "sf_test_backup.bin");

    HANDLE h = CreateFileA(tmp_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        TEST_IGNORE_MESSAGE("Cannot create temp file for backup test");
        return;
    }
    DWORD written;
    WriteFile(h, "hello", 5, &written, NULL);
    CloseHandle(h);

    char *bak_path = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_path_backup(tmp_path, false, &bak_path, NULL));
    TEST_ASSERT_NOT_NULL(bak_path);

    /* Verify .bak exists */
    DWORD attrs = GetFileAttributesA(bak_path);
    TEST_ASSERT_NOT_EQUAL(INVALID_FILE_ATTRIBUTES, attrs);

    /* Cleanup */
    DeleteFileA(tmp_path);
    DeleteFileA(bak_path);
    sf_free(NULL, bak_path);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_get_real_extension_dcx);
    RUN_TEST(test_get_real_extension_plain);
    RUN_TEST(test_get_real_extension_no_ext);
    RUN_TEST(test_get_real_extension_path);
    RUN_TEST(test_get_real_file_name_dcx);
    RUN_TEST(test_get_real_file_name_plain);
    RUN_TEST(test_get_real_file_name_path);
    RUN_TEST(test_backup_creates_bak);
    return UNITY_END();
}
