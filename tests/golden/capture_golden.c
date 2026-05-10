/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Pre-refactor SHA-256 golden baseline. See golden_hashes.h.
 *
 * Each test:
 *   1. Performs a deterministic operation on a synthetic fixture.
 *   2. Computes SHA-256 over the resulting byte buffer using Win32 BCrypt
 *      (same provider that backs sfi_md5_hash in src/crypto/md5_cng.c).
 *   3. Prints the hash to stdout in a header-paste-ready C array form.
 *   4. Asserts it matches the constant in golden_hashes.h.
 *
 * Bootstrapping a fresh hash table:
 *   - Initial run with all-zero placeholders fails the assert; the printed
 *     C array is copy-pasted into golden_hashes.h and the matching line
 *     is added to .sisyphus/evidence/golden-hashes/baseline.txt.
 *   - Re-run; should now pass.
 */

#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_encoding.h"
#include "souls_formats/sf_io.h"

#include "golden_hashes.h"
#include "unity.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <windows.h>
#include <bcrypt.h>

#ifndef BCRYPT_SUCCESS
#  define BCRYPT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif

void setUp(void) {}
void tearDown(void) {}

/*===========================================================================
 * SHA-256 helper (BCrypt). Single-shot — no streaming required.
 *===========================================================================*/
static void sha256_buf(const void *data, size_t size, uint8_t out[32]) {
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    TEST_ASSERT_TRUE_MESSAGE(BCRYPT_SUCCESS(st), "BCryptOpenAlgorithmProvider(SHA256)");
    st = BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0);
    TEST_ASSERT_TRUE_MESSAGE(BCRYPT_SUCCESS(st), "BCryptCreateHash");
    if (size > 0u) {
        st = BCryptHashData(hash, (PUCHAR)data, (ULONG)size, 0);
        TEST_ASSERT_TRUE_MESSAGE(BCRYPT_SUCCESS(st), "BCryptHashData");
    }
    st = BCryptFinishHash(hash, out, 32, 0);
    TEST_ASSERT_TRUE_MESSAGE(BCRYPT_SUCCESS(st), "BCryptFinishHash");
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
}

static void print_and_check(const char *name, const uint8_t actual[32],
                            const uint8_t expected[32]) {
    printf("\nstatic const uint8_t %s[32] = {\n", name);
    for (int row = 0; row < 4; row++) {
        printf("    ");
        for (int col = 0; col < 8; col++) {
            printf("0x%02X, ", actual[row * 8 + col]);
        }
        printf("\n");
    }
    printf("};\n");
    fflush(stdout);
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(expected, actual, 32, name);
}

/*===========================================================================
 * Helper: build a fresh (memory) writer.
 *===========================================================================*/
static void make_writer(sf_ostream_t **out_s, sf_binary_writer_t **out_w) {
    sf_ostream_t *s = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&s, NULL));
    sf_binary_writer_t *w = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&w, s, false, NULL));
    *out_s = s;
    *out_w = w;
}

/*===========================================================================
 * Test cases
 *===========================================================================*/

static void test_writer_bytes_4(void) {
    sf_ostream_t *s; sf_binary_writer_t *w; make_writer(&s, &w);
    static const uint8_t payload[] = { 0x01, 0x02, 0x03, 0x04 };
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_bytes(w, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));
    void *bytes = NULL; size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    uint8_t actual[32]; sha256_buf(bytes, n, actual);
    sf_free(NULL, bytes);
    sf_binary_writer_destroy(w); sf_ostream_close(s);
    print_and_check("GOLDEN_WRITER_BYTES_4", actual, GOLDEN_WRITER_BYTES_4);
}

static void test_writer_u32_deadbeef(void) {
    sf_ostream_t *s; sf_binary_writer_t *w; make_writer(&s, &w);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_u32(w, 0xDEADBEEFu));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));
    void *bytes = NULL; size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    uint8_t actual[32]; sha256_buf(bytes, n, actual);
    sf_free(NULL, bytes);
    sf_binary_writer_destroy(w); sf_ostream_close(s);
    print_and_check("GOLDEN_WRITER_U32_DEADBEEF", actual, GOLDEN_WRITER_U32_DEADBEEF);
}

static void test_writer_f32_pi(void) {
    sf_ostream_t *s; sf_binary_writer_t *w; make_writer(&s, &w);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_f32(w, 3.14f));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));
    void *bytes = NULL; size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    uint8_t actual[32]; sha256_buf(bytes, n, actual);
    sf_free(NULL, bytes);
    sf_binary_writer_destroy(w); sf_ostream_close(s);
    print_and_check("GOLDEN_WRITER_F32_PI", actual, GOLDEN_WRITER_F32_PI);
}

static void test_writer_ascii_hello(void) {
    sf_ostream_t *s; sf_binary_writer_t *w; make_writer(&s, &w);
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_write_ascii(w, "hello", true));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));
    void *bytes = NULL; size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &bytes, &n));
    uint8_t actual[32]; sha256_buf(bytes, n, actual);
    sf_free(NULL, bytes);
    sf_binary_writer_destroy(w); sf_ostream_close(s);
    print_and_check("GOLDEN_WRITER_ASCII_HELLO", actual, GOLDEN_WRITER_ASCII_HELLO);
}

static void test_input_64_sha256(void) {
    uint8_t actual[32]; sha256_buf(GOLDEN_INPUT_64, sizeof(GOLDEN_INPUT_64), actual);
    print_and_check("GOLDEN_INPUT_64_SHA256", actual, GOLDEN_INPUT_64_SHA256);
}

static void test_dcx_dflt_compress_64(void) {
    void *cx = NULL; size_t cxn = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_dcx_compress(GOLDEN_INPUT_64, sizeof(GOLDEN_INPUT_64),
                                             SF_DCX_TYPE_DCX_DFLT, &cx, &cxn, NULL));
    uint8_t actual[32]; sha256_buf(cx, cxn, actual);
    sf_free(NULL, cx);
    print_and_check("GOLDEN_DCX_DFLT_COMPRESS_64", actual, GOLDEN_DCX_DFLT_COMPRESS_64);
}

static void test_dcx_dflt_decompress_64(void) {
    void *cx = NULL; size_t cxn = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_dcx_compress(GOLDEN_INPUT_64, sizeof(GOLDEN_INPUT_64),
                                             SF_DCX_TYPE_DCX_DFLT, &cx, &cxn, NULL));
    void *dx = NULL; size_t dxn = 0; sf_dcx_type_t type;
    TEST_ASSERT_EQUAL(SF_OK, sf_dcx_decompress(cx, cxn, &dx, &dxn, &type, NULL));
    TEST_ASSERT_EQUAL(SF_DCX_TYPE_DCX_DFLT, type);
    TEST_ASSERT_EQUAL_size_t(sizeof(GOLDEN_INPUT_64), dxn);
    uint8_t actual[32]; sha256_buf(dx, dxn, actual);
    sf_free(NULL, cx); sf_free(NULL, dx);
    print_and_check("GOLDEN_DCX_DFLT_DECOMPRESS_64", actual, GOLDEN_DCX_DFLT_DECOMPRESS_64);
}

static void test_dcx_zstd_compress_64(void) {
    void *cx = NULL; size_t cxn = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_dcx_compress(GOLDEN_INPUT_64, sizeof(GOLDEN_INPUT_64),
                                             SF_DCX_TYPE_DCX_ZSTD, &cx, &cxn, NULL));
    uint8_t actual[32]; sha256_buf(cx, cxn, actual);
    sf_free(NULL, cx);
    print_and_check("GOLDEN_DCX_ZSTD_COMPRESS_64", actual, GOLDEN_DCX_ZSTD_COMPRESS_64);
}

static void test_encoding_hello_sjis(void) {
    void *bytes = NULL; size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf8_to_shift_jis("hello", false, &bytes, &n, NULL));
    uint8_t actual[32]; sha256_buf(bytes, n, actual);
    sf_free(NULL, bytes);
    print_and_check("GOLDEN_ENCODING_HELLO_SJIS", actual, GOLDEN_ENCODING_HELLO_SJIS);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_writer_bytes_4);
    RUN_TEST(test_writer_u32_deadbeef);
    RUN_TEST(test_writer_f32_pi);
    RUN_TEST(test_writer_ascii_hello);
    RUN_TEST(test_input_64_sha256);
    RUN_TEST(test_dcx_dflt_compress_64);
    RUN_TEST(test_dcx_dflt_decompress_64);
    RUN_TEST(test_dcx_zstd_compress_64);
    RUN_TEST(test_encoding_hello_sjis);
    return UNITY_END();
}
