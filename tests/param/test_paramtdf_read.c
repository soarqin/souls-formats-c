/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 4 QA — sf_paramtdf_read_from_text() mirrors PARAMTDF.cs:62-92
 * naive Trim('"') parsing semantics.
 */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_paramtdf.h"

#include "unity.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void assert_text_parses(const char *text, sf_paramtdf_t **out) {
    sf_result_t r = sf_paramtdf_read_from_text(text, strlen(text), out, NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SF_OK, r, sf_result_str(r));
    TEST_ASSERT_NOT_NULL(*out);
}

static void assert_text_rejects(const char *text, sf_result_t expected) {
    sf_paramtdf_t *tdf = NULL;
    sf_result_t r = sf_paramtdf_read_from_text(text, strlen(text), &tdf, NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(expected, r, sf_result_str(r));
    TEST_ASSERT_NULL(tdf);
}

/*===========================================================================
 * Happy paths
 *===========================================================================*/

static void test_three_entry_u32(void) {
    static const char text[] =
        "\"MyEnum\"\r\n"
        "\"u32\"\r\n"
        "\"None\",\"0\"\r\n"
        "\"On\",\"1\"\r\n"
        "\"Off\",\"2\"";

    sf_paramtdf_t *tdf = NULL;
    assert_text_parses(text, &tdf);

    TEST_ASSERT_EQUAL_STRING("MyEnum", sf_paramtdf_get_name(tdf));
    TEST_ASSERT_EQUAL_INT(SF_PARAMTDF_TYPE_U32, sf_paramtdf_get_type(tdf));
    TEST_ASSERT_EQUAL_size_t(3, sf_paramtdf_get_entry_count(tdf));

    const sf_paramtdf_entry_t *e0 = sf_paramtdf_get_entry(tdf, 0);
    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_EQUAL_STRING("None", sf_paramtdf_entry_get_name(e0));
    TEST_ASSERT_EQUAL_INT64(0, sf_paramtdf_entry_get_value(e0));

    const sf_paramtdf_entry_t *e1 = sf_paramtdf_get_entry(tdf, 1);
    TEST_ASSERT_NOT_NULL(e1);
    TEST_ASSERT_EQUAL_STRING("On", sf_paramtdf_entry_get_name(e1));
    TEST_ASSERT_EQUAL_INT64(1, sf_paramtdf_entry_get_value(e1));

    const sf_paramtdf_entry_t *e2 = sf_paramtdf_get_entry(tdf, 2);
    TEST_ASSERT_NOT_NULL(e2);
    TEST_ASSERT_EQUAL_STRING("Off", sf_paramtdf_entry_get_name(e2));
    TEST_ASSERT_EQUAL_INT64(2, sf_paramtdf_entry_get_value(e2));

    sf_paramtdf_destroy(tdf, NULL);
}

static void test_empty_name_entry_yields_null(void) {
    static const char text[] =
        "\"X\"\r\n"
        "\"s32\"\r\n"
        ",\"42\"";

    sf_paramtdf_t *tdf = NULL;
    assert_text_parses(text, &tdf);

    TEST_ASSERT_EQUAL_STRING("X", sf_paramtdf_get_name(tdf));
    TEST_ASSERT_EQUAL_INT(SF_PARAMTDF_TYPE_S32, sf_paramtdf_get_type(tdf));
    TEST_ASSERT_EQUAL_size_t(1, sf_paramtdf_get_entry_count(tdf));

    const sf_paramtdf_entry_t *e0 = sf_paramtdf_get_entry(tdf, 0);
    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_NULL(sf_paramtdf_entry_get_name(e0));
    TEST_ASSERT_EQUAL_INT64(42, sf_paramtdf_entry_get_value(e0));

    sf_paramtdf_destroy(tdf, NULL);
}

static void test_signed_negative_value(void) {
    static const char text[] =
        "\"Signed\"\r\n"
        "\"s16\"\r\n"
        "\"Neg\",\"-32768\"\r\n"
        "\"Pos\",\"32767\"";

    sf_paramtdf_t *tdf = NULL;
    assert_text_parses(text, &tdf);

    TEST_ASSERT_EQUAL_INT(SF_PARAMTDF_TYPE_S16, sf_paramtdf_get_type(tdf));
    TEST_ASSERT_EQUAL_size_t(2, sf_paramtdf_get_entry_count(tdf));
    TEST_ASSERT_EQUAL_INT64(-32768, sf_paramtdf_entry_get_value(sf_paramtdf_get_entry(tdf, 0)));
    TEST_ASSERT_EQUAL_INT64(32767, sf_paramtdf_entry_get_value(sf_paramtdf_get_entry(tdf, 1)));

    sf_paramtdf_destroy(tdf, NULL);
}

static void test_all_six_types_each_parse(void) {
    static const struct {
        const char *text;
        sf_paramtdf_type_t type;
        int64_t value;
    } cases[] = {
        { "\"T\"\r\n\"s8\"\r\n\"a\",\"-128\"",          SF_PARAMTDF_TYPE_S8,  -128         },
        { "\"T\"\r\n\"u8\"\r\n\"a\",\"255\"",           SF_PARAMTDF_TYPE_U8,  255          },
        { "\"T\"\r\n\"s16\"\r\n\"a\",\"-1\"",           SF_PARAMTDF_TYPE_S16, -1           },
        { "\"T\"\r\n\"u16\"\r\n\"a\",\"65535\"",        SF_PARAMTDF_TYPE_U16, 65535        },
        { "\"T\"\r\n\"s32\"\r\n\"a\",\"-2147483648\"",  SF_PARAMTDF_TYPE_S32, INT32_MIN    },
        { "\"T\"\r\n\"u32\"\r\n\"a\",\"4294967295\"",   SF_PARAMTDF_TYPE_U32, 4294967295LL },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        sf_paramtdf_t *tdf = NULL;
        assert_text_parses(cases[i].text, &tdf);
        TEST_ASSERT_EQUAL_INT(cases[i].type, sf_paramtdf_get_type(tdf));
        TEST_ASSERT_EQUAL_size_t(1, sf_paramtdf_get_entry_count(tdf));
        TEST_ASSERT_EQUAL_INT64(cases[i].value,
                                sf_paramtdf_entry_get_value(sf_paramtdf_get_entry(tdf, 0)));
        sf_paramtdf_destroy(tdf, NULL);
    }
}

static void test_unix_newlines_and_blank_lines_skipped(void) {
    /*  StringSplitOptions.RemoveEmptyEntries: blank lines and bare CR/LF
     *  runs must be skipped, matching upstream PARAMTDF.cs:64. */
    static const char text[] =
        "\n\n"
        "\"WithLF\"\n"
        "\"u8\"\n"
        "\n"
        "\"A\",\"7\"\n";

    sf_paramtdf_t *tdf = NULL;
    assert_text_parses(text, &tdf);

    TEST_ASSERT_EQUAL_STRING("WithLF", sf_paramtdf_get_name(tdf));
    TEST_ASSERT_EQUAL_INT(SF_PARAMTDF_TYPE_U8, sf_paramtdf_get_type(tdf));
    TEST_ASSERT_EQUAL_size_t(1, sf_paramtdf_get_entry_count(tdf));
    TEST_ASSERT_EQUAL_STRING("A",
                              sf_paramtdf_entry_get_name(sf_paramtdf_get_entry(tdf, 0)));
    TEST_ASSERT_EQUAL_INT64(7,
                            sf_paramtdf_entry_get_value(sf_paramtdf_get_entry(tdf, 0)));

    sf_paramtdf_destroy(tdf, NULL);
}

/*===========================================================================
 * Failure paths
 *===========================================================================*/

static void test_rejects_f32_type(void) {
    /*  PARAMTDF.cs:25-29 limits the type to s8/u8/s16/u16/s32/u32. */
    static const char text[] =
        "\"X\"\r\n"
        "\"f32\"\r\n"
        "\"a\",\"1\"";
    assert_text_rejects(text, SF_ERR_INVALID_ARG);
}

static void test_rejects_unknown_type(void) {
    static const char text[] =
        "\"X\"\r\n"
        "\"banana\"\r\n"
        "\"a\",\"1\"";
    assert_text_rejects(text, SF_ERR_INVALID_ARG);
}

static void test_rejects_malformed_value(void) {
    /*  "notanumber" is not parseable as integer → upstream FormatException. */
    static const char text[] =
        "\"X\"\r\n"
        "\"u32\"\r\n"
        "\"a\",\"notanumber\"";
    assert_text_rejects(text, SF_ERR_OUT_OF_RANGE);
}

static void test_rejects_overflow_value(void) {
    /*  256 overflows u8 → upstream OverflowException. */
    static const char text[] =
        "\"X\"\r\n"
        "\"u8\"\r\n"
        "\"a\",\"256\"";
    assert_text_rejects(text, SF_ERR_OUT_OF_RANGE);
}

static void test_rejects_unsigned_negative(void) {
    /*  byte.Parse("-1") throws in C# even though strtoul wraps in C. */
    static const char text[] =
        "\"X\"\r\n"
        "\"u32\"\r\n"
        "\"a\",\"-1\"";
    assert_text_rejects(text, SF_ERR_OUT_OF_RANGE);
}

static void test_rejects_truncated_input(void) {
    /*  Only a name line — upstream throws IndexOutOfRangeException at
     *  lines[1]. */
    static const char text[] = "\"X\"";
    assert_text_rejects(text, SF_ERR_TRUNCATED);
}

static void test_rejects_entry_without_comma(void) {
    /*  Upstream `lines[i].Split(',')[1]` throws IndexOutOfRange when the
     *  line lacks a comma. */
    static const char text[] =
        "\"X\"\r\n"
        "\"u32\"\r\n"
        "\"NoComma\"";
    assert_text_rejects(text, SF_ERR_INVALID_ARG);
}

/*===========================================================================
 * Argument validation
 *===========================================================================*/

static void test_null_args_rejected(void) {
    sf_paramtdf_t *tdf = NULL;
    sf_result_t r = sf_paramtdf_read_from_text("text", 4, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG, r);

    r = sf_paramtdf_read_from_text(NULL, 4, &tdf, NULL);
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG, r);
    TEST_ASSERT_NULL(tdf);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_three_entry_u32);
    RUN_TEST(test_empty_name_entry_yields_null);
    RUN_TEST(test_signed_negative_value);
    RUN_TEST(test_all_six_types_each_parse);
    RUN_TEST(test_unix_newlines_and_blank_lines_skipped);
    RUN_TEST(test_rejects_f32_type);
    RUN_TEST(test_rejects_unknown_type);
    RUN_TEST(test_rejects_malformed_value);
    RUN_TEST(test_rejects_overflow_value);
    RUN_TEST(test_rejects_unsigned_negative);
    RUN_TEST(test_rejects_truncated_input);
    RUN_TEST(test_rejects_entry_without_comma);
    RUN_TEST(test_null_args_rejected);
    return UNITY_END();
}
