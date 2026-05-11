/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * PARAMDEF XML e2e against real Paramdex data.
 */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_paramdef.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

#if defined(_WIN32)
#include <io.h>
#endif

void setUp(void) {}
void tearDown(void) {}

static const char *k_paramdex_sp_effect_xml =
    SOULS_FORMATS_ROOT_DIR "/../../dev/paramdex/ER/Defs/SpEffect.xml";

static const sf_paramdef_field_t *find_field_by_internal_name(const sf_paramdef_t *def,
                                                              const char *name)
{
    const size_t count = sf_paramdef_get_field_count(def);
    for (size_t i = 0; i < count; ++i) {
        const sf_paramdef_field_t *field = sf_paramdef_get_field(def, i);
        if (field != NULL && strcmp(sf_paramdef_field_get_internal_name(field), name) == 0) {
            return field;
        }
    }
    return NULL;
}

static void test_speffect_xml_e2e(void)
{
    if (access(k_paramdex_sp_effect_xml, F_OK) != 0) {
        TEST_IGNORE_MESSAGE("paramdex SpEffect.xml not found");
    }

    wchar_t wpath[512];
    const size_t converted = mbstowcs(wpath, k_paramdex_sp_effect_xml,
                                      sizeof(wpath) / sizeof(wpath[0]));
    TEST_ASSERT_NOT_EQUAL((size_t)-1, converted);
    TEST_ASSERT_TRUE(converted < (sizeof(wpath) / sizeof(wpath[0])));

    sf_paramdef_t *def = NULL;
    sf_result_t r = sf_paramdef_read_xml_from_path(&def, wpath, NULL);
    TEST_ASSERT_EQUAL(SF_OK, r);
    TEST_ASSERT_NOT_NULL(def);

    TEST_ASSERT_EQUAL_STRING("SP_EFFECT_PARAM_ST", sf_paramdef_get_param_type(def));
    TEST_ASSERT_EQUAL_INT16(4, sf_paramdef_get_data_version(def));
    TEST_ASSERT_TRUE(sf_paramdef_is_unicode(def));
    TEST_ASSERT_FALSE(sf_paramdef_is_big_endian(def));
    TEST_ASSERT_EQUAL_INT16(203, sf_paramdef_get_format_version(def));
    TEST_ASSERT_EQUAL_INT32(86, sf_paramdef_get_index(def));
    TEST_ASSERT_GREATER_THAN(100, (int)sf_paramdef_get_field_count(def));

    const sf_paramdef_field_t *icon_id = find_field_by_internal_name(def, "iconId");
    TEST_ASSERT_NOT_NULL(icon_id);
    TEST_ASSERT_EQUAL(SF_PARAMDEF_DEF_TYPE_S32,
                      sf_paramdef_field_get_display_type(icon_id));
    TEST_ASSERT_EQUAL_INT32(-1, sf_paramdef_field_get_default_value(icon_id).v.s32);
    TEST_ASSERT_EQUAL_INT32(-1, sf_paramdef_field_get_minimum(icon_id).v.s32);
    TEST_ASSERT_EQUAL_INT32(999999, sf_paramdef_field_get_maximum(icon_id).v.s32);
    TEST_ASSERT_EQUAL_INT32(1000, sf_paramdef_field_get_sort_id(icon_id));

    sf_paramdef_destroy(def);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_speffect_xml_e2e);
    return UNITY_END();
}
