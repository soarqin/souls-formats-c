/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — BHD5 archive header (skeleton).
 *
 * BHD5 is the dvdbnd container header used to package game files with
 * hashed filenames in modern FromSoftware titles (Sekiro, Elden Ring,
 * Nightreign, Armored Core VI in v1).
 *
 * Upstream reference: SoulsFormats/Formats/BHD5.cs
 *   https://github.com/soulsmods/SoulsFormatsNEXT/blob/.../BHD5.cs
 *
 * Upstream punts on the RSA-encryption layer that wraps the BHD5 file on
 * disk: BHD5.Read accepts a path/byte[] but says "Must already be
 * decrypted, if applicable." We integrate RSA decrypt inside the reader
 * (see sfi_rsa_decrypt_pkcs1 + sfi_bhd5_get_pem_key) so callers can hand
 * in an encrypted .bhd directly. Tracked as an extension in
 * docs/api-mapping/extensions.md.
 *
 * This skeleton header lands T0d (Phase 3 prep): opaque type + game enum
 * only. Read/write/extract entry points come in T10 of Phase 3.
 */

#ifndef SOULS_FORMATS_SF_BHD5_H
#define SOULS_FORMATS_SF_BHD5_H

#include "souls_formats/sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Opaque BHD5 reader handle
 *
 * Defined in src/archive/bhd5.c (Phase 3 / T10). Construct via the
 * sf_bhd5_read_* APIs (added in T10), destroy via sf_bhd5_destroy.
 *===========================================================================*/
typedef struct sf_bhd5 sf_bhd5_t;

/*===========================================================================
 * Game enum for BHD5 key + format selection
 *
 * v1 covers four target games. Older titles (DS1, DS1R, DS2, DS3) are
 * deferred to v2+; see legacy.md and extensions.md for the rationale.
 *===========================================================================*/
typedef enum sf_bhd5_game {
    SF_BHD5_GAME_SEKIRO       = 0,
    SF_BHD5_GAME_ELDENRING    = 1,
    SF_BHD5_GAME_NIGHTREIGN   = 2,
    SF_BHD5_GAME_ARMOREDCORE6 = 3,
    /* Sentinel — must remain last; do not use as a real value. */
    SF_BHD5_GAME_COUNT_
} sf_bhd5_game_t;

_Static_assert(SF_BHD5_GAME_COUNT_ == 4, "sf_bhd5_game_t drift");

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_BHD5_H */
