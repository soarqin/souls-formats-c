/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — legacy BND archive container.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/Binder/BND/BND.cs
 */

#ifndef SOULS_FORMATS_SF_BND_H
#define SOULS_FORMATS_SF_BND_H

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_bnd_file {
    int32_t        id;
    const char    *name_utf8;
    const uint8_t *data;
    size_t         size;
} sf_bnd_file_t;

SF_API bool sf_bnd_is_format(const uint8_t *data, size_t size);

SF_API sf_result_t sf_bnd_create(sf_bnd_t **out, const sf_allocator_t *a);
SF_API void        sf_bnd_destroy(sf_bnd_t *b);

SF_API sf_result_t sf_bnd_read_from_path(sf_bnd_t **out, const wchar_t *path,
                                         const sf_allocator_t *a);
SF_API sf_result_t sf_bnd_read_from_memory(sf_bnd_t **out, const uint8_t *data,
                                           size_t size, const sf_allocator_t *a);
SF_API sf_result_t sf_bnd_write_to_path(const sf_bnd_t *b, const wchar_t *path);
SF_API sf_result_t sf_bnd_write_to_memory(const sf_bnd_t *b, uint8_t **out,
                                          size_t *out_size, const sf_allocator_t *a);

SF_API size_t               sf_bnd_file_count(const sf_bnd_t *b);
SF_API const sf_bnd_file_t *sf_bnd_get_file(const sf_bnd_t *b, size_t index);
SF_API sf_result_t          sf_bnd_add_file(sf_bnd_t *b, const sf_bnd_file_t *file);
SF_API sf_result_t          sf_bnd_remove_file(sf_bnd_t *b, size_t index);

SF_API int32_t  sf_bnd_get_internal_version(const sf_bnd_t *b);
SF_API uint16_t sf_bnd_get_format0(const sf_bnd_t *b);
SF_API uint16_t sf_bnd_get_format1(const sf_bnd_t *b);
SF_API const char *sf_bnd_get_root_file_path(const sf_bnd_t *b);

SF_API void sf_bnd_set_internal_version(sf_bnd_t *b, int32_t v);
SF_API void sf_bnd_set_format0(sf_bnd_t *b, uint16_t v);
SF_API void sf_bnd_set_format1(sf_bnd_t *b, uint16_t v);
SF_API void sf_bnd_set_root_file_path(sf_bnd_t *b, const char *path_utf8);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_BND_H */
