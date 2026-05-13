/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — NVA navmesh area (BB/DS3/Sekiro).
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormatsNEXT @ 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a
 *   SoulsFormats/Formats/NVA.cs
 *
 * Defines the placement and properties of navmeshes in BB, DS3, and Sekiro.
 * The format is a list of sections (Navmeshes, Section1, Section2, Section3,
 * Connectors, ConnectorPoints, ConnectorConditions, Section7, and Sekiro-only
 * Section8) each tagged with index/version/count and a payload of fixed-size
 * entries. This C API exposes the file structure and provides full
 * byte-preserving round-trip; per-entry semantic decoding is intentionally
 * left to higher-level callers via raw entry payloads.
 */

#ifndef SOULS_FORMATS_SF_NVA_H
#define SOULS_FORMATS_SF_NVA_H

#include "sf_common.h"
#include "sf_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_nva         sf_nva_t;
typedef struct sf_nva_section sf_nva_section_t;

/** Mirrors upstream NVA.NVAVersion. */
typedef enum sf_nva_version {
    SF_NVA_VERSION_OLD_BLOODBORNE = 3,
    SF_NVA_VERSION_DARK_SOULS_3   = 4,
    SF_NVA_VERSION_SEKIRO         = 5,
} sf_nva_version_t;
_Static_assert(SF_NVA_VERSION_SEKIRO == 5, "NVA Version drift");

SF_API sf_result_t sf_nva_read_from_memory(sf_nva_t **out, const void *data, size_t size,
                                           const sf_allocator_t *alloc);
SF_API sf_result_t sf_nva_write_to_memory(const sf_nva_t *nva, void **out_data,
                                          size_t *out_size, const sf_allocator_t *alloc);
SF_API void sf_nva_destroy(sf_nva_t *nva);

SF_API sf_result_t sf_nva_create_empty(sf_nva_t **out, sf_nva_version_t version,
                                       const sf_allocator_t *alloc);

SF_API sf_nva_version_t sf_nva_version(const sf_nva_t *nva);
SF_API size_t           sf_nva_section_count(const sf_nva_t *nva);
SF_API const sf_nva_section_t *sf_nva_section(const sf_nva_t *nva, size_t i);
SF_API sf_nva_section_t       *sf_nva_section_mut(sf_nva_t *nva, size_t i);

SF_API int32_t        sf_nva_section_index   (const sf_nva_section_t *s);
SF_API int32_t        sf_nva_section_version (const sf_nva_section_t *s);
SF_API size_t         sf_nva_section_entry_count(const sf_nva_section_t *s);
SF_API size_t         sf_nva_section_entry_size (const sf_nva_section_t *s);
SF_API const uint8_t *sf_nva_section_entries   (const sf_nva_section_t *s);

/* Builder: append a new entry of fixed size to a section. The entry payload
 * is copied from `data`. `entry_size_bytes` must match the section's existing
 * entry size, or, on the first append, sets the size. */
SF_API sf_result_t sf_nva_section_append_entry(sf_nva_section_t *s,
                                               const void *data,
                                               size_t entry_size_bytes);

SF_API void sf_nva_section_set_version(sf_nva_section_t *s, int32_t version);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_NVA_H */
