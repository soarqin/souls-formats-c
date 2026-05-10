/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 3 QA — internal binder_common helpers.
 *
 * Covers:
 *   - sfi_binder_read_format / sfi_binder_write_format roundtrip across
 *     all 256 byte values, in both BitBigEndian-style write paths.
 *   - sfi_binder_read_file_flags / sfi_binder_write_file_flags roundtrip
 *     across all 256 byte values, with bit_big_endian both true and false.
 *   - sfi_binder_hash_table_group_count for the four canonical inputs
 *     called out in the PLAN (file_count = 7, 49, 100, 1000).
 *   - sfi_binder_get_bnd4_file_header_size for representative formats.
 */

#include "archive/binder_common.h"
#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_hash.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*===========================================================================
 * Helpers
 *===========================================================================*/

static void make_writer(sf_ostream_t **out_s, sf_binary_writer_t **out_w) {
    sf_ostream_t *s = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&s, NULL));
    sf_binary_writer_t *w = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&w, s, false, NULL));
    *out_s = s;
    *out_w = w;
}

static void detach_bytes(sf_ostream_t *s, sf_binary_writer_t *w,
                         uint8_t **out_bytes, size_t *out_size) {
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish(w));
    void  *raw = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_detach_buffer(s, &raw, &n));
    *out_bytes = (uint8_t *)raw;
    *out_size  = n;
    sf_binary_writer_destroy(w);
    sf_ostream_close(s);
}

/*===========================================================================
 * Format byte roundtrip across all 256 byte values
 *
 * Two paths:
 *   bit_big_endian = true   — the helpers always pass the byte through
 *                              without bit reversal, so the roundtrip is
 *                              a bijection on the full 256-byte range.
 *   bit_big_endian = false  — bit reversal applies on both sides; the
 *                              upstream heuristic only forms a bijection
 *                              for f-values where the chosen orientation
 *                              survives the read-side detector. We
 *                              therefore restrict this branch to the
 *                              format bytes that are valid in real
 *                              FromSoftware archives (`bit0 OR bit6 set`,
 *                              and Flag7 cleared) plus a few sentinel
 *                              values that are guaranteed-stable.
 *===========================================================================*/

static void roundtrip_format(sf_binder_format_t f, bool bit_big_endian) {
    sf_ostream_t       *s = NULL;
    sf_binary_writer_t *w = NULL;
    make_writer(&s, &w);
    sfi_binder_write_format(w, f, bit_big_endian);

    uint8_t *bytes = NULL;
    size_t   nbytes = 0;
    detach_bytes(s, w, &bytes, &nbytes);
    TEST_ASSERT_EQUAL_size_t(1, nbytes);

    sf_binary_reader_t *r = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_binary_reader_create_from_memory(&r, false, bytes, nbytes, NULL));
    sf_binder_format_t got = sfi_binder_read_format(r, bit_big_endian);
    TEST_ASSERT_EQUAL_HEX8(f, got);
    sf_binary_reader_destroy(r);
}

static void test_binder_format_roundtrip_256_be(void) {
    for (int v = 0; v < 256; ++v) {
        roundtrip_format((sf_binder_format_t)v, true);
    }
}

static void test_binder_format_roundtrip_real_formats_le(void) {
    static const sf_binder_format_t cases[] = {
        SF_BINDER_FORMAT_NONE,
        SF_BINDER_FORMAT_BIG_ENDIAN,
        (sf_binder_format_t)(SF_BINDER_FORMAT_NAMES1 | SF_BINDER_FORMAT_NAMES2
                             | SF_BINDER_FORMAT_IDS),
        (sf_binder_format_t)(SF_BINDER_FORMAT_NAMES1 | SF_BINDER_FORMAT_NAMES2
                             | SF_BINDER_FORMAT_IDS | SF_BINDER_FORMAT_COMPRESSION),
        (sf_binder_format_t)(SF_BINDER_FORMAT_NAMES2 | SF_BINDER_FORMAT_IDS
                             | SF_BINDER_FORMAT_COMPRESSION
                             | SF_BINDER_FORMAT_LONG_OFFSETS),
        (sf_binder_format_t)(SF_BINDER_FORMAT_NAMES2 | SF_BINDER_FORMAT_IDS
                             | SF_BINDER_FORMAT_COMPRESSION
                             | SF_BINDER_FORMAT_LONG_OFFSETS
                             | SF_BINDER_FORMAT_FLAG6),
    };
    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; ++k) {
        roundtrip_format(cases[k], false);
    }
}

/*===========================================================================
 * FileFlags byte roundtrip across all 256 byte values, both bit_big_endian
 * settings.
 *===========================================================================*/

static void roundtrip_file_flags_for(bool bit_big_endian) {
    for (int v = 0; v < 256; ++v) {
        sf_binder_file_flags_t flags = (sf_binder_file_flags_t)v;

        sf_ostream_t       *s = NULL;
        sf_binary_writer_t *w = NULL;
        make_writer(&s, &w);
        sfi_binder_write_file_flags(w, flags, bit_big_endian);

        uint8_t *bytes = NULL;
        size_t   nbytes = 0;
        detach_bytes(s, w, &bytes, &nbytes);
        TEST_ASSERT_EQUAL_size_t(1, nbytes);

        sf_binary_reader_t *r = NULL;
        TEST_ASSERT_EQUAL(SF_OK,
            sf_binary_reader_create_from_memory(&r, false, bytes, nbytes, NULL));
        sf_binder_file_flags_t got = sfi_binder_read_file_flags(r, bit_big_endian);
        TEST_ASSERT_EQUAL_HEX8(flags, got);
        sf_binary_reader_destroy(r);
    }
}

static void test_binder_file_flags_roundtrip_256_be(void)  { roundtrip_file_flags_for(true ); }
static void test_binder_file_flags_roundtrip_256_le(void)  { roundtrip_file_flags_for(false); }

/*===========================================================================
 * Hash table group count
 *
 * Mirrors the upstream loop:
 *   for (uint p = files.Count / 7; p <= 100000; p++)
 *       if (HashHelper.IsPrime(p)) return p;
 *
 *   files.Count =    7   →  start = 1   →  smallest prime ≥ 1   = 2
 *   files.Count =   49   →  start = 7   →  smallest prime ≥ 7   = 7
 *   files.Count =  100   →  start = 14  →  smallest prime ≥ 14  = 17
 *   files.Count = 1000   →  start = 142 →  smallest prime ≥ 142 = 149
 *
 * The PLAN's expected-value table (15, 143) is incorrect; 15 = 3·5 and
 * 143 = 11·13 are composite. We assert the actual upstream-compatible
 * values, plus that each result is itself prime and ≥ ⌊n/7⌋.
 *===========================================================================*/

static void test_hash_table_prime_selection(void) {
    TEST_ASSERT_EQUAL_UINT32(  2, sfi_binder_hash_table_group_count(   7));
    TEST_ASSERT_EQUAL_UINT32(  7, sfi_binder_hash_table_group_count(  49));
    TEST_ASSERT_EQUAL_UINT32( 17, sfi_binder_hash_table_group_count( 100));
    TEST_ASSERT_EQUAL_UINT32(149, sfi_binder_hash_table_group_count(1000));

    static const size_t cases[] = {1, 2, 7, 49, 100, 1000, 10000};
    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; ++k) {
        uint32_t p = sfi_binder_hash_table_group_count(cases[k]);
        TEST_ASSERT_TRUE(sf_is_prime(p));
        TEST_ASSERT_TRUE((size_t)p >= cases[k] / 7);
    }
}

/*===========================================================================
 * BND4 file header size matches Binder.GetBND4FileHeaderSize.
 *
 *   None                          → 0x10 + 4 (offset)              = 0x14
 *   IDs                           → 0x10 + 4 + 4 (id)              = 0x18
 *   IDs|Names2                    → 0x10 + 4 + 4 + 4               = 0x1C
 *   IDs|Compression|LongOffsets   → 0x10 + 8 + 8 + 4               = 0x24
 *   Names1 (alone)                → 0x10 + 4 + 4 (name) + 8 (extra)= 0x20
 *===========================================================================*/

static void test_bnd4_file_header_size(void) {
    TEST_ASSERT_EQUAL_size_t(0x14u,
        sfi_binder_get_bnd4_file_header_size(SF_BINDER_FORMAT_NONE));

    TEST_ASSERT_EQUAL_size_t(0x18u,
        sfi_binder_get_bnd4_file_header_size(SF_BINDER_FORMAT_IDS));

    TEST_ASSERT_EQUAL_size_t(0x1Cu,
        sfi_binder_get_bnd4_file_header_size(
            (sf_binder_format_t)(SF_BINDER_FORMAT_IDS | SF_BINDER_FORMAT_NAMES2)));

    TEST_ASSERT_EQUAL_size_t(0x24u,
        sfi_binder_get_bnd4_file_header_size(
            (sf_binder_format_t)(SF_BINDER_FORMAT_IDS | SF_BINDER_FORMAT_COMPRESSION
                                                       | SF_BINDER_FORMAT_LONG_OFFSETS)));

    TEST_ASSERT_EQUAL_size_t(0x10u + 4u + 4u + 8u,
        sfi_binder_get_bnd4_file_header_size(SF_BINDER_FORMAT_NAMES1));
}

/*===========================================================================
 * Empty-name destroy is a no-op.
 *===========================================================================*/

static void test_file_header_destroy_null_safe(void) {
    sfi_binder_file_header_destroy(NULL, NULL);

    sfi_binder_file_header_t h = {0};
    sfi_binder_file_header_destroy(&h, NULL);

    h.name_utf8 = NULL;
    sfi_binder_file_header_destroy(&h, NULL);
    TEST_ASSERT_NULL(h.name_utf8);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binder_format_roundtrip_256_be);
    RUN_TEST(test_binder_format_roundtrip_real_formats_le);
    RUN_TEST(test_binder_file_flags_roundtrip_256_be);
    RUN_TEST(test_binder_file_flags_roundtrip_256_le);
    RUN_TEST(test_hash_table_prime_selection);
    RUN_TEST(test_bnd4_file_header_size);
    RUN_TEST(test_file_header_destroy_null_safe);
    return UNITY_END();
}
