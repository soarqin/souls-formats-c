/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "script/emevd_internal.h" /* IWYU pragma: keep */
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static sf_emevd_t *make_emevd(sf_emevd_format_t format) {
    sf_emevd_t *emevd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_create(NULL, format, &emevd));
    TEST_ASSERT_NOT_NULL(emevd);

    emevd->event_count = 1;
    emevd->events = (sf_emevd_event_t *)sf_xalloc(emevd->alloc, sizeof(*emevd->events));
    TEST_ASSERT_NOT_NULL(emevd->events);
    memset(emevd->events, 0, sizeof(*emevd->events));

    sf_emevd_event_t *event = &emevd->events[0];
    event->id = 1000;
    event->rest_behavior = SF_EMEVD_REST_BEHAVIOR_RESTART;
    event->instruction_count = 1;
    event->instructions = (sf_emevd_instruction_t *)sf_xalloc(emevd->alloc,
                                                              sizeof(*event->instructions));
    TEST_ASSERT_NOT_NULL(event->instructions);
    memset(event->instructions, 0, sizeof(*event->instructions));
    event->instructions[0].bank = 2000;
    event->instructions[0].id = 3000;
    return emevd;
}

static void assert_minimal_roundtrip(sf_emevd_format_t format) {
    sf_emevd_t *emevd = make_emevd(format);
    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_write_to_memory(emevd, &data, &size, NULL));
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_GREATER_THAN_UINT64(0, size);

    sf_emevd_t *readback = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_read_from_memory(&readback, data, size, NULL));
    TEST_ASSERT_NOT_NULL(readback);
    TEST_ASSERT_EQUAL_INT(format, sf_emevd_get_format(readback));
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_get_event_count(readback));
    const sf_emevd_event_t *event = sf_emevd_get_event(readback, 0);
    TEST_ASSERT_NOT_NULL(event);
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_event_get_instruction_count(event));
    const sf_emevd_instruction_t *instr = sf_emevd_event_get_instruction(event, 0);
    TEST_ASSERT_NOT_NULL(instr);
    TEST_ASSERT_EQUAL_INT32(2000, sf_emevd_instruction_get_bank(instr));
    TEST_ASSERT_EQUAL_INT32(3000, sf_emevd_instruction_get_id(instr));

    uint8_t *data2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_write_to_memory(readback, &data2, &size2, NULL));
    TEST_ASSERT_EQUAL_UINT64(size, size2);
    TEST_ASSERT_EQUAL_MEMORY(data, data2, size);

    sf_free(NULL, data2);
    sf_emevd_destroy(readback, NULL);
    sf_free(NULL, data);
    sf_emevd_destroy(emevd, NULL);
}

static void test_writes_dark_souls_1_variant(void) {
    assert_minimal_roundtrip(SF_EMEVD_FORMAT_DARK_SOULS_1);
}

static void test_writes_dark_souls_1_be_variant(void) {
    assert_minimal_roundtrip(SF_EMEVD_FORMAT_DARK_SOULS_1_BE);
}

static void test_writes_bloodborne_variant(void) {
    assert_minimal_roundtrip(SF_EMEVD_FORMAT_BLOODBORNE);
}

static void test_writes_dark_souls_3_variant(void) {
    assert_minimal_roundtrip(SF_EMEVD_FORMAT_DARK_SOULS_3);
}

static void test_writes_sekiro_variant(void) {
    assert_minimal_roundtrip(SF_EMEVD_FORMAT_SEKIRO);
}

static void test_writes_sekiro_layer(void) {
    sf_emevd_t *emevd = make_emevd(SF_EMEVD_FORMAT_SEKIRO);
    emevd->events[0].instructions[0].has_layer = true;
    emevd->events[0].instructions[0].layer.mask = 0x00000005u;

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_write_to_memory(emevd, &data, &size, NULL));
    sf_emevd_t *readback = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_read_from_memory(&readback, data, size, NULL));
    const sf_emevd_event_t *event = sf_emevd_get_event(readback, 0);
    const sf_emevd_instruction_t *instr = sf_emevd_event_get_instruction(event, 0);
    const sf_emevd_layer_t *layer = sf_emevd_instruction_get_layer(instr);
    TEST_ASSERT_NOT_NULL(layer);
    TEST_ASSERT_EQUAL_UINT32(0x00000005u, sf_emevd_layer_get_mask(layer));

    sf_emevd_destroy(readback, NULL);
    sf_free(NULL, data);
    sf_emevd_destroy(emevd, NULL);
}

static void test_writes_ds3_parameter_and_linked_file(void) {
    sf_emevd_t *emevd = make_emevd(SF_EMEVD_FORMAT_DARK_SOULS_3);
    emevd->events[0].parameter_count = 1;
    emevd->events[0].parameters = (sf_emevd_parameter_t *)sf_xalloc(
        emevd->alloc, sizeof(*emevd->events[0].parameters));
    TEST_ASSERT_NOT_NULL(emevd->events[0].parameters);
    emevd->events[0].parameters[0] = (sf_emevd_parameter_t){
        .instruction_index = 0,
        .target_start_byte = 1,
        .source_start_byte = 2,
        .byte_count = 4,
        .unk_id = 99,
    };
    emevd->linked_file_count = 1;
    emevd->linked_file_offsets = (int64_t *)sf_xalloc(emevd->alloc,
                                                      sizeof(*emevd->linked_file_offsets));
    TEST_ASSERT_NOT_NULL(emevd->linked_file_offsets);
    emevd->linked_file_offsets[0] = 0x1234;

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_write_to_memory(emevd, &data, &size, NULL));
    sf_emevd_t *readback = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_read_from_memory(&readback, data, size, NULL));

    const sf_emevd_event_t *event = sf_emevd_get_event(readback, 0);
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_event_get_parameter_count(event));
    const sf_emevd_parameter_t *param = sf_emevd_event_get_parameter(event, 0);
    TEST_ASSERT_NOT_NULL(param);
    TEST_ASSERT_EQUAL_INT64(0, sf_emevd_parameter_get_instruction_index(param));
    TEST_ASSERT_EQUAL_INT64(1, sf_emevd_parameter_get_target_start_byte(param));
    TEST_ASSERT_EQUAL_INT64(2, sf_emevd_parameter_get_source_start_byte(param));
    TEST_ASSERT_EQUAL_INT32(4, sf_emevd_parameter_get_byte_count(param));
    TEST_ASSERT_EQUAL_INT32(99, sf_emevd_parameter_get_unk_id(param));
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_get_linked_file_count(readback));
    TEST_ASSERT_EQUAL_INT64(0x1234, sf_emevd_get_linked_file_offset(readback, 0));

    sf_emevd_destroy(readback, NULL);
    sf_free(NULL, data);
    sf_emevd_destroy(emevd, NULL);
}

static void test_write_rejects_invalid_format(void) {
    sf_emevd_t emevd;
    memset(&emevd, 0, sizeof(emevd));
    emevd.format = (sf_emevd_format_t)99;
    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_ERR_UNSUPPORTED_VERSION,
                          sf_emevd_write_to_memory(&emevd, &data, &size, NULL));
    TEST_ASSERT_NULL(data);
    TEST_ASSERT_EQUAL_UINT64(0, size);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_writes_dark_souls_1_variant);
    RUN_TEST(test_writes_dark_souls_1_be_variant);
    RUN_TEST(test_writes_bloodborne_variant);
    RUN_TEST(test_writes_dark_souls_3_variant);
    RUN_TEST(test_writes_sekiro_variant);
    RUN_TEST(test_writes_sekiro_layer);
    RUN_TEST(test_writes_ds3_parameter_and_linked_file);
    RUN_TEST(test_write_rejects_invalid_format);
    return UNITY_END();
}
