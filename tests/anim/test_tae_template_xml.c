/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * TAE Template XML parsing test.
 * Verifies sf_tae_template_read_from_memory with a synthetic XML string.
 */

#include "souls_formats/sf_tae_template.h"

#include "souls_formats/sf_tae.h"

#include "unity.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const char kTemplateXml[] =
    "<event_template game=\"SDT\">"
    "  <bank id=\"0\" name=\"TestBank\" basedon=\"-1\">"
    "    <event id=\"0\" name=\"SpawnEffect\">"
    "      <s32 name=\"EffectId\" assert=\"0\"/>"
    "      <f32 name=\"Delay\"/>"
    "      <aob name=\"Padding\" length=\"8\"/>"
    "    </event>"
    "    <event id=\"1\" name=\"PlaySound\">"
    "      <u32 name=\"SoundId\"/>"
    "      <s32 name=\"Volume\">"
    "        <entry name=\"Quiet\" value=\"0\"/>"
    "        <entry name=\"Normal\" value=\"1\"/>"
    "        <entry name=\"Loud\" value=\"2\"/>"
    "      </s32>"
    "    </event>"
    "  </bank>"
    "  <bank id=\"1\" name=\"InheritedBank\" basedon=\"0\">"
    "    <event id=\"2\" name=\"NewEvent\">"
    "      <f32 name=\"Duration\"/>"
    "    </event>"
    "  </bank>"
    "</event_template>";

static void test_template_parse_game(void) {
    sf_tae_template_t *tmpl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_tae_template_read_from_memory(&tmpl, kTemplateXml,
                                                            strlen(kTemplateXml), NULL));
    TEST_ASSERT_NOT_NULL(tmpl);
    TEST_ASSERT_EQUAL_INT(SF_TAE_FORMAT_SDT, sf_tae_template_game(tmpl));
    sf_tae_template_destroy(tmpl);
}

static void test_template_bank_count(void) {
    sf_tae_template_t *tmpl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_tae_template_read_from_memory(&tmpl, kTemplateXml,
                                                            strlen(kTemplateXml), NULL));
    TEST_ASSERT_NOT_NULL(tmpl);
    TEST_ASSERT_EQUAL_size_t(2u, sf_tae_template_bank_count(tmpl));
    sf_tae_template_destroy(tmpl);
}

static void test_template_find_bank(void) {
    sf_tae_template_t *tmpl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_tae_template_read_from_memory(&tmpl, kTemplateXml,
                                                            strlen(kTemplateXml), NULL));
    TEST_ASSERT_NOT_NULL(tmpl);
    const sf_tae_bank_template_t *bank0 = sf_tae_template_find_bank(tmpl, 0);
    TEST_ASSERT_NOT_NULL(bank0);
    TEST_ASSERT_EQUAL_INT64(0, sf_tae_bank_template_id(bank0));
    TEST_ASSERT_EQUAL_STRING("TestBank", sf_tae_bank_template_name(bank0));
    TEST_ASSERT_NULL(sf_tae_template_find_bank(tmpl, 99));
    sf_tae_template_destroy(tmpl);
}

static void test_template_event_count(void) {
    sf_tae_template_t *tmpl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_tae_template_read_from_memory(&tmpl, kTemplateXml,
                                                            strlen(kTemplateXml), NULL));
    TEST_ASSERT_NOT_NULL(tmpl);
    const sf_tae_bank_template_t *bank0 = sf_tae_template_find_bank(tmpl, 0);
    TEST_ASSERT_NOT_NULL(bank0);
    TEST_ASSERT_EQUAL_size_t(2u, sf_tae_bank_template_event_count(bank0));
    sf_tae_template_destroy(tmpl);
}

static void test_template_event_params(void) {
    sf_tae_template_t *tmpl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_tae_template_read_from_memory(&tmpl, kTemplateXml,
                                                            strlen(kTemplateXml), NULL));
    TEST_ASSERT_NOT_NULL(tmpl);
    const sf_tae_bank_template_t  *bank0 = sf_tae_template_find_bank(tmpl, 0);
    const sf_tae_event_template_t *ev0   = sf_tae_bank_template_find_event(bank0, 0);
    TEST_ASSERT_NOT_NULL(ev0);
    TEST_ASSERT_EQUAL_STRING("SpawnEffect", sf_tae_event_template_name(ev0));
    TEST_ASSERT_EQUAL_size_t(3u, sf_tae_event_template_param_count(ev0));
    TEST_ASSERT_EQUAL_INT32(16, sf_tae_event_template_total_byte_count(ev0));

    const sf_tae_param_template_t *p0 = sf_tae_event_template_param(ev0, 0);
    TEST_ASSERT_NOT_NULL(p0);
    TEST_ASSERT_EQUAL_INT(SF_TAE_PARAM_TYPE_S32, sf_tae_param_template_type(p0));
    TEST_ASSERT_EQUAL_STRING("EffectId", sf_tae_param_template_name(p0));
    TEST_ASSERT_TRUE(sf_tae_param_template_has_assert(p0));
    TEST_ASSERT_EQUAL_INT32(4, sf_tae_param_template_byte_count(p0));

    const sf_tae_param_template_t *p2 = sf_tae_event_template_param(ev0, 2);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_EQUAL_INT(SF_TAE_PARAM_TYPE_AOB, sf_tae_param_template_type(p2));
    TEST_ASSERT_EQUAL_INT32(8, sf_tae_param_template_aob_length(p2));
    TEST_ASSERT_EQUAL_INT32(8, sf_tae_param_template_byte_count(p2));

    sf_tae_template_destroy(tmpl);
}

static void test_template_enum_entries(void) {
    sf_tae_template_t *tmpl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_tae_template_read_from_memory(&tmpl, kTemplateXml,
                                                            strlen(kTemplateXml), NULL));
    TEST_ASSERT_NOT_NULL(tmpl);
    const sf_tae_bank_template_t  *bank0 = sf_tae_template_find_bank(tmpl, 0);
    const sf_tae_event_template_t *ev1   = sf_tae_bank_template_find_event(bank0, 1);
    TEST_ASSERT_NOT_NULL(ev1);
    const sf_tae_param_template_t *volume = sf_tae_event_template_find_param(ev1, "Volume");
    TEST_ASSERT_NOT_NULL(volume);
    TEST_ASSERT_EQUAL_size_t(3u, sf_tae_param_template_enum_count(volume));
    TEST_ASSERT_EQUAL_STRING("Quiet", sf_tae_param_template_enum_name(volume, 0));
    TEST_ASSERT_EQUAL_INT64(0, sf_tae_param_template_enum_value(volume, 0));
    TEST_ASSERT_EQUAL_STRING("Normal", sf_tae_param_template_enum_name(volume, 1));
    TEST_ASSERT_EQUAL_INT64(1, sf_tae_param_template_enum_value(volume, 1));
    sf_tae_template_destroy(tmpl);
}

static void test_template_basedon_inheritance(void) {
    sf_tae_template_t *tmpl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_tae_template_read_from_memory(&tmpl, kTemplateXml,
                                                            strlen(kTemplateXml), NULL));
    TEST_ASSERT_NOT_NULL(tmpl);
    const sf_tae_bank_template_t *bank1 = sf_tae_template_find_bank(tmpl, 1);
    TEST_ASSERT_NOT_NULL(bank1);
    TEST_ASSERT_EQUAL_size_t(3u, sf_tae_bank_template_event_count(bank1));
    TEST_ASSERT_NOT_NULL(sf_tae_bank_template_find_event(bank1, 0));
    TEST_ASSERT_NOT_NULL(sf_tae_bank_template_find_event(bank1, 1));
    TEST_ASSERT_NOT_NULL(sf_tae_bank_template_find_event(bank1, 2));
    sf_tae_template_destroy(tmpl);
}

static void test_template_null_input(void) {
    sf_tae_template_t *tmpl = NULL;
    TEST_ASSERT_NOT_EQUAL(SF_OK, sf_tae_template_read_from_memory(NULL, "", 0, NULL));
    TEST_ASSERT_NOT_EQUAL(SF_OK, sf_tae_template_read_from_memory(&tmpl, "<bad_xml>", 9, NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_template_parse_game);
    RUN_TEST(test_template_bank_count);
    RUN_TEST(test_template_find_bank);
    RUN_TEST(test_template_event_count);
    RUN_TEST(test_template_event_params);
    RUN_TEST(test_template_enum_entries);
    RUN_TEST(test_template_basedon_inheritance);
    RUN_TEST(test_template_null_input);
    return UNITY_END();
}
