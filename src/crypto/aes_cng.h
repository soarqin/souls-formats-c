/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_CRYPTO_AES_CNG_H
#define SF_CRYPTO_AES_CNG_H

#include "souls_formats/sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

sf_result_t sfi_aes_ecb_block(const void *key, size_t key_len, bool encrypt,
                              const uint8_t in[16], uint8_t out[16]);
sf_result_t sfi_aes_decrypt_cbc(const void *key, size_t key_len, const void *iv,
                                size_t iv_len, const void *in, size_t n, void *out);
sf_result_t sfi_aes_encrypt_cbc(const void *key, size_t key_len, const void *iv,
                                size_t iv_len, const void *in, size_t n, void *out);

/* Decrypt N×16 bytes in-place using AES-128-ECB.
 * key: 16-byte AES key. buf: N×16 bytes. size must be multiple of 16.
 * Returns SF_OK or SF_ERR_CRYPTO. Internal-only; no SF_API. */
sf_result_t sfi_aes_decrypt_ecb_buffer(const uint8_t key[16],
                                       uint8_t *buf, size_t size);

void sfi_crypto_shutdown(void);

#endif /* SF_CRYPTO_AES_CNG_H */
