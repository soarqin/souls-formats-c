/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — King's Field IV legacy formats.
 *
 * Simplified read-only parsers for KF4 DAT (main archive) and OM2
 * (object mesh). No test data is available; tests rely on synthetic
 * binaries.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/Other/KF4/DAT.cs
 *   SoulsFormats/Formats/Other/KF4/OM2.cs
 *
 * KF4 MAP and KF4 CHR are intentionally not exposed; see
 * docs/api-mapping/extensions.md for the deferral rationale.
 */

#ifndef SOULS_FORMATS_SF_KF4_H
#define SOULS_FORMATS_SF_KF4_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * KF4 DAT — main archive
 *
 * Header layout (little-endian):
 *   bytes[0..3]  = magic {0x00, 0x80, 0x04, 0x1E}
 *   bytes[4..7]  = int32 file_count
 *   bytes[8..63] = 0x38 zero bytes
 *
 * Each file entry is 0x40 bytes:
 *   bytes[0x00..0x33] = fixed ASCII name (NUL-padded, length 0x34)
 *   bytes[0x34..0x37] = int32 size
 *   bytes[0x38..0x3B] = int32 padded_size
 *   bytes[0x3C..0x3F] = int32 offset
 *===========================================================================*/

typedef struct sf_kf4_dat sf_kf4_dat_t;

/* Return true if @bytes starts with the KF4 DAT magic. */
SF_API bool sf_kf4_dat_is(const void *bytes, size_t size);

/* Read a KF4 DAT archive from memory. *out is heap-owned by the caller. */
SF_API sf_result_t sf_kf4_dat_read_from_memory(sf_kf4_dat_t **out,
                                               const void *bytes, size_t size,
                                               const sf_allocator_t *alloc);

/* Destroy a KF4 DAT archive. NULL-safe. */
SF_API void sf_kf4_dat_destroy(sf_kf4_dat_t *dat);

/* Number of files contained in the archive. */
SF_API size_t sf_kf4_dat_file_count(const sf_kf4_dat_t *dat);

/* Borrow the NUL-terminated file name at @index. The pointer is valid until
 * the parent archive is destroyed. */
SF_API sf_result_t sf_kf4_dat_get_file_name(const sf_kf4_dat_t *dat,
                                            size_t index, const char **out_name);

/* Borrow the file payload (bytes + size) at @index. Pointers remain valid
 * until the parent archive is destroyed. */
SF_API sf_result_t sf_kf4_dat_get_file_data(const sf_kf4_dat_t *dat,
                                            size_t index,
                                            const uint8_t **out_bytes,
                                            size_t *out_size);

/*===========================================================================
 * KF4 OM2 — object mesh
 *
 * The first 4 bytes are the on-disk file size as a little-endian int32.
 * The format has no reliable magic — callers must already know the file
 * is an OM2 from context (extension, archive entry, ...).
 *===========================================================================*/

typedef struct sf_kf4_om2 sf_kf4_om2_t;

/* Read a KF4 OM2 from memory. Only the leading file-size field is parsed;
 * the rest of the structure is consulted by callers via separate accessors
 * once more upstream coverage exists. */
SF_API sf_result_t sf_kf4_om2_read_from_memory(sf_kf4_om2_t **out,
                                               const void *bytes, size_t size,
                                               const sf_allocator_t *alloc);

/* Destroy a KF4 OM2. NULL-safe. */
SF_API void sf_kf4_om2_destroy(sf_kf4_om2_t *om2);

/* The leading file-size header field (in bytes). */
SF_API int32_t sf_kf4_om2_file_size(const sf_kf4_om2_t *om2);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_KF4_H */
