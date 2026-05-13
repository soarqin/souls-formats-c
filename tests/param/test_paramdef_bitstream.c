/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 4 QA — paramdef_apply.c bitstream helpers (extract_bits / insert_bits
 * / detect_orphaned_bits) match the literal Row.cs:236-244 shift formulae.
 *
 * The helpers are file-static; pull the .c file directly into this
 * translation unit to exercise them without touching the public API.
 */

#define SF_PARAMDEF_APPLY_BITSTREAM_ONLY
#include "param/paramdef_apply.c"

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*===========================================================================
 * extract_bits_unsigned — base cases
 *===========================================================================*/

static void test_extract_unsigned_byte_aligned(void) {
    /*  bit_size=8, bit_offset=0 → reads first byte verbatim. */
    static const uint8_t buf[16] = { 0xAB, 0xCD, 0xEF, 0x12, 0, 0, 0, 0,
                                     0,    0,    0,    0,    0, 0, 0, 0 };
    TEST_ASSERT_EQUAL_HEX64((uint64_t)0xAB, extract_bits_unsigned(buf, 0, 8));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)0xCD, extract_bits_unsigned(buf, 8, 8));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)0xEF, extract_bits_unsigned(buf, 16, 8));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)0x12, extract_bits_unsigned(buf, 24, 8));
}

static void test_extract_unsigned_high_nibble(void) {
    /*  bit_size=4, bit_offset=4 → reads high nibble of first byte (0xA from 0xAB). */
    static const uint8_t buf[16] = { 0xAB, 0, 0, 0, 0, 0, 0, 0,
                                     0,    0, 0, 0, 0, 0, 0, 0 };
    /*  Low nibble: bit_offset=0, bit_size=4 → 0xB */
    TEST_ASSERT_EQUAL_HEX64((uint64_t)0xB, extract_bits_unsigned(buf, 0, 4));
    /*  High nibble: bit_offset=4, bit_size=4 → 0xA */
    TEST_ASSERT_EQUAL_HEX64((uint64_t)0xA, extract_bits_unsigned(buf, 4, 4));
}

static void test_extract_unsigned_cross_byte(void) {
    /*  bit_size=12 spanning bits [4..15] of {0xAB, 0xCD, 0x00, ...}.
     *  Layout (LSB-first within byte): byte0 bits 4-7 = 0xA (high nibble of 0xAB),
     *  byte1 bits 0-7 = 0xCD. Together as 12 bits low-to-high: 0xCDA. */
    static const uint8_t buf[16] = { 0xAB, 0xCD, 0, 0, 0, 0, 0, 0,
                                     0,    0,    0, 0, 0, 0, 0, 0 };
    TEST_ASSERT_EQUAL_HEX64((uint64_t)0xCDA, extract_bits_unsigned(buf, 4, 12));
}

static void test_extract_unsigned_full_byte_at_offset(void) {
    /*  bit_size=8 at bit_offset=4 spans the high nibble of byte0 + low nibble of byte1.
     *  For {0xAB, 0xCD, ...}: bits 4-11 = (low nibble of 0xCD)<<4 | (high nibble of 0xAB) = 0xDA. */
    static const uint8_t buf[16] = { 0xAB, 0xCD, 0, 0, 0, 0, 0, 0,
                                     0,    0,    0, 0, 0, 0, 0, 0 };
    TEST_ASSERT_EQUAL_HEX64((uint64_t)0xDA, extract_bits_unsigned(buf, 4, 8));
}

static void test_extract_unsigned_one_bit(void) {
    /*  Single-bit reads, walking through 0b10110100 = 0xB4. */
    static const uint8_t buf[16] = { 0xB4, 0, 0, 0, 0, 0, 0, 0,
                                     0,    0, 0, 0, 0, 0, 0, 0 };
    /*  LSB first: 0,0,1,0,1,1,0,1 */
    TEST_ASSERT_EQUAL_HEX64((uint64_t)0, extract_bits_unsigned(buf, 0, 1));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)0, extract_bits_unsigned(buf, 1, 1));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)1, extract_bits_unsigned(buf, 2, 1));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)0, extract_bits_unsigned(buf, 3, 1));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)1, extract_bits_unsigned(buf, 4, 1));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)1, extract_bits_unsigned(buf, 5, 1));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)0, extract_bits_unsigned(buf, 6, 1));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)1, extract_bits_unsigned(buf, 7, 1));
}

/*===========================================================================
 * extract_bits_signed — sign extension
 *===========================================================================*/

static void test_extract_signed_negative_4bit(void) {
    /*  bit_size=4, bit_offset=0, value=0xF (all bits set) → sign-extended to -1. */
    static const uint8_t buf[16] = { 0x0F, 0, 0, 0, 0, 0, 0, 0,
                                     0,    0, 0, 0, 0, 0, 0, 0 };
    TEST_ASSERT_EQUAL_INT64((int64_t)-1, extract_bits_signed(buf, 0, 4));
}

static void test_extract_signed_positive_4bit(void) {
    /*  bit_size=4, value=0x7 (high bit clear) → +7, no sign extension. */
    static const uint8_t buf[16] = { 0x07, 0, 0, 0, 0, 0, 0, 0,
                                     0,    0, 0, 0, 0, 0, 0, 0 };
    TEST_ASSERT_EQUAL_INT64((int64_t)7, extract_bits_signed(buf, 0, 4));
}

static void test_extract_signed_full_widths(void) {
    /*  Min/max 8-bit signed: 0x80 → -128, 0x7F → +127. */
    static const uint8_t neg[16] = { 0x80, 0, 0, 0, 0, 0, 0, 0,
                                     0,    0, 0, 0, 0, 0, 0, 0 };
    static const uint8_t pos[16] = { 0x7F, 0, 0, 0, 0, 0, 0, 0,
                                     0,    0, 0, 0, 0, 0, 0, 0 };
    TEST_ASSERT_EQUAL_INT64((int64_t)-128, extract_bits_signed(neg, 0, 8));
    TEST_ASSERT_EQUAL_INT64((int64_t)127,  extract_bits_signed(pos, 0, 8));
}

/*===========================================================================
 * insert_bits + extract_bits round-trip
 *===========================================================================*/

static void test_insert_extract_round_trip(void) {
    uint8_t buf[16] = {0};
    /*  Insert 0xA at offset=0,size=4. */
    insert_bits(buf, 0, 4, 0xAu);
    /*  Insert 0xB at offset=4,size=4 (same byte). */
    insert_bits(buf, 4, 4, 0xBu);
    /*  Insert 0xCDE at offset=8,size=12 (cross-byte). */
    insert_bits(buf, 8, 12, 0xCDEu);
    /*  Insert one bit at offset=20. */
    insert_bits(buf, 20, 1, 1u);

    TEST_ASSERT_EQUAL_HEX64((uint64_t)0xA,   extract_bits_unsigned(buf, 0, 4));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)0xB,   extract_bits_unsigned(buf, 4, 4));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)0xCDE, extract_bits_unsigned(buf, 8, 12));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)1,     extract_bits_unsigned(buf, 20, 1));
    /*  Composite read of full inserted region (21 bits). */
    uint64_t composite = extract_bits_unsigned(buf, 0, 21);
    /*  Layout: bits  [0..3]=0xA, [4..7]=0xB, [8..19]=0xCDE, [20]=1
     *        → 0b1 1100 1101 1110 1011 1010
     *        = (1<<20) | (0xCDE<<8) | (0xB<<4) | 0xA */
    TEST_ASSERT_EQUAL_HEX64(((uint64_t)1 << 20) | ((uint64_t)0xCDE << 8)
                            | ((uint64_t)0xB << 4) | 0xAu,
                            composite);
}

static void test_insert_preserves_surrounding_bits(void) {
    /*  Pre-fill with 0xFF and insert a 4-bit zero in the middle — the
     *  surrounding bits must be preserved (read-modify-write semantics). */
    uint8_t buf[16];
    memset(buf, 0xFF, sizeof(buf));
    insert_bits(buf, 4, 4, 0x0u);
    /*  byte0 should now be 0x0F (high nibble cleared, low nibble untouched). */
    TEST_ASSERT_EQUAL_HEX8(0x0F, buf[0]);
    /*  byte1 onward must remain 0xFF. */
    for (size_t i = 1; i < sizeof(buf); i++) {
        TEST_ASSERT_EQUAL_UINT8(0xFF, buf[i]);
    }
}

/*===========================================================================
 * detect_orphaned_bits — Row.cs:136-140 mirror
 *===========================================================================*/

static void test_orphaned_bits_high_set(void) {
    /*  bit_offset=4, bit_value=0xFF — the upper (8-4)=4 bits are non-zero,
     *  so upstream would throw. Helper returns true. */
    TEST_ASSERT_TRUE(detect_orphaned_bits(0xFFu, 4));
}

static void test_orphaned_bits_high_clear(void) {
    /*  bit_offset=4, bit_value=0x0F — the upper bits are all zero, so
     *  upstream would NOT throw. Helper returns false. */
    TEST_ASSERT_FALSE(detect_orphaned_bits(0x0Fu, 4));
}

static void test_orphaned_bits_offset_zero(void) {
    /*  bit_offset=0 with any non-zero value → orphaned (everything is high). */
    TEST_ASSERT_TRUE(detect_orphaned_bits(0x1u, 0));
    TEST_ASSERT_FALSE(detect_orphaned_bits(0x0u, 0));
}

static void test_orphaned_bits_offset_64_safe(void) {
    /*  bit_offset >= 64 must NOT shift by 64+ (UB) — helper guards via
     *  early-return false. */
    TEST_ASSERT_FALSE(detect_orphaned_bits(~(uint64_t)0, 64));
    TEST_ASSERT_FALSE(detect_orphaned_bits(~(uint64_t)0, 128));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_extract_unsigned_byte_aligned);
    RUN_TEST(test_extract_unsigned_high_nibble);
    RUN_TEST(test_extract_unsigned_cross_byte);
    RUN_TEST(test_extract_unsigned_full_byte_at_offset);
    RUN_TEST(test_extract_unsigned_one_bit);
    RUN_TEST(test_extract_signed_negative_4bit);
    RUN_TEST(test_extract_signed_positive_4bit);
    RUN_TEST(test_extract_signed_full_widths);
    RUN_TEST(test_insert_extract_round_trip);
    RUN_TEST(test_insert_preserves_surrounding_bits);
    RUN_TEST(test_orphaned_bits_high_set);
    RUN_TEST(test_orphaned_bits_high_clear);
    RUN_TEST(test_orphaned_bits_offset_zero);
    RUN_TEST(test_orphaned_bits_offset_64_safe);
    return UNITY_END();
}
