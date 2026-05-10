/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 3 QA — ENFL zlib-payload + empty-archive synthetic checks.
 *
 * Goals:
 *   1. write_to_memory(read_from_memory(write_to_memory(b))) is byte-equal
 *      to the first write_to_memory(b) for a populated ENFL.
 *   2. The same round-trip succeeds for an EMPTY ENFL (zero counts) and
 *      respects all 0x10 alignment markers including the trailing string
 *      block.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "souls_formats/sf_enfl.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void roundtrip_assert(const sf_enfl_t *e1) {
    uint8_t *b1 = NULL;
    size_t   n1 = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_enfl_write_to_memory(e1, &b1, &n1, NULL));

    sf_enfl_t *e2 = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_enfl_read_from_memory(&e2, b1, n1, NULL));

    TEST_ASSERT_EQUAL_size_t(sf_enfl_struct1_count(e1), sf_enfl_struct1_count(e2));
    TEST_ASSERT_EQUAL_size_t(sf_enfl_struct2_count(e1), sf_enfl_struct2_count(e2));
    TEST_ASSERT_EQUAL_size_t(sf_enfl_string_count (e1), sf_enfl_string_count (e2));

    for (size_t i = 0; i < sf_enfl_struct1_count(e1); i++) {
        const sf_enfl_struct1_t *a = sf_enfl_get_struct1(e1, i);
        const sf_enfl_struct1_t *b = sf_enfl_get_struct1(e2, i);
        TEST_ASSERT_NOT_NULL(a);
        TEST_ASSERT_NOT_NULL(b);
        TEST_ASSERT_EQUAL_INT16(a->step,  b->step);
        TEST_ASSERT_EQUAL_INT16(a->index, b->index);
    }
    for (size_t i = 0; i < sf_enfl_struct2_count(e1); i++) {
        const sf_enfl_struct2_t *a = sf_enfl_get_struct2(e1, i);
        const sf_enfl_struct2_t *b = sf_enfl_get_struct2(e2, i);
        TEST_ASSERT_NOT_NULL(a);
        TEST_ASSERT_NOT_NULL(b);
        TEST_ASSERT_EQUAL_INT64(a->unk1, b->unk1);
    }
    for (size_t i = 0; i < sf_enfl_string_count(e1); i++) {
        TEST_ASSERT_EQUAL_STRING(sf_enfl_get_string(e1, i), sf_enfl_get_string(e2, i));
    }

    uint8_t *b2 = NULL;
    size_t   n2 = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_enfl_write_to_memory(e2, &b2, &n2, NULL));

    TEST_ASSERT_EQUAL_size_t(n1, n2);
    TEST_ASSERT_EQUAL_MEMORY(b1, b2, n1);

    sf_free(NULL, b1);
    sf_free(NULL, b2);
    sf_enfl_destroy(e2);
}

static void test_enfl_zlib_payload(void) {
    sf_enfl_t *e = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_enfl_create(&e, NULL));

    sf_enfl_struct1_t s1a = { .step = 1, .index = 0 };
    sf_enfl_struct1_t s1b = { .step = 0, .index = 1 };
    sf_enfl_struct1_t s1c = { .step = 2, .index = 2 };
    TEST_ASSERT_EQUAL(SF_OK, sf_enfl_add_struct1(e, s1a));
    TEST_ASSERT_EQUAL(SF_OK, sf_enfl_add_struct1(e, s1b));
    TEST_ASSERT_EQUAL(SF_OK, sf_enfl_add_struct1(e, s1c));

    sf_enfl_struct2_t s2a = { .unk1 = 0x0123456789ABCDEFLL };
    sf_enfl_struct2_t s2b = { .unk1 = 0x0FEEDFACEDEADBEEFLL };
    sf_enfl_struct2_t s2c = { .unk1 = 0x0000000000000001LL };
    TEST_ASSERT_EQUAL(SF_OK, sf_enfl_add_struct2(e, s2a));
    TEST_ASSERT_EQUAL(SF_OK, sf_enfl_add_struct2(e, s2b));
    TEST_ASSERT_EQUAL(SF_OK, sf_enfl_add_struct2(e, s2c));

    TEST_ASSERT_EQUAL(SF_OK, sf_enfl_add_string(e, "alpha"));
    TEST_ASSERT_EQUAL(SF_OK, sf_enfl_add_string(e, "beta.dat"));
    TEST_ASSERT_EQUAL(SF_OK, sf_enfl_add_string(e, "gamma/path.bin"));

    roundtrip_assert(e);
    sf_enfl_destroy(e);
}

static void test_enfl_empty(void) {
    sf_enfl_t *e = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_enfl_create(&e, NULL));
    TEST_ASSERT_EQUAL_size_t(0, sf_enfl_struct1_count(e));
    TEST_ASSERT_EQUAL_size_t(0, sf_enfl_struct2_count(e));
    TEST_ASSERT_EQUAL_size_t(0, sf_enfl_string_count(e));

    roundtrip_assert(e);
    sf_enfl_destroy(e);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_enfl_zlib_payload);
    RUN_TEST(test_enfl_empty);
    return UNITY_END();
}
