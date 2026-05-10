/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 1 QA — sf_encoding round-trips through Win32 MBCS/wide APIs.
 */

#include "souls_formats/sf_encoding.h"
#include "souls_formats/sf_io.h"  /* sf_free */

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*---------------------------------------------------------------------------
 * ASCII
 *---------------------------------------------------------------------------*/

static void test_ascii_roundtrip(void) {
    const char *src = "hello, world";
    void *bytes = NULL;
    size_t bytes_n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf8_to_ascii(src, /* terminate */ false,
                                              &bytes, &bytes_n, NULL));
    TEST_ASSERT_EQUAL_size_t(strlen(src), bytes_n);
    TEST_ASSERT_EQUAL_MEMORY(src, bytes, bytes_n);

    char *back = NULL;
    size_t back_len = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ascii_to_utf8(bytes, bytes_n, &back, &back_len, NULL));
    TEST_ASSERT_EQUAL_size_t(strlen(src), back_len);
    TEST_ASSERT_EQUAL_STRING(src, back);

    sf_free(NULL, bytes);
    sf_free(NULL, back);
}

static void test_ascii_terminate(void) {
    void *bytes = NULL;
    size_t bytes_n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf8_to_ascii("ABC", true, &bytes, &bytes_n, NULL));
    TEST_ASSERT_EQUAL_size_t(4, bytes_n);
    const uint8_t *b = (const uint8_t *)bytes;
    TEST_ASSERT_EQUAL_HEX8(0x41, b[0]);
    TEST_ASSERT_EQUAL_HEX8(0x42, b[1]);
    TEST_ASSERT_EQUAL_HEX8(0x43, b[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, b[3]);
    sf_free(NULL, bytes);
}

static void test_ascii_empty(void) {
    char *back = NULL;
    size_t back_len = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ascii_to_utf8(NULL, 0, &back, &back_len, NULL));
    TEST_ASSERT_NOT_NULL(back);
    TEST_ASSERT_EQUAL_size_t(0, back_len);
    TEST_ASSERT_EQUAL_HEX8(0x00, (uint8_t)back[0]);
    sf_free(NULL, back);
}

/*---------------------------------------------------------------------------
 * Shift-JIS
 *---------------------------------------------------------------------------*/

/*  Japanese: "エルデンリング" (Elden Ring) in:
 *  - UTF-8: 21 bytes (3 per katakana × 7)
 *  - Shift-JIS: 14 bytes (2 per katakana × 7) */
static const char k_elden_ring_jp_utf8[] =
    "\xE3\x82\xA8" "\xE3\x83\xAB" "\xE3\x83\x87" "\xE3\x83\xB3"
    "\xE3\x83\xAA" "\xE3\x83\xB3" "\xE3\x82\xB0";  /* エルデンリング */

static const uint8_t k_elden_ring_jp_sjis[] = {
    0x83, 0x47,  /* エ */
    0x83, 0x8B,  /* ル */
    0x83, 0x66,  /* デ */
    0x83, 0x93,  /* ン */
    0x83, 0x8A,  /* リ */
    0x83, 0x93,  /* ン */
    0x83, 0x4F,  /* グ */
};

static void test_shift_jis_decode_known_bytes(void) {
    char *out = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_shift_jis_to_utf8(k_elden_ring_jp_sjis,
                                                  sizeof(k_elden_ring_jp_sjis),
                                                  &out, &n, NULL));
    TEST_ASSERT_EQUAL_STRING(k_elden_ring_jp_utf8, out);
    sf_free(NULL, out);
}

static void test_shift_jis_encode_known_bytes(void) {
    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf8_to_shift_jis(k_elden_ring_jp_utf8, false,
                                                  &bytes, &n, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof(k_elden_ring_jp_sjis), n);
    TEST_ASSERT_EQUAL_MEMORY(k_elden_ring_jp_sjis, bytes, n);
    sf_free(NULL, bytes);
}

static void test_shift_jis_roundtrip_mixed(void) {
    /*  Mixed ASCII + Japanese. */
    const char *src = "Hello \xE3\x82\xA8\xE3\x83\xAB\xE3\x83\x87\xE3\x83\xB3"
                      "\xE3\x83\xAA\xE3\x83\xB3\xE3\x82\xB0!";
    void *enc = NULL;
    size_t enc_n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf8_to_shift_jis(src, false, &enc, &enc_n, NULL));

    char *dec = NULL;
    size_t dec_n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_shift_jis_to_utf8(enc, enc_n, &dec, &dec_n, NULL));
    TEST_ASSERT_EQUAL_STRING(src, dec);

    sf_free(NULL, enc);
    sf_free(NULL, dec);
}

static void test_shift_jis_roundtrip(void) {
    const char *src = "hello world";
    void *enc = NULL;
    size_t enc_n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf8_to_shift_jis(src, false, &enc, &enc_n, NULL));

    char *dec = NULL;
    size_t dec_n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_shift_jis_to_utf8(enc, enc_n, &dec, &dec_n, NULL));
    TEST_ASSERT_EQUAL_size_t(strlen(src), dec_n);
    TEST_ASSERT_EQUAL_STRING(src, dec);

    sf_free(NULL, enc);
    sf_free(NULL, dec);
}

/*---------------------------------------------------------------------------
 * UTF-16 LE / BE
 *
 * Chinese: "黑暗之魂" (Dark Souls) in:
 *  UTF-8 bytes (each char 3 bytes):
 *    黑 = E9 BB 91     (U+9ED1)
 *    暗 = E6 9A 97     (U+6697)
 *    之 = E4 B9 8B     (U+4E4B)
 *    魂 = E9 AD 82     (U+9B42)
 *---------------------------------------------------------------------------*/
static const char k_dark_souls_cn_utf8[] =
    "\xE9\xBB\x91" "\xE6\x9A\x97" "\xE4\xB9\x8B" "\xE9\xAD\x82";  /* 黑暗之魂 */

static const uint8_t k_dark_souls_cn_utf16le[] = {
    0xD1, 0x9E,  /* 黑 */
    0x97, 0x66,  /* 暗 */
    0x4B, 0x4E,  /* 之 */
    0x42, 0x9B,  /* 魂 */
};

static const uint8_t k_dark_souls_cn_utf16be[] = {
    0x9E, 0xD1,
    0x66, 0x97,
    0x4E, 0x4B,
    0x9B, 0x42,
};

static void test_utf16le_decode(void) {
    char *out = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf16le_to_utf8(k_dark_souls_cn_utf16le,
                                                sizeof(k_dark_souls_cn_utf16le),
                                                &out, &n, NULL));
    TEST_ASSERT_EQUAL_STRING(k_dark_souls_cn_utf8, out);
    sf_free(NULL, out);
}

static void test_utf16be_decode(void) {
    char *out = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf16be_to_utf8(k_dark_souls_cn_utf16be,
                                                sizeof(k_dark_souls_cn_utf16be),
                                                &out, &n, NULL));
    TEST_ASSERT_EQUAL_STRING(k_dark_souls_cn_utf8, out);
    sf_free(NULL, out);
}

static void test_utf16le_encode(void) {
    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf8_to_utf16le(k_dark_souls_cn_utf8, false,
                                                &bytes, &n, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof(k_dark_souls_cn_utf16le), n);
    TEST_ASSERT_EQUAL_MEMORY(k_dark_souls_cn_utf16le, bytes, n);
    sf_free(NULL, bytes);
}

static void test_utf16be_encode(void) {
    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf8_to_utf16be(k_dark_souls_cn_utf8, false,
                                                &bytes, &n, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof(k_dark_souls_cn_utf16be), n);
    TEST_ASSERT_EQUAL_MEMORY(k_dark_souls_cn_utf16be, bytes, n);
    sf_free(NULL, bytes);
}

static void test_utf16_roundtrip_terminated(void) {
    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf8_to_utf16le("ABC", true, &bytes, &n, NULL));
    /*  3 chars × 2 bytes + 2 byte NUL = 8. */
    TEST_ASSERT_EQUAL_size_t(8, n);
    const uint8_t *b = (const uint8_t *)bytes;
    TEST_ASSERT_EQUAL_HEX8(0x41, b[0]); TEST_ASSERT_EQUAL_HEX8(0x00, b[1]);
    TEST_ASSERT_EQUAL_HEX8(0x42, b[2]); TEST_ASSERT_EQUAL_HEX8(0x00, b[3]);
    TEST_ASSERT_EQUAL_HEX8(0x43, b[4]); TEST_ASSERT_EQUAL_HEX8(0x00, b[5]);
    TEST_ASSERT_EQUAL_HEX8(0x00, b[6]); TEST_ASSERT_EQUAL_HEX8(0x00, b[7]);
    sf_free(NULL, bytes);
}

static void test_utf16_le_roundtrip(void) {
    const char *src = "hello world";
    void *enc = NULL;
    size_t enc_n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf8_to_utf16le(src, false, &enc, &enc_n, NULL));

    char *dec = NULL;
    size_t dec_n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf16le_to_utf8(enc, enc_n, &dec, &dec_n, NULL));
    TEST_ASSERT_EQUAL_size_t(strlen(src), dec_n);
    TEST_ASSERT_EQUAL_STRING(src, dec);

    sf_free(NULL, enc);
    sf_free(NULL, dec);
}

static void test_utf16_be_roundtrip(void) {
    const char *src = "hello world";
    void *enc = NULL;
    size_t enc_n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf8_to_utf16be(src, false, &enc, &enc_n, NULL));

    char *dec = NULL;
    size_t dec_n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_utf16be_to_utf8(enc, enc_n, &dec, &dec_n, NULL));
    TEST_ASSERT_EQUAL_size_t(strlen(src), dec_n);
    TEST_ASSERT_EQUAL_STRING(src, dec);

    sf_free(NULL, enc);
    sf_free(NULL, dec);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ascii_roundtrip);
    RUN_TEST(test_ascii_terminate);
    RUN_TEST(test_ascii_empty);
    RUN_TEST(test_shift_jis_decode_known_bytes);
    RUN_TEST(test_shift_jis_encode_known_bytes);
    RUN_TEST(test_shift_jis_roundtrip_mixed);
    RUN_TEST(test_shift_jis_roundtrip);
    RUN_TEST(test_utf16le_decode);
    RUN_TEST(test_utf16be_decode);
    RUN_TEST(test_utf16le_encode);
    RUN_TEST(test_utf16be_encode);
    RUN_TEST(test_utf16_roundtrip_terminated);
    RUN_TEST(test_utf16_le_roundtrip);
    RUN_TEST(test_utf16_be_roundtrip);
    return UNITY_END();
}
