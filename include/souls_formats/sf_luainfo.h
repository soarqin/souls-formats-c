/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — LUAINFO (Lua AI goal list) public surface.
 *
 * A list of AI goals for Lua scripts shipped with FromSoftware games. Each
 * goal carries an integer ID, a Shift-JIS or UTF-16 name string, an optional
 * logic interrupt function name, and two boolean interrupt flags.
 *
 * Wire format:
 *   - Magic: "LUAI" (4 bytes ASCII).
 *   - Version: int32 — 1 little-endian or 0x01000000 read-as-LE for BE files.
 *   - GoalCount: int32.
 *   - Padding: int32 (0).
 *   - GoalCount goal entries, then string payload.
 *
 *   Format variants:
 *     - Short (DS1/DS2/BB): 32-bit offsets + Shift-JIS strings, 0x10 bytes/goal.
 *     - Long  (DS3/Sekiro/ER+): 64-bit offsets + UTF-16 strings, 0x18 bytes/goal.
 *
 *   Detection (from upstream, requires >= 1 goal):
 *     - goalCount >= 2 → LongFormat = (int32 at 0x24 == 0).
 *     - goalCount == 1 → LongFormat = true if int32 at 0x18 == 0x10 + 0x18,
 *                         false if int32 at 0x14 == 0x10 + 0x10.
 *     - Otherwise: SF_ERR_UNSUPPORTED_VERSION.
 *
 * A null logic_interrupt_name indicates "no interrupt name", which serializes
 * as a zero offset slot. Strings on the C boundary are always UTF-8.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/LUAINFO.cs
 */

#ifndef SOULS_FORMATS_SF_LUAINFO_H
#define SOULS_FORMATS_SF_LUAINFO_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_luainfo      sf_luainfo_t;
typedef struct sf_luainfo_goal sf_luainfo_goal_t;

/*  Public goal record. The container owns @name and @logic_interrupt_name;
 *  both are heap-allocated UTF-8 strings released by sf_luainfo_destroy().
 *  Pointers handed out via sf_luainfo_get_goal() remain valid only until the
 *  next mutating call (add / destroy) on the parent sf_luainfo_t. */
struct sf_luainfo_goal {
    int32_t id;
    bool    battle_interrupt;
    bool    logic_interrupt;
    char   *name;                   /* owned, UTF-8, never NULL */
    char   *logic_interrupt_name;   /* owned, UTF-8, or NULL = absent */
};

/*  Create an empty LUAINFO with the specified format flags. Pass NULL for
 *  @alloc to use the default allocator. The returned handle owns its
 *  internal allocations and must be released with sf_luainfo_destroy. */
SF_API sf_result_t sf_luainfo_create(sf_luainfo_t **out, bool big_endian,
                                     bool long_format,
                                     const sf_allocator_t *alloc);

/*  Release a LUAINFO handle. NULL-safe. */
SF_API void sf_luainfo_destroy(sf_luainfo_t *info);

/*  Read a LUAINFO from a contiguous in-memory byte buffer. The buffer is
 *  consumed in full and may be released as soon as this returns.
 *  Returns SF_ERR_BAD_MAGIC if the leading 4 bytes are not "LUAI", and
 *  SF_ERR_UNSUPPORTED_VERSION if goalCount == 0 or format detection fails. */
SF_API sf_result_t sf_luainfo_read_from_memory(sf_luainfo_t **out,
                                               const void *bytes, size_t size,
                                               const sf_allocator_t *alloc);

/*  Serialize a LUAINFO to a freshly allocated heap buffer. Caller takes
 *  ownership and frees the buffer with sf_free() using @alloc. */
SF_API sf_result_t sf_luainfo_write_to_memory(const sf_luainfo_t *info,
                                              uint8_t **out_data,
                                              size_t *out_size,
                                              const sf_allocator_t *alloc);

/*  Magic check. Returns true iff @size >= 4 and the first four bytes are
 *  "LUAI". Mirrors upstream SoulsFile<LUAINFO>.Is(). */
SF_API bool sf_luainfo_is(const void *bytes, size_t size);

/*  Accessors. */
SF_API bool   sf_luainfo_big_endian (const sf_luainfo_t *info);
SF_API bool   sf_luainfo_long_format(const sf_luainfo_t *info);
SF_API size_t sf_luainfo_goal_count (const sf_luainfo_t *info);

/*  Retrieve a const view of the goal at @index. The pointer is owned by the
 *  LUAINFO and remains valid until the next mutating call on the container. */
SF_API sf_result_t sf_luainfo_get_goal(const sf_luainfo_t *info, size_t index,
                                       const sf_luainfo_goal_t **out);

/*  Append a goal. Both @name (required, non-NULL) and @logic_interrupt_name
 *  (optional, may be NULL) are duplicated into storage owned by the LUAINFO. */
SF_API sf_result_t sf_luainfo_add_goal(sf_luainfo_t *info, int32_t id,
                                       const char *name,
                                       bool battle_interrupt,
                                       bool logic_interrupt,
                                       const char *logic_interrupt_name);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_LUAINFO_H */
