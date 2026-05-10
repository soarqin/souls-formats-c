/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Round-trip + structural tests for sf_fmg_write_*.
 * Mirrors the upstream Write path defined in
 *   SoulsFormatsNEXT/SoulsFormats/Formats/FMG.cs:144-276
 *
 * Strategy: build an FMG via the public API, write it to memory, then read
 * it back through sf_fmg_read_from_memory and compare entry-by-entry. A
 * few tests poke at raw bytes to verify specific structural invariants
 * (MD5 prefix, ReuseOffsets dedup, deleted-entry offset).
 */

#include "souls_formats/sf_fmg.h"

#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static int32_t le_i32(const uint8_t *p) {
    return (int32_t)(((uint32_t)p[0]) |
                     ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) |
                     ((uint32_t)p[3] << 24));
}

static int64_t le_i64(const uint8_t *p) {
    uint64_t v = 0;
    for (size_t i = 0; i < 8; i++) v |= ((uint64_t)p[i]) << (i * 8);
    return (int64_t)v;
}

static bool any_byte_nonzero(const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; i++) if (p[i] != 0) return true;
    return false;
}

static void assert_entry(const sf_fmg_t *fmg, size_t index, int32_t expected_id,
                         const char *expected_text) {
    const sf_fmg_entry_t *e = sf_fmg_get_entry(fmg, index);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_INT32(expected_id, sf_fmg_entry_get_id(e));
    if (expected_text) {
        TEST_ASSERT_NOT_NULL(sf_fmg_entry_get_text(e));
        TEST_ASSERT_EQUAL_STRING(expected_text, sf_fmg_entry_get_text(e));
    } else {
        TEST_ASSERT_NULL(sf_fmg_entry_get_text(e));
    }
}

static void test_v0_round_trip_shift_jis(void) {
    sf_fmg_t *fmg = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_create(NULL, SF_FMG_VERSION_DEMONS_SOULS, &fmg));
    TEST_ASSERT_NOT_NULL(fmg);
    sf_fmg_set_unicode(fmg, false);
    sf_fmg_set_big_endian(fmg, false);

    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 100, "Hello", NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 101, "World", NULL));

    uint8_t *buf = NULL;
    size_t   sz  = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_write_to_memory(fmg, &buf, &sz, NULL));
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_GREATER_THAN_size_t(0u, sz);

    sf_fmg_t *roundtrip = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_read_from_memory(&roundtrip, buf, sz, NULL));
    TEST_ASSERT_NOT_NULL(roundtrip);
    TEST_ASSERT_EQUAL(SF_FMG_VERSION_DEMONS_SOULS, sf_fmg_get_version(roundtrip));
    TEST_ASSERT_FALSE(sf_fmg_is_unicode(roundtrip));
    TEST_ASSERT_FALSE(sf_fmg_has_md5(roundtrip));
    TEST_ASSERT_EQUAL_size_t(2, sf_fmg_get_entry_count(roundtrip));
    assert_entry(roundtrip, 0, 100, "Hello");
    assert_entry(roundtrip, 1, 101, "World");

    sf_fmg_destroy(roundtrip, NULL);
    sf_fmg_destroy(fmg, NULL);
    free(buf);
}

static void test_v1_round_trip_unicode(void) {
    sf_fmg_t *fmg = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_create(NULL, SF_FMG_VERSION_DARK_SOULS_1, &fmg));

    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 1000, "Foo", NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 1001, "Bar", NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 1002, "Baz", NULL));

    uint8_t *buf = NULL;
    size_t   sz  = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_write_to_memory(fmg, &buf, &sz, NULL));

    sf_fmg_t *roundtrip = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_read_from_memory(&roundtrip, buf, sz, NULL));
    TEST_ASSERT_EQUAL(SF_FMG_VERSION_DARK_SOULS_1, sf_fmg_get_version(roundtrip));
    TEST_ASSERT_TRUE(sf_fmg_is_unicode(roundtrip));
    TEST_ASSERT_EQUAL_size_t(3, sf_fmg_get_entry_count(roundtrip));
    assert_entry(roundtrip, 0, 1000, "Foo");
    assert_entry(roundtrip, 1, 1001, "Bar");
    assert_entry(roundtrip, 2, 1002, "Baz");

    sf_fmg_destroy(roundtrip, NULL);
    sf_fmg_destroy(fmg, NULL);
    free(buf);
}

static void test_v2_wide_round_trip_utf16(void) {
    sf_fmg_t *fmg = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_create(NULL, SF_FMG_VERSION_DARK_SOULS_3, &fmg));

    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 5000, "\xCE\xB1", NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 5001, "\xCE\xB2", NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 5002, "\xCE\xB3", NULL));

    uint8_t *buf = NULL;
    size_t   sz  = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_write_to_memory(fmg, &buf, &sz, NULL));

    TEST_ASSERT_EQUAL_INT32(0xFF, le_i32(&buf[20]));

    sf_fmg_t *roundtrip = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_read_from_memory(&roundtrip, buf, sz, NULL));
    TEST_ASSERT_EQUAL(SF_FMG_VERSION_DARK_SOULS_3, sf_fmg_get_version(roundtrip));
    TEST_ASSERT_TRUE(sf_fmg_is_unicode(roundtrip));
    TEST_ASSERT_EQUAL_size_t(3, sf_fmg_get_entry_count(roundtrip));
    assert_entry(roundtrip, 0, 5000, "\xCE\xB1");
    assert_entry(roundtrip, 1, 5001, "\xCE\xB2");
    assert_entry(roundtrip, 2, 5002, "\xCE\xB3");

    sf_fmg_destroy(roundtrip, NULL);
    sf_fmg_destroy(fmg, NULL);
    free(buf);
}

static void test_md5_prefix_is_written_and_round_trips(void) {
    sf_fmg_t *fmg = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_create(NULL, SF_FMG_VERSION_DARK_SOULS_1, &fmg));
    sf_fmg_set_md5(fmg, true);

    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 42, "MD5_OK", NULL));

    uint8_t *buf = NULL;
    size_t   sz  = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_write_to_memory(fmg, &buf, &sz, NULL));
    TEST_ASSERT_GREATER_THAN_size_t(16u, sz);
    TEST_ASSERT_TRUE(any_byte_nonzero(buf, 16));

    sf_fmg_t *roundtrip = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_read_from_memory(&roundtrip, buf, sz, NULL));
    TEST_ASSERT_TRUE(sf_fmg_has_md5(roundtrip));
    assert_entry(roundtrip, 0, 42, "MD5_OK");

    sf_fmg_destroy(roundtrip, NULL);
    sf_fmg_destroy(fmg, NULL);
    free(buf);
}

static void test_reuse_offsets_dedup_shrinks_output(void) {
    const char *shared = "REPEATING_TEXT_FOR_DEDUP";

    sf_fmg_t *plain = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_create(NULL, SF_FMG_VERSION_DARK_SOULS_1, &plain));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(plain, 1, shared, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(plain, 2, shared, NULL));

    sf_fmg_t *deduped = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_create(NULL, SF_FMG_VERSION_DARK_SOULS_1, &deduped));
    sf_fmg_set_reuse_offsets(deduped, true);
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(deduped, 1, shared, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(deduped, 2, shared, NULL));

    uint8_t *plain_buf = NULL, *dedup_buf = NULL;
    size_t   plain_sz  = 0, dedup_sz = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_write_to_memory(plain,   &plain_buf, &plain_sz, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_write_to_memory(deduped, &dedup_buf, &dedup_sz, NULL));

    TEST_ASSERT_TRUE(dedup_sz < plain_sz);

    const int64_t string_offsets_offset = le_i32(&dedup_buf[20]);
    const int32_t offset_a = le_i32(&dedup_buf[(size_t)string_offsets_offset + 0]);
    const int32_t offset_b = le_i32(&dedup_buf[(size_t)string_offsets_offset + 4]);
    TEST_ASSERT_EQUAL_INT32(offset_a, offset_b);
    TEST_ASSERT_GREATER_THAN_INT32(0, offset_a);

    sf_fmg_t *roundtrip = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_read_from_memory(&roundtrip, dedup_buf, dedup_sz, NULL));
    assert_entry(roundtrip, 0, 1, shared);
    assert_entry(roundtrip, 1, 2, shared);

    sf_fmg_destroy(roundtrip, NULL);
    sf_fmg_destroy(deduped,   NULL);
    sf_fmg_destroy(plain,     NULL);
    free(plain_buf);
    free(dedup_buf);
}

static void test_deleted_entry_writes_zero_offset(void) {
    sf_fmg_t *fmg = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_create(NULL, SF_FMG_VERSION_DARK_SOULS_1, &fmg));

    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 1, "OK",  NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 2, NULL, NULL));

    uint8_t *buf = NULL;
    size_t   sz  = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_write_to_memory(fmg, &buf, &sz, NULL));

    const int64_t string_offsets_offset = le_i32(&buf[20]);
    const int32_t offset_ok      = le_i32(&buf[(size_t)string_offsets_offset + 0]);
    const int32_t offset_deleted = le_i32(&buf[(size_t)string_offsets_offset + 4]);
    TEST_ASSERT_GREATER_THAN_INT32(0, offset_ok);
    TEST_ASSERT_EQUAL_INT32(0, offset_deleted);

    sf_fmg_t *roundtrip = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_read_from_memory(&roundtrip, buf, sz, NULL));
    TEST_ASSERT_EQUAL_size_t(2, sf_fmg_get_entry_count(roundtrip));
    assert_entry(roundtrip, 0, 1, "OK");
    assert_entry(roundtrip, 1, 2, NULL);

    sf_fmg_destroy(roundtrip, NULL);
    sf_fmg_destroy(fmg, NULL);
    free(buf);
}

static void test_group_merging_splits_on_id_gap(void) {
    sf_fmg_t *fmg = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_create(NULL, SF_FMG_VERSION_DARK_SOULS_1, &fmg));

    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 5, "five",   NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 1, "one",    NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 6, "six",    NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 2, "two",    NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 100, "hundred", NULL));

    uint8_t *buf = NULL;
    size_t   sz  = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_write_to_memory(fmg, &buf, &sz, NULL));

    const int32_t group_count = le_i32(&buf[12]);
    TEST_ASSERT_EQUAL_INT32(3, group_count);

    const int32_t string_count = le_i32(&buf[16]);
    TEST_ASSERT_EQUAL_INT32(5, string_count);

    sf_fmg_t *roundtrip = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_read_from_memory(&roundtrip, buf, sz, NULL));
    TEST_ASSERT_EQUAL_size_t(5, sf_fmg_get_entry_count(roundtrip));
    assert_entry(roundtrip, 0, 1,   "one");
    assert_entry(roundtrip, 1, 2,   "two");
    assert_entry(roundtrip, 2, 5,   "five");
    assert_entry(roundtrip, 3, 6,   "six");
    assert_entry(roundtrip, 4, 100, "hundred");

    sf_fmg_destroy(roundtrip, NULL);
    sf_fmg_destroy(fmg, NULL);
    free(buf);
}

static void test_empty_fmg_round_trip(void) {
    sf_fmg_t *fmg = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_create(NULL, SF_FMG_VERSION_DARK_SOULS_1, &fmg));

    uint8_t *buf = NULL;
    size_t   sz  = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_write_to_memory(fmg, &buf, &sz, NULL));
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_GREATER_THAN_size_t(0u, sz);

    TEST_ASSERT_EQUAL_INT32(0, le_i32(&buf[12]));
    TEST_ASSERT_EQUAL_INT32(0, le_i32(&buf[16]));

    sf_fmg_t *roundtrip = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_read_from_memory(&roundtrip, buf, sz, NULL));
    TEST_ASSERT_EQUAL_size_t(0, sf_fmg_get_entry_count(roundtrip));

    sf_fmg_destroy(roundtrip, NULL);
    sf_fmg_destroy(fmg, NULL);
    free(buf);
}

static void test_v2_wide_offsets_are_8_bytes(void) {
    sf_fmg_t *fmg = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_create(NULL, SF_FMG_VERSION_DARK_SOULS_3, &fmg));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 0, "a", NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_add_entry(fmg, 1, "b", NULL));

    uint8_t *buf = NULL;
    size_t   sz  = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_fmg_write_to_memory(fmg, &buf, &sz, NULL));

    const int64_t string_offsets_offset = le_i64(&buf[24]);
    TEST_ASSERT_GREATER_THAN_INT64(0, string_offsets_offset);

    const int64_t offset_0 = le_i64(&buf[(size_t)string_offsets_offset + 0]);
    const int64_t offset_1 = le_i64(&buf[(size_t)string_offsets_offset + 8]);
    TEST_ASSERT_GREATER_THAN_INT64(0, offset_0);
    TEST_ASSERT_TRUE(offset_1 > offset_0);

    sf_fmg_destroy(fmg, NULL);
    free(buf);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_v0_round_trip_shift_jis);
    RUN_TEST(test_v1_round_trip_unicode);
    RUN_TEST(test_v2_wide_round_trip_utf16);
    RUN_TEST(test_md5_prefix_is_written_and_round_trips);
    RUN_TEST(test_reuse_offsets_dedup_shrinks_output);
    RUN_TEST(test_deleted_entry_writes_zero_offset);
    RUN_TEST(test_group_merging_splits_on_id_gap);
    RUN_TEST(test_empty_fmg_round_trip);
    RUN_TEST(test_v2_wide_offsets_are_8_bytes);
    return UNITY_END();
}
