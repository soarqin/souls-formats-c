/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "crypto/aes_cng.h"
#include "internal/sf_internal.h"

#include <windows.h>
#include <bcrypt.h>

#include <string.h>

static BCRYPT_ALG_HANDLE g_aes_ecb;
static BCRYPT_ALG_HANDLE g_aes_cbc;

static sf_result_t aes_alg(BCRYPT_ALG_HANDLE *out, const wchar_t *mode) {
    if (*out) return SF_OK;
    BCRYPT_ALG_HANDLE alg = NULL;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(st)) return SF_ERR_CRYPTO;
    st = BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, (PUCHAR)mode,
                           (ULONG)((wcslen(mode) + 1u) * sizeof(wchar_t)), 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return SF_ERR_CRYPTO;
    }
    *out = alg;
    return SF_OK;
}

static sf_result_t validate_aes_key(size_t key_len) {
    return (key_len == 16u || key_len == 24u || key_len == 32u) ? SF_OK : SF_ERR_INVALID_ARG;
}

sf_result_t sfi_aes_ecb_block(const void *key, size_t key_len, bool encrypt,
                              const uint8_t in[16], uint8_t out[16]) {
    SF_CHECK_ARG(key && in && out);
    SF_RETURN_IF(validate_aes_key(key_len) != SF_OK, SF_ERR_INVALID_ARG);
    SF_RETURN_IF(aes_alg(&g_aes_ecb, BCRYPT_CHAIN_MODE_ECB) != SF_OK, SF_ERR_CRYPTO);

    BCRYPT_KEY_HANDLE hkey = NULL;
    NTSTATUS st = BCryptGenerateSymmetricKey(g_aes_ecb, &hkey, NULL, 0, (PUCHAR)key,
                                             (ULONG)key_len, 0);
    if (!BCRYPT_SUCCESS(st)) return SF_ERR_CRYPTO;

    ULONG done = 0;
    if (encrypt) {
        st = BCryptEncrypt(hkey, (PUCHAR)in, 16, NULL, NULL, 0, out, 16, &done, 0);
    } else {
        st = BCryptDecrypt(hkey, (PUCHAR)in, 16, NULL, NULL, 0, out, 16, &done, 0);
    }
    BCryptDestroyKey(hkey);
    return (BCRYPT_SUCCESS(st) && done == 16u) ? SF_OK : SF_ERR_CRYPTO;
}

static sf_result_t aes_cbc_crypt(const void *key, size_t key_len, const void *iv, size_t iv_len,
                                 const void *in, size_t n, void *out, bool encrypt) {
    SF_CHECK_ARG(key && iv && in && out);
    SF_RETURN_IF(validate_aes_key(key_len) != SF_OK, SF_ERR_INVALID_ARG);
    SF_RETURN_IF(iv_len != 16u || (n % 16u) != 0u, SF_ERR_INVALID_ARG);
    SF_RETURN_IF(aes_alg(&g_aes_cbc, BCRYPT_CHAIN_MODE_CBC) != SF_OK, SF_ERR_CRYPTO);

    BCRYPT_KEY_HANDLE hkey = NULL;
    NTSTATUS st = BCryptGenerateSymmetricKey(g_aes_cbc, &hkey, NULL, 0, (PUCHAR)key,
                                             (ULONG)key_len, 0);
    if (!BCRYPT_SUCCESS(st)) return SF_ERR_CRYPTO;

    uint8_t iv_scratch[16];
    memcpy(iv_scratch, iv, sizeof(iv_scratch));
    ULONG done = 0;
    if (encrypt) {
        st = BCryptEncrypt(hkey, (PUCHAR)in, (ULONG)n, NULL, iv_scratch, 16, (PUCHAR)out,
                           (ULONG)n, &done, 0);
    } else {
        st = BCryptDecrypt(hkey, (PUCHAR)in, (ULONG)n, NULL, iv_scratch, 16, (PUCHAR)out,
                           (ULONG)n, &done, 0);
    }
    BCryptDestroyKey(hkey);
    return (BCRYPT_SUCCESS(st) && done == n) ? SF_OK : SF_ERR_CRYPTO;
}

sf_result_t sfi_aes_decrypt_cbc(const void *key, size_t key_len, const void *iv, size_t iv_len,
                                const void *in, size_t n, void *out) {
    return aes_cbc_crypt(key, key_len, iv, iv_len, in, n, out, false);
}

sf_result_t sfi_aes_encrypt_cbc(const void *key, size_t key_len, const void *iv, size_t iv_len,
                                const void *in, size_t n, void *out) {
    return aes_cbc_crypt(key, key_len, iv, iv_len, in, n, out, true);
}

sf_result_t sfi_aes_decrypt_ecb_buffer(const uint8_t key[16], uint8_t *buf, size_t size) {
    SF_CHECK_ARG(key && buf);
    SF_RETURN_IF((size % 16u) != 0u, SF_ERR_INVALID_ARG);
    if (size == 0u) return SF_OK;
    SF_RETURN_IF(aes_alg(&g_aes_ecb, BCRYPT_CHAIN_MODE_ECB) != SF_OK, SF_ERR_CRYPTO);

    BCRYPT_KEY_HANDLE hkey = NULL;
    NTSTATUS st = BCryptGenerateSymmetricKey(g_aes_ecb, &hkey, NULL, 0, (PUCHAR)key, 16, 0);
    if (!BCRYPT_SUCCESS(st)) return SF_ERR_CRYPTO;

    ULONG done = 0;
    st = BCryptDecrypt(hkey, buf, (ULONG)size, NULL, NULL, 0, buf, (ULONG)size, &done, 0);
    BCryptDestroyKey(hkey);
    return (BCRYPT_SUCCESS(st) && done == size) ? SF_OK : SF_ERR_CRYPTO;
}

void sfi_crypto_shutdown(void) {
    if (g_aes_ecb) BCryptCloseAlgorithmProvider(g_aes_ecb, 0);
    if (g_aes_cbc) BCryptCloseAlgorithmProvider(g_aes_cbc, 0);
    g_aes_ecb = NULL;
    g_aes_cbc = NULL;
}
