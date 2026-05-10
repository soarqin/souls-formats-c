/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 4 T4.2 — Integrated synthetic round-trip tests for EMEVD.
 *
 * Pattern: build EMEVD in-memory via internal headers (Sekiro format),
 * write → read → assert → write again → memcmp. Same self-consistency
 * contract as tests/archive/test_bnd4_synthetic.c.
 *
 * Two fixtures cover the public surface promised by Phase 4:
 *   1. Basic Sekiro EMEVD with 1 event × 1 instruction.
 *   2. Sekiro EMEVD with an attached layer and an attached parameter.
 */

#include "script/emevd_internal.h" /* IWYU pragma: keep — internal struct access */

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static sf_emevd_t *build_sekiro_with_one_event(void) {
    sf_emevd_t *emevd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_emevd_create(NULL, SF_EMEVD_FORMAT_SEKIRO, &emevd));
    TEST_ASSERT_NOT_NULL(emevd);

    emevd->event_count = 1;
    emevd->events = (sf_emevd_event_t *)sf_xalloc(emevd->alloc, sizeof(*emevd->events));
    TEST_ASSERT_NOT_NULL(emevd->events);
    memset(emevd->events, 0, sizeof(*emevd->events));

    sf_emevd_event_t *event = &emevd->events[0];
    event->id = 1000;
    event->rest_behavior = SF_EMEVD_REST_BEHAVIOR_RESTART;

    event->instruction_count = 1;
    event->instructions = (sf_emevd_instruction_t *)sf_xalloc(
        emevd->alloc, sizeof(*event->instructions));
    TEST_ASSERT_NOT_NULL(event->instructions);
    memset(event->instructions, 0, sizeof(*event->instructions));
    event->instructions[0].bank = 2000;
    event->instructions[0].id   = 3000;
    return emevd;
}

static void test_emevd_basic_sekiro_roundtrip(void) {
    sf_emevd_t *emevd = build_sekiro_with_one_event();

    uint8_t *write1 = NULL;
    size_t size1 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_emevd_write_to_memory(emevd, &write1, &size1, NULL));
    TEST_ASSERT_NOT_NULL(write1);
    TEST_ASSERT_GREATER_THAN_UINT64(0, size1);

    sf_emevd_t *rebound = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_emevd_read_from_memory(&rebound, write1, size1, NULL));
    TEST_ASSERT_NOT_NULL(rebound);
    TEST_ASSERT_EQUAL_INT(SF_EMEVD_FORMAT_SEKIRO, sf_emevd_get_format(rebound));
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_get_event_count(rebound));

    const sf_emevd_event_t *event = sf_emevd_get_event(rebound, 0);
    TEST_ASSERT_NOT_NULL(event);
    TEST_ASSERT_EQUAL_INT64(1000, sf_emevd_event_get_id(event));
    TEST_ASSERT_EQUAL_INT(SF_EMEVD_REST_BEHAVIOR_RESTART,
                          sf_emevd_event_get_rest_behavior(event));
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_event_get_instruction_count(event));

    const sf_emevd_instruction_t *instr = sf_emevd_event_get_instruction(event, 0);
    TEST_ASSERT_NOT_NULL(instr);
    TEST_ASSERT_EQUAL_INT32(2000, sf_emevd_instruction_get_bank(instr));
    TEST_ASSERT_EQUAL_INT32(3000, sf_emevd_instruction_get_id(instr));
    TEST_ASSERT_NULL(sf_emevd_instruction_get_layer(instr));

    uint8_t *write2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_emevd_write_to_memory(rebound, &write2, &size2, NULL));
    TEST_ASSERT_EQUAL_UINT64(size1, size2);
    TEST_ASSERT_EQUAL_MEMORY(write1, write2, size1);

    sf_free(NULL, write2);
    sf_emevd_destroy(rebound, NULL);
    sf_free(NULL, write1);
    sf_emevd_destroy(emevd, NULL);
}

static void test_emevd_sekiro_with_layer_and_parameter_roundtrip(void) {
    sf_emevd_t *emevd = build_sekiro_with_one_event();

    emevd->events[0].instructions[0].has_layer  = true;
    emevd->events[0].instructions[0].layer.mask = 0x0000000Au;

    emevd->events[0].parameter_count = 1;
    emevd->events[0].parameters = (sf_emevd_parameter_t *)sf_xalloc(
        emevd->alloc, sizeof(*emevd->events[0].parameters));
    TEST_ASSERT_NOT_NULL(emevd->events[0].parameters);
    emevd->events[0].parameters[0] = (sf_emevd_parameter_t){
        .instruction_index  = 0,
        .target_start_byte  = 4,
        .source_start_byte  = 8,
        .byte_count         = 4,
        .unk_id             = 77,
    };

    uint8_t *write1 = NULL;
    size_t size1 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_emevd_write_to_memory(emevd, &write1, &size1, NULL));
    TEST_ASSERT_NOT_NULL(write1);
    TEST_ASSERT_GREATER_THAN_UINT64(0, size1);

    sf_emevd_t *rebound = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_emevd_read_from_memory(&rebound, write1, size1, NULL));
    TEST_ASSERT_NOT_NULL(rebound);
    TEST_ASSERT_EQUAL_INT(SF_EMEVD_FORMAT_SEKIRO, sf_emevd_get_format(rebound));

    const sf_emevd_event_t *event = sf_emevd_get_event(rebound, 0);
    TEST_ASSERT_NOT_NULL(event);
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_event_get_parameter_count(event));

    const sf_emevd_instruction_t *instr = sf_emevd_event_get_instruction(event, 0);
    const sf_emevd_layer_t *layer = sf_emevd_instruction_get_layer(instr);
    TEST_ASSERT_NOT_NULL(layer);
    TEST_ASSERT_EQUAL_UINT32(0x0000000Au, sf_emevd_layer_get_mask(layer));

    const sf_emevd_parameter_t *param = sf_emevd_event_get_parameter(event, 0);
    TEST_ASSERT_NOT_NULL(param);
    TEST_ASSERT_EQUAL_INT64(0, sf_emevd_parameter_get_instruction_index(param));
    TEST_ASSERT_EQUAL_INT64(4, sf_emevd_parameter_get_target_start_byte(param));
    TEST_ASSERT_EQUAL_INT64(8, sf_emevd_parameter_get_source_start_byte(param));
    TEST_ASSERT_EQUAL_INT32(4, sf_emevd_parameter_get_byte_count(param));
    TEST_ASSERT_EQUAL_INT32(77, sf_emevd_parameter_get_unk_id(param));

    uint8_t *write2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_emevd_write_to_memory(rebound, &write2, &size2, NULL));
    TEST_ASSERT_EQUAL_UINT64(size1, size2);
    TEST_ASSERT_EQUAL_MEMORY(write1, write2, size1);

    sf_free(NULL, write2);
    sf_emevd_destroy(rebound, NULL);
    sf_free(NULL, write1);
    sf_emevd_destroy(emevd, NULL);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_emevd_basic_sekiro_roundtrip);
    RUN_TEST(test_emevd_sekiro_with_layer_and_parameter_roundtrip);
    return UNITY_END();
}
