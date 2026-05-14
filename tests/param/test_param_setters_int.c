/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_param.h"
#include "souls_formats/sf_paramdef.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct sf_paramdef_field {
    char *display_name;
    char *internal_type;
    char *internal_name;
    char *description;
    char *display_format;
    sf_paramdef_def_type_t display_type;
    sf_paramdef_default_value_t default_value;
    sf_paramdef_default_value_t minimum;
    sf_paramdef_default_value_t maximum;
    sf_paramdef_default_value_t increment;
    sf_paramdef_edit_flags_t edit_flags;
    int32_t byte_count;
    int32_t bit_size;
    int32_t array_length;
    int32_t sort_id;
    uint64_t first_regulation_version;
    uint64_t removed_regulation_version;
};

struct sf_paramdef {
    const sf_allocator_t *alloc;
    sf_paramdef_field_t *fields;
    size_t field_count;
    char *param_type;
    int16_t data_version;
    int16_t format_version;
    int32_t row_size;
    int32_t index;
    bool big_endian;
    bool unicode;
    bool version_aware;
    bool basic_fields;
    void *layout_cache;
};

typedef struct param_fixture {
    uint8_t data[512];
    size_t size;
} param_fixture_t;

void setUp(void) {}
void tearDown(void) {}

static void put_u16(uint8_t *p, size_t off, uint16_t v, bool big_endian) {
    if (big_endian) {
        p[off + 0] = (uint8_t)(v >> 8);
        p[off + 1] = (uint8_t)v;
    } else {
        p[off + 0] = (uint8_t)v;
        p[off + 1] = (uint8_t)(v >> 8);
    }
}

static void put_u32(uint8_t *p, size_t off, uint32_t v, bool big_endian) {
    if (big_endian) {
        p[off + 0] = (uint8_t)(v >> 24);
        p[off + 1] = (uint8_t)(v >> 16);
        p[off + 2] = (uint8_t)(v >> 8);
        p[off + 3] = (uint8_t)v;
    } else {
        p[off + 0] = (uint8_t)v;
        p[off + 1] = (uint8_t)(v >> 8);
        p[off + 2] = (uint8_t)(v >> 16);
        p[off + 3] = (uint8_t)(v >> 24);
    }
}

static size_t type_size(sf_paramdef_def_type_t type) {
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:
    case SF_PARAMDEF_DEF_TYPE_U8:
        return 1;
    case SF_PARAMDEF_DEF_TYPE_S16:
    case SF_PARAMDEF_DEF_TYPE_U16:
        return 2;
    case SF_PARAMDEF_DEF_TYPE_S32:
    case SF_PARAMDEF_DEF_TYPE_U32:
        return 4;
    default:
        return 0;
    }
}

static sf_paramdef_field_t make_field(sf_paramdef_def_type_t type, const char *name) {
    sf_paramdef_field_t field;
    memset(&field, 0, sizeof(field));
    field.display_name = (char *)name;
    field.internal_type = (char *)"";
    field.internal_name = (char *)name;
    field.description = (char *)"";
    field.display_format = (char *)"";
    field.display_type = type;
    field.bit_size = -1;
    field.array_length = 1;
    field.byte_count = (int32_t)type_size(type);
    return field;
}

static sf_paramdef_t make_def(sf_paramdef_field_t *fields, size_t field_count) {
    sf_paramdef_t def;
    memset(&def, 0, sizeof(def));
    def.fields = fields;
    def.field_count = field_count;
    def.param_type = (char *)"TEST_PARAM";
    def.data_version = 123;
    def.format_version = 201;
    def.row_size = 14;
    return def;
}

static param_fixture_t make_param_fixture(bool big_endian, const uint8_t *row_data,
                                          size_t row_size) {
    param_fixture_t fx;
    memset(&fx, 0, sizeof(fx));
    const size_t rows_start = 0x30;
    const size_t data_start = rows_start + 12;
    const size_t strings_offset = data_start + row_size;

    put_u32(fx.data, 0x00, (uint32_t)strings_offset, big_endian);
    put_u16(fx.data, 0x04, (uint16_t)data_start, big_endian);
    put_u16(fx.data, 0x06, 0, big_endian);
    put_u16(fx.data, 0x08, 123, big_endian);
    put_u16(fx.data, 0x0A, 1, big_endian);
    memcpy(&fx.data[0x0C], "TEST_PARAM", sizeof("TEST_PARAM") - 1);
    fx.data[0x2C] = big_endian ? 0xFFu : 0x00u;
    fx.data[0x2D] = SF_PARAM_FORMAT_FLAGS1_NONE;
    fx.data[0x2E] = SF_PARAM_FORMAT_FLAGS2_NONE;
    fx.data[0x2F] = 0x6A;
    put_u32(fx.data, rows_start + 0, 1000, big_endian);
    put_u16(fx.data, rows_start + 4, (uint16_t)data_start, big_endian);
    put_u16(fx.data, rows_start + 6, 0, big_endian);
    put_u32(fx.data, rows_start + 8, (uint32_t)strings_offset, big_endian);
    memcpy(&fx.data[data_start], row_data, row_size);
    memcpy(&fx.data[strings_offset], "row0", sizeof("row0"));
    fx.size = strings_offset + sizeof("row0");
    return fx;
}

static sf_param_t *read_applied_param(bool big_endian, const sf_paramdef_t *def) {
    uint8_t row[14];
    size_t off = 0;
    row[off++] = 0xFEu;
    row[off++] = 2u;
    put_u16(row, off, (uint16_t)-300, big_endian); off += 2;
    put_u16(row, off, 400u, big_endian); off += 2;
    put_u32(row, off, (uint32_t)-50000, big_endian); off += 4;
    put_u32(row, off, 60000u, big_endian);

    param_fixture_t fx = make_param_fixture(big_endian, row, sizeof(row));
    sf_param_t *param = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_read_from_memory(&param, fx.data, fx.size, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_apply_paramdef(param, def, SF_PARAM_APPLY_CAREFUL));
    return param;
}

static void test_integer_setters_update_cells_and_roundtrip(void) {
    sf_paramdef_field_t fields[] = {
        make_field(SF_PARAMDEF_DEF_TYPE_S8, "s8"),
        make_field(SF_PARAMDEF_DEF_TYPE_U8, "u8"),
        make_field(SF_PARAMDEF_DEF_TYPE_S16, "s16"),
        make_field(SF_PARAMDEF_DEF_TYPE_U16, "u16"),
        make_field(SF_PARAMDEF_DEF_TYPE_S32, "s32"),
        make_field(SF_PARAMDEF_DEF_TYPE_U32, "u32"),
    };
    sf_paramdef_t def = make_def(fields, sizeof(fields) / sizeof(fields[0]));
    sf_param_t *param = read_applied_param(false, &def);
    sf_param_row_t *row = sf_param_find_row_by_id_mut(param, 1000);
    TEST_ASSERT_NOT_NULL(row);

    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_set_s8(sf_param_row_find_cell_mut(row, "s8"), -11));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_set_u8(sf_param_row_find_cell_mut(row, "u8"), 22));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_set_s16(sf_param_row_find_cell_mut(row, "s16"), -333));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_set_u16(sf_param_row_find_cell_mut(row, "u16"), 444));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_set_s32(sf_param_row_find_cell_mut(row, "s32"), -55555));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_set_u32(sf_param_row_find_cell_mut(row, "u32"), 66666));

    const sf_param_row_t *const_row = sf_param_get_row(param, 0);
    TEST_ASSERT_EQUAL_INT8(-11, sf_param_cell_get_s8(sf_param_row_find_cell(const_row, "s8")));
    TEST_ASSERT_EQUAL_UINT8(22, sf_param_cell_get_u8(sf_param_row_find_cell(const_row, "u8")));
    TEST_ASSERT_EQUAL_INT16(-333, sf_param_cell_get_s16(sf_param_row_find_cell(const_row, "s16")));
    TEST_ASSERT_EQUAL_UINT16(444, sf_param_cell_get_u16(sf_param_row_find_cell(const_row, "u16")));
    TEST_ASSERT_EQUAL_INT32(-55555, sf_param_cell_get_s32(sf_param_row_find_cell(const_row, "s32")));
    TEST_ASSERT_EQUAL_UINT32(66666, sf_param_cell_get_u32(sf_param_row_find_cell(const_row, "u32")));

    uint8_t *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_write_to_memory(param, &written, &written_size, NULL));
    sf_param_t *roundtrip = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_read_from_memory(&roundtrip, written, written_size, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_apply_paramdef(roundtrip, &def, SF_PARAM_APPLY_CAREFUL));
    const sf_param_row_t *rt_row = sf_param_get_row(roundtrip, 0);
    TEST_ASSERT_EQUAL_INT32(-55555, sf_param_cell_get_s32(sf_param_row_find_cell(rt_row, "s32")));
    TEST_ASSERT_EQUAL_UINT32(66666, sf_param_cell_get_u32(sf_param_row_find_cell(rt_row, "u32")));

    sf_param_destroy(roundtrip);
    sf_free(NULL, written);
    sf_param_destroy(param);
}

static void test_big_endian_setter_roundtrip(void) {
    sf_paramdef_field_t fields[] = {
        make_field(SF_PARAMDEF_DEF_TYPE_S8, "s8"),
        make_field(SF_PARAMDEF_DEF_TYPE_U8, "u8"),
        make_field(SF_PARAMDEF_DEF_TYPE_S16, "s16"),
        make_field(SF_PARAMDEF_DEF_TYPE_U16, "u16"),
        make_field(SF_PARAMDEF_DEF_TYPE_S32, "s32"),
        make_field(SF_PARAMDEF_DEF_TYPE_U32, "u32"),
    };
    sf_paramdef_t def = make_def(fields, sizeof(fields) / sizeof(fields[0]));
    sf_param_t *param = read_applied_param(true, &def);
    sf_param_row_t *row = sf_param_get_row_mut(param, 0);
    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_set_u32(sf_param_row_get_cell_mut(row, 5), 0xAABBCCDDu));

    uint8_t *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_write_to_memory(param, &written, &written_size, NULL));
    sf_param_t *roundtrip = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_read_from_memory(&roundtrip, written, written_size, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_apply_paramdef(roundtrip, &def, SF_PARAM_APPLY_CAREFUL));
    TEST_ASSERT_EQUAL_HEX32(0xAABBCCDDu,
                            sf_param_cell_get_u32(sf_param_row_get_cell(sf_param_get_row(roundtrip, 0), 5)));
    sf_param_destroy(roundtrip);
    sf_free(NULL, written);
    sf_param_destroy(param);
}

static void test_type_mismatch_reports_cell_kind_detail(void) {
    sf_paramdef_field_t fields[] = {
        make_field(SF_PARAMDEF_DEF_TYPE_S8, "s8"),
        make_field(SF_PARAMDEF_DEF_TYPE_U8, "u8"),
        make_field(SF_PARAMDEF_DEF_TYPE_S16, "s16"),
        make_field(SF_PARAMDEF_DEF_TYPE_U16, "u16"),
        make_field(SF_PARAMDEF_DEF_TYPE_S32, "s32"),
        make_field(SF_PARAMDEF_DEF_TYPE_U32, "u32"),
    };
    sf_paramdef_t def = make_def(fields, sizeof(fields) / sizeof(fields[0]));
    sf_param_t *param = read_applied_param(false, &def);
    sf_param_row_t *row = sf_param_get_row_mut(param, 0);
    sf_param_cell_t *cell = sf_param_row_find_cell_mut(row, "u32");

    TEST_ASSERT_EQUAL(SF_ERR_INVALID_ARG, sf_param_cell_set_s32(cell, -1));
    const char *detail = sf_last_error_detail();
    TEST_ASSERT_NOT_NULL(detail);
    TEST_ASSERT_NOT_NULL(strstr(detail, "cell kind"));
    TEST_ASSERT_NOT_NULL(strstr(detail, "does not accept s32"));
    sf_param_destroy(param);
}

static void test_null_inputs_return_invalid_arg(void) {
    TEST_ASSERT_NULL(sf_param_get_row_mut(NULL, 0));
    TEST_ASSERT_NULL(sf_param_find_row_by_id_mut(NULL, 1));
    TEST_ASSERT_NULL(sf_param_row_get_cell_mut(NULL, 0));
    TEST_ASSERT_NULL(sf_param_row_find_cell_mut(NULL, "x"));
    TEST_ASSERT_EQUAL(SF_ERR_INVALID_ARG, sf_param_cell_set_s8(NULL, 1));
    TEST_ASSERT_EQUAL(SF_ERR_INVALID_ARG, sf_param_cell_set_u8(NULL, 1));
    TEST_ASSERT_EQUAL(SF_ERR_INVALID_ARG, sf_param_cell_set_s16(NULL, 1));
    TEST_ASSERT_EQUAL(SF_ERR_INVALID_ARG, sf_param_cell_set_u16(NULL, 1));
    TEST_ASSERT_EQUAL(SF_ERR_INVALID_ARG, sf_param_cell_set_s32(NULL, 1));
    TEST_ASSERT_EQUAL(SF_ERR_INVALID_ARG, sf_param_cell_set_u32(NULL, 1));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_integer_setters_update_cells_and_roundtrip);
    RUN_TEST(test_big_endian_setter_roundtrip);
    RUN_TEST(test_type_mismatch_reports_cell_kind_detail);
    RUN_TEST(test_null_inputs_return_invalid_arg);
    return UNITY_END();
}
