/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — LUAGNL (Lua global names list) public surface.
 *
 * A list of global variable names referenced by Lua scripts shipped with
 * FromSoftware games. The container has no magic bytes; endianness and
 * offset width are inferred from the first few bytes.
 *
 * Wire format variants:
 *   - Short: 32-bit offsets + Shift-JIS strings (pre-DS3 / PC DS1).
 *   - Long:  64-bit offsets + UTF-16 strings    (DS3 / Sekiro / ER).
 *
 * Endian detection: a non-empty short-format little-endian file always
 * begins with a non-zero 32-bit offset; if the first two bytes are zero
 * the file is big endian. Long format adds a leading zero high-half to
 * every offset, so it is detected by checking whether the 32-bit slot
 * immediately following the high half of the first offset is zero.
 *
 * The globals list is offset-terminated: the offset table ends with a
 * single zero offset (no count field).
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/LUAGNL.cs
 */

#ifndef SOULS_FORMATS_SF_LUAGNL_H
#define SOULS_FORMATS_SF_LUAGNL_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_luagnl sf_luagnl_t;

/*  Create an empty LUAGNL with the specified format flags. Pass NULL for
 *  @alloc to use the default allocator. The returned handle owns its
 *  internal allocations and must be released with sf_luagnl_destroy. */
SF_API sf_result_t sf_luagnl_create(sf_luagnl_t **out, bool big_endian,
                                    bool long_format,
                                    const sf_allocator_t *alloc);

/*  Release a LUAGNL handle. NULL-safe. */
SF_API void sf_luagnl_destroy(sf_luagnl_t *gnl);

/*  Read a LUAGNL from a contiguous in-memory byte buffer. The buffer is
 *  consumed in full and may be released as soon as this returns. */
SF_API sf_result_t sf_luagnl_read_from_memory(sf_luagnl_t **out,
                                              const void *bytes, size_t size,
                                              const sf_allocator_t *alloc);

/*  Serialize a LUAGNL to a freshly allocated heap buffer. Caller takes
 *  ownership and frees the buffer with sf_free() using @alloc. */
SF_API sf_result_t sf_luagnl_write_to_memory(const sf_luagnl_t *gnl,
                                             uint8_t **out_data,
                                             size_t *out_size,
                                             const sf_allocator_t *alloc);

/*  Quick magic check. LUAGNL has no magic bytes; any buffer of at least
 *  four bytes is accepted. Mirrors upstream SoulsFile<LUAGNL>.Is(). */
SF_API bool sf_luagnl_is(const void *bytes, size_t size);

/*  Accessors. */
SF_API bool   sf_luagnl_big_endian (const sf_luagnl_t *gnl);
SF_API bool   sf_luagnl_long_format(const sf_luagnl_t *gnl);
SF_API size_t sf_luagnl_global_count(const sf_luagnl_t *gnl);

/*  Retrieve a UTF-8 view of the global at @index. The pointer is owned
 *  by the LUAGNL and remains valid until sf_luagnl_destroy(). */
SF_API sf_result_t sf_luagnl_get_global(const sf_luagnl_t *gnl, size_t index,
                                        const char **out_utf8);

/*  Append a UTF-8 global variable name. The input string is duplicated
 *  into storage owned by the LUAGNL. */
SF_API sf_result_t sf_luagnl_add_global(sf_luagnl_t *gnl, const char *utf8);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_LUAGNL_H */
