/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_mqb.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_mqb_is_function(void) {
    const uint8_t valid[] = { 'M', 'Q', 'B', ' ', 0, 0, 0, 0 };
    const uint8_t invalid[] = { 'M', 'Q', 'B', 0, 0, 0, 0, 0 };

    TEST_ASSERT_TRUE(sf_mqb_is(valid, sizeof(valid)));
    TEST_ASSERT_FALSE(sf_mqb_is(invalid, sizeof(invalid)));
    TEST_ASSERT_FALSE(sf_mqb_is(valid, 3));
    TEST_ASSERT_FALSE(sf_mqb_is(NULL, 0));
}

static void test_mqb_round_trip_minimal_ds3(void) {
    sf_mqb_t *m = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_create(&m, SF_MQB_VERSION_DARK_SOULS_3, false, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_set_name(m, "minimal"));
    sf_mqb_set_framerate(m, 60.0f);

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_write_to_memory(m, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_TRUE(size > 0);
    TEST_ASSERT_TRUE(sf_mqb_is(bytes, size));

    sf_mqb_t *n = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_read_from_memory(&n, bytes, size, NULL));
    TEST_ASSERT_EQUAL_INT(SF_MQB_VERSION_DARK_SOULS_3, sf_mqb_version(n));
    TEST_ASSERT_FALSE(sf_mqb_big_endian(n));
    TEST_ASSERT_EQUAL_STRING("minimal", sf_mqb_name(n));
    TEST_ASSERT_EQUAL_FLOAT(60.0f, sf_mqb_framerate(n));
    TEST_ASSERT_EQUAL_size_t(0, sf_mqb_resource_count(n));
    TEST_ASSERT_EQUAL_size_t(0, sf_mqb_cut_count(n));

    sf_mqb_destroy(n);
    sf_free(NULL, bytes);
    sf_mqb_destroy(m);
}

static void test_mqb_round_trip_resource_cut_event_int_parameter(void) {
    sf_mqb_t *m = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_create(&m, SF_MQB_VERSION_DARK_SOULS_3, false, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_set_name(m, "cutscene"));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_set_resource_directory(m, "N:\\movie"));

    sf_mqb_resource_t *res = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_add_resource(m, &res));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_resource_set_name(res, "cam"));
    sf_mqb_resource_set_parent_index(res, -1);
    sf_mqb_resource_set_unk48(res, 7);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_resource_set_path(res, "camera.hkx"));

    sf_mqb_cut_t *cut = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_add_cut(m, &cut));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_cut_set_name(cut, "cut0"));
    sf_mqb_cut_set_unk44(cut, 2);
    sf_mqb_cut_set_duration(cut, 120);

    sf_mqb_timeline_t *tl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_cut_add_timeline(cut, &tl));
    sf_mqb_timeline_set_unk10(tl, 3);

    sf_mqb_event_t *ev = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_timeline_add_event(tl, &ev));
    sf_mqb_event_set_id(ev, 99);
    sf_mqb_event_set_resource_index(ev, 0);
    sf_mqb_event_set_start_frame(ev, 10);
    sf_mqb_event_set_duration(ev, 20);

    sf_mqb_parameter_t *param = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_event_add_parameter(ev, &param));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_parameter_set_name(param, "count"));
    sf_mqb_parameter_set_member_count(param, 1);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_parameter_set_int(param, 1234));

    void *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_write_to_memory(m, &bytes, &size, NULL));
    TEST_ASSERT_TRUE(sf_mqb_is(bytes, size));

    sf_mqb_t *n = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_read_from_memory(&n, bytes, size, NULL));
    TEST_ASSERT_EQUAL_STRING("cutscene", sf_mqb_name(n));
    TEST_ASSERT_EQUAL_STRING("N:\\movie", sf_mqb_resource_directory(n));
    TEST_ASSERT_EQUAL_size_t(1, sf_mqb_resource_count(n));
    TEST_ASSERT_EQUAL_size_t(1, sf_mqb_cut_count(n));

    sf_mqb_resource_t *rr = sf_mqb_resource_at(n, 0);
    TEST_ASSERT_EQUAL_STRING("cam", sf_mqb_resource_name(rr));
    TEST_ASSERT_EQUAL_INT(-1, sf_mqb_resource_parent_index(rr));
    TEST_ASSERT_EQUAL_INT(7, sf_mqb_resource_unk48(rr));
    TEST_ASSERT_EQUAL_STRING("camera.hkx", sf_mqb_resource_path(rr));

    sf_mqb_cut_t *cc = sf_mqb_cut_at(n, 0);
    TEST_ASSERT_EQUAL_STRING("cut0", sf_mqb_cut_name(cc));
    TEST_ASSERT_EQUAL_INT(120, sf_mqb_cut_duration(cc));
    TEST_ASSERT_EQUAL_size_t(1, sf_mqb_cut_timeline_count(cc));

    sf_mqb_timeline_t *tt = sf_mqb_cut_timeline_at(cc, 0);
    TEST_ASSERT_EQUAL_INT(3, sf_mqb_timeline_unk10(tt));
    TEST_ASSERT_EQUAL_size_t(1, sf_mqb_timeline_event_count(tt));

    sf_mqb_event_t *ee = sf_mqb_timeline_event_at(tt, 0);
    TEST_ASSERT_EQUAL_INT(99, sf_mqb_event_id(ee));
    TEST_ASSERT_EQUAL_INT(0, sf_mqb_event_resource_index(ee));
    TEST_ASSERT_EQUAL_INT(10, sf_mqb_event_start_frame(ee));
    TEST_ASSERT_EQUAL_INT(20, sf_mqb_event_duration(ee));
    TEST_ASSERT_EQUAL_size_t(1, sf_mqb_event_parameter_count(ee));

    sf_mqb_parameter_t *pp = sf_mqb_event_parameter_at(ee, 0);
    TEST_ASSERT_EQUAL_STRING("count", sf_mqb_parameter_name(pp));
    TEST_ASSERT_EQUAL_INT(SF_MQB_PARAM_TYPE_INT, sf_mqb_parameter_type(pp));
    TEST_ASSERT_EQUAL_INT(1, sf_mqb_parameter_member_count(pp));
    int32_t value = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mqb_parameter_get_int(pp, &value));
    TEST_ASSERT_EQUAL_INT(1234, value);

    sf_mqb_destroy(n);
    sf_free(NULL, bytes);
    sf_mqb_destroy(m);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mqb_is_function);
    RUN_TEST(test_mqb_round_trip_minimal_ds3);
    RUN_TEST(test_mqb_round_trip_resource_cut_event_int_parameter);
    return UNITY_END();
}
