/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "er_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_param.h"
#include "souls_formats/sf_paramdef.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

#if defined(_WIN32)
#include <io.h>
#  if defined(_MSC_VER)
#    define access _access
#    define F_OK 0
#  endif
#else
#include <unistd.h>
#endif

void setUp(void) {}
void tearDown(void) {}

#ifndef SF_E2E_ITEM_LOT_PARAMDEF
#define SF_E2E_ITEM_LOT_PARAMDEF "../../external/paramdefs/ItemLotParam.xml"
#endif

static bool path_to_wide(const char *utf8, wchar_t *out, size_t out_len) {
    size_t converted = mbstowcs(out, utf8, out_len);
    return converted != (size_t)-1 && converted < out_len;
}

static sf_param_t *load_item_lot_map_or_skip(sf_paramdef_t **out_def) {
    *out_def = NULL;
    if (!er_helper_is_available()) {
        TEST_IGNORE_MESSAGE("ER game directory not available; skipping add-row e2e");
    }
    if (access(SF_E2E_ITEM_LOT_PARAMDEF, F_OK) != 0) {
        TEST_IGNORE_MESSAGE("ItemLotParam.xml fixture not available; skipping add-row e2e");
    }

    void *param_bytes = NULL;
    size_t param_size = 0;
    sf_result_t r = er_load_param("ItemLotParam_map", &param_bytes, &param_size, NULL);
    if (r != SF_OK || !param_bytes) {
        TEST_IGNORE_MESSAGE("ItemLotParam_map fixture not available; skipping add-row e2e");
    }

    sf_param_t *param = NULL;
    r = sf_param_read_from_memory(&param, (const uint8_t *)param_bytes, param_size, NULL);
    sf_free(NULL, param_bytes);
    if (r != SF_OK || !param) {
        TEST_IGNORE_MESSAGE("sf_param_read_from_memory failed for ItemLotParam_map");
    }

    wchar_t wpath[512];
    TEST_ASSERT_TRUE_MESSAGE(path_to_wide(SF_E2E_ITEM_LOT_PARAMDEF, wpath,
                                          sizeof(wpath) / sizeof(wpath[0])),
                             "mbstowcs failed for ItemLotParam.xml path");

    sf_paramdef_t *def = NULL;
    r = sf_paramdef_read_xml_from_path(&def, wpath, NULL);
    if (r != SF_OK || !def) {
        sf_param_destroy(param);
        TEST_IGNORE_MESSAGE("sf_paramdef_read_xml_from_path failed for ItemLotParam.xml");
    }

    r = sf_param_apply_paramdef(param, def, SF_PARAM_APPLY_SOMEWHAT_CAREFUL);
    if (r != SF_OK) {
        sf_paramdef_destroy(def);
        sf_param_destroy(param);
        TEST_IGNORE_MESSAGE("sf_param_apply_paramdef failed for ItemLotParam_map");
    }

    *out_def = def;
    return param;
}

static sf_result_t set_integer_cell(sf_param_cell_t *cell, int32_t value) {
    sf_param_cell_kind_t kind = sf_param_cell_get_value(cell).kind;
    switch (kind) {
    case SF_PARAM_CELL_KIND_S8: return sf_param_cell_set_s8(cell, (int8_t)value);
    case SF_PARAM_CELL_KIND_U8: return sf_param_cell_set_u8(cell, (uint8_t)value);
    case SF_PARAM_CELL_KIND_S16: return sf_param_cell_set_s16(cell, (int16_t)value);
    case SF_PARAM_CELL_KIND_U16: return sf_param_cell_set_u16(cell, (uint16_t)value);
    case SF_PARAM_CELL_KIND_S32: return sf_param_cell_set_s32(cell, value);
    case SF_PARAM_CELL_KIND_U32: return sf_param_cell_set_u32(cell, (uint32_t)value);
    default: return SF_ERR_INVALID_ARG;
    }
}

static int32_t get_integer_cell(const sf_param_cell_t *cell) {
    sf_param_cell_kind_t kind = sf_param_cell_get_value(cell).kind;
    switch (kind) {
    case SF_PARAM_CELL_KIND_S8: return sf_param_cell_get_s8(cell);
    case SF_PARAM_CELL_KIND_U8: return (int32_t)sf_param_cell_get_u8(cell);
    case SF_PARAM_CELL_KIND_S16: return sf_param_cell_get_s16(cell);
    case SF_PARAM_CELL_KIND_U16: return (int32_t)sf_param_cell_get_u16(cell);
    case SF_PARAM_CELL_KIND_S32: return sf_param_cell_get_s32(cell);
    case SF_PARAM_CELL_KIND_U32: return (int32_t)sf_param_cell_get_u32(cell);
    default: return INT32_MIN;
    }
}

static void assert_row_values(const sf_param_row_t *row) {
    const sf_param_cell_t *lot_item = sf_param_row_find_cell(row, "lotItemId01");
    TEST_ASSERT_NOT_NULL_MESSAGE(lot_item, "lotItemId01 cell not found");
    TEST_ASSERT_EQUAL_INT32(123, get_integer_cell(lot_item));

    const sf_param_cell_t *base_point = sf_param_row_find_cell(row, "lotItemBasePoint01");
    TEST_ASSERT_NOT_NULL_MESSAGE(base_point, "lotItemBasePoint01 cell not found");
    TEST_ASSERT_EQUAL_INT32(1000, get_integer_cell(base_point));
}

static void test_param_add_row_by_id_round_trips(void) {
    sf_paramdef_t *def = NULL;
    sf_param_t *param = load_item_lot_map_or_skip(&def);

    sf_param_row_t *row = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_param_add_row_by_id(param, 99999, "test", &row));
    TEST_ASSERT_NOT_NULL(row);
    TEST_ASSERT_EQUAL_INT32(99999, sf_param_row_get_id(row));
    TEST_ASSERT_EQUAL_STRING("test", sf_param_row_get_name(row));

    sf_param_cell_t *lot_item = sf_param_row_find_cell_mut(row, "lotItemId01");
    TEST_ASSERT_NOT_NULL_MESSAGE(lot_item, "lotItemId01 mutable cell not found");
    TEST_ASSERT_EQUAL_INT(SF_OK, set_integer_cell(lot_item, 123));

    sf_param_cell_t *base_point = sf_param_row_find_cell_mut(row, "lotItemBasePoint01");
    TEST_ASSERT_NOT_NULL_MESSAGE(base_point, "lotItemBasePoint01 mutable cell not found");
    TEST_ASSERT_EQUAL_INT(SF_OK, set_integer_cell(base_point, 1000));

    const sf_param_row_t *found = sf_param_find_row_by_id(param, 99999);
    TEST_ASSERT_EQUAL_PTR(row, found);
    assert_row_values(found);

    uint8_t *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_param_write_to_memory(param, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN_size_t(0, size);

    sf_param_t *round = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_param_read_from_memory(&round, bytes, size, NULL));
    sf_free(NULL, bytes);
    TEST_ASSERT_NOT_NULL(round);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_param_apply_paramdef(round, def, SF_PARAM_APPLY_SOMEWHAT_CAREFUL));

    const sf_param_row_t *round_row = sf_param_find_row_by_id(round, 99999);
    TEST_ASSERT_NOT_NULL(round_row);
    TEST_ASSERT_EQUAL_STRING("test", sf_param_row_get_name(round_row));
    assert_row_values(round_row);

    sf_param_destroy(round);
    sf_param_destroy(param);
    sf_paramdef_destroy(def);
}

/* Regression test: adding a row with name_optional=NULL must round-trip with an
   empty name, not garbage bytes from the strings section. */
static void test_param_add_row_null_name_round_trips(void) {
    sf_paramdef_t *def = NULL;
    sf_param_t *param = load_item_lot_map_or_skip(&def);

    sf_param_row_t *row = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_param_add_row_by_id(param, 88888, NULL, &row));
    TEST_ASSERT_NOT_NULL(row);
    TEST_ASSERT_EQUAL_INT32(88888, sf_param_row_get_id(row));
    TEST_ASSERT_EQUAL_STRING("", sf_param_row_get_name(row));

    uint8_t *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_param_write_to_memory(param, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN_size_t(0, size);

    sf_param_t *round = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_param_read_from_memory(&round, bytes, size, NULL));
    sf_free(NULL, bytes);
    TEST_ASSERT_NOT_NULL(round);

    const sf_param_row_t *round_row = sf_param_find_row_by_id(round, 88888);
    TEST_ASSERT_NOT_NULL(round_row);
    /* Name must be empty string, not garbage from the strings section */
    TEST_ASSERT_EQUAL_STRING("", sf_param_row_get_name(round_row));

    sf_param_destroy(round);
    sf_param_destroy(param);
    sf_paramdef_destroy(def);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_param_add_row_by_id_round_trips);
    RUN_TEST(test_param_add_row_null_name_round_trips);
    return UNITY_END();
}
