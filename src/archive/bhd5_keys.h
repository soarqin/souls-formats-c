/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_ARCHIVE_BHD5_KEYS_H
#define SF_ARCHIVE_BHD5_KEYS_H

#include "souls_formats/sf_bhd5.h"

#include <stdint.h>

const char *sfi_bhd5_get_pem_key(sf_bhd5_game_t game);

/* shard is the 1-based Sekiro Data shard index (1..5). Returns NULL for
 * out-of-range values. */
const char *sfi_bhd5_get_sekiro_shard_key(int shard);

#endif /* SF_ARCHIVE_BHD5_KEYS_H */
