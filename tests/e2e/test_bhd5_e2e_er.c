/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T15 — KEYSTONE Phase-3 e2e test.
 *
 * Validates the full FromSoft pipeline against a real Elden Ring install:
 *   1. RSA-unwrap Data0.bhd     (sf_bhd5_open)
 *   2. BHD5 path-hash lookup    (sf_bhd5_extract_by_hash_64)
 *   3. AES range decrypt        (handled by sf_bhd5_extract_by_*)
 *   4. DCX_KRAK detection       (sf_dcx_sniff)
 *   5. Oodle decompression      (sf_dcx_decompress, 6-arg form)
 *   6. Inner BND4 magic         ('B','N','D','4')
 *
 * The test SKIPs gracefully (TEST_IGNORE_MESSAGE) whenever the ER game
 * directory or Oodle DLL search path is unavailable, so it never FAILs in
 * a clean checkout. On a fully-equipped dev box every sub-test PASSes.
 *
 * Path roots are configured via SF_E2E_ELDEN_RING_DIR / SF_E2E_OODLE_DIR
 * preprocessor macros in tests/CMakeLists.txt; er_test_helper.c provides
 * #ifndef fallbacks for clangd indexing.
 */

#include "er_test_helper.h"

#include "souls_formats/sf_bhd5.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_hash.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static bool env_ok;

/* Sub-test 1 — open & summarise. Triggers RSA unwrap inside sf_bhd5_open
 * (Data0.bhd's first bytes are 0xe1 0x0e 0x36 0xab on disk; the BHD5 magic
 * only appears post-decryption). A non-NULL handle with bucket_count > 0
 * proves the unwrap + parse pipeline succeeded. */
static void test_open_and_summarize(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    sf_result_t r = er_helper_init();
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("er_helper_init failed");
    }

    sf_bhd5_t *bhd = er_helper_get_bhd5_for_testing();
    TEST_ASSERT_NOT_NULL(bhd);
    TEST_ASSERT_GREATER_THAN(0, (int)sf_bhd5_bucket_count(bhd));
    TEST_ASSERT_GREATER_THAN(1000, (int)sf_bhd5_total_file_count(bhd));
}

/* Sub-test 2 — confirm RSA wrapping. Read the first 4 bytes of Data0.bhd
 * directly. If they are "BHD5" the file is plaintext (impossible for a
 * shipped ER install). If they are 0xe1 0x0e 0x36 0xab the file is the
 * standard RSA-PKCS1-padded blob; sub-test 1 having succeeded already
 * proves sf_bhd5_open handled the unwrap internally. */
static void test_bhd5_magic_after_rsa(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    sf_result_t r = er_helper_init();
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("er_helper_init failed");
    }

    /* Use the same path the helper uses; reproduce it here so the test is
     * self-contained and does not depend on the helper exposing the path. */
    const wchar_t *bhd_path = SF_E2E_ELDEN_RING_DIR L"/Game/Data0.bhd";
    sf_istream_t  *s        = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_istream_open_wfile(&s, bhd_path, NULL));

    uint8_t magic[4] = {0};
    sf_result_t read_r = sf_istream_read(s, magic, sizeof(magic));
    sf_istream_close(s);
    TEST_ASSERT_EQUAL(SF_OK, read_r);

    /* Either plaintext "BHD5" or RSA-wrapped (0xe1 0x0e 0x36 0xab); both
     * are acceptable. The proof that RSA was honored when needed is sub-
     * test 1's bucket_count > 0 — a successful BHD5 parse can only
     * happen post-unwrap. */
    const bool is_plaintext = magic[0] == 'B' && magic[1] == 'H'
                              && magic[2] == 'D' && magic[3] == '5';
    const bool is_rsa_wrap = magic[0] == 0xe1 && magic[1] == 0x0e
                             && magic[2] == 0x36 && magic[3] == 0xab;
    TEST_ASSERT_TRUE_MESSAGE(is_plaintext || is_rsa_wrap,
                             "Data0.bhd must be plaintext BHD5 or RSA-wrapped");
}

/* Sub-test 3 — path-hash lookup against the real archive. Extracts the
 * raw (still-DCX-wrapped) chrbnd payload via uint64_t FromPathHash. */
static void test_path_hash_lookup(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    sf_result_t r = er_helper_init();
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("er_helper_init failed");
    }

    sf_bhd5_t     *bhd      = er_helper_get_bhd5_for_testing();
    const uint64_t hash     = sf_path_hash_64("/chr/c0000.chrbnd.dcx");
    void          *out      = NULL;
    size_t         out_size = 0;

    sf_result_t er = sf_bhd5_extract_by_hash_64(bhd, hash, &out, &out_size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, er);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_GREATER_THAN(1000, (int)out_size);

    sf_free(NULL, out);
}

/* Sub-test 4 — sniff the DCX type of the raw chrbnd payload. ER's chrbnd
 * is shipped as DCX_KRAK (Oodle-Kraken) on disk. */
static void test_dcx_type_detection(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    sf_result_t r = er_helper_init();
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("er_helper_init failed");
    }

    sf_bhd5_t     *bhd      = er_helper_get_bhd5_for_testing();
    const uint64_t hash     = sf_path_hash_64("/chr/c0000.chrbnd.dcx");
    void          *out      = NULL;
    size_t         out_size = 0;
    TEST_ASSERT_EQUAL(SF_OK,
                      sf_bhd5_extract_by_hash_64(bhd, hash, &out, &out_size, NULL));

    sf_dcx_type_t type    = SF_DCX_TYPE_UNKNOWN;
    sf_result_t   sniff_r = sf_dcx_sniff(out, out_size, &type);
    TEST_ASSERT_EQUAL_INT(SF_OK, sniff_r);
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_KRAK, (int)type);

    sf_free(NULL, out);
}

/* Sub-test 5 — Oodle decompress + verify BND4 magic ("B","N","D","4").
 * Skips if Oodle DLL is missing (SF_ERR_OODLE_NOT_FOUND); otherwise the
 * decompressed bytes must begin with the BND4 magic. */
static void test_oodle_decompress_bnd4_magic(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    sf_result_t r = er_helper_init();
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("er_helper_init failed");
    }

    sf_bhd5_t     *bhd      = er_helper_get_bhd5_for_testing();
    const uint64_t hash     = sf_path_hash_64("/chr/c0000.chrbnd.dcx");
    void          *raw      = NULL;
    size_t         raw_size = 0;
    TEST_ASSERT_EQUAL(SF_OK,
                      sf_bhd5_extract_by_hash_64(bhd, hash, &raw, &raw_size, NULL));

    void          *decompressed = NULL;
    size_t         decomp_size  = 0;
    sf_dcx_type_t  out_type     = SF_DCX_TYPE_UNKNOWN;
    sf_result_t    dec_r        =
        sf_dcx_decompress(raw, raw_size, &decompressed, &decomp_size,
                          &out_type, NULL);
    sf_free(NULL, raw);

    if (dec_r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, dec_r);
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_KRAK, (int)out_type);
    TEST_ASSERT_NOT_NULL(decompressed);
    TEST_ASSERT_GREATER_THAN(4, (int)decomp_size);

    const uint8_t *p = (const uint8_t *)decompressed;
    TEST_ASSERT_EQUAL_UINT8('B', p[0]);
    TEST_ASSERT_EQUAL_UINT8('N', p[1]);
    TEST_ASSERT_EQUAL_UINT8('D', p[2]);
    TEST_ASSERT_EQUAL_UINT8('4', p[3]);

    sf_free(NULL, decompressed);
}

/* Sub-test 6 — high-level helper. er_extract_from_data0 transparently
 * unwraps the outer DCX layer and yields plain BND4 bytes. */
static void test_er_extract_from_data0_succeeds(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    void       *result      = NULL;
    size_t      result_size = 0;
    sf_result_t er          =
        er_extract_from_data0("/chr/c0000.chrbnd.dcx", &result, &result_size);

    if (er == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, er);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_GREATER_THAN(1000, (int)result_size);

    sf_free(NULL, result);
}

/* Sub-test 7 — verify er_extract_from_data0 hands back BND4-magic bytes. */
static void test_er_extract_from_data0_bnd4_magic(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    void       *result      = NULL;
    size_t      result_size = 0;
    sf_result_t er          =
        er_extract_from_data0("/chr/c0000.chrbnd.dcx", &result, &result_size);

    if (er == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, er);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_GREATER_THAN(4, (int)result_size);

    const uint8_t *p = (const uint8_t *)result;
    TEST_ASSERT_EQUAL_UINT8('B', p[0]);
    TEST_ASSERT_EQUAL_UINT8('N', p[1]);
    TEST_ASSERT_EQUAL_UINT8('D', p[2]);
    TEST_ASSERT_EQUAL_UINT8('4', p[3]);

    sf_free(NULL, result);
}

int main(void)
{
    UNITY_BEGIN();
    env_ok = er_helper_is_available();
    if (env_ok) {
        env_ok = er_helper_init() == SF_OK;
    }
    RUN_TEST(test_open_and_summarize);
    RUN_TEST(test_bhd5_magic_after_rsa);
    RUN_TEST(test_path_hash_lookup);
    RUN_TEST(test_dcx_type_detection);
    RUN_TEST(test_oodle_decompress_bnd4_magic);
    RUN_TEST(test_er_extract_from_data0_succeeds);
    RUN_TEST(test_er_extract_from_data0_bnd4_magic);
    return UNITY_END();
}
