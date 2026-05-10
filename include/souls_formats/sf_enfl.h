/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — ENFL (entryfilelist) format.
 *
 * Mysterious file format used in BB and DS3. Speculation: determines
 * which assets to load based on map location. Extension: .entryfilelist.
 *
 * On-disk layout (little-endian):
 *
 *   off 0x00  ASCII "ENFL"
 *   off 0x04  int32   0x10415
 *   off 0x08  int32   compressedSize
 *   off 0x0C  int32   uncompressedSize
 *   off 0x10  zlib(compressedSize) {
 *                 int32   0
 *                 int32   struct1Count
 *                 int32   struct2Count
 *                 int32   0
 *                 Struct1[struct1Count]    (4 bytes each: int16 step,
 *                                                          int16 index)
 *                 pad to 0x10
 *                 Struct2[struct2Count]    (8 bytes each: int64 unk1)
 *                 pad to 0x10
 *                 int16   0
 *                 UTF-16LE NUL-terminated string × struct2Count
 *                 pad to 0x10
 *             }
 *
 * Note: ENFL uses internal zlib (NOT DCX wrapping) and the number of
 * strings is exactly equal to struct2Count.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/ENFL.cs
 */

#ifndef SOULS_FORMATS_SF_ENFL_H
#define SOULS_FORMATS_SF_ENFL_H

#include "souls_formats/sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_enfl sf_enfl_t;

typedef struct sf_enfl_struct1 {
    int16_t step;
    int16_t index;
} sf_enfl_struct1_t;

typedef struct sf_enfl_struct2 {
    int64_t unk1;
} sf_enfl_struct2_t;

SF_API sf_result_t sf_enfl_create (sf_enfl_t **out, const sf_allocator_t *a);
SF_API void        sf_enfl_destroy(sf_enfl_t *b);

SF_API sf_result_t sf_enfl_read_from_path  (sf_enfl_t **out, const wchar_t *path,
                                            const sf_allocator_t *a);
SF_API sf_result_t sf_enfl_read_from_memory(sf_enfl_t **out, const uint8_t *data,
                                            size_t size, const sf_allocator_t *a);

SF_API sf_result_t sf_enfl_write_to_path  (const sf_enfl_t *b, const wchar_t *path);
SF_API sf_result_t sf_enfl_write_to_memory(const sf_enfl_t *b, uint8_t **out,
                                           size_t *out_size, const sf_allocator_t *a);

SF_API size_t sf_enfl_struct1_count(const sf_enfl_t *b);
SF_API size_t sf_enfl_struct2_count(const sf_enfl_t *b);
SF_API size_t sf_enfl_string_count (const sf_enfl_t *b);

SF_API const sf_enfl_struct1_t *sf_enfl_get_struct1(const sf_enfl_t *b, size_t idx);
SF_API const sf_enfl_struct2_t *sf_enfl_get_struct2(const sf_enfl_t *b, size_t idx);
SF_API const char              *sf_enfl_get_string (const sf_enfl_t *b, size_t idx);

SF_API sf_result_t sf_enfl_add_struct1(sf_enfl_t *b, sf_enfl_struct1_t s);
SF_API sf_result_t sf_enfl_add_struct2(sf_enfl_t *b, sf_enfl_struct2_t s);
SF_API sf_result_t sf_enfl_add_string (sf_enfl_t *b, const char *utf8);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_ENFL_H */
