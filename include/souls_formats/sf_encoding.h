/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — text encoding helpers (Win32-backed).
 *
 * The library converts everything to UTF-8 at the boundary. These primitives
 * sit underneath the binary reader/writer string APIs but are exposed for
 * callers who already have raw bytes in hand.
 */

#ifndef SOULS_FORMATS_SF_ENCODING_H
#define SOULS_FORMATS_SF_ENCODING_H

#include "sf_common.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * All converters allocate the output via the supplied allocator (NULL = default).
 * `*out_utf8` is heap-owned by the caller and freed via sf_free(a, ptr).
 * `out_len_bytes` (optional) receives the UTF-8 byte length excluding the NUL.
 * Output is always NUL-terminated.
 *
 * Input length semantics: `in_size` is the byte count of `in`. For UTF-16
 * variants this MUST be a multiple of 2; an odd `in_size` truncates the last
 * unpaired byte (mirrors upstream behaviour).
 */

SF_API sf_result_t sf_ascii_to_utf8     (const void *in, size_t in_size,
                                         char **out_utf8, size_t *out_len_bytes,
                                         const sf_allocator_t *a);

SF_API sf_result_t sf_shift_jis_to_utf8 (const void *in, size_t in_size,
                                         char **out_utf8, size_t *out_len_bytes,
                                         const sf_allocator_t *a);

SF_API sf_result_t sf_utf16le_to_utf8   (const void *in, size_t in_size,
                                         char **out_utf8, size_t *out_len_bytes,
                                         const sf_allocator_t *a);

SF_API sf_result_t sf_utf16be_to_utf8   (const void *in, size_t in_size,
                                         char **out_utf8, size_t *out_len_bytes,
                                         const sf_allocator_t *a);

/*  utf8 → encoding. Output is heap-owned; freed via sf_free(a, ptr).
 *  When `terminate` is true, a NUL terminator is appended (1 byte for
 *  ASCII / Shift-JIS, 2 bytes for UTF-16). out_size_bytes counts ALL bytes
 *  written including the optional terminator. */
SF_API sf_result_t sf_utf8_to_ascii    (const char *utf8, bool terminate,
                                        void **out, size_t *out_size_bytes,
                                        const sf_allocator_t *a);

SF_API sf_result_t sf_utf8_to_shift_jis(const char *utf8, bool terminate,
                                        void **out, size_t *out_size_bytes,
                                        const sf_allocator_t *a);

SF_API sf_result_t sf_utf8_to_utf16le  (const char *utf8, bool terminate,
                                        void **out, size_t *out_size_bytes,
                                        const sf_allocator_t *a);

SF_API sf_result_t sf_utf8_to_utf16be  (const char *utf8, bool terminate,
                                        void **out, size_t *out_size_bytes,
                                        const sf_allocator_t *a);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_ENCODING_H */
