/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Metal Wolf Chaos legacy formats.
 *
 * Simplified read-only parsers for MMD (map model data) and OTR (collision
 * mesh). No test data is available; tests rely on synthetic binaries.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/Other/MWC/MMD.cs
 *   SoulsFormats/Formats/Other/MWC/OTR.cs
 *
 * MWC DEV/MDAT/SMD/TDAT are intentionally not exposed; see
 * docs/api-mapping/extensions.md for the deferral rationale.
 */

#ifndef SOULS_FORMATS_SF_MWC_H
#define SOULS_FORMATS_SF_MWC_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * MWC MMD — map model data
 *
 * The simplified detection used here matches a four-byte ASCII tag
 * "MMD\0" at offset 0. This is a souls-formats-c extension; upstream's
 * Is() probe reads a leading int32 file-size before checking the magic
 * tag "MMD " at offset 4. The downstream consumer should treat both
 * placements as MMD candidates — they are mutually exclusive on disk.
 *===========================================================================*/

typedef struct sf_mwc_mmd sf_mwc_mmd_t;

/* Return true if @bytes starts with the simplified MMD magic "MMD\0". */
SF_API bool sf_mwc_mmd_is(const void *bytes, size_t size);

/* Read an MMD header from memory. Only the magic tag is consumed; the
 * remainder of the format is intentionally not decoded. */
SF_API sf_result_t sf_mwc_mmd_read_from_memory(sf_mwc_mmd_t **out,
                                               const void *bytes, size_t size,
                                               const sf_allocator_t *alloc);

/* Destroy an MMD. NULL-safe. */
SF_API void sf_mwc_mmd_destroy(sf_mwc_mmd_t *mmd);

/*===========================================================================
 * MWC OTR — collision mesh
 *
 * The simplified detection used here matches a four-byte ASCII tag
 * "OTR\0" at offset 0. This is a souls-formats-c extension: upstream
 * OTR has no magic at all, only a leading int32 that varies by file.
 * Downstream consumers requiring tighter detection should match by file
 * extension instead.
 *===========================================================================*/

typedef struct sf_mwc_otr sf_mwc_otr_t;

/* Return true if @bytes starts with the simplified OTR magic "OTR\0". */
SF_API bool sf_mwc_otr_is(const void *bytes, size_t size);

/* Read an OTR header from memory. Only the magic tag is consumed; the
 * remainder of the format is intentionally not decoded. */
SF_API sf_result_t sf_mwc_otr_read_from_memory(sf_mwc_otr_t **out,
                                               const void *bytes, size_t size,
                                               const sf_allocator_t *alloc);

/* Destroy an OTR. NULL-safe. */
SF_API void sf_mwc_otr_destroy(sf_mwc_otr_t *otr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_MWC_H */
