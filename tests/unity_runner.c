/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 0 smoke runner.
 *
 * Goal: prove the full toolchain works end-to-end:
 *   WSL2 + MinGW-w64 cross-compile  →  PE x86_64 .exe
 *     →  WSL interop hands .exe to Windows kernel
 *       →  CRT init OK
 *         →  Win32 BCrypt API reachable
 *           →  souls-formats-c static lib linkable
 *             →  ctest reports green.
 *
 * If any of those layers is broken, this is the first place we'll know.
 */

#include "souls_formats/souls_formats.h"

#include <stdio.h>
#include <string.h>

#include <windows.h>
#include <bcrypt.h>

#include "unity.h"

#ifndef NT_SUCCESS
#  define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif

void setUp(void) {}
void tearDown(void) {}

/*---------------------------------------------------------------------------
 * Test 1 — souls-formats-c lib produces sane error strings.
 *---------------------------------------------------------------------------*/
static void test_sf_result_str_basic(void) {
    TEST_ASSERT_EQUAL_STRING("ok", sf_result_str(SF_OK));
    TEST_ASSERT_EQUAL_STRING("oodle DLL not found", sf_result_str(SF_ERR_OODLE_NOT_FOUND));
    TEST_ASSERT_EQUAL_STRING("(unknown sf_result_t)", sf_result_str((sf_result_t)9999));

    /* Every defined enum value must yield a non-empty string. */
    for (int i = 0; i < (int)SF_RESULT_COUNT_; i++) {
        const char *s = sf_result_str((sf_result_t)i);
        TEST_ASSERT_NOT_NULL(s);
        TEST_ASSERT_TRUE(strlen(s) > 0);
    }
}

/*---------------------------------------------------------------------------
 * Test 2 — sf_default_allocator is a usable malloc/realloc/free wrapper.
 *---------------------------------------------------------------------------*/
static void test_sf_default_allocator_roundtrip(void) {
    const sf_allocator_t *a = sf_default_allocator();
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(a->alloc);
    TEST_ASSERT_NOT_NULL(a->realloc);
    TEST_ASSERT_NOT_NULL(a->free);

    void *p = a->alloc(64, a->user);
    TEST_ASSERT_NOT_NULL(p);
    memset(p, 0xAB, 64);

    p = a->realloc(p, 64, 256, a->user);
    TEST_ASSERT_NOT_NULL(p);

    /* First 64 bytes must still be 0xAB after the realloc grow. */
    unsigned char *bp = (unsigned char *)p;
    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_EQUAL_UINT8(0xAB, bp[i]);
    }

    a->free(p, a->user);
}

/*---------------------------------------------------------------------------
 * Test 3 — Win32 BCrypt is reachable. This is THE smoke test for the
 *          WSL2 → MinGW → interop → Win32 pipeline. If it fails, the dev
 *          environment is wrong, not the library.
 *---------------------------------------------------------------------------*/
static void test_bcrypt_aes_provider_reachable(void) {
    BCRYPT_ALG_HANDLE h = NULL;
    NTSTATUS s = BCryptOpenAlgorithmProvider(&h, BCRYPT_AES_ALGORITHM, NULL, 0);

    char detail[128];
    snprintf(detail, sizeof(detail),
             "BCryptOpenAlgorithmProvider(AES) returned 0x%08lx", (unsigned long)s);
    TEST_ASSERT_TRUE_MESSAGE(NT_SUCCESS(s), detail);
    TEST_ASSERT_NOT_NULL(h);

    BCryptCloseAlgorithmProvider(h, 0);
}

/*---------------------------------------------------------------------------
 * Test 4 — sf_last_error_detail is a non-crashing stub for now.
 *---------------------------------------------------------------------------*/
static void test_sf_last_error_detail_stub(void) {
    /* Phase 0 stub returns NULL; Phase 1 will populate it. */
    const char *d = sf_last_error_detail();
    (void)d;  /* either NULL or a string; never crashes */
    TEST_PASS();
}

/*---------------------------------------------------------------------------
 * Entry
 *---------------------------------------------------------------------------*/
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sf_result_str_basic);
    RUN_TEST(test_sf_default_allocator_roundtrip);
    RUN_TEST(test_bcrypt_aes_provider_reachable);
    RUN_TEST(test_sf_last_error_detail_stub);
    return UNITY_END();
}
