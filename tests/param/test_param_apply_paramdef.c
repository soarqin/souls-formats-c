/* SPDX-License-Identifier: GPL-3.0-or-later */

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

static void put_u64(uint8_t *p, size_t off, uint64_t v, bool big_endian) {
    if (big_endian) {
        for (size_t i = 0; i < 8; i++) p[off + i] = (uint8_t)(v >> ((7 - i) * 8));
    } else {
        for (size_t i = 0; i < 8; i++) p[off + i] = (uint8_t)(v >> (i * 8));
    }
}

static size_t type_size(sf_paramdef_def_type_t type) {
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:
    case SF_PARAMDEF_DEF_TYPE_U8:
    case SF_PARAMDEF_DEF_TYPE_DUMMY8:
    case SF_PARAMDEF_DEF_TYPE_FIXSTR:
        return 1;
    case SF_PARAMDEF_DEF_TYPE_S16:
    case SF_PARAMDEF_DEF_TYPE_U16:
    case SF_PARAMDEF_DEF_TYPE_FIXSTR_W:
        return 2;
    case SF_PARAMDEF_DEF_TYPE_S32:
    case SF_PARAMDEF_DEF_TYPE_U32:
    case SF_PARAMDEF_DEF_TYPE_B32:
    case SF_PARAMDEF_DEF_TYPE_F32:
    case SF_PARAMDEF_DEF_TYPE_ANGLE32:
        return 4;
    case SF_PARAMDEF_DEF_TYPE_F64:
        return 8;
    default:
        return 0;
    }
}

static sf_paramdef_field_t make_field(sf_paramdef_def_type_t type, const char *name,
                                      int32_t bit_size, int32_t array_length) {
    sf_paramdef_field_t field;
    memset(&field, 0, sizeof(field));
    field.display_name = (char *)name;
    field.internal_type = (char *)"";
    field.internal_name = (char *)name;
    field.description = (char *)"";
    field.display_format = (char *)"";
    field.display_type = type;
    field.bit_size = bit_size;
    field.array_length = array_length > 0 ? array_length : 1;
    field.byte_count = (int32_t)(type_size(type) * (size_t)field.array_length);
    return field;
}

static sf_paramdef_t make_def(const char *param_type, int16_t data_version,
                              int32_t row_size, bool big_endian,
                              sf_paramdef_field_t *fields, size_t field_count) {
    sf_paramdef_t def;
    memset(&def, 0, sizeof(def));
    def.fields = fields;
    def.field_count = field_count;
    def.param_type = (char *)param_type;
    def.data_version = data_version;
    def.format_version = 201;
    def.row_size = row_size;
    def.big_endian = big_endian;
    return def;
}

static param_fixture_t make_param_fixture(const char *param_type, int16_t data_version,
                                          bool big_endian, const uint8_t *row_data,
                                          size_t row_size) {
    param_fixture_t fx;
    memset(&fx, 0, sizeof(fx));

    const size_t rows_start = 0x30;
    const size_t data_start = rows_start + 12;
    const size_t strings_offset = data_start + row_size;
    TEST_ASSERT_LESS_THAN_size_t(sizeof(fx.data), strings_offset + strlen("row0") + 1);

    put_u32(fx.data, 0x00, (uint32_t)strings_offset, big_endian);
    put_u16(fx.data, 0x04, (uint16_t)data_start, big_endian);
    put_u16(fx.data, 0x06, 0, big_endian);
    put_u16(fx.data, 0x08, (uint16_t)data_version, big_endian);
    put_u16(fx.data, 0x0A, 1, big_endian);
    if (param_type && param_type[0] != '\0') {
        size_t len = strlen(param_type);
        if (len > 0x20) len = 0x20;
        memcpy(&fx.data[0x0C], param_type, len);
    }
    fx.data[0x2C] = big_endian ? 0xFFu : 0x00u;
    fx.data[0x2D] = SF_PARAM_FORMAT_FLAGS1_NONE;
    fx.data[0x2E] = SF_PARAM_FORMAT_FLAGS2_NONE;
    fx.data[0x2F] = 0x6A;

    put_u32(fx.data, rows_start + 0, 1000, big_endian);
    put_u16(fx.data, rows_start + 4, (uint16_t)data_start, big_endian);
    put_u16(fx.data, rows_start + 6, 0, big_endian);
    put_u32(fx.data, rows_start + 8, (uint32_t)strings_offset, big_endian);
    if (row_size > 0 && row_data) memcpy(&fx.data[data_start], row_data, row_size);
    memcpy(&fx.data[strings_offset], "row0", sizeof("row0"));

    fx.size = strings_offset + sizeof("row0");
    return fx;
}

static sf_param_t *read_param_ok(const param_fixture_t *fx) {
    sf_param_t *param = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_read_from_memory(&param, fx->data, fx->size, NULL));
    TEST_ASSERT_NOT_NULL(param);
    return param;
}

static const sf_param_cell_t *find_cell(const sf_param_t *param, const char *name) {
    const sf_param_row_t *row = sf_param_get_row(param, 0);
    TEST_ASSERT_NOT_NULL(row);
    const sf_param_cell_t *cell = sf_param_row_find_cell(row, name);
    TEST_ASSERT_NOT_NULL(cell);
    return cell;
}

static void assert_single_u32_cell(sf_param_t *param, const sf_paramdef_t *def,
                                   sf_param_apply_mode_t mode, uint32_t expected) {
    TEST_ASSERT_EQUAL(SF_OK, sf_param_apply_paramdef(param, def, mode));
    const sf_param_row_t *row = sf_param_get_row(param, 0);
    TEST_ASSERT_NOT_NULL(row);
    TEST_ASSERT_EQUAL_size_t(1, sf_param_row_get_cell_count(row));
    TEST_ASSERT_EQUAL_HEX32(expected, sf_param_cell_get_u32(find_cell(param, "value")));
}

static void test_apply_modes_happy_paths(void) {
    uint8_t row[4];
    put_u32(row, 0, 0xDEADBEEFu, false);
    sf_paramdef_field_t field = make_field(SF_PARAMDEF_DEF_TYPE_U32, "value", -1, 1);
    sf_paramdef_t def = make_def("TEST_PARAM", 123, 4, false, &field, 1);
    const sf_param_apply_mode_t modes[] = {
        SF_PARAM_APPLY_UNCONDITIONAL,
        SF_PARAM_APPLY_SOMEWHAT_CAREFUL,
        SF_PARAM_APPLY_CAREFUL,
    };

    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        param_fixture_t fx = make_param_fixture("TEST_PARAM", 123, false, row, sizeof(row));
        sf_param_t *param = read_param_ok(&fx);
        assert_single_u32_cell(param, &def, modes[i], 0xDEADBEEFu);
        sf_param_destroy(param);
    }
}

static void test_apply_multi_uses_first_successful_paramdef(void) {
    uint8_t row[4];
    put_u32(row, 0, 0x12345678u, false);
    sf_paramdef_field_t bad_field = make_field(SF_PARAMDEF_DEF_TYPE_U32, "bad", -1, 1);
    sf_paramdef_t bad = make_def("WRONG_PARAM", 123, 4, false, &bad_field, 1);
    sf_paramdef_field_t good_field = make_field(SF_PARAMDEF_DEF_TYPE_U32, "value", -1, 1);
    sf_paramdef_t good = make_def("TEST_PARAM", 123, 4, false, &good_field, 1);
    const sf_paramdef_t *defs[] = { &bad, &good };
    param_fixture_t fx = make_param_fixture("TEST_PARAM", 123, false, row, sizeof(row));
    sf_param_t *param = read_param_ok(&fx);

    TEST_ASSERT_EQUAL(SF_OK, sf_param_apply_paramdef_multi(param, defs, 2,
                                                           SF_PARAM_APPLY_CAREFUL));
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, sf_param_cell_get_u32(find_cell(param, "value")));

    sf_param_destroy(param);
}

static void test_somewhat_careful_accepts_empty_param_type(void) {
    uint8_t row[4];
    put_u32(row, 0, 77u, false);
    sf_paramdef_field_t field = make_field(SF_PARAMDEF_DEF_TYPE_U32, "value", -1, 1);
    sf_paramdef_t def = make_def("TEST_PARAM", 123, 4, false, &field, 1);
    param_fixture_t fx = make_param_fixture("", 123, false, row, sizeof(row));
    sf_param_t *param = read_param_ok(&fx);

    assert_single_u32_cell(param, &def, SF_PARAM_APPLY_SOMEWHAT_CAREFUL, 77u);

    sf_param_destroy(param);
}

static void test_careful_rejects_mismatched_data_version(void) {
    uint8_t row[4];
    put_u32(row, 0, 77u, false);
    sf_paramdef_field_t field = make_field(SF_PARAMDEF_DEF_TYPE_U32, "value", -1, 1);
    sf_paramdef_t def = make_def("TEST_PARAM", 2, 4, false, &field, 1);
    param_fixture_t fx = make_param_fixture("TEST_PARAM", 1, false, row, sizeof(row));
    sf_param_t *param = read_param_ok(&fx);

    TEST_ASSERT_EQUAL(SF_ERR_NOT_FOUND,
                      sf_param_apply_paramdef(param, &def, SF_PARAM_APPLY_CAREFUL));
    const sf_param_row_t *param_row = sf_param_get_row(param, 0);
    TEST_ASSERT_NOT_NULL(param_row);
    TEST_ASSERT_EQUAL_size_t(0, sf_param_row_get_cell_count(param_row));

    sf_param_destroy(param);
}

static void assert_bit_values(const sf_param_t *param) {
    TEST_ASSERT_EQUAL_UINT8(1, sf_param_cell_get_u8(find_cell(param, "flag")));
    TEST_ASSERT_EQUAL_UINT8(0x0A, sf_param_cell_get_u8(find_cell(param, "nibble")));
    TEST_ASSERT_EQUAL_UINT16(0x0BCD, sf_param_cell_get_u16(find_cell(param, "wide")));
}

static void test_bit_packed_fields_round_trip(void) {
    const uint8_t row[] = { 0x15, 0xCD, 0x0B };
    sf_paramdef_field_t fields[] = {
        make_field(SF_PARAMDEF_DEF_TYPE_U8, "flag", 1, 1),
        make_field(SF_PARAMDEF_DEF_TYPE_U8, "nibble", 4, 1),
        make_field(SF_PARAMDEF_DEF_TYPE_U16, "wide", 12, 1),
    };
    sf_paramdef_t def = make_def("TEST_PARAM", 123, (int32_t)sizeof(row), false,
                                 fields, sizeof(fields) / sizeof(fields[0]));
    param_fixture_t fx = make_param_fixture("TEST_PARAM", 123, false, row, sizeof(row));
    sf_param_t *param = read_param_ok(&fx);

    TEST_ASSERT_EQUAL(SF_OK, sf_param_apply_paramdef(param, &def, SF_PARAM_APPLY_CAREFUL));
    assert_bit_values(param);

    uint8_t *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_write_to_memory(param, &written, &written_size, NULL));
    TEST_ASSERT_NOT_NULL(written);

    sf_param_t *roundtrip = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_read_from_memory(&roundtrip, written, written_size, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_apply_paramdef(roundtrip, &def, SF_PARAM_APPLY_CAREFUL));
    assert_bit_values(roundtrip);

    sf_param_destroy(roundtrip);
    sf_free(NULL, written);
    sf_param_destroy(param);
}

static void test_signed_one_bit_field_sign_extends_to_minus_one(void) {
    const uint8_t row[] = { 0x01 };
    sf_paramdef_field_t field = make_field(SF_PARAMDEF_DEF_TYPE_S8, "signed_flag", 1, 1);
    sf_paramdef_t def = make_def("TEST_PARAM", 123, 1, false, &field, 1);
    param_fixture_t fx = make_param_fixture("TEST_PARAM", 123, false, row, sizeof(row));
    sf_param_t *param = read_param_ok(&fx);

    TEST_ASSERT_EQUAL(SF_OK, sf_param_apply_paramdef(param, &def, SF_PARAM_APPLY_CAREFUL));
    TEST_ASSERT_EQUAL_INT8(-1, sf_param_cell_get_s8(find_cell(param, "signed_flag")));

    sf_param_destroy(param);
}

static void test_u8_array_getter_returns_cell_bytes(void) {
    const uint8_t row[] = { 1, 2, 3, 4 };
    const uint8_t expected[] = { 1, 2, 3, 4 };
    sf_paramdef_field_t field = make_field(SF_PARAMDEF_DEF_TYPE_U8, "bytes", -1, 4);
    sf_paramdef_t def = make_def("TEST_PARAM", 123, 4, false, &field, 1);
    param_fixture_t fx = make_param_fixture("TEST_PARAM", 123, false, row, sizeof(row));
    sf_param_t *param = read_param_ok(&fx);

    TEST_ASSERT_EQUAL(SF_OK, sf_param_apply_paramdef(param, &def, SF_PARAM_APPLY_CAREFUL));
    const uint8_t *actual = NULL;
    size_t actual_size = 0;
    TEST_ASSERT_EQUAL(SF_OK,
                      sf_param_cell_get_bytes(find_cell(param, "bytes"), &actual, &actual_size));
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), actual_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, sizeof(expected));

    sf_param_destroy(param);
}

static void test_orphan_bits_are_rejected(void) {
    const uint8_t row[] = { 0x81 };
    sf_paramdef_field_t field = make_field(SF_PARAMDEF_DEF_TYPE_U8, "flag", 1, 1);
    sf_paramdef_t def = make_def("TEST_PARAM", 123, 1, false, &field, 1);
    param_fixture_t fx = make_param_fixture("TEST_PARAM", 123, false, row, sizeof(row));
    sf_param_t *param = read_param_ok(&fx);

    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC,
                      sf_param_apply_paramdef(param, &def, SF_PARAM_APPLY_CAREFUL));

    sf_param_destroy(param);
}

static void test_endianness_mismatch_is_silently_accepted(void) {
    uint8_t row[4];
    put_u32(row, 0, 0x11223344u, true);
    sf_paramdef_field_t field = make_field(SF_PARAMDEF_DEF_TYPE_U32, "value", -1, 1);
    sf_paramdef_t def = make_def("TEST_PARAM", 123, 4, false, &field, 1);
    param_fixture_t fx = make_param_fixture("TEST_PARAM", 123, true, row, sizeof(row));
    sf_param_t *param = read_param_ok(&fx);

    TEST_ASSERT_TRUE(sf_param_is_big_endian(param));
    TEST_ASSERT_FALSE(sf_paramdef_is_big_endian(&def));
    TEST_ASSERT_EQUAL(SF_OK, sf_param_apply_paramdef(param, &def, SF_PARAM_APPLY_CAREFUL));
    TEST_ASSERT_EQUAL_HEX32(0x11223344u, sf_param_cell_get_u32(find_cell(param, "value")));

    sf_param_destroy(param);
}

static void test_all_paramdef_types_decode_to_typed_getters(void) {
    uint8_t row[64];
    memset(row, 0, sizeof(row));
    size_t off = 0;
    row[off++] = 0xFBu;
    row[off++] = 250u;
    put_u16(row, off, (uint16_t)-1234, false); off += 2;
    put_u16(row, off, 4567u, false); off += 2;
    put_u32(row, off, (uint32_t)-123456, false); off += 4;
    put_u32(row, off, 0x89ABCDEFu, false); off += 4;
    put_u32(row, off, 1u, false); off += 4;
    float f32 = 1.25f;
    uint32_t f32_raw = 0;
    memcpy(&f32_raw, &f32, sizeof(f32_raw));
    put_u32(row, off, f32_raw, false); off += 4;
    float angle = -2.5f;
    uint32_t angle_raw = 0;
    memcpy(&angle_raw, &angle, sizeof(angle_raw));
    put_u32(row, off, angle_raw, false); off += 4;
    double f64 = 3.5;
    uint64_t f64_raw = 0;
    memcpy(&f64_raw, &f64, sizeof(f64_raw));
    put_u64(row, off, f64_raw, false); off += 8;
    const uint8_t dummy[] = { 0xAA, 0xBB, 0xCC };
    memcpy(&row[off], dummy, sizeof(dummy)); off += sizeof(dummy);
    memcpy(&row[off], "abc", 3); off += 6;
    row[off + 0] = 'W'; row[off + 1] = 0; off += 8;

    sf_paramdef_field_t fields[] = {
        make_field(SF_PARAMDEF_DEF_TYPE_S8, "s8", -1, 1),
        make_field(SF_PARAMDEF_DEF_TYPE_U8, "u8", -1, 1),
        make_field(SF_PARAMDEF_DEF_TYPE_S16, "s16", -1, 1),
        make_field(SF_PARAMDEF_DEF_TYPE_U16, "u16", -1, 1),
        make_field(SF_PARAMDEF_DEF_TYPE_S32, "s32", -1, 1),
        make_field(SF_PARAMDEF_DEF_TYPE_U32, "u32", -1, 1),
        make_field(SF_PARAMDEF_DEF_TYPE_B32, "b32", -1, 1),
        make_field(SF_PARAMDEF_DEF_TYPE_F32, "f32", -1, 1),
        make_field(SF_PARAMDEF_DEF_TYPE_ANGLE32, "angle32", -1, 1),
        make_field(SF_PARAMDEF_DEF_TYPE_F64, "f64", -1, 1),
        make_field(SF_PARAMDEF_DEF_TYPE_DUMMY8, "dummy8", -1, 3),
        make_field(SF_PARAMDEF_DEF_TYPE_FIXSTR, "fixstr", -1, 6),
        make_field(SF_PARAMDEF_DEF_TYPE_FIXSTR_W, "fixstrW", -1, 4),
    };
    sf_paramdef_t def = make_def("TEST_PARAM", 123, (int32_t)off, false,
                                 fields, sizeof(fields) / sizeof(fields[0]));
    param_fixture_t fx = make_param_fixture("TEST_PARAM", 123, false, row, off);
    sf_param_t *param = read_param_ok(&fx);

    TEST_ASSERT_EQUAL(SF_OK, sf_param_apply_paramdef(param, &def, SF_PARAM_APPLY_CAREFUL));
    const sf_param_row_t *param_row = sf_param_get_row(param, 0);
    TEST_ASSERT_EQUAL_size_t(13, sf_param_row_get_cell_count(param_row));
    TEST_ASSERT_EQUAL_INT8(-5, sf_param_cell_get_s8(find_cell(param, "s8")));
    TEST_ASSERT_EQUAL_UINT8(250, sf_param_cell_get_u8(find_cell(param, "u8")));
    TEST_ASSERT_EQUAL_INT16(-1234, sf_param_cell_get_s16(find_cell(param, "s16")));
    TEST_ASSERT_EQUAL_UINT16(4567, sf_param_cell_get_u16(find_cell(param, "u16")));
    TEST_ASSERT_EQUAL_INT32(-123456, sf_param_cell_get_s32(find_cell(param, "s32")));
    TEST_ASSERT_EQUAL_HEX32(0x89ABCDEFu, sf_param_cell_get_u32(find_cell(param, "u32")));
    TEST_ASSERT_TRUE(sf_param_cell_get_bool(find_cell(param, "b32")));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.25f, sf_param_cell_get_f32(find_cell(param, "f32")));
    const sf_param_cell_t *angle_cell = find_cell(param, "angle32");
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -2.5f, sf_param_cell_get_angle32(angle_cell));
    sf_param_cell_value_t angle_value = sf_param_cell_get_value(angle_cell);
    TEST_ASSERT_EQUAL(SF_PARAM_CELL_KIND_ANGLE32, angle_value.kind);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 3.5, sf_param_cell_get_f64(find_cell(param, "f64")));

    const uint8_t *actual_dummy = NULL;
    size_t actual_dummy_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_cell_get_bytes(find_cell(param, "dummy8"),
                                                     &actual_dummy, &actual_dummy_size));
    TEST_ASSERT_EQUAL_size_t(sizeof(dummy), actual_dummy_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(dummy, actual_dummy, sizeof(dummy));
    TEST_ASSERT_EQUAL_STRING("abc", sf_param_cell_get_string(find_cell(param, "fixstr")));
    TEST_ASSERT_EQUAL_STRING("W", sf_param_cell_get_string(find_cell(param, "fixstrW")));

    sf_param_destroy(param);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_apply_modes_happy_paths);
    RUN_TEST(test_apply_multi_uses_first_successful_paramdef);
    RUN_TEST(test_somewhat_careful_accepts_empty_param_type);
    RUN_TEST(test_careful_rejects_mismatched_data_version);
    RUN_TEST(test_bit_packed_fields_round_trip);
    RUN_TEST(test_signed_one_bit_field_sign_extends_to_minus_one);
    RUN_TEST(test_u8_array_getter_returns_cell_bytes);
    RUN_TEST(test_orphan_bits_are_rejected);
    RUN_TEST(test_endianness_mismatch_is_silently_accepted);
    RUN_TEST(test_all_paramdef_types_decode_to_typed_getters);
    return UNITY_END();
}
