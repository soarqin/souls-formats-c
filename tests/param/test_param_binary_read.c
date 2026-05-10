/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_param.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

typedef struct fixture {
    uint8_t data[512];
    size_t size;
} fixture_t;

void setUp(void) {}
void tearDown(void) {}

static void put_u16(uint8_t *p, size_t off, uint16_t v) {
    p[off + 0] = (uint8_t)(v & 0xFFu);
    p[off + 1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, size_t off, uint32_t v) {
    p[off + 0] = (uint8_t)(v & 0xFFu);
    p[off + 1] = (uint8_t)((v >> 8) & 0xFFu);
    p[off + 2] = (uint8_t)((v >> 16) & 0xFFu);
    p[off + 3] = (uint8_t)(v >> 24);
}

static void put_u64(uint8_t *p, size_t off, uint64_t v) {
    for (size_t i = 0; i < 8; i++) p[off + i] = (uint8_t)(v >> (i * 8));
}

static void put_cstr(uint8_t *p, size_t off, const char *s) {
    memcpy(&p[off], s, strlen(s) + 1);
}

static void write_common_header(fixture_t *fx, uint16_t row_count, uint32_t strings_offset,
                                uint16_t data_start, sf_param_format_flags1_t flags1,
                                sf_param_format_flags2_t flags2) {
    memset(fx, 0, sizeof(*fx));
    put_u32(fx->data, 0x00, strings_offset);
    put_u16(fx->data, 0x04, data_start);
    put_u16(fx->data, 0x06, 0);
    put_u16(fx->data, 0x08, 123);
    put_u16(fx->data, 0x0A, row_count);
    memcpy(&fx->data[0x0C], "TEST_PARAM", 10);
    fx->data[0x2C] = 0;
    fx->data[0x2D] = flags1;
    fx->data[0x2E] = flags2;
    fx->data[0x2F] = 0x6A;
}

static fixture_t make_old_fixture(uint16_t row_count) {
    fixture_t fx;
    const uint16_t rows_start = 0x30;
    const uint16_t row_header_size = 12;
    const uint16_t data_start = (uint16_t)(rows_start + row_count * row_header_size);
    const uint16_t row_data_size = 0x10;
    const uint16_t strings_offset = (uint16_t)(data_start + row_count * row_data_size);

    write_common_header(&fx, row_count, strings_offset, data_start,
                        SF_PARAM_FORMAT_FLAGS1_NONE, SF_PARAM_FORMAT_FLAGS2_NONE);

    size_t name_off = strings_offset;
    for (uint16_t i = 0; i < row_count; i++) {
        size_t row_off = rows_start + (size_t)i * row_header_size;
        put_u32(fx.data, row_off + 0, 1000u + i);
        put_u16(fx.data, row_off + 4, (uint16_t)(data_start + i * row_data_size));
        put_u16(fx.data, row_off + 6, 0);
        put_u32(fx.data, row_off + 8, (uint32_t)name_off);

        char name[] = { 'r', 'o', 'w', (char)('0' + i), '\0' };
        put_cstr(fx.data, name_off, name);
        name_off += strlen(name) + 1;
    }

    fx.size = name_off > 0 ? name_off : strings_offset;
    return fx;
}

static fixture_t make_long_offset_fixture(void) {
    fixture_t fx;
    const uint64_t rows_start = 0x40;
    const uint64_t data_start = rows_start + 24;
    const uint64_t strings_offset = data_start + 0x10;
    const uint64_t param_type_offset = strings_offset + 0x10;

    write_common_header(&fx, 1, (uint32_t)strings_offset, 0,
                        SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET,
                        SF_PARAM_FORMAT_FLAGS2_NONE);
    put_u32(fx.data, 0x0C, (uint32_t)param_type_offset);
    put_u64(fx.data, 0x30, data_start);
    put_u64(fx.data, 0x38, 0);

    put_u32(fx.data, 0x40, 4242);
    put_u32(fx.data, 0x44, 0);
    put_u64(fx.data, 0x48, data_start);
    put_u64(fx.data, 0x50, strings_offset);

    put_cstr(fx.data, (size_t)strings_offset, "long-row");
    put_cstr(fx.data, (size_t)param_type_offset, "LONG_PARAM");
    fx.size = (size_t)param_type_offset + strlen("LONG_PARAM") + 1;
    return fx;
}

static fixture_t make_unicode_fixture(void) {
    fixture_t fx;
    const uint16_t rows_start = 0x30;
    const uint16_t data_start = rows_start + 12;
    const uint16_t strings_offset = data_start + 0x10;

    write_common_header(&fx, 1, strings_offset, data_start,
                        SF_PARAM_FORMAT_FLAGS1_NONE,
                        SF_PARAM_FORMAT_FLAGS2_UNICODE_ROW_NAMES);
    put_u32(fx.data, 0x30, 7);
    put_u16(fx.data, 0x34, data_start);
    put_u16(fx.data, 0x36, 0);
    put_u32(fx.data, 0x38, strings_offset);

    put_u16(fx.data, strings_offset + 0, 0x540D); /* 名 */
    put_u16(fx.data, strings_offset + 2, 0);
    fx.size = strings_offset + 4;
    return fx;
}

static fixture_t make_unnamed_fixture(void) {
    fixture_t fx;
    write_common_header(&fx, 2, 0x60, 0x38, SF_PARAM_FORMAT_FLAGS1_NONE,
                        SF_PARAM_FORMAT_FLAGS2_NONE);
    fx.size = 0x60;
    return fx;
}

static sf_param_t *read_ok(const fixture_t *fx) {
    sf_param_t *param = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_read_from_memory(&param, fx->data, fx->size, NULL));
    TEST_ASSERT_NOT_NULL(param);
    return param;
}

static void test_zero_row_param_reads_header(void) {
    fixture_t fx = make_old_fixture(0);
    sf_param_t *param = read_ok(&fx);

    TEST_ASSERT_EQUAL_size_t(0, sf_param_get_row_count(param));
    TEST_ASSERT_EQUAL_STRING("TEST_PARAM", sf_param_get_param_type(param));
    TEST_ASSERT_FALSE(sf_param_is_big_endian(param));
    TEST_ASSERT_EQUAL_UINT8(SF_PARAM_FORMAT_FLAGS1_NONE, sf_param_get_format_flags1(param));
    TEST_ASSERT_EQUAL_UINT8(SF_PARAM_FORMAT_FLAGS2_NONE, sf_param_get_format_flags2(param));
    TEST_ASSERT_EQUAL_UINT8(0x6A, sf_param_get_paramdef_format_version(param));
    TEST_ASSERT_EQUAL_INT16(123, sf_param_get_paramdef_data_version(param));
    TEST_ASSERT_NULL(sf_param_get_row(param, 0));

    sf_param_destroy(param);
}

static void test_one_row_param_reads_id_name_and_empty_cells(void) {
    fixture_t fx = make_old_fixture(1);
    sf_param_t *param = read_ok(&fx);
    const sf_param_row_t *row = sf_param_get_row(param, 0);

    TEST_ASSERT_EQUAL_size_t(1, sf_param_get_row_count(param));
    TEST_ASSERT_NOT_NULL(row);
    TEST_ASSERT_EQUAL_INT32(1000, sf_param_row_get_id(row));
    TEST_ASSERT_EQUAL_STRING("row0", sf_param_row_get_name(row));
    TEST_ASSERT_EQUAL_size_t(0, sf_param_row_get_cell_count(row));
    TEST_ASSERT_NULL(sf_param_row_get_cell(row, 0));
    TEST_ASSERT_NULL(sf_param_row_find_cell(row, "anything"));
    TEST_ASSERT_EQUAL_PTR(row, sf_param_find_row_by_id(param, 1000));

    sf_param_destroy(param);
}

static void test_three_row_param_reads_all_ids(void) {
    fixture_t fx = make_old_fixture(3);
    sf_param_t *param = read_ok(&fx);

    TEST_ASSERT_EQUAL_size_t(3, sf_param_get_row_count(param));
    for (size_t i = 0; i < 3; i++) {
        const sf_param_row_t *row = sf_param_get_row(param, i);
        TEST_ASSERT_NOT_NULL(row);
        TEST_ASSERT_EQUAL_INT32(1000 + (int32_t)i, sf_param_row_get_id(row));
    }
    TEST_ASSERT_NULL(sf_param_find_row_by_id(param, 9999));

    sf_param_destroy(param);
}

static void test_er_style_long_offsets_read(void) {
    fixture_t fx = make_long_offset_fixture();
    sf_param_t *param = read_ok(&fx);
    const sf_param_row_t *row = sf_param_get_row(param, 0);

    TEST_ASSERT_EQUAL_UINT8(SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET,
                            sf_param_get_format_flags1(param));
    TEST_ASSERT_EQUAL_STRING("LONG_PARAM", sf_param_get_param_type(param));
    TEST_ASSERT_NOT_NULL(row);
    TEST_ASSERT_EQUAL_INT32(4242, sf_param_row_get_id(row));
    TEST_ASSERT_EQUAL_STRING("long-row", sf_param_row_get_name(row));

    sf_param_destroy(param);
}

static void test_unicode_row_names_decode_to_utf8(void) {
    fixture_t fx = make_unicode_fixture();
    sf_param_t *param = read_ok(&fx);
    const sf_param_row_t *row = sf_param_get_row(param, 0);

    TEST_ASSERT_NOT_NULL(row);
    TEST_ASSERT_EQUAL_STRING("名", sf_param_row_get_name(row));

    sf_param_destroy(param);
}

static void test_unnamed_rows_are_rejected_for_v1(void) {
    fixture_t fx = make_unnamed_fixture();
    sf_param_t *param = NULL;

    TEST_ASSERT_EQUAL(SF_ERR_UNSUPPORTED_VERSION,
                      sf_param_read_from_memory(&param, fx.data, fx.size, NULL));
    TEST_ASSERT_NULL(param);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_row_param_reads_header);
    RUN_TEST(test_one_row_param_reads_id_name_and_empty_cells);
    RUN_TEST(test_three_row_param_reads_all_ids);
    RUN_TEST(test_er_style_long_offsets_read);
    RUN_TEST(test_unicode_row_names_decode_to_utf8);
    RUN_TEST(test_unnamed_rows_are_rejected_for_v1);
    return UNITY_END();
}
