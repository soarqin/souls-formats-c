/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_sl2.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Bytes lifted from upstream SL2Decryptor.cs:12 */
static const uint8_t k_ds2_expected[16] = {
    0xB7, 0xFD, 0x46, 0x3E, 0x4A, 0x9C, 0x11, 0x02,
    0xDF, 0x17, 0x39, 0xE5, 0xF3, 0xB2, 0xA5, 0x0F,
};
/* Bytes lifted from upstream SL2Decryptor.cs:13 */
static const uint8_t k_scholar_expected[16] = {
    0x59, 0x9F, 0x9B, 0x69, 0x96, 0x40, 0xA5, 0x52,
    0x36, 0xEE, 0x2D, 0x70, 0x83, 0x5E, 0xC7, 0x44,
};
/* Bytes lifted from upstream SL2Decryptor.cs:14 */
static const uint8_t k_ds3_expected[16] = {
    0xFD, 0x46, 0x4D, 0x69, 0x5E, 0x69, 0xA3, 0x9A,
    0x10, 0xE3, 0x19, 0xA7, 0xAC, 0xE8, 0xB7, 0xFA,
};

static void test_key_getters_match_upstream(void) {
    const uint8_t *k = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_sl2_get_ds2_key(&k));
    TEST_ASSERT_NOT_NULL(k);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(k_ds2_expected, k, 16);

    k = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_sl2_get_scholar_key(&k));
    TEST_ASSERT_NOT_NULL(k);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(k_scholar_expected, k, 16);

    k = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_sl2_get_ds3_key(&k));
    TEST_ASSERT_NOT_NULL(k);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(k_ds3_expected, k, 16);
}

static void test_key_getter_arg_validation(void) {
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG, sf_sl2_get_ds2_key(NULL));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG, sf_sl2_get_scholar_key(NULL));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG, sf_sl2_get_ds3_key(NULL));
}

static void run_roundtrip_for_key(const uint8_t *key) {
    /* 256-byte synthetic plaintext aligned to AES block size. */
    uint8_t plain[256];
    for (size_t i = 0; i < sizeof(plain); ++i) {
        plain[i] = (uint8_t)(i * 7u + 3u);
    }

    uint8_t *enc = NULL;
    size_t enc_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_sl2_encrypt(plain, sizeof(plain), key, &enc, &enc_size, NULL));
    TEST_ASSERT_EQUAL_size_t(32u + sizeof(plain), enc_size);
    TEST_ASSERT_NOT_NULL(enc);

    /* Envelope must NOT be the plaintext (sanity: AES actually ran). */
    TEST_ASSERT_FALSE(memcmp(enc + 32u, plain, sizeof(plain)) == 0);

    uint8_t *dec = NULL;
    size_t dec_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_sl2_decrypt(enc, enc_size, key, &dec, &dec_size, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof(plain), dec_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(plain, dec, sizeof(plain));

    sf_free(NULL, enc);
    sf_free(NULL, dec);
}

static void test_roundtrip_ds2(void) {
    const uint8_t *key = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_sl2_get_ds2_key(&key));
    run_roundtrip_for_key(key);
}

static void test_roundtrip_scholar(void) {
    const uint8_t *key = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_sl2_get_scholar_key(&key));
    run_roundtrip_for_key(key);
}

static void test_roundtrip_ds3(void) {
    const uint8_t *key = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_sl2_get_ds3_key(&key));
    run_roundtrip_for_key(key);
}

static void test_decrypt_arg_validation(void) {
    const uint8_t *key = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_sl2_get_ds2_key(&key));

    uint8_t in[64] = {0};
    uint8_t *out = NULL;
    size_t out_size = 0;

    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_sl2_decrypt(NULL, 64, key, &out, &out_size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_sl2_decrypt(in, 64, NULL, &out, &out_size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_sl2_decrypt(in, 64, key, NULL, &out_size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_sl2_decrypt(in, 64, key, &out, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_sl2_decrypt(in, 16, key, &out, &out_size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_sl2_decrypt(in, 33, key, &out, &out_size, NULL));
}

static void test_encrypt_arg_validation(void) {
    const uint8_t *key = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_sl2_get_ds2_key(&key));

    uint8_t in[32] = {0};
    uint8_t *out = NULL;
    size_t out_size = 0;

    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_sl2_encrypt(in, 32, NULL, &out, &out_size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_sl2_encrypt(in, 32, key, NULL, &out_size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_sl2_encrypt(in, 32, key, &out, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_sl2_encrypt(in, 17, key, &out, &out_size, NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_key_getters_match_upstream);
    RUN_TEST(test_key_getter_arg_validation);
    RUN_TEST(test_roundtrip_ds2);
    RUN_TEST(test_roundtrip_scholar);
    RUN_TEST(test_roundtrip_ds3);
    RUN_TEST(test_decrypt_arg_validation);
    RUN_TEST(test_encrypt_arg_validation);
    return UNITY_END();
}
