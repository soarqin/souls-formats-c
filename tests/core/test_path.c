/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * PathHelper-equivalent path helper tests.
 */

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_path.h"

#include "unity.h"

#include <string.h>

#include <windows.h>

void setUp(void) {}
void tearDown(void) {}

static void assert_real_extension(const char *path, const char *expected) {
    char *ext = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_path_get_real_extension(path, &ext, NULL));
    TEST_ASSERT_EQUAL_STRING(expected, ext);
    sf_free(NULL, ext);
}

static void assert_real_file_name(const char *path, const char *expected) {
    char *name = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_path_get_real_file_name(path, &name, NULL));
    TEST_ASSERT_EQUAL_STRING(expected, name);
    sf_free(NULL, name);
}

static void test_get_real_extension_dcx(void) {
    assert_real_extension("bar.flver.dcx", ".flver");
}

static void test_get_real_extension_plain(void) {
    assert_real_extension("bar.txt", ".txt");
}

static void test_get_real_extension_no_ext(void) {
    assert_real_extension("bar", "");
}

static void test_get_real_extension_path(void) {
    assert_real_extension("/path/bar.flver.dcx", ".flver");
}

static void test_get_real_file_name_dcx(void) {
    assert_real_file_name("bar.flver.dcx", "bar");
}

static void test_get_real_file_name_plain(void) {
    assert_real_file_name("bar.txt", "bar");
}

static void test_get_real_file_name_path(void) {
    assert_real_file_name("/path/bar.flver.dcx", "bar");
}

static void write_file_or_fail(const wchar_t *path, const char *content) {
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    TEST_ASSERT_NOT_EQUAL(INVALID_HANDLE_VALUE, h);

    DWORD written = 0;
    DWORD size = (DWORD)strlen(content);
    TEST_ASSERT_TRUE(WriteFile(h, content, size, &written, NULL));
    TEST_ASSERT_EQUAL_UINT32(size, written);
    TEST_ASSERT_TRUE(CloseHandle(h));
}

static void read_file_or_fail(const wchar_t *path, char *buffer, DWORD buffer_size,
                              DWORD *out_read) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    TEST_ASSERT_NOT_EQUAL(INVALID_HANDLE_VALUE, h);
    TEST_ASSERT_TRUE(ReadFile(h, buffer, buffer_size, out_read, NULL));
    TEST_ASSERT_TRUE(CloseHandle(h));
}

static void test_backup_creates_bak(void) {
    const wchar_t *dir = L"/tmp/sf_test_path";
    const wchar_t *src_w = L"/tmp/sf_test_path/source.txt";
    const wchar_t *bak_w = L"/tmp/sf_test_path/source.txt.bak";
    const char *src = "/tmp/sf_test_path/source.txt";
    const char *content = "PathHelper backup fixture";

    (void)CreateDirectoryW(L"/tmp", NULL);
    (void)CreateDirectoryW(dir, NULL);
    (void)DeleteFileW(bak_w);
    write_file_or_fail(src_w, content);

    char *backup_path = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_path_backup(src, false, &backup_path, NULL));
    TEST_ASSERT_EQUAL_STRING("/tmp/sf_test_path/source.txt.bak", backup_path);

    char buffer[64] = {0};
    DWORD read = 0;
    read_file_or_fail(bak_w, buffer, sizeof(buffer), &read);
    TEST_ASSERT_EQUAL_UINT32((DWORD)strlen(content), read);
    TEST_ASSERT_EQUAL_MEMORY(content, buffer, read);

    sf_free(NULL, backup_path);
    (void)DeleteFileW(src_w);
    (void)DeleteFileW(bak_w);
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
