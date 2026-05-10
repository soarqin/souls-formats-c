/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_regulation.h"

#include "crypto/regulation.h"
#include "crypto/aes_cng.h"
#include "internal/sf_internal.h"

#include <string.h>

sf_result_t sf_regulation_decrypt(const uint8_t *in, size_t in_size,
                                  sf_regulation_key_t key,
                                  uint8_t **out, size_t *out_size,
                                  const sf_allocator_t *alloc) {
    SF_CHECK_ARG(in && out && out_size);
    SF_RETURN_IF(in_size < 16u, SF_ERR_TRUNCATED);

    const uint8_t *key_bytes = NULL;
    SF_RETURN_IF(sfi_regulation_key(key, &key_bytes) != SF_OK, SF_ERR_INVALID_ARG);

    size_t cipher_size = in_size - 16u;
    size_t padded = (cipher_size + 15u) & ~(size_t)15u;
    if (padded == 0u) padded = 16u;

    uint8_t *cipher = (uint8_t *)sf_xalloc(alloc, padded);
    if (!cipher) return SF_ERR_OOM;
    memset(cipher, 0, padded);
    memcpy(cipher, in + 16u, cipher_size);

    uint8_t *plain = (uint8_t *)sf_xalloc(alloc, padded);
    if (!plain) {
        sf_xfree(alloc, cipher);
        return SF_ERR_OOM;
    }

    sf_result_t r = sfi_aes_decrypt_cbc(key_bytes, 32u, in, 16u, cipher, padded, plain);
    sf_xfree(alloc, cipher);
    if (r != SF_OK) {
        sf_xfree(alloc, plain);
        return r;
    }

    *out = plain;
    *out_size = padded;
    return SF_OK;
}

sf_result_t sf_regulation_encrypt(const uint8_t *in, size_t in_size,
                                  sf_regulation_key_t key,
                                  uint8_t **out, size_t *out_size,
                                  const sf_allocator_t *alloc) {
    SF_CHECK_ARG((in || in_size == 0u) && out && out_size);

    const uint8_t *key_bytes = NULL;
    SF_RETURN_IF(sfi_regulation_key(key, &key_bytes) != SF_OK, SF_ERR_INVALID_ARG);

    size_t pad = 16u - (in_size % 16u);
    if (pad == 0u) pad = 16u;
    size_t cipher_size = in_size + pad;

    uint8_t *buf = (uint8_t *)sf_xalloc(alloc, 16u + cipher_size);
    if (!buf) return SF_ERR_OOM;
    memset(buf, 0, 16u + cipher_size);
    if (in_size != 0u) memcpy(buf + 16u, in, in_size);
    memset(buf + 16u + in_size, (int)pad, pad);

    sf_result_t r = sfi_aes_encrypt_cbc(key_bytes, 32u, buf, 16u, buf + 16u, cipher_size,
                                        buf + 16u);
    if (r != SF_OK) {
        sf_xfree(alloc, buf);
        return r;
    }

    *out = buf;
    *out_size = 16u + cipher_size;
    return SF_OK;
}

sf_result_t sf_regulation_decrypt_ds3(const uint8_t *in, size_t in_size,
                                      uint8_t **out, size_t *out_size,
                                      const sf_allocator_t *alloc) {
    return sf_regulation_decrypt(in, in_size, SF_REGULATION_KEY_DARK_SOULS_3, out, out_size,
                                 alloc);
}

sf_result_t sf_regulation_encrypt_ds3(const uint8_t *in, size_t in_size,
                                      uint8_t **out, size_t *out_size,
                                      const sf_allocator_t *alloc) {
    return sf_regulation_encrypt(in, in_size, SF_REGULATION_KEY_DARK_SOULS_3, out, out_size,
                                 alloc);
}

sf_result_t sf_regulation_decrypt_er(const uint8_t *in, size_t in_size,
                                     uint8_t **out, size_t *out_size,
                                     const sf_allocator_t *alloc) {
    return sf_regulation_decrypt(in, in_size, SF_REGULATION_KEY_ELDEN_RING, out, out_size,
                                 alloc);
}

sf_result_t sf_regulation_encrypt_er(const uint8_t *in, size_t in_size,
                                     uint8_t **out, size_t *out_size,
                                     const sf_allocator_t *alloc) {
    return sf_regulation_encrypt(in, in_size, SF_REGULATION_KEY_ELDEN_RING, out, out_size,
                                 alloc);
}

sf_result_t sf_regulation_decrypt_ac6(const uint8_t *in, size_t in_size,
                                      uint8_t **out, size_t *out_size,
                                      const sf_allocator_t *alloc) {
    return sf_regulation_decrypt(in, in_size, SF_REGULATION_KEY_ARMORED_CORE_6, out, out_size,
                                 alloc);
}

sf_result_t sf_regulation_encrypt_ac6(const uint8_t *in, size_t in_size,
                                      uint8_t **out, size_t *out_size,
                                      const sf_allocator_t *alloc) {
    return sf_regulation_encrypt(in, in_size, SF_REGULATION_KEY_ARMORED_CORE_6, out, out_size,
                                 alloc);
}

sf_result_t sf_regulation_decrypt_ernr(const uint8_t *in, size_t in_size,
                                       uint8_t **out, size_t *out_size,
                                       const sf_allocator_t *alloc) {
    return sf_regulation_decrypt(in, in_size, SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN, out,
                                 out_size, alloc);
}

/*  NIGHTREIGN ENCRYPT QUIRK: upstream `EncryptERNRRegulation`
 *  (RegulationDecryptor.cs:117) calls `EncryptRegulationWithKey(path, bnd,
 *  RegulationKey.EldenRing)` — i.e. it uses the EldenRing key, NOT the
 *  Nightreign key. We faithfully mirror that bit-identical behavior here so
 *  files produced by the C port match upstream-tooling output byte-for-byte.
 *  Callers wanting the true Nightreign key must call sf_regulation_encrypt()
 *  directly with SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN.
 */
sf_result_t sf_regulation_encrypt_ernr(const uint8_t *in, size_t in_size,
                                       uint8_t **out, size_t *out_size,
                                       const sf_allocator_t *alloc) {
    return sf_regulation_encrypt(in, in_size, SF_REGULATION_KEY_ELDEN_RING, out, out_size,
                                 alloc);
}
