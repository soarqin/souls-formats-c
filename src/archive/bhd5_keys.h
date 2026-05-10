/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_ARCHIVE_BHD5_KEYS_H
#define SF_ARCHIVE_BHD5_KEYS_H

#include "souls_formats/sf_bhd5.h"

#include <stdint.h>

/* Returns the embedded public PEM key for the main Data0.bhd archive of
 * `game`, or NULL if `game` is out of range. The returned pointer is
 * owned by static program data; do not free.
 *
 * Phase 3 / T10 will add per-archive variants (Data1..N, sd, sd_dlc02);
 * for the T0d skeleton we ship the Data0 / Data1 (Sekiro: Data1) key
 * which is the entry point for all four target games' main dvdbnd. */
const char *sfi_bhd5_get_pem_key(sf_bhd5_game_t game);

#endif /* SF_ARCHIVE_BHD5_KEYS_H */
