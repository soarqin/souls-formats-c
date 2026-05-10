/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_CRYPTO_RSA_CNG_H
#define SF_CRYPTO_RSA_CNG_H

#include "souls_formats/sf_common.h"

#include <stddef.h>
#include <stdint.h>

/* Decrypt one RSA-encrypted block using BCRYPT_PAD_NONE (raw modular
 * exponentiation, no PKCS#1 unpadding by CNG).
 *
 * Used by BHD5 reader to unwrap FromSoftware's sign-encrypted archive
 * headers: the game has signed each ciphertext block with its private key
 * using raw RSA, and we recover the bytes by "decrypting" with the
 * matching public key.
 *
 * pem_public_key
 *     Null-terminated PEM string. Both X.509 SubjectPublicKeyInfo
 *     ("-----BEGIN PUBLIC KEY-----") and PKCS#1 RSAPublicKey
 *     ("-----BEGIN RSA PUBLIC KEY-----") are accepted; the latter is the
 *     form used by all four shipped game keys.
 * in / in_size
 *     Encrypted input. Size must equal the key modulus byte length
 *     (256 for the 2048-bit FromSoft keys).
 * out / out_size
 *     On success, *out is a heap buffer with the recovered plaintext and
 *     *out_size is its byte length. The buffer is allocated through `alloc`
 *     and the caller must free it with sf_free(alloc, *out). Leading 0x00
 *     padding bytes from the raw modular exponentiation result are
 *     stripped before return.
 * alloc
 *     Allocator (NULL = default). Used both internally and for *out.
 *
 * Returns SF_OK on success, SF_ERR_INVALID_ARG for bad pointers/sizes,
 * SF_ERR_OOM on allocation failure, or SF_ERR_CRYPTO on any CNG/PEM
 * decode failure. Internal-only — do NOT decorate with SF_API. */
sf_result_t sfi_rsa_decrypt_pkcs1(const char *pem_public_key,
                                  const uint8_t *in, size_t in_size,
                                  uint8_t **out, size_t *out_size,
                                  const sf_allocator_t *alloc);

#endif /* SF_CRYPTO_RSA_CNG_H */
