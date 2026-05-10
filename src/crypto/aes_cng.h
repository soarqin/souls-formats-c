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
void sfi_crypto_shutdown(void);

#endif /* SF_CRYPTO_AES_CNG_H */
