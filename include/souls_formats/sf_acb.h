/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — ACB (Asset Config Binary) public surface.
 *
 * A rendering configuration file for game assets, used only in DS2.
 * Magic: "ACB\0", endian auto-detected from header layout. Each asset
 * has a type tag (PWV/General/Model/Texture/GITexture/Motion) and a
 * pair of UTF-16 paths (absolute and relative).
 *
 * This implementation is read-only. The write path requires a complex
 * offset-index ledger and is deferred.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/ACB.cs
 */

#ifndef SOULS_FORMATS_SF_ACB_H
#define SOULS_FORMATS_SF_ACB_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sf_acb_asset_type {
    SF_ACB_ASSET_PWV        = 0,
    SF_ACB_ASSET_GENERAL    = 1,
    SF_ACB_ASSET_MODEL      = 2,
    SF_ACB_ASSET_TEXTURE    = 3,
    SF_ACB_ASSET_GI_TEXTURE = 4,
    SF_ACB_ASSET_MOTION     = 5,
} sf_acb_asset_type_t;

_Static_assert(SF_ACB_ASSET_PWV == 0, "ACB asset type enum drift");
_Static_assert(SF_ACB_ASSET_MOTION == 5, "ACB asset type enum drift");

typedef struct sf_acb sf_acb_t;

SF_API sf_result_t sf_acb_create(sf_acb_t **out, bool big_endian,
                                 const sf_allocator_t *alloc);
SF_API void sf_acb_destroy(sf_acb_t *acb);

SF_API sf_result_t sf_acb_read_from_memory(sf_acb_t **out, const void *bytes,
                                           size_t size, const sf_allocator_t *alloc);

SF_API bool sf_acb_is(const void *bytes, size_t size);

SF_API bool sf_acb_big_endian(const sf_acb_t *acb);
SF_API size_t sf_acb_asset_count(const sf_acb_t *acb);

SF_API sf_result_t sf_acb_get_asset_type(const sf_acb_t *acb, size_t index,
                                         sf_acb_asset_type_t *out);

/* Returns pointers to internal UTF-8 strings owned by the ACB. The
 * strings live until sf_acb_destroy() is called. Either output pointer
 * may be NULL to skip retrieval. */
SF_API sf_result_t sf_acb_get_asset_paths(const sf_acb_t *acb, size_t index,
                                          const char **out_absolute,
                                          const char **out_relative);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_ACB_H */
