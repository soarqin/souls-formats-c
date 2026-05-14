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

#define TEST_ASSERT_OK(expr) do { \
    sf_result_t r__ = (expr); \
    if (r__ != SF_OK) TEST_FAIL_MESSAGE(sf_last_error_detail() ? sf_last_error_detail() : sf_result_str(r__)); \
} while (0)

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

static void put_u64(uint8_t *p, size_t off, uint64_t v, bool big_endian) {
    if (big_endian) {
        for (size_t i = 0; i < 8; i++) p[off + i] = (uint8_t)(v >> ((7 - i) * 8));
    } else {
        for (size_t i = 0; i < 8; i++) p[off + i] = (uint8_t)(v >> (i * 8));
    }
}

static size_t type_size(sf_paramdef_def_type_t type, int32_t array_length) {
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S64:
    case SF_PARAMDEF_DEF_TYPE_U64:
    case SF_PARAMDEF_DEF_TYPE_F64:
        return 8;
    case SF_PARAMDEF_DEF_TYPE_F32:
    case SF_PARAMDEF_DEF_TYPE_B32:
        return 4;
    case SF_PARAMDEF_DEF_TYPE_U8:
        return 1;
    case SF_PARAMDEF_DEF_TYPE_FIXSTR:
        return (size_t)array_length;
    default:
        return 0;
    }
}

static sf_paramdef_field_t make_field(sf_paramdef_def_type_t type, const char *name,
                                      int32_t array_length) {
    sf_paramdef_field_t field;
    memset(&field, 0, sizeof(field));
    field.display_name = (char *)name;
    field.internal_type = (char *)"";
    field.internal_name = (char *)name;
    field.description = (char *)"";
    field.display_format = (char *)"";
    field.display_type = type;
    field.bit_size = -1;
    field.array_length = array_length;
    field.byte_count = (int32_t)type_size(type, array_length);
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
    def.row_size = 38;
    return def;
}

static param_fixture_t make_param_fixture(bool big_endian) {
    param_fixture_t fx;
    memset(&fx, 0, sizeof(fx));
    const size_t rows_start = 0x30;
    const size_t data_start = rows_start + 12;
    const size_t row_size = 38;
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

    size_t off = data_start;
    put_u64(fx.data, off, (uint64_t)-1, big_endian); off += 8;
    put_u64(fx.data, off, 2, big_endian); off += 8;
    { float f = 3.0f; uint32_t raw = 0; memcpy(&raw, &f, sizeof(raw)); put_u32(fx.data, off, raw, big_endian); off += 4; }
    { double d = 4.0; uint64_t raw = 0; memcpy(&raw, &d, sizeof(raw)); put_u64(fx.data, off, raw, big_endian); off += 8; }
    put_u32(fx.data, off, 0, big_endian); off += 4;
    fx.data[off++] = 0x11u;
    memcpy(&fx.data[off], "old", 3);

    memcpy(&fx.data[strings_offset], "row0", sizeof("row0"));
    fx.size = strings_offset + sizeof("row0");
    return fx;
}

static sf_param_t *read_applied_param(bool big_endian, const sf_paramdef_t *def) {
    param_fixture_t fx = make_param_fixture(big_endian);
    sf_param_t *param = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_read_from_memory(&param, fx.data, fx.size, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_apply_paramdef(param, def, SF_PARAM_APPLY_CAREFUL));
    return param;
}

static sf_paramdef_t make_ext_def(sf_paramdef_field_t fields[7]) {
    fields[0] = make_field(SF_PARAMDEF_DEF_TYPE_S64, "s64", 1);
    fields[1] = make_field(SF_PARAMDEF_DEF_TYPE_U64, "u64", 1);
    fields[2] = make_field(SF_PARAMDEF_DEF_TYPE_F32, "f32", 1);
    fields[3] = make_field(SF_PARAMDEF_DEF_TYPE_F64, "f64", 1);
    fields[4] = make_field(SF_PARAMDEF_DEF_TYPE_B32, "bool", 1);
    fields[5] = make_field(SF_PARAMDEF_DEF_TYPE_U8, "byte", 1);
    fields[6] = make_field(SF_PARAMDEF_DEF_TYPE_FIXSTR, "fixstr", 5);
    return make_def(fields, 7);
}

static void assert_success_values(const sf_param_row_t *row) {
    sf_param_cell_value_t value = sf_param_cell_get_value(sf_param_row_find_cell(row, "s64"));
    TEST_ASSERT_EQUAL_INT64(-0x1020304050607LL, value.v.s64);
    value = sf_param_cell_get_value(sf_param_row_find_cell(row, "u64"));
    TEST_ASSERT_EQUAL_HEX64(0x8877665544332211ULL, value.v.u64);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 12.5f, sf_param_cell_get_f32(sf_param_row_find_cell(row, "f32")));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 25.25, sf_param_cell_get_f64(sf_param_row_find_cell(row, "f64")));
    TEST_ASSERT_TRUE(sf_param_cell_get_bool(sf_param_row_find_cell(row, "bool")));
    TEST_ASSERT_EQUAL_UINT8(0xABu, sf_param_cell_get_u8(sf_param_row_find_cell(row, "byte")));
    TEST_ASSERT_EQUAL_STRING("abc", sf_param_cell_get_string(sf_param_row_find_cell(row, "fixstr")));
}

static void test_extended_setters_update_cells_and_roundtrip(void) {
    sf_paramdef_field_t fields[7];
    sf_paramdef_t def = make_ext_def(fields);
    sf_param_t *param = read_applied_param(false, &def);
    sf_param_row_t *row = sf_param_get_row_mut(param, 0);

    TEST_ASSERT_OK(sf_param_cell_set_s64(sf_param_row_find_cell_mut(row, "s64"), -0x1020304050607LL));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_set_u64(sf_param_row_find_cell_mut(row, "u64"), 0x8877665544332211ULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_set_f32(sf_param_row_find_cell_mut(row, "f32"), 12.5f));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_set_f64(sf_param_row_find_cell_mut(row, "f64"), 25.25));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_set_bool(sf_param_row_find_cell_mut(row, "bool"), true));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_set_byte(sf_param_row_find_cell_mut(row, "byte"), 0xABu));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_set_fixstr(sf_param_row_find_cell_mut(row, "fixstr"), "abc", 3));
    assert_success_values(sf_param_get_row(param, 0));

    uint8_t *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_write_to_memory(param, &written, &written_size, NULL));
    sf_param_t *roundtrip = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_read_from_memory(&roundtrip, written, written_size, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_apply_paramdef(roundtrip, &def, SF_PARAM_APPLY_CAREFUL));
    assert_success_values(sf_param_get_row(roundtrip, 0));

    sf_param_destroy(roundtrip);
    sf_free(NULL, written);
    sf_param_destroy(param);
}

static void test_big_endian_extended_setters_roundtrip(void) {
    sf_paramdef_field_t fields[7];
    sf_paramdef_t def = make_ext_def(fields);
    sf_param_t *param = read_applied_param(true, &def);
    sf_param_row_t *row = sf_param_get_row_mut(param, 0);
    TEST_ASSERT_OK(sf_param_cell_set_u64(sf_param_row_find_cell_mut(row, "u64"), 0x0102030405060708ULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_set_f64(sf_param_row_find_cell_mut(row, "f64"), 123.5));

    uint8_t *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_write_to_memory(param, &written, &written_size, NULL));
    sf_param_t *roundtrip = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_read_from_memory(&roundtrip, written, written_size, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_apply_paramdef(roundtrip, &def, SF_PARAM_APPLY_CAREFUL));
    const sf_param_row_t *rt_row = sf_param_get_row(roundtrip, 0);
    sf_param_cell_value_t value = sf_param_cell_get_value(sf_param_row_find_cell(rt_row, "u64"));
    TEST_ASSERT_EQUAL_HEX64(0x0102030405060708ULL, value.v.u64);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 123.5, sf_param_cell_get_f64(sf_param_row_find_cell(rt_row, "f64")));

    sf_param_destroy(roundtrip);
    sf_free(NULL, written);
    sf_param_destroy(param);
}

static void test_fixstr_overflow_reports_capacity_detail(void) {
    sf_paramdef_field_t fields[7];
    sf_paramdef_t def = make_ext_def(fields);
    sf_param_t *param = read_applied_param(false, &def);
    sf_param_row_t *row = sf_param_get_row_mut(param, 0);

    TEST_ASSERT_EQUAL(SF_ERR_OUT_OF_RANGE,
                      sf_param_cell_set_fixstr(sf_param_row_find_cell_mut(row, "fixstr"), "abcdef", 6));
    TEST_ASSERT_EQUAL_STRING("value length 6 > cell capacity 5", sf_last_error_detail());
    sf_param_destroy(param);
}

static void test_type_mismatch_reports_cell_kind_detail(void) {
    sf_paramdef_field_t fields[7];
    sf_paramdef_t def = make_ext_def(fields);
    sf_param_t *param = read_applied_param(false, &def);
    sf_param_row_t *row = sf_param_get_row_mut(param, 0);

    TEST_ASSERT_EQUAL(SF_ERR_INVALID_ARG, sf_param_cell_set_f64(sf_param_row_find_cell_mut(row, "u64"), 1.0));
    const char *detail = sf_last_error_detail();
    TEST_ASSERT_NOT_NULL(detail);
    TEST_ASSERT_NOT_NULL(strstr(detail, "cell kind u64"));
    TEST_ASSERT_NOT_NULL(strstr(detail, "does not accept f64"));
    sf_param_destroy(param);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_extended_setters_update_cells_and_roundtrip);
    RUN_TEST(test_big_endian_extended_setters_roundtrip);
    RUN_TEST(test_fixstr_overflow_reports_capacity_detail);
    RUN_TEST(test_type_mismatch_reports_cell_kind_detail);
    return UNITY_END();
}
