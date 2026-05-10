/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — SL2 save-file byte-level cryptography.
 *
 * Public mirror of upstream SoulsFormats `Cryptography/SL2Decryptor.cs`.
 * Provides the three known PC keys (DS2, DS2 Scholar, DS3) plus the
 * AES-128-CBC + MD5 envelope decrypt/encrypt helpers used to
 * (de)serialise raw `.sl2` save blobs.
 *
 * Envelope layout (matches upstream byte-for-byte):
 *
 *     [ 0..16) MD5 of (IV || ciphertext)
 *     [16..32) AES IV
 *     [32..  ) AES-128-CBC ciphertext (PaddingMode.None,
 *              must be a whole number of 16-byte blocks)
 *
 * NOTE: this header exposes only the byte-level primitives; high-level
 * SL2 file (BND4) parsing is layered on top in a future phase.
 */

#ifndef SOULS_FORMATS_SF_SL2_H
#define SOULS_FORMATS_SF_SL2_H

#include "souls_formats/sf_common.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Key getters
 *
 * Each getter returns a pointer to a 16-byte static buffer owned by the
 * library. The pointer is valid for the entire program lifetime and MUST
 * NOT be freed by the caller. The bytes are read-only.
 *
 * Mirrors upstream `SL2Decryptor.GetDS2SaveKey` /
 * `GetScholarSaveKey` / `GetDS3SaveKey`. Upstream returns a heap-allocated
 * copy; we return a borrowed pointer to avoid a needless allocation.
 *===========================================================================*/

/* Key used by original DS2 PC saves. */
SF_API sf_result_t sf_sl2_get_ds2_key(const uint8_t **out_key_16);

/* Key used by DS2 Scholar of the First Sin PC saves. */
SF_API sf_result_t sf_sl2_get_scholar_key(const uint8_t **out_key_16);

/* Key used by DS3 PC saves. */
SF_API sf_result_t sf_sl2_get_ds3_key(const uint8_t **out_key_16);

/*===========================================================================
 * Decrypt
 *
 * Decrypts a single SL2 sub-file with the supplied 16-byte key.
 *
 * `in` MUST point to the full 32-byte envelope header followed by the
 * ciphertext, i.e. `in_size >= 32` and `(in_size - 32) % 16 == 0`. The
 * leading 16-byte MD5 hash is NOT verified (matches upstream).
 *
 * `*out` is heap-owned by the caller and must be released via
 *   `sf_free(alloc, *out)` (using the same `alloc` as the call).
 * `*out_size` receives the plaintext length, equal to `in_size - 32`.
 *
 * Mirrors upstream `SL2Decryptor.DecryptSL2File`.
 *===========================================================================*/
SF_API sf_result_t sf_sl2_decrypt(const uint8_t *in, size_t in_size,
                                  const uint8_t *key_16, uint8_t **out,
                                  size_t *out_size,
                                  const sf_allocator_t *alloc);

/*===========================================================================
 * Encrypt
 *
 * Encrypts a single SL2 sub-file with the supplied 16-byte key, generating
 * a fresh 16-byte IV via `BCryptGenRandom`. `in_size` MUST be a multiple
 * of 16 bytes (AES block size; upstream uses `PaddingMode.None`).
 *
 * `*out` is heap-owned by the caller and must be released via
 *   `sf_free(alloc, *out)` (using the same `alloc` as the call).
 * `*out_size` receives `32 + in_size` (the full envelope).
 *
 * Mirrors upstream `SL2Decryptor.EncryptSL2File`.
 *===========================================================================*/
SF_API sf_result_t sf_sl2_encrypt(const uint8_t *in, size_t in_size,
                                  const uint8_t *key_16, uint8_t **out,
                                  size_t *out_size,
                                  const sf_allocator_t *alloc);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_SL2_H */
