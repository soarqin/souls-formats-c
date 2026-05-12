/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "map/msbv/msbv_internal.h"

#include "souls_formats/sf_msbv.h"

#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_msbv_synthetic_roundtrip(void) {
    sf_msbv_model_t models[1];
    sf_msbv_event_t events[1];
    sf_msbv_region_t regions[1];
    sf_msbv_part_t parts[1];
    memset(models, 0, sizeof(models));
    memset(events, 0, sizeof(events));
    memset(regions, 0, sizeof(regions));
    memset(parts, 0, sizeof(parts));

    models[0].data.kind = SF_MSB_MODEL_MAP_PIECE;
    models[0].data.name = (char *)"m0000";
    models[0].data.resource_path = (char *)"N:\\m0000.sib";
    models[0].data.instance_count = 1;

    events[0].data.type = MSBV_EVENT_SCRIPT;
    events[0].data.name = (char *)"m0000";
    events[0].data.unique_id = 10;

    regions[0].data.type = MSBV_REGION_SPAWN;
    regions[0].data.shape_type = MSBV_REGION_SHAPE_POINT;
    regions[0].data.name = (char *)"Spawn 0";
    regions[0].data.position = (sf_vec3_t){ 1.0f, 2.0f, 3.0f };
    regions[0].data.rotation = (sf_vec3_t){ 4.0f, 5.0f, 6.0f };
    regions[0].data.unique_id = 100;
    regions[0].data.point_id = 300;

    parts[0].data.type = MSBV_PART_MAP_PIECE;
    parts[0].data.name = (char *)"m0000_0000";
    parts[0].data.model_index = 0;
    parts[0].data.resource_path = (char *)"N:\\m0000.sib";
    parts[0].data.position = (sf_vec3_t){ 7.0f, 8.0f, 9.0f };
    parts[0].data.rotation = (sf_vec3_t){ 10.0f, 11.0f, 12.0f };
    parts[0].data.scale = (sf_vec3_t){ 1.0f, 1.0f, 1.0f };
    parts[0].data.entity_group_id = -1;
    parts[0].data.entity_id = 400;

    sf_msbv_t msb;
    memset(&msb, 0, sizeof(msb));
    msb.models = models;
    msb.model_count = 1;
    msb.events = events;
    msb.event_count = 1;
    msb.regions = regions;
    msb.region_count = 1;
    msb.parts = parts;
    msb.part_count = 1;

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbv_write_to_memory(&msb, &data, &size, NULL));

    sf_msbv_t *read_msb = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbv_read_from_memory(&read_msb, data, size, NULL));
    TEST_ASSERT_EQUAL_INT32(1, sf_msbv_model_count(read_msb));
    TEST_ASSERT_EQUAL_INT32(1, sf_msbv_event_count(read_msb));
    TEST_ASSERT_EQUAL_INT32(1, sf_msbv_region_count(read_msb));
    TEST_ASSERT_EQUAL_INT32(1, sf_msbv_part_count(read_msb));

    const sf_msbv_model_t *read_model = sf_msbv_model_at(read_msb, 0);
    const sf_msbv_event_t *read_event = sf_msbv_event_at(read_msb, 0);
    const sf_msbv_region_t *read_region = sf_msbv_region_at(read_msb, 0);
    const sf_msbv_part_t *read_part = sf_msbv_part_at(read_msb, 0);
    TEST_ASSERT_NOT_NULL(read_model);
    TEST_ASSERT_NOT_NULL(read_event);
    TEST_ASSERT_NOT_NULL(read_region);
    TEST_ASSERT_NOT_NULL(read_part);
    TEST_ASSERT_EQUAL(SF_MSB_MODEL_MAP_PIECE, read_model->data.kind);
    TEST_ASSERT_EQUAL_STRING("m0000", read_model->data.name);
    TEST_ASSERT_EQUAL(MSBV_EVENT_SCRIPT, read_event->data.type);
    TEST_ASSERT_EQUAL_INT32(10, read_event->data.unique_id);
    TEST_ASSERT_EQUAL(MSBV_REGION_SHAPE_POINT, read_region->data.shape_type);
    TEST_ASSERT_EQUAL_INT32(300, read_region->data.point_id);
    TEST_ASSERT_EQUAL_STRING("m0000_0000", read_part->data.name);
    TEST_ASSERT_EQUAL_INT32(400, read_part->data.entity_id);

    sf_msbv_destroy(read_msb);
    sf_free(NULL, data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_msbv_synthetic_roundtrip);
    return UNITY_END();
}
