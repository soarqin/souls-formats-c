/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_paramtdf.h"

#include "unity.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static sf_paramtdf_t *parse_text(const char *text) {
    sf_paramtdf_t *tdf = NULL;
    sf_result_t r = sf_paramtdf_read_from_text(text, strlen(text), &tdf, NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SF_OK, r, sf_result_str(r));
    TEST_ASSERT_NOT_NULL(tdf);
    return tdf;
}

static char *write_text(const sf_paramtdf_t *tdf, size_t *out_size) {
    char *text = NULL;
    sf_result_t r = sf_paramtdf_write_to_text(tdf, &text, out_size, NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SF_OK, r, sf_result_str(r));
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(out_size);
    return text;
}

static void assert_round_trip_matches(const char *input, const char *expected_output) {
    sf_paramtdf_t *tdf = parse_text(input);

    size_t out_size = 0;
    char *text = write_text(tdf, &out_size);
    TEST_ASSERT_EQUAL_STRING(expected_output, text);
    TEST_ASSERT_EQUAL_size_t(strlen(expected_output), out_size);

    sf_paramtdf_t *roundtrip = parse_text(text);
    TEST_ASSERT_EQUAL_STRING(sf_paramtdf_get_name(tdf), sf_paramtdf_get_name(roundtrip));
    TEST_ASSERT_EQUAL_INT(sf_paramtdf_get_type(tdf), sf_paramtdf_get_type(roundtrip));
    TEST_ASSERT_EQUAL_size_t(sf_paramtdf_get_entry_count(tdf), sf_paramtdf_get_entry_count(roundtrip));

    for (size_t i = 0; i < sf_paramtdf_get_entry_count(tdf); i++) {
        const sf_paramtdf_entry_t *lhs = sf_paramtdf_get_entry(tdf, i);
        const sf_paramtdf_entry_t *rhs = sf_paramtdf_get_entry(roundtrip, i);
        TEST_ASSERT_NOT_NULL(lhs);
        TEST_ASSERT_NOT_NULL(rhs);
        TEST_ASSERT_EQUAL_STRING(sf_paramtdf_entry_get_name(lhs), sf_paramtdf_entry_get_name(rhs));
        TEST_ASSERT_EQUAL_INT64(sf_paramtdf_entry_get_value(lhs), sf_paramtdf_entry_get_value(rhs));
    }

    sf_free(NULL, text);
    sf_paramtdf_destroy(roundtrip, NULL);
    sf_paramtdf_destroy(tdf, NULL);
}

static void test_three_entry_round_trip_u32(void) {
    static const char input[] =
        "\"MyEnum\"\r\n"
        "\"u32\"\r\n"
        "\"None\",\"0\"\r\n"
        "\"On\",\"1\"\r\n"
        "\"Off\",\"2\"";
    static const char expected[] =
        "\"MyEnum\"\r\n"
        "\"u32\"\r\n"
        "\"None\",\"0\"\r\n"
        "\"On\",\"1\"\r\n"
        "\"Off\",\"2\"\r\n";

    assert_round_trip_matches(input, expected);
}

static void test_null_name_entry_writes_empty_name_slot(void) {
    static const char input[] =
        "\"X\"\r\n"
        "\"s32\"\r\n"
        ",\"42\"";
    static const char expected[] =
        "\"X\"\r\n"
        "\"s32\"\r\n"
        ",\"42\"\r\n";

    sf_paramtdf_t *tdf = parse_text(input);

    size_t out_size = 0;
    char *text = write_text(tdf, &out_size);
    TEST_ASSERT_EQUAL_STRING(expected, text);
    TEST_ASSERT_EQUAL_size_t(strlen(expected), out_size);

    sf_paramtdf_t *roundtrip = parse_text(text);
    TEST_ASSERT_EQUAL_STRING("X", sf_paramtdf_get_name(roundtrip));
    TEST_ASSERT_EQUAL_INT(SF_PARAMTDF_TYPE_S32, sf_paramtdf_get_type(roundtrip));
    TEST_ASSERT_EQUAL_size_t(1, sf_paramtdf_get_entry_count(roundtrip));
    TEST_ASSERT_NULL(sf_paramtdf_entry_get_name(sf_paramtdf_get_entry(roundtrip, 0)));
    TEST_ASSERT_EQUAL_INT64(42, sf_paramtdf_entry_get_value(sf_paramtdf_get_entry(roundtrip, 0)));

    sf_free(NULL, text);
    sf_paramtdf_destroy(roundtrip, NULL);
    sf_paramtdf_destroy(tdf, NULL);
}

static void test_all_six_types_write_type_names(void) {
    static const struct {
        const char *input;
        const char *expected;
    } cases[] = {
        { "\"T\"\r\n\"s8\"\r\n\"a\",\"-128\"",
          "\"T\"\r\n\"s8\"\r\n\"a\",\"-128\"\r\n" },
        { "\"T\"\r\n\"u8\"\r\n\"a\",\"255\"",
          "\"T\"\r\n\"u8\"\r\n\"a\",\"255\"\r\n" },
        { "\"T\"\r\n\"s16\"\r\n\"a\",\"-1\"",
          "\"T\"\r\n\"s16\"\r\n\"a\",\"-1\"\r\n" },
        { "\"T\"\r\n\"u16\"\r\n\"a\",\"65535\"",
          "\"T\"\r\n\"u16\"\r\n\"a\",\"65535\"\r\n" },
        { "\"T\"\r\n\"s32\"\r\n\"a\",\"-2147483648\"",
          "\"T\"\r\n\"s32\"\r\n\"a\",\"-2147483648\"\r\n" },
        { "\"T\"\r\n\"u32\"\r\n\"a\",\"4294967295\"",
          "\"T\"\r\n\"u32\"\r\n\"a\",\"4294967295\"\r\n" },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        sf_paramtdf_t *tdf = parse_text(cases[i].input);
        size_t out_size = 0;
        char *text = write_text(tdf, &out_size);
        TEST_ASSERT_EQUAL_STRING(cases[i].expected, text);
        TEST_ASSERT_EQUAL_size_t(strlen(cases[i].expected), out_size);
        sf_free(NULL, text);
        sf_paramtdf_destroy(tdf, NULL);
    }
}

static void test_null_paramtdf_rejected(void) {
    char *text = (char *)0x1;
    size_t out_size = 123;
    sf_result_t r = sf_paramtdf_write_to_text(NULL, &text, &out_size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG, r);
    TEST_ASSERT_EQUAL_PTR((char *)0x1, text);
    TEST_ASSERT_EQUAL_size_t(123, out_size);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_three_entry_round_trip_u32);
    RUN_TEST(test_null_name_entry_writes_empty_name_slot);
    RUN_TEST(test_all_six_types_write_type_names);
    RUN_TEST(test_null_paramtdf_rejected);
    return UNITY_END();
}
