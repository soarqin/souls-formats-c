/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — EMELD public surface.
 *
 * A companion file to EMEVD that assigns names to different events. The
 * container has a fixed magic ("ELD\0"), a single-byte bigEndian flag,
 * and a single-byte is64Bit flag (-1 or 0) which together select one of
 * three supported variants:
 *
 *   - DS1        : bigEndian=0, is64Bit=0 (32-bit varints, little-endian)
 *   - DS1 BE     : bigEndian=1, is64Bit=0 (32-bit varints, big-endian)
 *   - Bloodborne : bigEndian=0, is64Bit=1 (64-bit varints, little-endian)
 *
 * The (bigEndian=1, is64Bit=1) combination is rejected on read. DS3 and
 * later games do not produce EMELDs.
 *
 * Wire layout (high level):
 *   magic "ELD\0"      : 4 bytes
 *   bigEndian          : bool (1 byte)
 *   is64Bit            : sbyte, 0=false / -1=true
 *   padding            : 2 bytes (0)
 *   int16 0x65         : 2 bytes
 *   int16 0xCC         : 2 bytes
 *   fileSize           : int32
 *   varint eventCount
 *   varint eventsOffset
 *   varint 0  (asserted)
 *   varint unusedOffset2
 *   varint 0  (asserted)
 *   varint unusedOffset3
 *   varint stringsLength
 *   varint stringsOffset
 *   if !is64Bit: int32(0), int32(0) padding
 *   events @ eventsOffset:
 *     varint id
 *     varint nameOffset (relative to stringsOffset)
 *     if !is64Bit: int32(0) padding
 *   strings @ stringsOffset:
 *     UTF-16 NUL-terminated name per event
 *     padded to 0x10
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/EMELD.cs
 */

#ifndef SOULS_FORMATS_SF_EMELD_H
#define SOULS_FORMATS_SF_EMELD_H

#include "sf_common.h"
#include "souls_formats/sf_emevd.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_emeld       sf_emeld_t;
typedef struct sf_emeld_event sf_emeld_event_t;

/*  An event name binding. The name is UTF-8 and is owned by the EMELD
 *  that produced this entry; it must NOT be freed by the caller and
 *  remains valid until sf_emeld_destroy(). */
struct sf_emeld_event {
    int64_t id;
    char   *name;
};

/*  Create an empty EMELD with the specified format. Only DS1, DS1_BE and
 *  Bloodborne are accepted; any other sf_emevd_format_t value yields
 *  SF_ERR_INVALID_ARG. Pass NULL for @alloc to use the default allocator.
 *  The returned handle owns all internal storage and is released with
 *  sf_emeld_destroy(). */
SF_API sf_result_t sf_emeld_create(sf_emeld_t **out, sf_emevd_format_t format,
                                   const sf_allocator_t *alloc);

/*  Release an EMELD handle. NULL-safe. */
SF_API void sf_emeld_destroy(sf_emeld_t *emeld);

/*  Read an EMELD from a contiguous in-memory byte buffer. The buffer is
 *  consumed in full and may be released as soon as this returns. */
SF_API sf_result_t sf_emeld_read_from_memory(sf_emeld_t **out,
                                             const void *bytes, size_t size,
                                             const sf_allocator_t *alloc);

/*  Serialize an EMELD to a freshly allocated heap buffer. Caller takes
 *  ownership and frees the buffer with sf_free() using @alloc. */
SF_API sf_result_t sf_emeld_write_to_memory(const sf_emeld_t *emeld,
                                            uint8_t **out_data,
                                            size_t *out_size,
                                            const sf_allocator_t *alloc);

/*  Quick magic check. Returns true iff the buffer is at least 4 bytes
 *  and begins with "ELD\0". Mirrors upstream SoulsFile<EMELD>.Is(). */
SF_API bool sf_emeld_is(const void *bytes, size_t size);

/*  Accessors. */
SF_API sf_emevd_format_t sf_emeld_get_format(const sf_emeld_t *emeld);
SF_API size_t            sf_emeld_event_count(const sf_emeld_t *emeld);

/*  Retrieve a pointer to the event at @index. The pointer is owned by
 *  the EMELD and remains valid until sf_emeld_destroy(). */
SF_API sf_result_t sf_emeld_get_event(const sf_emeld_t *emeld, size_t index,
                                      const sf_emeld_event_t **out);

/*  Append an event with the given numeric id and UTF-8 name. The name is
 *  duplicated into storage owned by the EMELD. */
SF_API sf_result_t sf_emeld_add_event(sf_emeld_t *emeld, int64_t id,
                                      const char *name);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_EMELD_H */
