/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — RegulationDecryptor: AES-256-CBC byte-level (de)encryption
 * for FromSoftware regulation BND4 archives.
 *
 * Upstream: SoulsFormats/Utilities/Cryptography/RegulationDecryptor.cs at
 * pinned commit 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a.
 *
 * Wire format: every regulation file begins with a 16-byte AES IV, followed
 * by AES-256-CBC ciphertext. Encryption uses PKCS#7 padding; decryption uses
 * no-padding mode and zero-pads the ciphertext to a 16-byte multiple before
 * driving the cipher (this matches upstream `DecryptByteArray`'s "Epic
 * Encryption Technology" comment).
 *
 * Phase 2 scope: byte-buffer-level (de)encryption only. BND4-aware overloads
 * of the form `sf_regulation_*_bnd4()` will land in Phase 3 once `sf_bnd4_t`
 * becomes a public type.
 */

#ifndef SOULS_FORMATS_SF_REGULATION_H
#define SOULS_FORMATS_SF_REGULATION_H

#include "sf_common.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Regulation key identifier
 *
 * Mirrors upstream RegulationDecryptor.RegulationKey enum. Ordinals match
 * upstream exactly so callers can reuse cross-tool tables verbatim.
 *===========================================================================*/
typedef enum sf_regulation_key {
    SF_REGULATION_KEY_DARK_SOULS_3 = 0,
    SF_REGULATION_KEY_ELDEN_RING = 1,
    SF_REGULATION_KEY_ARMORED_CORE_6 = 2,
    SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN = 3
} sf_regulation_key_t;

#if defined(__cplusplus)
#define SF_REGULATION_STATIC_ASSERT static_assert
#else
#define SF_REGULATION_STATIC_ASSERT _Static_assert
#endif

SF_REGULATION_STATIC_ASSERT(SF_REGULATION_KEY_DARK_SOULS_3 == 0,
                            "sf_regulation_key_t drift");
SF_REGULATION_STATIC_ASSERT(SF_REGULATION_KEY_ELDEN_RING == 1,
                            "sf_regulation_key_t drift");
SF_REGULATION_STATIC_ASSERT(SF_REGULATION_KEY_ARMORED_CORE_6 == 2,
                            "sf_regulation_key_t drift");
SF_REGULATION_STATIC_ASSERT(SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN == 3,
                            "sf_regulation_key_t drift");

#undef SF_REGULATION_STATIC_ASSERT

/*===========================================================================
 * Generic byte-buffer (de)encryption
 *
 * Both functions allocate the output buffer via `alloc` (NULL = default
 * malloc/free); ownership transfers to the caller, who must release it via
 * `sf_free(alloc, *out)`. On any error the output pointer is left untouched.
 *
 * BND4 overloads (sf_regulation_*_bnd4) will land in Phase 3.
 *===========================================================================*/

/*  Decrypt a regulation byte buffer with the specified key.
 *
 *  Reads the leading 16 bytes as the AES IV and AES-256-CBC-decrypts the
 *  remainder using `key` (no padding, internally zero-padded to 16-byte
 *  multiple). Output size is the 16-byte-aligned ciphertext length, not the
 *  inner BND4 length — callers reading BND4 must use Phase 3 overloads or
 *  feed the result through `sf_bnd4_read()`.
 *
 *  Mirrors upstream `RegulationDecryptor.DecryptBndWithKey(..., RegulationKey)`
 *  byte-array pipeline.
 */
SF_API sf_result_t sf_regulation_decrypt(const uint8_t *in, size_t in_size,
                                         sf_regulation_key_t key,
                                         uint8_t **out, size_t *out_size,
                                         const sf_allocator_t *alloc);

/*  Encrypt a regulation plaintext with the specified key.
 *
 *  Generates a 16-byte zero IV, AES-256-CBC-encrypts the plaintext with PKCS#7
 *  padding, and returns the IV concatenated with the ciphertext. Output size
 *  is `16 + ((in_size / 16) + 1) * 16`.
 *
 *  Mirrors upstream `RegulationDecryptor.EncryptRegulationWithKey(..., RegulationKey)`
 *  byte-array pipeline.
 */
SF_API sf_result_t sf_regulation_encrypt(const uint8_t *in, size_t in_size,
                                         sf_regulation_key_t key,
                                         uint8_t **out, size_t *out_size,
                                         const sf_allocator_t *alloc);

/*===========================================================================
 * Game-specific convenience wrappers
 *
 * Each wrapper forwards to the generic sf_regulation_{decrypt,encrypt}
 * with the matching SF_REGULATION_KEY_* constant.
 *
 * NIGHTREIGN ENCRYPT QUIRK: sf_regulation_encrypt_ernr() forwards with
 * SF_REGULATION_KEY_ELDEN_RING (NOT _ELDEN_RING_NIGHTREIGN), faithfully
 * mirroring upstream `EncryptERNRRegulation` in RegulationDecryptor.cs:117.
 * Decryption uses the matching Nightreign key (sf_regulation_decrypt_ernr).
 * Callers wanting the true Nightreign encryption key must invoke
 * sf_regulation_encrypt() directly with SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN.
 *===========================================================================*/

/* Dark Souls 3 */
SF_API sf_result_t sf_regulation_decrypt_ds3(const uint8_t *in, size_t in_size,
                                             uint8_t **out, size_t *out_size,
                                             const sf_allocator_t *alloc);
SF_API sf_result_t sf_regulation_encrypt_ds3(const uint8_t *in, size_t in_size,
                                             uint8_t **out, size_t *out_size,
                                             const sf_allocator_t *alloc);

/* Elden Ring */
SF_API sf_result_t sf_regulation_decrypt_er(const uint8_t *in, size_t in_size,
                                            uint8_t **out, size_t *out_size,
                                            const sf_allocator_t *alloc);
SF_API sf_result_t sf_regulation_encrypt_er(const uint8_t *in, size_t in_size,
                                            uint8_t **out, size_t *out_size,
                                            const sf_allocator_t *alloc);

/* Armored Core VI */
SF_API sf_result_t sf_regulation_decrypt_ac6(const uint8_t *in, size_t in_size,
                                             uint8_t **out, size_t *out_size,
                                             const sf_allocator_t *alloc);
SF_API sf_result_t sf_regulation_encrypt_ac6(const uint8_t *in, size_t in_size,
                                             uint8_t **out, size_t *out_size,
                                             const sf_allocator_t *alloc);

/* Elden Ring: Nightreign — see "NIGHTREIGN ENCRYPT QUIRK" above. */
SF_API sf_result_t sf_regulation_decrypt_ernr(const uint8_t *in, size_t in_size,
                                              uint8_t **out, size_t *out_size,
                                              const sf_allocator_t *alloc);
SF_API sf_result_t sf_regulation_encrypt_ernr(const uint8_t *in, size_t in_size,
                                              uint8_t **out, size_t *out_size,
                                              const sf_allocator_t *alloc);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_REGULATION_H */
