/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — FMG string container.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormatsNEXT @ 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a
 *   SoulsFormats/Formats/FMG.cs
 */

#ifndef SOULS_FORMATS_SF_FMG_H
#define SOULS_FORMATS_SF_FMG_H

#include "sf_common.h"
#include "sf_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_fmg sf_fmg_t;
typedef struct sf_fmg_entry sf_fmg_entry_t;

typedef enum sf_fmg_version {
    SF_FMG_VERSION_DEMONS_SOULS = 0,
    SF_FMG_VERSION_DARK_SOULS_1 = 1,
    SF_FMG_VERSION_DARK_SOULS_3 = 2, /* Also used for Bloodborne, DS3, ER */
} sf_fmg_version_t;

_Static_assert(SF_FMG_VERSION_DARK_SOULS_3 == 2, "FMG version constants must be stable");

/* NOTE: A NULL text pointer means the entry is DELETED (tombstone).
 * An empty string "" is a valid non-deleted entry with no text.
 * This mirrors upstream FMG behavior exactly. */

SF_API sf_result_t sf_fmg_create(const sf_allocator_t *alloc, sf_fmg_version_t version,
                                 sf_fmg_t **out);
SF_API void        sf_fmg_destroy(sf_fmg_t *fmg);

SF_API sf_result_t sf_fmg_read_from_memory(sf_fmg_t **out, const uint8_t *data, size_t size,
                                           const sf_allocator_t *alloc);
SF_API sf_result_t sf_fmg_read_from_stream(sf_fmg_t **out, sf_istream_t *stream,
                                          const sf_allocator_t *alloc);
SF_API sf_result_t sf_fmg_read_from_path(sf_fmg_t **out, const char *utf8_path,
                                         const sf_allocator_t *alloc);

SF_API sf_result_t sf_fmg_write_to_memory(const sf_fmg_t *fmg, uint8_t **out_data,
                                          size_t *out_size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_fmg_write_to_stream(const sf_fmg_t *fmg, sf_ostream_t *stream,
                                         const sf_allocator_t *alloc);
SF_API sf_result_t sf_fmg_write_to_path(const sf_fmg_t *fmg, const char *utf8_path,
                                        const sf_allocator_t *alloc);

SF_API sf_fmg_version_t sf_fmg_get_version(const sf_fmg_t *fmg);
SF_API bool             sf_fmg_is_big_endian(const sf_fmg_t *fmg);
SF_API bool             sf_fmg_is_unicode(const sf_fmg_t *fmg);
SF_API bool             sf_fmg_has_md5(const sf_fmg_t *fmg);
SF_API bool             sf_fmg_get_reuse_offsets(const sf_fmg_t *fmg);
SF_API size_t           sf_fmg_get_entry_count(const sf_fmg_t *fmg);
SF_API const sf_fmg_entry_t *sf_fmg_get_entry(const sf_fmg_t *fmg, size_t index);
SF_API const sf_fmg_entry_t *sf_fmg_find_entry_by_id(const sf_fmg_t *fmg, int32_t id);

SF_API void sf_fmg_set_big_endian(sf_fmg_t *fmg, bool value);
SF_API void sf_fmg_set_unicode(sf_fmg_t *fmg, bool value);
SF_API void sf_fmg_set_md5(sf_fmg_t *fmg, bool value);
SF_API void sf_fmg_set_reuse_offsets(sf_fmg_t *fmg, bool value);

SF_API sf_result_t sf_fmg_add_entry(sf_fmg_t *fmg, int32_t id, const char *text_utf8,
                                    const sf_allocator_t *alloc);
SF_API sf_result_t sf_fmg_remove_entry(sf_fmg_t *fmg, int32_t id);
SF_API sf_result_t sf_fmg_set_entry_text(sf_fmg_t *fmg, int32_t id, const char *text_utf8,
                                         const sf_allocator_t *alloc);

SF_API int32_t      sf_fmg_entry_get_id(const sf_fmg_entry_t *entry);
SF_API const char  *sf_fmg_entry_get_text(const sf_fmg_entry_t *entry);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_FMG_H */
