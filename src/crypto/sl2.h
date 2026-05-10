/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_CRYPTO_SL2_H
#define SF_CRYPTO_SL2_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_sl2.h"

#include <stddef.h>
#include <stdint.h>

sf_result_t sfi_sl2_decrypt(const void *key, size_t key_len, const void *in, size_t in_size,
                            void **out, size_t *out_size, const sf_allocator_t *a);
sf_result_t sfi_sl2_encrypt(const void *key, size_t key_len, const void *in, size_t in_size,
                            void **out, size_t *out_size, const sf_allocator_t *a);

#endif /* SF_CRYPTO_SL2_H */
