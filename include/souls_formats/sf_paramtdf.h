/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — PARAMTDF public surface.
 *
 * Upstream reference (pinned commit 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a):
 *   SoulsFormats/Formats/PARAM/PARAMTDF.cs
 */

#ifndef SOULS_FORMATS_SF_PARAMTDF_H
#define SOULS_FORMATS_SF_PARAMTDF_H

#include "sf_common.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_paramtdf sf_paramtdf_t;
typedef struct sf_paramtdf_entry sf_paramtdf_entry_t;

#if defined(__cplusplus)
#define SF_PARAMTDF_STATIC_ASSERT static_assert
#else
#define SF_PARAMTDF_STATIC_ASSERT _Static_assert
#endif

typedef enum sf_paramtdf_type {
    SF_PARAMTDF_TYPE_S8 = 0,
    SF_PARAMTDF_TYPE_U8,
    SF_PARAMTDF_TYPE_S16,
    SF_PARAMTDF_TYPE_U16,
    SF_PARAMTDF_TYPE_S32,
    SF_PARAMTDF_TYPE_U32,
} sf_paramtdf_type_t;

SF_PARAMTDF_STATIC_ASSERT(SF_PARAMTDF_TYPE_U32 + 1 - SF_PARAMTDF_TYPE_S8 == 6,
                          "PARAMTDF type count must be 6");

/* NULL entry names are valid; some upstream entries intentionally omit a name. */

SF_API sf_result_t sf_paramtdf_read_from_text(const char *utf8_text, size_t size,
                                               sf_paramtdf_t **out,
                                               const sf_allocator_t *alloc);
SF_API sf_result_t sf_paramtdf_write_to_text(const sf_paramtdf_t *tdf,
                                             char **out_text, size_t *out_size,
                                             const sf_allocator_t *alloc);
SF_API void sf_paramtdf_destroy(sf_paramtdf_t *tdf, const sf_allocator_t *alloc);

SF_API const char *sf_paramtdf_get_name(const sf_paramtdf_t *tdf);
SF_API sf_paramtdf_type_t sf_paramtdf_get_type(const sf_paramtdf_t *tdf);
SF_API size_t sf_paramtdf_get_entry_count(const sf_paramtdf_t *tdf);
SF_API const sf_paramtdf_entry_t *sf_paramtdf_get_entry(const sf_paramtdf_t *tdf,
                                                        size_t index);

SF_API const char *sf_paramtdf_entry_get_name(const sf_paramtdf_entry_t *entry);
SF_API int64_t sf_paramtdf_entry_get_value(const sf_paramtdf_entry_t *entry);

#undef SF_PARAMTDF_STATIC_ASSERT

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_PARAMTDF_H */
