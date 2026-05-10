/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_CRYPTO_MD5_CNG_H
#define SF_CRYPTO_MD5_CNG_H

#include "souls_formats/sf_common.h"

#include <stddef.h>
#include <stdint.h>

sf_result_t sfi_md5_hash(const void *data, size_t size, uint8_t out[16]);

#endif /* SF_CRYPTO_MD5_CNG_H */
