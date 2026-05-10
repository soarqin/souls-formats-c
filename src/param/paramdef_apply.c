/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — PARAMDEF apply (bitstream helpers).
 *
 * Bitstream helpers — literal mirror of Row.cs:236-244.
 * DO NOT "beautify" the shift math. The (64 - bitSize - bitOffset) pattern
 * is intentional and must match upstream exactly for round-trip correctness.
 *
 * Upstream reference (pinned commit 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a):
 *   SoulsFormats/Formats/PARAM/PARAM/Row.cs (lines 124-281, 320-432)
 *   SoulsFormats/Formats/PARAM/ParamUtil.cs  (lines 239-292)
 *
 * NOTE: The full sf_param_apply_paramdef public API is not implemented here
 * yet — this translation unit only contains the static bitstream primitives
 * that T3.3 will build on. Until then the file is consumed exclusively by
 * tests/param/test_paramdef_bitstream.c via direct `#include`, so it is
 * intentionally NOT listed in CMakeLists.txt's SF_SOURCES.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_param.h"
#include "souls_formats/sf_paramdef.h"

_Static_assert(sizeof(uint64_t) == 8, "bitstream helpers require 64-bit uint64_t");

/*===========================================================================
 * Bitstream primitives — literal mirror of Row.cs:236-244.
 *
 * All helpers operate on a byte buffer with a *global* bit offset measured
 * from the start of the buffer. Internally they decompose the global offset
 * into a byte index (offset / 8) plus a *local* bit offset (offset % 8) —
 * the latter is what feeds into the upstream `(64 - bitSize - bitOffset)`
 * shift formula. Buffer access is unaligned-safe via memcpy.
 *===========================================================================*/

/*  Read `bit_size` bits starting at `bit_offset` (zero-indexed, little-endian
 *  bit order) from `buf` and return them as a zero-extended uint64_t.
 *
 *  Mirrors Row.cs:244 (unsigned branch):
 *      shifted = (long)(bitValue << leftShift >> rightShift);
 *  with leftShift = 64 - bitSize - bitOffset, rightShift = 64 - bitSize.
 *
 *  Preconditions (caller-enforced, matches upstream — no in-helper checks):
 *      - 1 <= bit_size <= 64 - (bit_offset % 8)   (i.e. bits fit in 64-bit window)
 *      - buf has at least 8 readable bytes from buf + (bit_offset / 8). */
static uint64_t extract_bits_unsigned(const uint8_t *buf, size_t bit_offset,
                                      size_t bit_size) {
    /*  Load the 8-byte little-endian window covering the requested bits. */
    uint64_t bit_value;
    memcpy(&bit_value, buf + bit_offset / 8, sizeof(uint64_t));
    /*  Row.cs:236-244:
     *      leftShift  = 64 - bitSize - bitOffset
     *      rightShift = 64 - bitSize
     *      result     = (bitValue << leftShift) >> rightShift          */
    size_t local_bit_offset = bit_offset % 8;
    return (bit_value << (64 - bit_size - local_bit_offset)) >> (64 - bit_size);
}

/*  Same as extract_bits_unsigned but sign-extends the high bit of the
 *  bit_size-wide field into the upper bits of the returned int64_t.
 *
 *  Equivalent to Row.cs:241 (signed branch):
 *      shifted = (long)bitValue << leftShift >> rightShift;
 *  but uses an explicit sign-extension mask to avoid relying on the
 *  implementation-defined behaviour of right-shifting negative signed
 *  integers in C.                                                          */
static int64_t extract_bits_signed(const uint8_t *buf, size_t bit_offset,
                                   size_t bit_size) {
    uint64_t u = extract_bits_unsigned(buf, bit_offset, bit_size);
    /*  Sign-extend: if the high bit of the bit_size-wide field is set,
     *  fill the upper (64 - bit_size) bits with 1s. */
    if (bit_size < 64 && ((u >> (bit_size - 1)) & 1u)) {
        u |= ~(uint64_t)0 << bit_size;
    }
    return (int64_t)u;
}

/*  Insert `bit_size` low bits of `value` at `bit_offset` into `buf`,
 *  preserving the surrounding bits via read-modify-write.
 *
 *  Mirrors the shift-and-OR pattern from Row.cs:396-397:
 *      shifted = shifted << (BIT_VALUE_SIZE - field.BitSize)
 *                       >> (BIT_VALUE_SIZE - field.BitSize - bitOffset);
 *      bitValue |= shifted;
 *  reformulated as a bounded mask + insert so callers can update individual
 *  fields (T3.3 will queue full bit groups before flushing, exactly like
 *  upstream WriteCells). */
static void insert_bits(uint8_t *buf, size_t bit_offset, size_t bit_size,
                        uint64_t value) {
    uint64_t existing;
    memcpy(&existing, buf + bit_offset / 8, sizeof(uint64_t));
    size_t local_bit_offset = bit_offset % 8;
    uint64_t mask = (bit_size < 64) ? (((uint64_t)1 << bit_size) - 1u)
                                    : ~(uint64_t)0;
    mask <<= local_bit_offset;
    existing = (existing & ~mask) | ((value << local_bit_offset) & mask);
    memcpy(buf + bit_offset / 8, &existing, sizeof(uint64_t));
}

/*  Detect orphaned bits left over from a partially consumed bit window —
 *  literal mirror of Row.cs:136-140 checkOrphanedBits():
 *      if (bitOffset != -1 && (bitValue >> bitOffset) != 0)
 *          throw ...;
 *  Returns true when the high (window_bits - bit_offset) bits of `bit_value`
 *  are non-zero, i.e. when the upstream check would throw.
 *
 *  `bit_offset` here is the LOCAL offset within the current bit window
 *  (0..bit_limit). At bit_offset >= 64 the right-shift would be UB; upstream
 *  guards bit_value's width to 64 bits via the ulong cast, so we mirror that
 *  by returning false (no further bits to inspect).                         */
static bool detect_orphaned_bits(uint64_t bit_value, size_t bit_offset) {
    return (bit_offset < 64) ? ((bit_value >> bit_offset) != 0) : false;
}
