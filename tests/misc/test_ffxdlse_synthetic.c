/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_ffxdlse.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_ffxdlse_create_destroy(void) {
    sf_ffxdlse_t *ffx = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_create(&ffx, NULL));
    TEST_ASSERT_NOT_NULL(ffx);

    sf_ffxdlse_effect_t *e = sf_ffxdlse_effect(ffx);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_INT32(0, sf_ffxdlse_effect_id(e));

    sf_ffxdlse_destroy(ffx);
}

static void test_ffxdlse_is_function(void) {
    const uint8_t valid[]   = { 'D', 'L', 's', 'E', 0, 0, 0, 0 };
    const uint8_t bad[]     = { 'd', 'L', 's', 'E', 0, 0, 0, 0 };
    const uint8_t too_short[] = { 'D', 'L', 's' };

    TEST_ASSERT_TRUE (sf_ffxdlse_is(valid, sizeof(valid)));
    TEST_ASSERT_FALSE(sf_ffxdlse_is(bad,   sizeof(bad)));
    TEST_ASSERT_FALSE(sf_ffxdlse_is(too_short, sizeof(too_short)));
    TEST_ASSERT_FALSE(sf_ffxdlse_is(NULL, 0));
}

static void test_ffxdlse_effect_accessors(void) {
    sf_ffxdlse_t *ffx = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_create(&ffx, NULL));

    sf_ffxdlse_effect_t *e = sf_ffxdlse_effect(ffx);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_INT32(0, sf_ffxdlse_effect_id(e));

    sf_ffxdlse_effect_set_id(e, 42);
    TEST_ASSERT_EQUAL_INT32(42, sf_ffxdlse_effect_id(e));

    TEST_ASSERT_NOT_NULL(sf_ffxdlse_effect_param_list1(e));
    TEST_ASSERT_NOT_NULL(sf_ffxdlse_effect_param_list2(e));
    TEST_ASSERT_NOT_NULL(sf_ffxdlse_effect_state_map(e));
    TEST_ASSERT_NOT_NULL(sf_ffxdlse_effect_resource_set(e));

    sf_ffxdlse_destroy(ffx);
}

static void test_ffxdlse_state_map_add(void) {
    sf_ffxdlse_t *ffx = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_create(&ffx, NULL));

    sf_ffxdlse_effect_t   *e  = sf_ffxdlse_effect(ffx);
    sf_ffxdlse_state_map_t *sm = sf_ffxdlse_effect_state_map(e);
    TEST_ASSERT_NOT_NULL(sm);
    TEST_ASSERT_EQUAL_size_t(0, sf_ffxdlse_state_map_count(sm));

    sf_ffxdlse_state_t *s = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_state_map_add(sm, &s));
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_size_t(1, sf_ffxdlse_state_map_count(sm));
    TEST_ASSERT_EQUAL_PTR(s, sf_ffxdlse_state_map_at(sm, 0));

    TEST_ASSERT_EQUAL_size_t(0, sf_ffxdlse_state_action_count(s));
    TEST_ASSERT_EQUAL_size_t(0, sf_ffxdlse_state_trigger_count(s));

    sf_ffxdlse_destroy(ffx);
}

static void test_ffxdlse_state_add_action_trigger(void) {
    sf_ffxdlse_t *ffx = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_create(&ffx, NULL));

    sf_ffxdlse_effect_t    *e  = sf_ffxdlse_effect(ffx);
    sf_ffxdlse_state_map_t *sm = sf_ffxdlse_effect_state_map(e);

    sf_ffxdlse_state_t *s = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_state_map_add(sm, &s));

    sf_ffxdlse_action_t *act = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_state_add_action(s, &act));
    TEST_ASSERT_NOT_NULL(act);
    TEST_ASSERT_EQUAL_size_t(1, sf_ffxdlse_state_action_count(s));
    sf_ffxdlse_action_set_id(act, 7);
    TEST_ASSERT_EQUAL_INT32(7, sf_ffxdlse_action_id(act));
    TEST_ASSERT_NOT_NULL(sf_ffxdlse_action_param_list(act));

    sf_ffxdlse_trigger_t *trg = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_state_add_trigger(s, &trg));
    TEST_ASSERT_NOT_NULL(trg);
    TEST_ASSERT_EQUAL_size_t(1, sf_ffxdlse_state_trigger_count(s));
    sf_ffxdlse_trigger_set_state_index(trg, 0);
    TEST_ASSERT_EQUAL_INT32(0, sf_ffxdlse_trigger_state_index(trg));

    sf_ffxdlse_evaluatable_t *ev = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_ffxdlse_evaluatable_create_constant(&ev, 100, NULL));
    TEST_ASSERT_NOT_NULL(ev);
    TEST_ASSERT_EQUAL_INT(SF_FFXDLSE_EVAL_CONSTANT,
        sf_ffxdlse_evaluatable_opcode(ev));

    int32_t cval = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_ffxdlse_evaluatable_constant_value(ev, &cval));
    TEST_ASSERT_EQUAL_INT32(100, cval);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_trigger_set_evaluator(trg, ev));
    /* Trigger now owns ev. */
    TEST_ASSERT_EQUAL_PTR(ev, sf_ffxdlse_trigger_evaluator(trg));

    sf_ffxdlse_destroy(ffx);
}

static void test_ffxdlse_round_trip(void) {
    sf_ffxdlse_t *ffx = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_create(&ffx, NULL));

    sf_ffxdlse_effect_t *e = sf_ffxdlse_effect(ffx);
    sf_ffxdlse_effect_set_id(e, 42);

    sf_ffxdlse_state_map_t *sm = sf_ffxdlse_effect_state_map(e);
    sf_ffxdlse_state_t *s = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_state_map_add(sm, &s));

    sf_ffxdlse_action_t *act = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_state_add_action(s, &act));
    sf_ffxdlse_action_set_id(act, 7);

    sf_ffxdlse_trigger_t *trg = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_state_add_trigger(s, &trg));
    sf_ffxdlse_trigger_set_state_index(trg, 0);

    sf_ffxdlse_evaluatable_t *ev = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_ffxdlse_evaluatable_create_constant(&ev, 100, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_trigger_set_evaluator(trg, ev));

    void  *bytes = NULL;
    size_t size  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_write_to_memory(ffx, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_TRUE(size >= 4);
    TEST_ASSERT_TRUE(sf_ffxdlse_is(bytes, size));

    sf_ffxdlse_t *parsed = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_read_from_memory(&parsed, bytes, size, NULL));
    TEST_ASSERT_NOT_NULL(parsed);

    sf_ffxdlse_effect_t *pe = sf_ffxdlse_effect(parsed);
    TEST_ASSERT_NOT_NULL(pe);
    TEST_ASSERT_EQUAL_INT32(42, sf_ffxdlse_effect_id(pe));

    sf_ffxdlse_state_map_t *psm = sf_ffxdlse_effect_state_map(pe);
    TEST_ASSERT_EQUAL_size_t(1, sf_ffxdlse_state_map_count(psm));

    sf_ffxdlse_state_t *ps = sf_ffxdlse_state_map_at(psm, 0);
    TEST_ASSERT_NOT_NULL(ps);
    TEST_ASSERT_EQUAL_size_t(1, sf_ffxdlse_state_action_count(ps));
    TEST_ASSERT_EQUAL_size_t(1, sf_ffxdlse_state_trigger_count(ps));

    sf_ffxdlse_action_t *pact = sf_ffxdlse_state_action_at(ps, 0);
    TEST_ASSERT_NOT_NULL(pact);
    TEST_ASSERT_EQUAL_INT32(7, sf_ffxdlse_action_id(pact));

    sf_ffxdlse_trigger_t *ptrg = sf_ffxdlse_state_trigger_at(ps, 0);
    TEST_ASSERT_NOT_NULL(ptrg);
    TEST_ASSERT_EQUAL_INT32(0, sf_ffxdlse_trigger_state_index(ptrg));

    sf_ffxdlse_evaluatable_t *pev = sf_ffxdlse_trigger_evaluator(ptrg);
    TEST_ASSERT_NOT_NULL(pev);
    TEST_ASSERT_EQUAL_INT(SF_FFXDLSE_EVAL_CONSTANT, sf_ffxdlse_evaluatable_opcode(pev));
    int32_t pcval = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_evaluatable_constant_value(pev, &pcval));
    TEST_ASSERT_EQUAL_INT32(100, pcval);

    sf_ffxdlse_destroy(parsed);
    sf_free(NULL, bytes);
    sf_ffxdlse_destroy(ffx);
}

static void test_ffxdlse_resource_set(void) {
    sf_ffxdlse_t *ffx = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ffxdlse_create(&ffx, NULL));

    sf_ffxdlse_effect_t       *e  = sf_ffxdlse_effect(ffx);
    sf_ffxdlse_resource_set_t *rs = sf_ffxdlse_effect_resource_set(e);
    TEST_ASSERT_NOT_NULL(rs);

    for (int v = 0; v < SF_FFXDLSE_RES_VECTOR_COUNT; ++v) {
        size_t c = 999;
        TEST_ASSERT_EQUAL_INT(SF_OK,
            sf_ffxdlse_resource_set_count(rs, (sf_ffxdlse_resource_vector_t)v, &c));
        TEST_ASSERT_EQUAL_size_t(0, c);
    }

    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_ffxdlse_resource_set_add(rs, SF_FFXDLSE_RES_VECTOR1, 11));
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_ffxdlse_resource_set_add(rs, SF_FFXDLSE_RES_VECTOR1, 22));
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_ffxdlse_resource_set_add(rs, SF_FFXDLSE_RES_VECTOR3, 33));

    size_t c = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_ffxdlse_resource_set_count(rs, SF_FFXDLSE_RES_VECTOR1, &c));
    TEST_ASSERT_EQUAL_size_t(2, c);

    int32_t val = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_ffxdlse_resource_set_at(rs, SF_FFXDLSE_RES_VECTOR1, 0, &val));
    TEST_ASSERT_EQUAL_INT32(11, val);
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_ffxdlse_resource_set_at(rs, SF_FFXDLSE_RES_VECTOR1, 1, &val));
    TEST_ASSERT_EQUAL_INT32(22, val);

    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_ffxdlse_resource_set_count(rs, SF_FFXDLSE_RES_VECTOR3, &c));
    TEST_ASSERT_EQUAL_size_t(1, c);
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_ffxdlse_resource_set_at(rs, SF_FFXDLSE_RES_VECTOR3, 0, &val));
    TEST_ASSERT_EQUAL_INT32(33, val);

    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_ffxdlse_resource_set_count(rs, SF_FFXDLSE_RES_VECTOR2, &c));
    TEST_ASSERT_EQUAL_size_t(0, c);

    sf_ffxdlse_destroy(ffx);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ffxdlse_create_destroy);
    RUN_TEST(test_ffxdlse_is_function);
    RUN_TEST(test_ffxdlse_effect_accessors);
    RUN_TEST(test_ffxdlse_state_map_add);
    RUN_TEST(test_ffxdlse_state_add_action_trigger);
    RUN_TEST(test_ffxdlse_round_trip);
    RUN_TEST(test_ffxdlse_resource_set);
    return UNITY_END();
}
