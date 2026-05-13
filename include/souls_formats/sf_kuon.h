/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Kuon legacy formats.
 *
 * Simplified read-only parser for Kuon BND (per-file binder, distinct from
 * the main DVDBND archive). No test data is available; tests rely on
 * synthetic binaries.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/Other/Kuon/BND.cs
 *
 * Kuon DVDBND is intentionally not exposed; see
 * docs/api-mapping/extensions.md for the deferral rationale.
 */

#ifndef SOULS_FORMATS_SF_KUON_H
#define SOULS_FORMATS_SF_KUON_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Kuon BND — per-file binder
 *
 * Header layout (little-endian):
 *   bytes[0..3]   = magic "BND\0"
 *   bytes[4..7]   = int32 file_version (200 or 202 observed upstream)
 *   bytes[8..11]  = int32 file_size
 *   bytes[12..15] = int32 file_count
 *===========================================================================*/

typedef struct sf_kuon_bnd sf_kuon_bnd_t;

/* Return true if @bytes starts with the Kuon BND magic. */
SF_API bool sf_kuon_bnd_is(const void *bytes, size_t size);

/* Read a Kuon BND header from memory. */
SF_API sf_result_t sf_kuon_bnd_read_from_memory(sf_kuon_bnd_t **out,
                                                const void *bytes, size_t size,
                                                const sf_allocator_t *alloc);

/* Destroy a Kuon BND. NULL-safe. */
SF_API void sf_kuon_bnd_destroy(sf_kuon_bnd_t *bnd);

/* Number of file entries declared in the header. */
SF_API size_t sf_kuon_bnd_file_count(const sf_kuon_bnd_t *bnd);

/* File-version field from the header (200 or 202 observed upstream). */
SF_API int32_t sf_kuon_bnd_file_version(const sf_kuon_bnd_t *bnd);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_KUON_H */
