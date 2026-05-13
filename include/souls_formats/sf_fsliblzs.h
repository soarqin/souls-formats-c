/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — FSLIBLZS public surface.
 *
 * A compression format used in ACLR and ACNB.
 * Decompression is not implemented (upstream also has NotImplementedException).
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/FSLIBLZS.cs
 */

#ifndef SOULS_FORMATS_SF_FSLIBLZS_H
#define SOULS_FORMATS_SF_FSLIBLZS_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Magic bytes at offset 0: "fsliblzs" (8 ASCII bytes). */
#define SF_FSLIBLZS_MAGIC "fsliblzs"
#define SF_FSLIBLZS_MAGIC_LEN 8u

/* Minimum header size in bytes. */
#define SF_FSLIBLZS_HEADER_SIZE 32u

/**
 * Returns true if the buffer begins with the fsliblzs magic.
 * Mirrors upstream FSLIBLZS.Is().
 */
SF_API bool sf_fsliblzs_is(const void *bytes, size_t size);

/**
 * Parse the fsliblzs header and return the compressed and decompressed sizes.
 * Does NOT decompress the payload.
 *
 * @param bytes           Buffer containing the full fsliblzs file.
 * @param size            Size of the buffer in bytes.
 * @param out_compressed_size   Receives the compressed payload size (bytes).
 * @param out_decompressed_size Receives the decompressed size (bytes).
 * @return SF_OK on success, SF_ERR_BAD_MAGIC if magic is wrong,
 *         SF_ERR_INVALID_DATA if the header is malformed.
 */
SF_API sf_result_t sf_fsliblzs_read_header(const void *bytes, size_t size,
                                           int32_t *out_compressed_size,
                                           int32_t *out_decompressed_size);

/**
 * Decompress an fsliblzs buffer.
 * Always returns SF_ERR_UNSUPPORTED_VERSION — decompression is not implemented.
 * Mirrors upstream FSLIBLZS.Decompress() which throws NotImplementedException.
 */
SF_API sf_result_t sf_fsliblzs_decompress(const void *bytes, size_t size,
                                          void **out_bytes, size_t *out_size,
                                          const sf_allocator_t *alloc);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_FSLIBLZS_H */
