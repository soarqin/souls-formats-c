/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_param.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

typedef struct fixture {
    uint8_t data[768];
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
    const uint16_t row_data_size = 4;
    const uint16_t data_start = (uint16_t)(rows_start + row_count * row_header_size);
    const uint16_t strings_offset = (uint16_t)(data_start + row_count * row_data_size);

    write_common_header(&fx, row_count, strings_offset, data_start,
                        SF_PARAM_FORMAT_FLAGS1_NONE, SF_PARAM_FORMAT_FLAGS2_NONE);

    size_t name_off = strings_offset;
    put_u16(fx.data, name_off, 0);
    name_off += 2;
    for (uint16_t i = 0; i < row_count; i++) {
        size_t row_off = rows_start + (size_t)i * row_header_size;
        size_t data_off = data_start + (size_t)i * row_data_size;
        put_u32(fx.data, row_off + 0, 1000u + i);
        put_u16(fx.data, row_off + 4, (uint16_t)data_off);
        put_u16(fx.data, row_off + 6, 0);
        put_u32(fx.data, row_off + 8, (uint32_t)name_off);
        put_u32(fx.data, data_off, 0xA0B0C000u + i);

        char name[] = { 'r', 'o', 'w', (char)('0' + i), '\0' };
        put_cstr(fx.data, name_off, name);
        name_off += strlen(name) + 1;
    }
    put_u16(fx.data, name_off, 0);
    fx.size = name_off + 2;
    return fx;
}

static fixture_t make_long_offset_fixture(void) {
    fixture_t fx;
    const uint64_t rows_start = 0x40;
    const uint64_t data_start = rows_start + 24;
    const uint64_t row_data_size = 8;
    const uint64_t strings_offset = data_start + row_data_size;

    write_common_header(&fx, 1, (uint32_t)strings_offset, 0,
                        SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET,
                        SF_PARAM_FORMAT_FLAGS2_NONE);
    put_u32(fx.data, 0x0C, (uint32_t)strings_offset);
    put_u64(fx.data, 0x30, data_start);
    put_u64(fx.data, 0x38, 0);

    put_u32(fx.data, rows_start + 0, 4242);
    put_u32(fx.data, rows_start + 4, 0);
    put_u64(fx.data, rows_start + 8, data_start);
    size_t name_off = (size_t)strings_offset + strlen("LONG_PARAM") + 1 + 2;
    put_u64(fx.data, rows_start + 16, name_off);
    put_u64(fx.data, data_start, 0x0102030405060708ull);

    put_cstr(fx.data, (size_t)strings_offset, "LONG_PARAM");
    put_u16(fx.data, (size_t)strings_offset + strlen("LONG_PARAM") + 1, 0);
    put_cstr(fx.data, name_off, "long-row");
    put_u16(fx.data, name_off + strlen("long-row") + 1, 0);
    fx.size = name_off + strlen("long-row") + 3;
    return fx;
}

static sf_param_t *read_ok(const uint8_t *data, size_t size) {
    sf_param_t *param = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_read_from_memory(&param, data, size, NULL));
    TEST_ASSERT_NOT_NULL(param);
    return param;
}

static void assert_written_round_trip(const fixture_t *fx, size_t expected_rows,
                                      const int32_t *expected_ids,
                                      const char *const *expected_names) {
    sf_param_t *param = read_ok(fx->data, fx->size);
    uint8_t *bytes_first = NULL;
    size_t size_first = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_write_to_memory(param, &bytes_first, &size_first, NULL));
    TEST_ASSERT_NOT_NULL(bytes_first);
    TEST_ASSERT_GREATER_THAN_size_t(0, size_first);

    sf_param_t *round = read_ok(bytes_first, size_first);
    TEST_ASSERT_EQUAL_size_t(expected_rows, sf_param_get_row_count(round));
    for (size_t i = 0; i < expected_rows; i++) {
        const sf_param_row_t *row = sf_param_get_row(round, i);
        TEST_ASSERT_NOT_NULL(row);
        TEST_ASSERT_EQUAL_INT32(expected_ids[i], sf_param_row_get_id(row));
        TEST_ASSERT_EQUAL_STRING(expected_names[i], sf_param_row_get_name(row));
    }

    uint8_t *bytes_second = NULL;
    size_t size_second = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_param_write_to_memory(round, &bytes_second, &size_second, NULL));
    TEST_ASSERT_EQUAL_size_t(size_first, size_second);
    TEST_ASSERT_EQUAL_MEMORY(bytes_first, bytes_second, size_first);

    sf_free(NULL, bytes_first);
    sf_free(NULL, bytes_second);
    sf_param_destroy(round);
    sf_param_destroy(param);
}

static void test_zero_row_param_write_round_trips(void) {
    fixture_t fx = make_old_fixture(0);
    assert_written_round_trip(&fx, 0, NULL, NULL);
}

static void test_one_row_param_write_round_trips(void) {
    const int32_t ids[] = { 1000 };
    const char *const names[] = { "row0" };
    fixture_t fx = make_old_fixture(1);
    assert_written_round_trip(&fx, 1, ids, names);
}

static void test_three_row_param_write_round_trips(void) {
    const int32_t ids[] = { 1000, 1001, 1002 };
    const char *const names[] = { "row0", "row1", "row2" };
    fixture_t fx = make_old_fixture(3);
    assert_written_round_trip(&fx, 3, ids, names);
}

static void test_er_style_64_bit_offsets_write_round_trips(void) {
    const int32_t ids[] = { 4242 };
    const char *const names[] = { "long-row" };
    fixture_t fx = make_long_offset_fixture();
    assert_written_round_trip(&fx, 1, ids, names);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_row_param_write_round_trips);
    RUN_TEST(test_one_row_param_write_round_trips);
    RUN_TEST(test_three_row_param_write_round_trips);
    RUN_TEST(test_er_style_64_bit_offsets_write_round_trips);
    return UNITY_END();
}
