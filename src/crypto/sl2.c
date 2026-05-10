/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "crypto/sl2.h"
#include "crypto/aes_cng.h"
#include "crypto/md5_cng.h"
#include "internal/sf_internal.h"

#include <windows.h>
#include <bcrypt.h>

#include <string.h>

/*===========================================================================
 * SL2 PC keys — verbatim from upstream SL2Decryptor.cs (lines 12-14).
 *===========================================================================*/

static const uint8_t g_sl2_ds2_key[16] = {
    0xB7, 0xFD, 0x46, 0x3E, 0x4A, 0x9C, 0x11, 0x02,
    0xDF, 0x17, 0x39, 0xE5, 0xF3, 0xB2, 0xA5, 0x0F,
};
static const uint8_t g_sl2_scholar_key[16] = {
    0x59, 0x9F, 0x9B, 0x69, 0x96, 0x40, 0xA5, 0x52,
    0x36, 0xEE, 0x2D, 0x70, 0x83, 0x5E, 0xC7, 0x44,
};
static const uint8_t g_sl2_ds3_key[16] = {
    0xFD, 0x46, 0x4D, 0x69, 0x5E, 0x69, 0xA3, 0x9A,
    0x10, 0xE3, 0x19, 0xA7, 0xAC, 0xE8, 0xB7, 0xFA,
};

/*===========================================================================
 * Internal byte-level (de)cryptors — used by both the public API and other
 * src/crypto modules that need the raw envelope (e.g. SL2 BND4 wrappers).
 *===========================================================================*/

sf_result_t sfi_sl2_decrypt(const void *key, size_t key_len, const void *in, size_t in_size,
                            void **out, size_t *out_size, const sf_allocator_t *a) {
    SF_CHECK_ARG(key && in && out && out_size);
    SF_RETURN_IF(in_size < 32u || ((in_size - 32u) % 16u) != 0u, SF_ERR_INVALID_ARG);
    size_t n = in_size - 32u;
    uint8_t *plain = (uint8_t *)sf_xalloc(a, n ? n : 16u);
    if (!plain) return SF_ERR_OOM;
    sf_result_t r = sfi_aes_decrypt_cbc(key, key_len, (const uint8_t *)in + 16u, 16u,
                                        (const uint8_t *)in + 32u, n, plain);
    if (r != SF_OK) { sf_xfree(a, plain); return r; }
    *out = plain;
    *out_size = n;
    return SF_OK;
}

sf_result_t sfi_sl2_encrypt(const void *key, size_t key_len, const void *in, size_t in_size,
                            void **out, size_t *out_size, const sf_allocator_t *a) {
    SF_CHECK_ARG(key && (in || in_size == 0u) && out && out_size);
    SF_RETURN_IF((in_size % 16u) != 0u, SF_ERR_INVALID_ARG);
    uint8_t *buf = (uint8_t *)sf_xalloc(a, 32u + in_size);
    if (!buf) return SF_ERR_OOM;
    NTSTATUS st = BCryptGenRandom(NULL, buf + 16u, 16u, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(st)) { sf_xfree(a, buf); return SF_ERR_CRYPTO; }
    sf_result_t r = sfi_aes_encrypt_cbc(key, key_len, buf + 16u, 16u, in, in_size, buf + 32u);
    if (r == SF_OK) r = sfi_md5_hash(buf + 16u, 16u + in_size, buf);
    if (r != SF_OK) { sf_xfree(a, buf); return r; }
    *out = buf;
    *out_size = 32u + in_size;
    return SF_OK;
}

/*===========================================================================
 * Public API — borrowed-pointer key getters + thin wrappers around the
 * internal byte-level (de)cryptors with a fixed 16-byte AES-128 key.
 *===========================================================================*/

sf_result_t sf_sl2_get_ds2_key(const uint8_t **out_key_16) {
    SF_CHECK_ARG(out_key_16);
    *out_key_16 = g_sl2_ds2_key;
    return SF_OK;
}

sf_result_t sf_sl2_get_scholar_key(const uint8_t **out_key_16) {
    SF_CHECK_ARG(out_key_16);
    *out_key_16 = g_sl2_scholar_key;
    return SF_OK;
}

sf_result_t sf_sl2_get_ds3_key(const uint8_t **out_key_16) {
    SF_CHECK_ARG(out_key_16);
    *out_key_16 = g_sl2_ds3_key;
    return SF_OK;
}

sf_result_t sf_sl2_decrypt(const uint8_t *in, size_t in_size, const uint8_t *key_16,
                           uint8_t **out, size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(key_16 && out);
    void *raw = NULL;
    sf_result_t r = sfi_sl2_decrypt(key_16, 16u, in, in_size, &raw, out_size, alloc);
    if (r != SF_OK) return r;
    *out = (uint8_t *)raw;
    return SF_OK;
}

sf_result_t sf_sl2_encrypt(const uint8_t *in, size_t in_size, const uint8_t *key_16,
                           uint8_t **out, size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(key_16 && out);
    void *raw = NULL;
    sf_result_t r = sfi_sl2_encrypt(key_16, 16u, in, in_size, &raw, out_size, alloc);
    if (r != SF_OK) return r;
    *out = (uint8_t *)raw;
    return SF_OK;
}
