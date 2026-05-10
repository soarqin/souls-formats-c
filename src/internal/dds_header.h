/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Internal-only minimal DDS header parser.
 *
 * Used by the TPF reader to derive texture metadata (cubemap, mipmap_count,
 * depth, dxgi_format) without performing any pixel decoding. Upstream
 * `DDS.cs` is _skipped_ — see `docs/api-mapping/extensions.md`.
 *
 * NEVER include this from public headers.
 */

#ifndef SF_INTERNAL_DDS_HEADER_H
#define SF_INTERNAL_DDS_HEADER_H

#include "souls_formats/sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fields extracted from a DDS file header for TPF metadata derivation.
 * Does NOT decode pixel data. */
typedef struct {
    bool     cubemap;        /* true if dwCaps2 & DDSCAPS2_CUBEMAP */
    uint32_t mipmap_count;   /* dwMipMapCount (0 or 1 if not mipmapped) */
    uint32_t depth;          /* dwDepth (0 for 2D textures) */
    uint32_t dxgi_format;    /* from DDS_HEADER_DXT10.dxgiFormat, or 0 if no DX10 extension */
} sfi_dds_metadata_t;

/* Parse up to 144 bytes of DDS header data.
 * Returns SF_OK on success, SF_ERR_BAD_MAGIC on wrong magic or dwSize != 124,
 * SF_ERR_INVALID_ARG if bytes is NULL or size < 128. */
sf_result_t sfi_dds_parse_header(const uint8_t *bytes, size_t size,
                                 sfi_dds_metadata_t *out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SF_INTERNAL_DDS_HEADER_H */
