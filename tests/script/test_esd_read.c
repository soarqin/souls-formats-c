/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_esd.h"
#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct fixture_buf {
    uint8_t data[4096];
    size_t size;
    bool long_format;
} fixture_buf_t;

void setUp(void) {}
void tearDown(void) {}

static size_t vint(const fixture_buf_t *f) { return f->long_format ? 8u : 4u; }
static size_t state_size(const fixture_buf_t *f) { return f->long_format ? 0x48u : 0x24u; }
static size_t condition_size(const fixture_buf_t *f) { return f->long_format ? 0x38u : 0x1Cu; }
static size_t command_size(const fixture_buf_t *f) { return f->long_format ? 0x18u : 0x10u; }
static size_t arg_size(const fixture_buf_t *f) { return f->long_format ? 0x10u : 0x8u; }

static void put_u8(fixture_buf_t *f, uint8_t value) { f->data[f->size++] = value; }

static void put_i32_at(fixture_buf_t *f, size_t off, int32_t value) {
    uint32_t v = (uint32_t)value;
    f->data[off + 0] = (uint8_t)v;
    f->data[off + 1] = (uint8_t)(v >> 8);
    f->data[off + 2] = (uint8_t)(v >> 16);
    f->data[off + 3] = (uint8_t)(v >> 24);
}

static void put_i64_at(fixture_buf_t *f, size_t off, int64_t value) {
    uint64_t v = (uint64_t)value;
    for (size_t i = 0; i < 8; i++) f->data[off + i] = (uint8_t)(v >> (i * 8));
}

static void put_i32(fixture_buf_t *f, int32_t value) {
    put_i32_at(f, f->size, value);
    f->size += 4;
}

static void put_var(fixture_buf_t *f, int64_t value) {
    if (f->long_format) {
        put_i64_at(f, f->size, value);
        f->size += 8;
    } else {
        put_i32(f, (int32_t)value);
    }
}

static void pad_to(fixture_buf_t *f, size_t size) {
    while (f->size < size) put_u8(f, 0);
}

static void write_command(fixture_buf_t *f, int32_t id, int64_t args_rel) {
    put_i32(f, 1);
    put_i32(f, id);
    put_var(f, args_rel);
    put_var(f, 1);
}

static void build_fixture(fixture_buf_t *f, bool long_format, int32_t dark_souls_count) {
    memset(f, 0, sizeof(*f));
    f->long_format = long_format;

    const int32_t group_count = 2;
    const int32_t real_state_count = 6;
    const int32_t stored_state_count = 8;
    const int32_t condition_count = 12;
    const int32_t command_count = 18;
    const int32_t command_arg_count = 18;
    const size_t data_start = 0x6Cu;
    const int64_t data_header_size = long_format ? 0x48 : 0x2C;
    const int64_t groups_rel = data_header_size;
    const int64_t states_rel = groups_rel + group_count * (int32_t)(4 * vint(f));
    const int64_t conditions_rel = states_rel + stored_state_count * (int32_t)state_size(f);
    const int64_t state_cond_lists_rel = conditions_rel + condition_count * (int32_t)condition_size(f);
    const int64_t entry_cmds_rel = state_cond_lists_rel + real_state_count * (int32_t)(2 * vint(f));
    const int64_t pass_cmds_rel = entry_cmds_rel + real_state_count * (int32_t)command_size(f);
    const int64_t arg_tables_rel = pass_cmds_rel + condition_count * (int32_t)command_size(f);
    int64_t byte_rel = arg_tables_rel + command_count * (int32_t)arg_size(f);
    int64_t evaluator_rels[12];
    int64_t arg_rels[18];

    memcpy(f->data, long_format ? "fsSL" : "fSSL", 4);
    f->size = 4;
    put_i32(f, 1);
    put_i32(f, dark_souls_count);
    put_i32(f, dark_souls_count);
    put_i32(f, 0x54);
    const size_t data_size_patch = f->size;
    put_i32(f, 0);
    put_i32(f, 6);
    put_i32(f, long_format ? 0x48 : 0x2C);
    put_i32(f, 1);
    put_i32(f, long_format ? 0x20 : 0x10);
    put_i32(f, group_count);
    put_i32(f, long_format ? 0x48 : 0x24);
    put_i32(f, stored_state_count);
    put_i32(f, long_format ? 0x38 : 0x1C);
    put_i32(f, condition_count);
    put_i32(f, long_format ? 0x18 : 0x10);
    put_i32(f, command_count);
    put_i32(f, long_format ? 0x10 : 0x8);
    put_i32(f, command_arg_count);
    put_i32(f, 0);
    put_i32(f, 0);
    const size_t name_block_patch = f->size;
    put_i32(f, 0);
    put_i32(f, 0);
    const size_t unk1_patch = f->size;
    put_i32(f, 0);
    put_i32(f, 0);
    const size_t unk2_patch = f->size;
    put_i32(f, 0);
    put_i32(f, 0);
    TEST_ASSERT_EQUAL_size_t(data_start, f->size);

    put_i32(f, 1);
    put_i32(f, 0x70);
    put_i32(f, 0x74);
    put_i32(f, 0x78);
    put_i32(f, 0x7C);
    if (long_format) put_i32(f, 0);
    put_var(f, groups_rel);
    put_var(f, group_count);
    put_var(f, -1);
    put_var(f, 0);
    put_var(f, dark_souls_count == 1 ? 0 : -1);
    put_var(f, dark_souls_count == 1 ? 0 : -1);

    pad_to(f, data_start + (size_t)groups_rel);
    put_var(f, 10);
    put_var(f, states_rel);
    put_var(f, 3);
    put_var(f, states_rel);
    put_var(f, 20);
    put_var(f, states_rel + 4 * (int64_t)state_size(f));
    put_var(f, 3);
    put_var(f, states_rel + 4 * (int64_t)state_size(f));

    pad_to(f, data_start + (size_t)states_rel);
    for (int i = 0; i < real_state_count; i++) {
        const int group_base = i < 3 ? 0 : 3;
        const int local = i - group_base;
        put_var(f, 100 + i);
        put_var(f, state_cond_lists_rel + i * (int32_t)(2 * vint(f)));
        put_var(f, 2);
        put_var(f, entry_cmds_rel + i * (int32_t)command_size(f));
        put_var(f, 1);
        put_var(f, 0);
        put_var(f, 0);
        put_var(f, 0);
        put_var(f, 0);
        if (local == 2) {
            const size_t first_state = data_start + (size_t)(states_rel + group_base * (int32_t)state_size(f));
            memcpy(&f->data[f->size], &f->data[first_state], state_size(f));
            f->size += state_size(f);
        }
    }

    pad_to(f, data_start + (size_t)conditions_rel);
    for (int c = 0; c < condition_count; c++) {
        const int state_idx = c / 2;
        const int group_base = state_idx < 3 ? 0 : 3;
        const int local = state_idx - group_base;
        const int target = group_base + ((local + (c % 2) + 1) % 3);
        put_var(f, states_rel + (target < 3 ? target : target + 1) * (int32_t)state_size(f));
        put_var(f, pass_cmds_rel + c * (int32_t)command_size(f));
        put_var(f, 1);
        put_var(f, 0);
        put_var(f, 0);
        evaluator_rels[c] = byte_rel;
        byte_rel += 2;
        put_var(f, evaluator_rels[c]);
        put_var(f, 2);
    }

    pad_to(f, data_start + (size_t)state_cond_lists_rel);
    for (int s = 0; s < real_state_count; s++) {
        put_var(f, conditions_rel + (s * 2) * (int32_t)condition_size(f));
        put_var(f, conditions_rel + (s * 2 + 1) * (int32_t)condition_size(f));
    }

    pad_to(f, data_start + (size_t)entry_cmds_rel);
    for (int i = 0; i < real_state_count; i++) {
        write_command(f, 1000 + i, arg_tables_rel + i * (int32_t)arg_size(f));
    }
    for (int c = 0; c < condition_count; c++) {
        write_command(f, 2000 + c, arg_tables_rel + (real_state_count + c) * (int32_t)arg_size(f));
    }

    for (int i = 0; i < command_count; i++) {
        arg_rels[i] = byte_rel;
        byte_rel += 1;
        put_var(f, arg_rels[i]);
        put_var(f, 1);
    }

    pad_to(f, data_start + (size_t)evaluator_rels[0]);
    for (int c = 0; c < condition_count; c++) {
        put_u8(f, (uint8_t)(0xA0 + c));
        put_u8(f, (uint8_t)(0xB0 + c));
    }
    for (int i = 0; i < command_count; i++) put_u8(f, (uint8_t)(0xC0 + i));

    put_i32_at(f, data_size_patch, (int32_t)(f->size - data_start));
    put_i32_at(f, name_block_patch, (int32_t)(f->size - data_start));
    put_i32_at(f, unk1_patch, (int32_t)(f->size - data_start));
    put_i32_at(f, unk2_patch, (int32_t)(f->size - data_start));
}

static void assert_fixture_read(bool long_format, int32_t version) {
    fixture_buf_t f;
    build_fixture(&f, long_format, version);
    sf_esd_t *esd = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_esd_read_from_memory(&esd, f.data, f.size, NULL));
    TEST_ASSERT_NOT_NULL(esd);
    TEST_ASSERT_EQUAL(long_format, sf_esd_is_long_format(esd));
    TEST_ASSERT_EQUAL_INT32(version, sf_esd_get_format_version(esd));
    TEST_ASSERT_EQUAL_INT32(2, sf_esd_get_state_group_count(esd));

    int64_t group_id = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_esd_get_state_group_id(esd, 0, &group_id));
    TEST_ASSERT_EQUAL_INT64(10, group_id);
    TEST_ASSERT_EQUAL_INT32(3, sf_esd_get_state_count(esd, 10));
    TEST_ASSERT_EQUAL(SF_OK, sf_esd_get_state_group_id(esd, 1, &group_id));
    TEST_ASSERT_EQUAL_INT64(20, group_id);
    TEST_ASSERT_EQUAL_INT32(3, sf_esd_get_state_count(esd, 20));

    const sf_esd_state_t *state = sf_esd_get_state(esd, 10, 0);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_INT64(100, sf_esd_state_get_id(state));
    TEST_ASSERT_EQUAL_INT32(2, sf_esd_state_get_condition_count(state));
    TEST_ASSERT_EQUAL_INT32(1, sf_esd_state_get_entry_command_count(state));
    const sf_esd_command_call_t *entry = sf_esd_state_get_entry_command(state, 0);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_INT32(1, sf_esd_command_call_get_bank(entry));
    TEST_ASSERT_EQUAL_INT32(1000, sf_esd_command_call_get_id(entry));
    TEST_ASSERT_EQUAL_INT32(1, sf_esd_command_call_get_argument_count(entry));
    const uint8_t *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_esd_command_call_get_argument(entry, 0, &bytes, &size));
    TEST_ASSERT_EQUAL_size_t(1, size);
    TEST_ASSERT_EQUAL_UINT8(0xC0, bytes[0]);

    const sf_esd_condition_t *condition = sf_esd_state_get_condition(state, 0);
    TEST_ASSERT_NOT_NULL(condition);
    TEST_ASSERT_EQUAL_INT64(101, sf_esd_condition_get_target_state(condition));
    TEST_ASSERT_EQUAL_INT32(0, sf_esd_condition_get_subcondition_count(condition));
    TEST_ASSERT_EQUAL_INT32(1, sf_esd_condition_get_pass_command_count(condition));
    TEST_ASSERT_EQUAL(SF_OK, sf_esd_condition_get_evaluator(condition, &bytes, &size));
    TEST_ASSERT_EQUAL_size_t(2, size);
    TEST_ASSERT_EQUAL_UINT8(0xA0, bytes[0]);
    TEST_ASSERT_EQUAL_UINT8(0xB0, bytes[1]);
    const sf_esd_command_call_t *pass = sf_esd_condition_get_pass_command(condition, 0);
    TEST_ASSERT_NOT_NULL(pass);
    TEST_ASSERT_EQUAL_INT32(2000, sf_esd_command_call_get_id(pass));

    state = sf_esd_get_state(esd, 20, 2);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_INT64(105, sf_esd_state_get_id(state));
    condition = sf_esd_state_get_condition(state, 1);
    TEST_ASSERT_NOT_NULL(condition);
    TEST_ASSERT_EQUAL_INT64(101 + 3, sf_esd_condition_get_target_state(condition));

    sf_esd_destroy(esd);
}

static void test_short_format_dark_souls_1(void) { assert_fixture_read(false, 1); }
static void test_long_format_dark_souls_3(void) { assert_fixture_read(true, 3); }

static void test_corrupt_magic_returns_bad_magic(void) {
    fixture_buf_t f;
    build_fixture(&f, false, 1);
    f.data[0] = 'X';
    sf_esd_t *esd = NULL;
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC, sf_esd_read_from_memory(&esd, f.data, f.size, NULL));
    TEST_ASSERT_NULL(esd);
}

static void test_truncated_buffer_returns_truncated(void) {
    fixture_buf_t f;
    build_fixture(&f, false, 1);
    sf_esd_t *esd = NULL;
    TEST_ASSERT_EQUAL(SF_ERR_TRUNCATED, sf_esd_read_from_memory(&esd, f.data, 3, NULL));
    TEST_ASSERT_NULL(esd);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_short_format_dark_souls_1);
    RUN_TEST(test_long_format_dark_souls_3);
    RUN_TEST(test_corrupt_magic_returns_bad_magic);
    RUN_TEST(test_truncated_buffer_returns_truncated);
    return UNITY_END();
}
