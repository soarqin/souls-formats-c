/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — miscellaneous legacy formats.
 *
 * Simplified read-only parsers for:
 *   - MGF (Dreamcast / Frame Gride archive)
 *   - DDL (Murakumo: Renegade Mech Pursuit texture container)
 *
 * No test data is available; tests rely on synthetic binaries.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/Other/Dreamcast/MGF.cs
 *   SoulsFormats/Formats/Other/Murakumo/DDL.cs
 *
 * Zero3, LDMU, SOM/MDO, Otogi2 DAT are intentionally not exposed; see
 * docs/api-mapping/extensions.md for the deferral rationale.
 */

#ifndef SOULS_FORMATS_SF_LEGACY_MISC_H
#define SOULS_FORMATS_SF_LEGACY_MISC_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * MGF — Dreamcast/Frame Gride archive
 *
 * Header layout (little-endian):
 *   bytes[0..3]  = magic "MGFL"
 *   bytes[4..7]  = int32 unk04 (observed: 1 or 2)
 *   bytes[8..11] = int32 file_count
 *===========================================================================*/

typedef struct sf_mgf sf_mgf_t;

/* Return true if @bytes starts with the MGF magic. */
SF_API bool sf_mgf_is(const void *bytes, size_t size);

/* Read an MGF archive header from memory. The simplified parser does not
 * decode individual file entries. */
SF_API sf_result_t sf_mgf_read_from_memory(sf_mgf_t **out,
                                           const void *bytes, size_t size,
                                           const sf_allocator_t *alloc);

/* Destroy an MGF. NULL-safe. */
SF_API void sf_mgf_destroy(sf_mgf_t *mgf);

/* The unk04 header field (observed values: 1 or 2). */
SF_API int32_t sf_mgf_unk04(const sf_mgf_t *mgf);

/* Number of file entries declared in the header. */
SF_API size_t sf_mgf_file_count(const sf_mgf_t *mgf);

/*===========================================================================
 * DDL — Murakumo texture container
 *
 * Header layout (little-endian):
 *   bytes[0..3]   = magic "DDL\0"
 *   bytes[4..7]   = int32 version (observed: 20000)
 *
 * The simplified detection used here matches "DDL\0"; upstream uses
 * "DDL " (DDL followed by a space) at offset 0. Downstream consumers
 * may need to accept both spellings depending on the source asset.
 *===========================================================================*/

typedef struct sf_ddl sf_ddl_t;

/* Return true if @bytes starts with the simplified DDL magic "DDL\0". */
SF_API bool sf_ddl_is(const void *bytes, size_t size);

/* Read a DDL header from memory. Only the magic tag is consumed; the
 * remainder of the format is intentionally not decoded. */
SF_API sf_result_t sf_ddl_read_from_memory(sf_ddl_t **out,
                                           const void *bytes, size_t size,
                                           const sf_allocator_t *alloc);

/* Destroy a DDL. NULL-safe. */
SF_API void sf_ddl_destroy(sf_ddl_t *ddl);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_LEGACY_MISC_H */
