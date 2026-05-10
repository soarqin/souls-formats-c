/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Tests the PARAMDEF XML reader against synthetic XML fragments. Mirrors the
 * upstream XmlSerializer test surface; e2e tests against real Paramdex files
 * live in tests/param/test_paramdef_xml_e2e.c.
 */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_paramdef.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const char *const k_three_field_xml =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<PARAMDEF XmlVersion=\"2\">"
    "  <ParamType>TEST_PARAM</ParamType>"
    "  <DataVersion>1</DataVersion>"
    "  <BigEndian>False</BigEndian>"
    "  <Unicode>True</Unicode>"
    "  <FormatVersion>203</FormatVersion>"
    "  <Fields>"
    "    <Field Def=\"s32 iconId = -1\">"
    "      <DisplayName>Icon</DisplayName>"
    "      <Minimum>-1</Minimum>"
    "      <Maximum>999999</Maximum>"
    "      <SortID>1000</SortID>"
    "    </Field>"
    "    <Field Def=\"f32 effectEndurance = 0.0\"></Field>"
    "    <Field Def=\"u8 flag:1\"></Field>"
    "  </Fields>"
    "</PARAMDEF>";

static void test_xml_reads_three_field_happy_path(void) {
    sf_paramdef_t *def = NULL;
    sf_result_t r = sf_paramdef_read_xml_from_memory(
        &def, k_three_field_xml, strlen(k_three_field_xml), NULL);
    TEST_ASSERT_EQUAL(SF_OK, r);
    TEST_ASSERT_NOT_NULL(def);

    TEST_ASSERT_EQUAL_STRING("TEST_PARAM", sf_paramdef_get_param_type(def));
    TEST_ASSERT_EQUAL_INT16(1, sf_paramdef_get_data_version(def));
    TEST_ASSERT_FALSE(sf_paramdef_is_big_endian(def));
    TEST_ASSERT_TRUE(sf_paramdef_is_unicode(def));
    TEST_ASSERT_EQUAL_INT16(203, sf_paramdef_get_format_version(def));
    TEST_ASSERT_EQUAL_INT32(-1, sf_paramdef_get_index(def));
    TEST_ASSERT_EQUAL_size_t(3, sf_paramdef_get_field_count(def));

    const sf_paramdef_field_t *f0 = sf_paramdef_get_field(def, 0);
    TEST_ASSERT_NOT_NULL(f0);
    TEST_ASSERT_EQUAL_STRING("iconId", sf_paramdef_field_get_internal_name(f0));
    TEST_ASSERT_EQUAL_STRING("Icon", sf_paramdef_field_get_display_name(f0));
    TEST_ASSERT_EQUAL(SF_PARAMDEF_DEF_TYPE_S32,
                      sf_paramdef_field_get_display_type(f0));
    TEST_ASSERT_EQUAL_INT32(-1, sf_paramdef_field_get_default_value(f0).v.s32);
    TEST_ASSERT_EQUAL_INT32(-1, sf_paramdef_field_get_minimum(f0).v.s32);
    TEST_ASSERT_EQUAL_INT32(999999, sf_paramdef_field_get_maximum(f0).v.s32);
    TEST_ASSERT_EQUAL_INT32(1000, sf_paramdef_field_get_sort_id(f0));
    TEST_ASSERT_EQUAL_INT32(-1, sf_paramdef_field_get_bit_size(f0));

    const sf_paramdef_field_t *f1 = sf_paramdef_get_field(def, 1);
    TEST_ASSERT_NOT_NULL(f1);
    TEST_ASSERT_EQUAL_STRING("effectEndurance", sf_paramdef_field_get_internal_name(f1));
    TEST_ASSERT_EQUAL(SF_PARAMDEF_DEF_TYPE_F32, sf_paramdef_field_get_display_type(f1));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sf_paramdef_field_get_default_value(f1).v.f32);

    const sf_paramdef_field_t *f2 = sf_paramdef_get_field(def, 2);
    TEST_ASSERT_NOT_NULL(f2);
    TEST_ASSERT_EQUAL_STRING("flag", sf_paramdef_field_get_internal_name(f2));
    TEST_ASSERT_EQUAL(SF_PARAMDEF_DEF_TYPE_U8, sf_paramdef_field_get_display_type(f2));
    TEST_ASSERT_EQUAL_INT32(1, sf_paramdef_field_get_bit_size(f2));

    sf_paramdef_destroy(def);
}

static void test_xml_rejects_missing_param_type(void) {
    static const char *xml =
        "<PARAMDEF>"
        "  <DataVersion>1</DataVersion>"
        "  <BigEndian>False</BigEndian>"
        "  <Unicode>True</Unicode>"
        "  <FormatVersion>203</FormatVersion>"
        "  <Fields></Fields>"
        "</PARAMDEF>";
    sf_paramdef_t *def = NULL;
    sf_result_t r = sf_paramdef_read_xml_from_memory(&def, xml, strlen(xml), NULL);
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, r);
    TEST_ASSERT_NULL(def);
}

static void test_xml_rejects_missing_fields(void) {
    static const char *xml =
        "<PARAMDEF>"
        "  <ParamType>X</ParamType>"
        "</PARAMDEF>";
    sf_paramdef_t *def = NULL;
    sf_result_t r = sf_paramdef_read_xml_from_memory(&def, xml, strlen(xml), NULL);
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, r);
    TEST_ASSERT_NULL(def);
}

static void test_xml_rejects_malformed_xml(void) {
    static const char *xml = "<PARAMDEF><ParamType>X";
    sf_paramdef_t *def = NULL;
    sf_result_t r = sf_paramdef_read_xml_from_memory(&def, xml, strlen(xml), NULL);
    TEST_ASSERT_EQUAL(SF_ERR_INTERNAL, r);
    TEST_ASSERT_NULL(def);
}

static void test_xml_reads_paramdex_index_extension(void) {
    static const char *xml =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<PARAMDEF XmlVersion=\"2\">"
        "  <ParamType>SP_EFFECT_PARAM_ST</ParamType>"
        "  <Index>86</Index>"
        "  <DataVersion>4</DataVersion>"
        "  <BigEndian>False</BigEndian>"
        "  <Unicode>True</Unicode>"
        "  <FormatVersion>203</FormatVersion>"
        "  <Fields>"
        "    <Field Def=\"s32 iconId\"></Field>"
        "  </Fields>"
        "</PARAMDEF>";
    sf_paramdef_t *def = NULL;
    sf_result_t r = sf_paramdef_read_xml_from_memory(&def, xml, strlen(xml), NULL);
    TEST_ASSERT_EQUAL(SF_OK, r);
    TEST_ASSERT_NOT_NULL(def);
    TEST_ASSERT_EQUAL_INT32(86, sf_paramdef_get_index(def));
    TEST_ASSERT_EQUAL_STRING("SP_EFFECT_PARAM_ST", sf_paramdef_get_param_type(def));
    sf_paramdef_destroy(def);
}

static void test_xml_parses_array_and_unk06_alias(void) {
    static const char *xml =
        "<PARAMDEF>"
        "  <ParamType>ARRAY_TEST</ParamType>"
        "  <Unk06>7</Unk06>"
        "  <Version>104</Version>"
        "  <BigEndian>False</BigEndian>"
        "  <Unicode>False</Unicode>"
        "  <Fields>"
        "    <Field Def=\"dummy8 padding[16]\"></Field>"
        "    <Field Def=\"fixstrW name[32]\"></Field>"
        "  </Fields>"
        "</PARAMDEF>";
    sf_paramdef_t *def = NULL;
    sf_result_t r = sf_paramdef_read_xml_from_memory(&def, xml, strlen(xml), NULL);
    TEST_ASSERT_EQUAL(SF_OK, r);
    TEST_ASSERT_NOT_NULL(def);

    TEST_ASSERT_EQUAL_INT16(7, sf_paramdef_get_data_version(def));
    TEST_ASSERT_EQUAL_INT16(104, sf_paramdef_get_format_version(def));
    TEST_ASSERT_EQUAL_size_t(2, sf_paramdef_get_field_count(def));

    const sf_paramdef_field_t *f0 = sf_paramdef_get_field(def, 0);
    TEST_ASSERT_EQUAL(SF_PARAMDEF_DEF_TYPE_DUMMY8,
                      sf_paramdef_field_get_display_type(f0));
    TEST_ASSERT_EQUAL_STRING("padding", sf_paramdef_field_get_internal_name(f0));
    TEST_ASSERT_EQUAL_INT32(16, sf_paramdef_field_get_array_length(f0));
    TEST_ASSERT_EQUAL_INT32(16, sf_paramdef_field_get_byte_count(f0));

    const sf_paramdef_field_t *f1 = sf_paramdef_get_field(def, 1);
    TEST_ASSERT_EQUAL(SF_PARAMDEF_DEF_TYPE_FIXSTR_W,
                      sf_paramdef_field_get_display_type(f1));
    TEST_ASSERT_EQUAL_STRING("name", sf_paramdef_field_get_internal_name(f1));
    TEST_ASSERT_EQUAL_INT32(32, sf_paramdef_field_get_array_length(f1));
    TEST_ASSERT_EQUAL_INT32(64, sf_paramdef_field_get_byte_count(f1));

    sf_paramdef_destroy(def);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_xml_reads_three_field_happy_path);
    RUN_TEST(test_xml_rejects_missing_param_type);
    RUN_TEST(test_xml_rejects_missing_fields);
    RUN_TEST(test_xml_rejects_malformed_xml);
    RUN_TEST(test_xml_reads_paramdex_index_extension);
    RUN_TEST(test_xml_parses_array_and_unk06_alias);
    return UNITY_END();
}
