/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_regulation.h"
#include "unity.h"

#include <windows.h>

#include <stdint.h>
#include <string.h>

#ifndef SF_E2E_ELDEN_RING_DIR
#define SF_E2E_ELDEN_RING_DIR L"C:/Games/ELDEN RING"
#endif

void setUp(void) {}
void tearDown(void) {}

static const uint8_t k_synthetic_plain[33] = {
    'B','N','D','4','\0','\0','\0','\0',
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    16,17,18,19,20,21,22,23,24
};

static void roundtrip_for_key(sf_regulation_key_t key) {
    uint8_t *enc = NULL;
    size_t enc_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_encrypt(k_synthetic_plain, sizeof(k_synthetic_plain), key,
                              &enc, &enc_size, NULL));
    TEST_ASSERT_NOT_NULL(enc);

    size_t expected_pad = 16u - (sizeof(k_synthetic_plain) % 16u);
    if (expected_pad == 0u) expected_pad = 16u;
    TEST_ASSERT_EQUAL_size_t(16u + sizeof(k_synthetic_plain) + expected_pad, enc_size);

    uint8_t *dec = NULL;
    size_t dec_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_decrypt(enc, enc_size, key, &dec, &dec_size, NULL));
    TEST_ASSERT_NOT_NULL(dec);
    TEST_ASSERT_TRUE(dec_size >= sizeof(k_synthetic_plain));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(k_synthetic_plain, dec, sizeof(k_synthetic_plain));

    sf_free(NULL, enc);
    sf_free(NULL, dec);
}

static void test_roundtrip_ds3(void) {
    roundtrip_for_key(SF_REGULATION_KEY_DARK_SOULS_3);
}

static void test_roundtrip_er(void) {
    roundtrip_for_key(SF_REGULATION_KEY_ELDEN_RING);
}

static void test_roundtrip_ac6(void) {
    roundtrip_for_key(SF_REGULATION_KEY_ARMORED_CORE_6);
}

static void test_roundtrip_ernr(void) {
    roundtrip_for_key(SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN);
}

static void test_wrappers_match_generic_decrypt(void) {
    uint8_t *enc_generic = NULL;
    size_t enc_generic_size = 0;
    uint8_t *enc_wrapper = NULL;
    size_t enc_wrapper_size = 0;

    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_encrypt(k_synthetic_plain, sizeof(k_synthetic_plain),
                              SF_REGULATION_KEY_DARK_SOULS_3, &enc_generic,
                              &enc_generic_size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_encrypt_ds3(k_synthetic_plain, sizeof(k_synthetic_plain),
                                  &enc_wrapper, &enc_wrapper_size, NULL));
    TEST_ASSERT_EQUAL_size_t(enc_generic_size, enc_wrapper_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(enc_generic, enc_wrapper, enc_generic_size);
    sf_free(NULL, enc_generic);
    sf_free(NULL, enc_wrapper);

    enc_generic = NULL;
    enc_wrapper = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_encrypt(k_synthetic_plain, sizeof(k_synthetic_plain),
                              SF_REGULATION_KEY_ELDEN_RING, &enc_generic,
                              &enc_generic_size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_encrypt_er(k_synthetic_plain, sizeof(k_synthetic_plain),
                                 &enc_wrapper, &enc_wrapper_size, NULL));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(enc_generic, enc_wrapper, enc_generic_size);
    sf_free(NULL, enc_generic);
    sf_free(NULL, enc_wrapper);

    enc_generic = NULL;
    enc_wrapper = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_encrypt(k_synthetic_plain, sizeof(k_synthetic_plain),
                              SF_REGULATION_KEY_ARMORED_CORE_6, &enc_generic,
                              &enc_generic_size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_encrypt_ac6(k_synthetic_plain, sizeof(k_synthetic_plain),
                                  &enc_wrapper, &enc_wrapper_size, NULL));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(enc_generic, enc_wrapper, enc_generic_size);
    sf_free(NULL, enc_generic);
    sf_free(NULL, enc_wrapper);
}

/*  Verifies the deliberately-faithful Nightreign quirk: upstream
 *  EncryptERNRRegulation calls with RegulationKey.EldenRing (not Nightreign).
 *  sf_regulation_encrypt_ernr() must produce byte-identical output to
 *  sf_regulation_encrypt(..., SF_REGULATION_KEY_ELDEN_RING, ...) and must NOT
 *  match sf_regulation_encrypt(..., SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN,
 *  ...) — guarding against any future "fix" that would silently break upstream
 *  byte compatibility.
 */
static void test_ernr_encrypt_uses_er_key_quirk(void) {
    uint8_t *enc_ernr_wrapper = NULL;
    size_t enc_ernr_wrapper_size = 0;
    uint8_t *enc_er_generic = NULL;
    size_t enc_er_generic_size = 0;
    uint8_t *enc_nr_generic = NULL;
    size_t enc_nr_generic_size = 0;

    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_encrypt_ernr(k_synthetic_plain, sizeof(k_synthetic_plain),
                                   &enc_ernr_wrapper, &enc_ernr_wrapper_size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_encrypt(k_synthetic_plain, sizeof(k_synthetic_plain),
                              SF_REGULATION_KEY_ELDEN_RING, &enc_er_generic,
                              &enc_er_generic_size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_encrypt(k_synthetic_plain, sizeof(k_synthetic_plain),
                              SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN, &enc_nr_generic,
                              &enc_nr_generic_size, NULL));

    TEST_ASSERT_EQUAL_size_t(enc_er_generic_size, enc_ernr_wrapper_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(
        enc_er_generic, enc_ernr_wrapper, enc_er_generic_size,
        "Nightreign quirk: encrypt_ernr() must produce identical bytes to "
        "encrypt(..., ELDEN_RING, ...)");

    TEST_ASSERT_EQUAL_size_t(enc_nr_generic_size, enc_ernr_wrapper_size);
    int matches_nr = (memcmp(enc_nr_generic, enc_ernr_wrapper, enc_nr_generic_size) == 0);
    TEST_ASSERT_FALSE_MESSAGE(matches_nr,
        "Nightreign quirk regression: encrypt_ernr() must NOT match "
        "encrypt(..., ELDEN_RING_NIGHTREIGN, ...) — see RegulationDecryptor.cs:117");

    sf_free(NULL, enc_ernr_wrapper);
    sf_free(NULL, enc_er_generic);
    sf_free(NULL, enc_nr_generic);
}

static void test_decrypt_wrappers_use_matching_key(void) {
    uint8_t *enc = NULL;
    size_t enc_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_encrypt(k_synthetic_plain, sizeof(k_synthetic_plain),
                              SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN, &enc, &enc_size,
                              NULL));

    uint8_t *dec = NULL;
    size_t dec_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_decrypt_ernr(enc, enc_size, &dec, &dec_size, NULL));
    TEST_ASSERT_TRUE(dec_size >= sizeof(k_synthetic_plain));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(k_synthetic_plain, dec, sizeof(k_synthetic_plain));

    sf_free(NULL, enc);
    sf_free(NULL, dec);
}

static void test_invalid_args(void) {
    uint8_t *out = NULL;
    size_t out_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_regulation_decrypt(NULL, 32, SF_REGULATION_KEY_ELDEN_RING, &out, &out_size,
                              NULL));
    TEST_ASSERT_EQUAL_INT(SF_ERR_TRUNCATED,
        sf_regulation_decrypt(k_synthetic_plain, 8, SF_REGULATION_KEY_ELDEN_RING, &out,
                              &out_size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
        sf_regulation_decrypt(k_synthetic_plain, 32, (sf_regulation_key_t)999, &out,
                              &out_size, NULL));
}

static void test_regulation_decrypt_er_file_if_available(void) {
    wchar_t path[MAX_PATH];
    int n = swprintf(path, MAX_PATH, L"%ls/Game/regulation.bin", SF_E2E_ELDEN_RING_DIR);
    TEST_ASSERT_TRUE(n > 0 && n < MAX_PATH);
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
        TEST_IGNORE_MESSAGE("ELDEN RING regulation.bin missing");
    }
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    TEST_ASSERT_NOT_EQUAL(INVALID_HANDLE_VALUE, h);
    LARGE_INTEGER sz;
    TEST_ASSERT_TRUE(GetFileSizeEx(h, &sz));
    uint8_t *data = (uint8_t *)sf_default_allocator()->alloc((size_t)sz.QuadPart, NULL);
    TEST_ASSERT_NOT_NULL(data);
    DWORD got = 0;
    TEST_ASSERT_TRUE(ReadFile(h, data, (DWORD)sz.QuadPart, &got, NULL));
    CloseHandle(h);
    TEST_ASSERT_EQUAL_UINT32((DWORD)sz.QuadPart, got);

    uint8_t *decrypted = NULL;
    size_t dec_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_decrypt_er(data, got, &decrypted, &dec_size, NULL));
    sf_free(NULL, data);
    data = NULL;

    TEST_ASSERT_TRUE_MESSAGE(dec_size >= 4u, "decrypted output too short");

    void *bnd4 = NULL;
    size_t bnd4_size = 0;
    TEST_ASSERT_EQUAL_INT_MESSAGE(SF_OK,
        sf_dcx_unwrap(decrypted, dec_size, &bnd4, &bnd4_size, NULL),
        "DCX unwrap of decrypted regulation.bin failed");

    TEST_ASSERT_TRUE_MESSAGE(bnd4_size >= 4u, "inner content too short");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE("BND4", bnd4, 4, "inner content is not BND4");

    uint8_t *reenc = NULL;
    size_t reenc_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_encrypt_er(decrypted, dec_size, &reenc, &reenc_size, NULL));
    uint8_t *again = NULL;
    size_t again_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_regulation_decrypt_er(reenc, reenc_size, &again, &again_size, NULL));
    TEST_ASSERT_TRUE(again_size >= dec_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(decrypted, again, dec_size);

    sf_free(NULL, decrypted);
    sf_free(NULL, bnd4);
    sf_free(NULL, reenc);
    sf_free(NULL, again);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip_ds3);
    RUN_TEST(test_roundtrip_er);
    RUN_TEST(test_roundtrip_ac6);
    RUN_TEST(test_roundtrip_ernr);
    RUN_TEST(test_wrappers_match_generic_decrypt);
    RUN_TEST(test_ernr_encrypt_uses_er_key_quirk);
    RUN_TEST(test_decrypt_wrappers_use_matching_key);
    RUN_TEST(test_invalid_args);
    RUN_TEST(test_regulation_decrypt_er_file_if_available);
    return UNITY_END();
}
