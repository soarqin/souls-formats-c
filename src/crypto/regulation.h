/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Internal regulation helpers. Public API lives in
 * <souls_formats/sf_regulation.h>. Only the key lookup is exposed here for
 * cross-TU use within src/crypto/.
 */

#ifndef SF_CRYPTO_REGULATION_H
#define SF_CRYPTO_REGULATION_H

#include "souls_formats/sf_regulation.h"

#include <stddef.h>
#include <stdint.h>

sf_result_t sfi_regulation_key(sf_regulation_key_t key, const uint8_t **out_key);

#endif /* SF_CRYPTO_REGULATION_H */
