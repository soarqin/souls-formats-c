/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — filename hash for BND / BHD lookup tables.
 */

#ifndef SOULS_FORMATS_SF_HASH_H
#define SOULS_FORMATS_SF_HASH_H

#include "souls_formats/sf_common.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * FromSoftware path hash, used by BND4 / BHD5 hash tables.
 *
 * Algorithm (mirrors upstream HashHelper.FromPathHash):
 *   1. Lowercase the path (ASCII-only fold; non-ASCII bytes pass through).
 *   2. Replace '\\' with '/'.
 *   3. Prepend '/' if not already prefixed.
 *   4. Fold uint32_t accumulator: for each byte c, acc = acc * 37 + c.
 *
 * `utf8_path` must be NUL-terminated UTF-8. Real game paths are pure ASCII;
 * UTF-8 bytes outside ASCII are passed through unchanged (which matches
 * upstream behaviour because String.ToLowerInvariant() leaves non-letter
 * code units untouched).
 */
SF_API uint32_t sf_path_hash(const char *utf8_path);

/*
 * 64-bit-wide variant of sf_path_hash, for BHD5 ER+ tables.
 *
 * Elden Ring (and later) BHD5 files store the file-name hash as a UInt64
 * field on disk, even though the upstream HashHelper.FromPathHash returns
 * a 32-bit unsigned integer. Upstream relies on an implicit C# widening
 * conversion (`uint → ulong`); we expose a named wrapper so C callers
 * cannot accidentally truncate the 64-bit field when populating BHD5
 * APIs.
 *
 * Behaviour: identical to sf_path_hash, then zero-extended to 64 bits.
 * No separate algorithm exists upstream — this is a documented widening
 * cast (see docs/api-mapping/extensions.md).
 */
SF_API uint64_t sf_path_hash_64(const char *utf8_path);

/*
 * Trial-division primality test.
 *
 * Mirrors upstream HashHelper.IsPrime (HashHelper.cs:24-40):
 *   - candidate < 2     → false
 *   - candidate == 2    → true
 *   - even candidates   → false
 *   - otherwise: trial-divide by every odd integer 3,5,…, ⌊√candidate⌋
 *
 * Used by BHD5/BND4 hash-table sizing helpers in later phases.
 */
SF_API bool sf_is_prime(uint32_t candidate);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_HASH_H */
