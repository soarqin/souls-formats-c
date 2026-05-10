/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_emevd.h"
#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct fixture_buf {
    uint8_t data[768];
    size_t size;
    bool big_endian;
    bool is_64_bit;
} fixture_buf_t;

void setUp(void) {}
void tearDown(void) {}

static void put_u8(fixture_buf_t *f, uint8_t value) {
    f->data[f->size++] = value;
}

static void put_i32_at(fixture_buf_t *f, size_t offset, int32_t value) {
    uint32_t v = (uint32_t)value;
    if (f->big_endian) {
        f->data[offset + 0] = (uint8_t)(v >> 24);
        f->data[offset + 1] = (uint8_t)(v >> 16);
        f->data[offset + 2] = (uint8_t)(v >> 8);
        f->data[offset + 3] = (uint8_t)v;
    } else {
        f->data[offset + 0] = (uint8_t)v;
        f->data[offset + 1] = (uint8_t)(v >> 8);
        f->data[offset + 2] = (uint8_t)(v >> 16);
        f->data[offset + 3] = (uint8_t)(v >> 24);
    }
}

static void put_i64_at(fixture_buf_t *f, size_t offset, int64_t value) {
    uint64_t v = (uint64_t)value;
    if (f->big_endian) {
        for (size_t i = 0; i < 8; i++) f->data[offset + i] = (uint8_t)(v >> ((7 - i) * 8));
    } else {
        for (size_t i = 0; i < 8; i++) f->data[offset + i] = (uint8_t)(v >> (i * 8));
    }
}

static void put_i32(fixture_buf_t *f, int32_t value) {
    put_i32_at(f, f->size, value);
    f->size += 4;
}

static void put_u32(fixture_buf_t *f, uint32_t value) {
    put_i32(f, (int32_t)value);
}

static void put_i64(fixture_buf_t *f, int64_t value) {
    put_i64_at(f, f->size, value);
    f->size += 8;
}

static void put_varint(fixture_buf_t *f, int64_t value) {
    if (f->is_64_bit) {
        put_i64(f, value);
    } else {
        put_i32(f, (int32_t)value);
    }
}

static size_t varint_size(const fixture_buf_t *f) {
    return f->is_64_bit ? 8u : 4u;
}

static void pad_to(fixture_buf_t *f, size_t offset) {
    while (f->size < offset) put_u8(f, 0);
}

static void build_fixture(fixture_buf_t *f, bool big_endian, bool is_64_bit, bool unk06,
                          bool unk07, int32_t version, bool has_layer) {
    memset(f, 0, sizeof(*f));
    f->big_endian = big_endian;
    f->is_64_bit = is_64_bit;

    const size_t vint = varint_size(f);
    const size_t header_size = 16u + (16u * vint) + (is_64_bit ? 0u : 4u);
    const size_t event_size = is_64_bit ? 48u : 28u;
    const size_t instr_size = is_64_bit ? 32u : 24u;
    const size_t param_size = (3u * vint) + 8u;
    const size_t layer_size = has_layer ? (8u + (3u * vint)) : 0u;
    const int64_t arg_len = 4;
    const int64_t string_len = 3;

    const int64_t events_offset = (int64_t)header_size;
    const int64_t instructions_offset = events_offset + (int64_t)event_size;
    const int64_t parameters_offset = instructions_offset + (int64_t)instr_size;
    const int64_t layers_offset = parameters_offset + (int64_t)param_size;
    const int64_t args_offset = layers_offset + (int64_t)layer_size;
    const int64_t strings_offset = args_offset + arg_len;
    const int64_t linked_files_offset = strings_offset + string_len;
    const int64_t file_size = linked_files_offset + (int64_t)vint;

    put_u8(f, 'E');
    put_u8(f, 'V');
    put_u8(f, 'D');
    put_u8(f, 0);
    put_u8(f, big_endian ? 1u : 0u);
    put_u8(f, is_64_bit ? 0xFFu : 0u);
    put_u8(f, unk06 ? 1u : 0u);
    put_u8(f, unk07 ? 0xFFu : 0u);
    put_i32(f, version);
    put_i32(f, (int32_t)file_size);

    put_varint(f, 1);                 /* Event count */
    put_varint(f, events_offset);
    put_varint(f, 1);                 /* Instruction count */
    put_varint(f, instructions_offset);
    put_varint(f, 0);                 /* Unknown struct count */
    put_varint(f, 0);                 /* Unknown struct offset */
    put_varint(f, has_layer ? 1 : 0); /* Layer count */
    put_varint(f, layers_offset);
    put_varint(f, 1);                 /* Parameter count */
    put_varint(f, parameters_offset);
    put_varint(f, 1);                 /* Linked file count */
    put_varint(f, linked_files_offset);
    put_varint(f, arg_len);
    put_varint(f, args_offset);
    put_varint(f, string_len);
    put_varint(f, strings_offset);
    if (!is_64_bit) put_i32(f, 0);

    pad_to(f, (size_t)events_offset);
    put_varint(f, 1000); /* Event ID */
    put_varint(f, 1);    /* Instruction count */
    put_varint(f, 0);    /* Instruction offset relative to section */
    put_varint(f, 1);    /* Parameter count */
    put_varint(f, 0);    /* Parameter offset relative to section */
    put_i32(f, SF_EMEVD_REST_BEHAVIOR_RESTART);
    put_i32(f, 0);

    pad_to(f, (size_t)instructions_offset);
    put_i32(f, 2000);
    put_i32(f, 3000);
    put_varint(f, arg_len);
    put_varint(f, 0);
    if (has_layer) {
        put_i64(f, 0);
    } else if (is_64_bit && unk06) {
        put_i64(f, -1);
    } else {
        put_i32(f, -1);
        put_i32(f, 0);
    }

    pad_to(f, (size_t)parameters_offset);
    put_varint(f, 0);
    put_varint(f, 1);
    put_varint(f, 2);
    put_i32(f, 4);
    put_i32(f, 99);

    if (has_layer) {
        pad_to(f, (size_t)layers_offset);
        put_i32(f, 2);
        put_u32(f, 0x00000005u);
        put_varint(f, 0);
        put_varint(f, -1);
        put_varint(f, 1);
    }

    pad_to(f, (size_t)args_offset);
    put_u8(f, 1);
    put_u8(f, 2);
    put_u8(f, 3);
    put_u8(f, 4);

    pad_to(f, (size_t)strings_offset);
    put_u8(f, 'a');
    put_u8(f, 'b');
    put_u8(f, 0);

    pad_to(f, (size_t)linked_files_offset);
    put_varint(f, 0x1234);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)file_size, (uint64_t)f->size);
}

static void assert_common_fixture(sf_emevd_t *emevd, sf_emevd_format_t expected_format,
                                  bool expect_layer) {
    TEST_ASSERT_NOT_NULL(emevd);
    TEST_ASSERT_EQUAL_INT(expected_format, sf_emevd_get_format(emevd));
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_get_event_count(emevd));
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_get_linked_file_count(emevd));
    TEST_ASSERT_EQUAL_INT64(0x1234, sf_emevd_get_linked_file_offset(emevd, 0));
    TEST_ASSERT_EQUAL_UINT64(3, sf_emevd_get_string_data_size(emevd));
    TEST_ASSERT_EQUAL_MEMORY("ab\0", sf_emevd_get_string_data(emevd), 3);

    const sf_emevd_event_t *event = sf_emevd_get_event(emevd, 0);
    TEST_ASSERT_NOT_NULL(event);
    TEST_ASSERT_EQUAL_INT64(1000, sf_emevd_event_get_id(event));
    TEST_ASSERT_EQUAL_INT(SF_EMEVD_REST_BEHAVIOR_RESTART,
                          sf_emevd_event_get_rest_behavior(event));
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_event_get_instruction_count(event));
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_event_get_parameter_count(event));

    const sf_emevd_instruction_t *instr = sf_emevd_event_get_instruction(event, 0);
    TEST_ASSERT_NOT_NULL(instr);
    TEST_ASSERT_EQUAL_INT32(2000, sf_emevd_instruction_get_bank(instr));
    TEST_ASSERT_EQUAL_INT32(3000, sf_emevd_instruction_get_id(instr));
    const uint8_t *arg_data = NULL;
    size_t arg_data_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_instruction_get_arg_data(instr, &arg_data,
                                                                   &arg_data_size));
    const uint8_t expected_args[4] = {1, 2, 3, 4};
    TEST_ASSERT_EQUAL_UINT64(4, arg_data_size);
    TEST_ASSERT_EQUAL_MEMORY(expected_args, arg_data, sizeof(expected_args));

    const sf_emevd_layer_t *layer = sf_emevd_instruction_get_layer(instr);
    if (expect_layer) {
        TEST_ASSERT_NOT_NULL(layer);
        TEST_ASSERT_EQUAL_UINT32(0x00000005u, sf_emevd_layer_get_mask(layer));
    } else {
        TEST_ASSERT_NULL(layer);
    }

    const sf_emevd_parameter_t *parameter = sf_emevd_event_get_parameter(event, 0);
    TEST_ASSERT_NOT_NULL(parameter);
    TEST_ASSERT_EQUAL_INT64(0, sf_emevd_parameter_get_instruction_index(parameter));
    TEST_ASSERT_EQUAL_INT64(1, sf_emevd_parameter_get_target_start_byte(parameter));
    TEST_ASSERT_EQUAL_INT64(2, sf_emevd_parameter_get_source_start_byte(parameter));
    TEST_ASSERT_EQUAL_INT32(4, sf_emevd_parameter_get_byte_count(parameter));
    TEST_ASSERT_EQUAL_INT32(99, sf_emevd_parameter_get_unk_id(parameter));
}

static void test_reads_ds1_variant(void) {
    fixture_buf_t f;
    build_fixture(&f, false, false, false, false, 0xCC, false);
    sf_emevd_t *emevd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_read_from_memory(&emevd, f.data, f.size, NULL));
    assert_common_fixture(emevd, SF_EMEVD_FORMAT_DARK_SOULS_1, false);
    sf_emevd_destroy(emevd, NULL);
}

static void test_reads_ds1_be_variant(void) {
    fixture_buf_t f;
    build_fixture(&f, true, false, false, false, 0xCC, false);
    sf_emevd_t *emevd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_read_from_memory(&emevd, f.data, f.size, NULL));
    assert_common_fixture(emevd, SF_EMEVD_FORMAT_DARK_SOULS_1_BE, false);
    sf_emevd_destroy(emevd, NULL);
}

static void test_reads_bloodborne_variant(void) {
    fixture_buf_t f;
    build_fixture(&f, false, true, false, false, 0xCC, false);
    sf_emevd_t *emevd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_read_from_memory(&emevd, f.data, f.size, NULL));
    assert_common_fixture(emevd, SF_EMEVD_FORMAT_BLOODBORNE, false);
    sf_emevd_destroy(emevd, NULL);
}

static void test_reads_dark_souls_3_variant(void) {
    fixture_buf_t f;
    build_fixture(&f, false, true, true, false, 0xCD, false);
    sf_emevd_t *emevd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_read_from_memory(&emevd, f.data, f.size, NULL));
    assert_common_fixture(emevd, SF_EMEVD_FORMAT_DARK_SOULS_3, false);
    sf_emevd_destroy(emevd, NULL);
}

static void test_reads_sekiro_variant_with_layer(void) {
    fixture_buf_t f;
    build_fixture(&f, false, true, true, true, 0xCD, true);
    sf_emevd_t *emevd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_read_from_memory(&emevd, f.data, f.size, NULL));
    assert_common_fixture(emevd, SF_EMEVD_FORMAT_SEKIRO, true);
    sf_emevd_destroy(emevd, NULL);
}

static void test_rejects_novel_flags(void) {
    fixture_buf_t f;
    build_fixture(&f, false, true, false, true, 0xCD, false);
    sf_emevd_t *emevd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_ERR_UNSUPPORTED_VERSION,
                          sf_emevd_read_from_memory(&emevd, f.data, f.size, NULL));
    TEST_ASSERT_NULL(emevd);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reads_ds1_variant);
    RUN_TEST(test_reads_ds1_be_variant);
    RUN_TEST(test_reads_bloodborne_variant);
    RUN_TEST(test_reads_dark_souls_3_variant);
    RUN_TEST(test_reads_sekiro_variant_with_layer);
    RUN_TEST(test_rejects_novel_flags);
    return UNITY_END();
}
