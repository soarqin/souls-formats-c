/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Internal-only helpers shared across all src subdirectories.
 * NEVER include this from public headers.
 */

#ifndef SF_INTERNAL_H
#define SF_INTERNAL_H

#include "souls_formats/sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*===========================================================================
 * Allocator helpers
 *===========================================================================*/

static inline const sf_allocator_t *sf_alloc_or_default(const sf_allocator_t *a) {
    return a ? a : sf_default_allocator();
}

static inline void *sf_xalloc(const sf_allocator_t *a, size_t size) {
    a = sf_alloc_or_default(a);
    return a->alloc(size, a->user);
}

static inline void *sf_xrealloc(const sf_allocator_t *a, void *p,
                                size_t old_size, size_t new_size) {
    a = sf_alloc_or_default(a);
    return a->realloc(p, old_size, new_size, a->user);
}

static inline void sf_xfree(const sf_allocator_t *a, void *p) {
    if (!p) return;
    a = sf_alloc_or_default(a);
    a->free(p, a->user);
}

/*===========================================================================
 * Endian helpers (host = little on x86_64; we always emit/expect little
 * for "small" reads then byte-swap manually for big-endian).
 *===========================================================================*/

static inline uint16_t sf_bswap16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

static inline uint32_t sf_bswap32(uint32_t v) {
    return ((v >> 24) & 0x000000FFu) | ((v >> 8) & 0x0000FF00u) |
           ((v << 8) & 0x00FF0000u) | ((v << 24) & 0xFF000000u);
}

static inline uint64_t sf_bswap64(uint64_t v) {
    v = ((v & 0x00000000FFFFFFFFULL) << 32) | ((v & 0xFFFFFFFF00000000ULL) >> 32);
    v = ((v & 0x0000FFFF0000FFFFULL) << 16) | ((v & 0xFFFF0000FFFF0000ULL) >> 16);
    v = ((v & 0x00FF00FF00FF00FFULL) << 8)  | ((v & 0xFF00FF00FF00FF00ULL) >> 8);
    return v;
}

/*===========================================================================
 * Internal stream API (read/write hooks reused by binary reader/writer)
 *
 * Callers in src/core/binary_reader.c and src/core/binary_writer.c access
 * the underlying stream through these forwarders; nobody else should care.
 *===========================================================================*/

typedef struct sf_istream sf_istream_t;
typedef struct sf_ostream sf_ostream_t;

const sf_allocator_t *sfi_istream_allocator(const sf_istream_t *s);
const sf_allocator_t *sfi_ostream_allocator(const sf_ostream_t *s);
sf_result_t sfi_ostream_to_array(const sf_ostream_t *s, const sf_allocator_t *a,
                                 uint8_t **out, size_t *out_size);

/*===========================================================================
 * Argument check macro — common path of returning SF_ERR_INVALID_ARG.
 *===========================================================================*/

#define SF_RETURN_IF(cond, code) do { if ((cond)) return (code); } while (0)
#define SF_CHECK_ARG(cond)       SF_RETURN_IF(!(cond), SF_ERR_INVALID_ARG)

/*===========================================================================
 * Heap string duplicate via allocator.
 *===========================================================================*/

static inline char *sf_strdup(const sf_allocator_t *a, const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)sf_xalloc(a, n);
    if (p) memcpy(p, s, n);
    return p;
}

#endif /* SF_INTERNAL_H */
