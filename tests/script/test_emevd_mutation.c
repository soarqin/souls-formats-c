/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "script/emevd_internal.h" /* IWYU pragma: keep */
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static sf_emevd_t *make_base_emevd(void) {
    sf_emevd_t *emevd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_create(NULL, SF_EMEVD_FORMAT_SEKIRO, &emevd));
    TEST_ASSERT_NOT_NULL(emevd);

    sf_emevd_event_t *event = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_add_event(emevd, 100, SF_EMEVD_REST_BEHAVIOR_RESTART,
                                                    &event));
    TEST_ASSERT_NOT_NULL(event);

    const uint8_t arg[] = {1, 2, 3, 4};
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_insert_instruction(event, 0, 2003, 66, arg,
                                                                  sizeof(arg)));
    event->parameter_count = 2;
    event->parameters = (sf_emevd_parameter_t *)sf_xalloc(emevd->alloc,
        event->parameter_count * sizeof(*event->parameters));
    TEST_ASSERT_NOT_NULL(event->parameters);
    event->parameters[0] = (sf_emevd_parameter_t){0, 0, 0, 1, 10};
    event->parameters[1] = (sf_emevd_parameter_t){0, 2, 2, 1, 11};
    return emevd;
}

static sf_emevd_event_t *only_event(sf_emevd_t *emevd) {
    sf_emevd_event_t *event = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_find_by_id(emevd, 100, &event));
    TEST_ASSERT_NOT_NULL(event);
    return event;
}

static void test_find_by_id_success_and_invalid_arg(void) {
    sf_emevd_t *emevd = make_base_emevd();
    sf_emevd_event_t *event = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_find_by_id(emevd, 100, &event));
    TEST_ASSERT_NOT_NULL(event);
    TEST_ASSERT_EQUAL_INT64(100, sf_emevd_event_get_id(event));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG, sf_emevd_event_find_by_id(NULL, 100, &event));
    sf_emevd_destroy(emevd, NULL);
}

static void test_find_by_id_reports_miss(void) {
    sf_emevd_t *emevd = make_base_emevd();
    sf_emevd_event_t *event = (sf_emevd_event_t *)1;
    TEST_ASSERT_EQUAL_INT(SF_ERR_NOT_FOUND, sf_emevd_event_find_by_id(emevd, 999, &event));
    TEST_ASSERT_NULL(event);
    sf_emevd_destroy(emevd, NULL);
}

static void test_add_event_success_and_invalid_arg(void) {
    sf_emevd_t *emevd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_create(NULL, SF_EMEVD_FORMAT_SEKIRO, &emevd));
    sf_emevd_event_t *event = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_add_event(emevd, 200, SF_EMEVD_REST_BEHAVIOR_END,
                                                    &event));
    TEST_ASSERT_NOT_NULL(event);
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_get_event_count(emevd));
    TEST_ASSERT_EQUAL_INT(SF_ERR_ALREADY_EXISTS,
                          sf_emevd_add_event(emevd, 200, SF_EMEVD_REST_BEHAVIOR_END, &event));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
                          sf_emevd_add_event(NULL, 201, SF_EMEVD_REST_BEHAVIOR_END, &event));
    sf_emevd_destroy(emevd, NULL);
}

static void test_insert_instruction_success_and_invalid_arg(void) {
    sf_emevd_t *emevd = make_base_emevd();
    sf_emevd_event_t *event = only_event(emevd);
    const uint8_t arg[] = {9};
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_insert_instruction(event, 0, 1014, 18, arg,
                                                                  sizeof(arg)));
    TEST_ASSERT_EQUAL_UINT64(2, sf_emevd_event_get_instruction_count(event));
    TEST_ASSERT_EQUAL_INT32(1014, sf_emevd_instruction_get_bank(&event->instructions[0]));
    TEST_ASSERT_EQUAL_INT64(1, sf_emevd_parameter_get_instruction_index(&event->parameters[0]));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
                          sf_emevd_event_insert_instruction(event, 3, 1, 1, arg, sizeof(arg)));
    sf_emevd_destroy(emevd, NULL);
}

static void test_replace_instruction_success_and_invalid_arg(void) {
    sf_emevd_t *emevd = make_base_emevd();
    sf_emevd_event_t *event = only_event(emevd);
    const uint8_t arg[] = {5, 6};
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_replace_instruction(event, 0, 1014, 18, arg,
                                                                   sizeof(arg)));
    TEST_ASSERT_EQUAL_INT32(1014, sf_emevd_instruction_get_bank(&event->instructions[0]));
    TEST_ASSERT_EQUAL_UINT64(sizeof(arg), event->instructions[0].arg_data_size);
    TEST_ASSERT_EQUAL_MEMORY(arg, event->instructions[0].arg_data, sizeof(arg));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
                          sf_emevd_event_replace_instruction(event, 2, 0, 0, arg, sizeof(arg)));
    sf_emevd_destroy(emevd, NULL);
}

static void test_remove_instruction_success_and_invalid_arg(void) {
    sf_emevd_t *emevd = make_base_emevd();
    sf_emevd_event_t *event = only_event(emevd);
    const uint8_t arg[] = {8};
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_insert_instruction(event, 0, 1, 2, arg,
                                                                  sizeof(arg)));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_remove_instruction_at(event, 0));
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_event_get_instruction_count(event));
    TEST_ASSERT_EQUAL_INT64(0, sf_emevd_parameter_get_instruction_index(&event->parameters[0]));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG, sf_emevd_event_remove_instruction_at(event, 1));
    sf_emevd_destroy(emevd, NULL);
}

static void test_set_arg_data_success_and_invalid_arg(void) {
    sf_emevd_t *emevd = make_base_emevd();
    sf_emevd_event_t *event = only_event(emevd);
    const uint8_t arg[] = {4, 3, 2, 1};
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_instruction_set_arg_data(&event->instructions[0], arg,
                                                                  sizeof(arg)));
    TEST_ASSERT_EQUAL_MEMORY(arg, event->instructions[0].arg_data, sizeof(arg));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
                          sf_emevd_instruction_set_arg_data(&event->instructions[0], arg, 2));
    sf_emevd_destroy(emevd, NULL);
}

static void test_clear_parameters_success_and_invalid_arg(void) {
    sf_emevd_t *emevd = make_base_emevd();
    sf_emevd_event_t *event = only_event(emevd);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_clear_parameters(event));
    TEST_ASSERT_EQUAL_UINT64(0, sf_emevd_event_get_parameter_count(event));
    TEST_ASSERT_NULL(event->parameters);
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG, sf_emevd_event_clear_parameters(NULL));
    sf_emevd_destroy(emevd, NULL);
}

static void test_remove_parameter_success_and_invalid_arg(void) {
    sf_emevd_t *emevd = make_base_emevd();
    sf_emevd_event_t *event = only_event(emevd);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_remove_parameter_at(event, 0));
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_event_get_parameter_count(event));
    TEST_ASSERT_EQUAL_INT32(11, sf_emevd_parameter_get_unk_id(&event->parameters[0]));
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG, sf_emevd_event_remove_parameter_at(event, 1));
    sf_emevd_destroy(emevd, NULL);
}

static void test_all_mutations_survive_roundtrip(void) {
    sf_emevd_t *source = make_base_emevd();
    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_write_to_memory(source, &data, &size, NULL));

    sf_emevd_t *emevd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_read_from_memory(&emevd, data, size, NULL));
    sf_free(NULL, data);
    sf_emevd_destroy(source, NULL);

    sf_emevd_event_t *event = only_event(emevd);
    const uint8_t insert_arg[] = {7, 7, 7, 7};
    const uint8_t replace_arg[] = {6, 6};
    const uint8_t set_arg[] = {1, 1};
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_insert_instruction(event, 0, 2000, 0, insert_arg,
                                                                  sizeof(insert_arg)));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_replace_instruction(event, 1, 1014, 18,
                                                                   replace_arg,
                                                                   sizeof(replace_arg)));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_instruction_set_arg_data(&event->instructions[1],
                                                                  set_arg, sizeof(set_arg)));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_remove_parameter_at(event, 0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_remove_instruction_at(event, 0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_clear_parameters(event));

    sf_emevd_event_t *added = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_add_event(emevd, 279551111,
                                                    SF_EMEVD_REST_BEHAVIOR_DEFAULT, &added));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_insert_instruction(added, 0, 2003, 78,
                                                                  insert_arg,
                                                                  sizeof(insert_arg)));

    uint8_t *mutated = NULL;
    size_t mutated_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_write_to_memory(emevd, &mutated, &mutated_size, NULL));
    sf_emevd_t *readback = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_read_from_memory(&readback, mutated, mutated_size, NULL));

    sf_emevd_event_t *read_event = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_find_by_id(readback, 100, &read_event));
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_event_get_instruction_count(read_event));
    TEST_ASSERT_EQUAL_UINT64(0, sf_emevd_event_get_parameter_count(read_event));
    TEST_ASSERT_EQUAL_INT32(1014, sf_emevd_instruction_get_bank(&read_event->instructions[0]));
    TEST_ASSERT_EQUAL_MEMORY(set_arg, read_event->instructions[0].arg_data, sizeof(set_arg));

    sf_emevd_event_t *read_added = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_emevd_event_find_by_id(readback, 279551111, &read_added));
    TEST_ASSERT_EQUAL_UINT64(1, sf_emevd_event_get_instruction_count(read_added));
    TEST_ASSERT_EQUAL_INT32(2003, sf_emevd_instruction_get_bank(&read_added->instructions[0]));
    TEST_ASSERT_EQUAL_INT32(78, sf_emevd_instruction_get_id(&read_added->instructions[0]));

    sf_emevd_destroy(readback, NULL);
    sf_free(NULL, mutated);
    sf_emevd_destroy(emevd, NULL);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_find_by_id_success_and_invalid_arg);
    RUN_TEST(test_find_by_id_reports_miss);
    RUN_TEST(test_add_event_success_and_invalid_arg);
    RUN_TEST(test_insert_instruction_success_and_invalid_arg);
    RUN_TEST(test_replace_instruction_success_and_invalid_arg);
    RUN_TEST(test_remove_instruction_success_and_invalid_arg);
    RUN_TEST(test_set_arg_data_success_and_invalid_arg);
    RUN_TEST(test_clear_parameters_success_and_invalid_arg);
    RUN_TEST(test_remove_parameter_success_and_invalid_arg);
    RUN_TEST(test_all_mutations_survive_roundtrip);
    return UNITY_END();
}
