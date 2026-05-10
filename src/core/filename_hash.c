/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * sf_path_hash — FromSoftware filename hash for BND/BHD lookup tables.
 *
 * Mirrors upstream HashHelper.FromPathHash:
 *
 *     hashable = path.ToLowerInvariant().Replace('\\', '/');
 *     if (!hashable.StartsWith('/')) hashable = '/' + hashable;
 *     return hashable.Aggregate(0u, (i, c) => i * 37u + c);
 *
 * For ASCII paths (the only kind FromSoft ships) UTF-8 byte iteration is
 * 1:1 with C# char iteration, so we can fold byte-by-byte without decoding.
 */

#include "souls_formats/sf_hash.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

uint32_t sf_path_hash(const char *utf8_path) {
    if (!utf8_path) {
        utf8_path = "";
    }
    size_t len = strlen(utf8_path);

    uint32_t acc = 0u;

    /*  Determine if a leading slash already exists (treating '\\' the same
     *  as '/' since that's what the Replace step does). */
    bool has_leading_slash = (len > 0 && (utf8_path[0] == '/' || utf8_path[0] == '\\'));
    if (!has_leading_slash) {
        acc = acc * 37u + (uint32_t)'/';
    }

    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)utf8_path[i];

        /*  ToLowerInvariant for ASCII letters. (Non-ASCII bytes pass through
         *  untouched, which matches upstream behaviour for paths in the BMP
         *  range that aren't cased.) */
        if (c >= 'A' && c <= 'Z') {
            c = (uint8_t)(c - 'A' + 'a');
        }

        /*  Replace('\\', '/'). */
        if (c == '\\') {
            c = (uint8_t)'/';
        }

        acc = acc * 37u + (uint32_t)c;
    }

    return acc;
}

/*  sf_is_prime — mirrors upstream HashHelper.IsPrime (HashHelper.cs:24-40).
 *
 *  Algorithm transcript:
 *      if (candidate <  2) return false;
 *      if (candidate == 2) return true;
 *      if (candidate %  2 == 0) return false;
 *      for (i = 3; i*i <= candidate; i += 2)
 *          if (candidate % i == 0) return false;
 *      return true;
 *
 *  We use uint64_t for the loop counter so `i * i` cannot overflow the
 *  iteration bound for any uint32_t input — upstream uses C# `int i` which
 *  is promoted to `long` by the implicit comparison-against-uint conversion.
 *  The observable behaviour is identical for every candidate ≤ UINT32_MAX. */
bool sf_is_prime(uint32_t candidate) {
    if (candidate < 2u) {
        return false;
    }
    if (candidate == 2u) {
        return true;
    }
    if (candidate % 2u == 0u) {
        return false;
    }

    for (uint64_t i = 3u; i * i <= (uint64_t)candidate; i += 2u) {
        if (candidate % (uint32_t)i == 0u) {
            return false;
        }
    }

    return true;
}
