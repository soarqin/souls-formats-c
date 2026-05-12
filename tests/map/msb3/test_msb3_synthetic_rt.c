/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "map/msb3/msb3_internal.h"

#include "souls_formats/sf_msb3.h"

#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_msb3_synthetic_roundtrip(void) {
    sf_msb3_model_t models[1];
    sf_msb3_event_t events[1];
    sf_msb3_region_t regions[1];
    sf_msb3_part_t parts[1];
    memset(models, 0, sizeof(models));
    memset(events, 0, sizeof(events));
    memset(regions, 0, sizeof(regions));
    memset(parts, 0, sizeof(parts));

    models[0].data.kind = SF_MSB_MODEL_MAP_PIECE;
    models[0].data.name = (char *)"m30_00_00_00";
    models[0].data.sib_path = (char *)"N:\\m30_00_00_00.sib";
    models[0].data.instance_count = 1;

    events[0].data.type = MSB3_EVENT_TREASURE;
    events[0].data.name = (char *)"Treasure 0";
    events[0].data.event_id = 10;
    events[0].data.part_index = 0;
    events[0].data.region_index = 0;
    events[0].data.entity_id = 100;
    events[0].data.treasure_part_index = 0;
    events[0].data.item_lot1 = 200;

    regions[0].data.type = MSB3_REGION_SPAWN_POINT;
    regions[0].data.shape_type = MSB3_REGION_SHAPE_POINT;
    regions[0].data.name = (char *)"Spawn 0";
    regions[0].data.position = (sf_vec3_t){ 1.0f, 2.0f, 3.0f };
    regions[0].data.rotation = (sf_vec3_t){ 4.0f, 5.0f, 6.0f };
    regions[0].data.entity_id = 300;
    regions[0].data.activation_part_index = -1;
    regions[0].data.type_value0 = -1;

    parts[0].data.type = MSB3_PART_MAP_PIECE;
    parts[0].data.name = (char *)"m30_00_00_00_0000";
    parts[0].data.model_index = 0;
    parts[0].data.sib_path = (char *)"N:\\m30_00_00_00.sib";
    parts[0].data.position = (sf_vec3_t){ 7.0f, 8.0f, 9.0f };
    parts[0].data.rotation = (sf_vec3_t){ 10.0f, 11.0f, 12.0f };
    parts[0].data.scale = (sf_vec3_t){ 1.0f, 1.0f, 1.0f };
    parts[0].data.entity_id = 400;

    sf_msb3_t msb3;
    memset(&msb3, 0, sizeof(msb3));
    msb3.models = models;
    msb3.model_count = 1;
    msb3.events = events;
    msb3.event_count = 1;
    msb3.regions = regions;
    msb3.region_count = 1;
    msb3.parts = parts;
    msb3.part_count = 1;

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msb3_write_to_memory(&msb3, &data, &size, NULL));

    sf_msb3_t *read_msb3 = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msb3_read_from_memory(&read_msb3, data, size, NULL));
    TEST_ASSERT_EQUAL_INT32(1, sf_msb3_model_count(read_msb3));
    TEST_ASSERT_EQUAL_INT32(1, sf_msb3_event_count(read_msb3));
    TEST_ASSERT_EQUAL_INT32(1, sf_msb3_region_count(read_msb3));
    TEST_ASSERT_EQUAL_INT32(1, sf_msb3_part_count(read_msb3));

    const sf_msb3_model_t *read_model = sf_msb3_model_at(read_msb3, 0);
    const sf_msb3_event_t *read_event = sf_msb3_event_at(read_msb3, 0);
    const sf_msb3_region_t *read_region = sf_msb3_region_at(read_msb3, 0);
    const sf_msb3_part_t *read_part = sf_msb3_part_at(read_msb3, 0);
    TEST_ASSERT_NOT_NULL(read_model);
    TEST_ASSERT_NOT_NULL(read_event);
    TEST_ASSERT_NOT_NULL(read_region);
    TEST_ASSERT_NOT_NULL(read_part);
    TEST_ASSERT_EQUAL(SF_MSB_MODEL_MAP_PIECE, read_model->data.kind);
    TEST_ASSERT_EQUAL_STRING("m30_00_00_00", read_model->data.name);
    TEST_ASSERT_EQUAL(MSB3_EVENT_TREASURE, read_event->data.type);
    TEST_ASSERT_EQUAL_INT32(100, read_event->data.entity_id);
    TEST_ASSERT_EQUAL(MSB3_REGION_SHAPE_POINT, read_region->data.shape_type);
    TEST_ASSERT_EQUAL_INT32(300, read_region->data.entity_id);
    TEST_ASSERT_EQUAL_STRING("m30_00_00_00_0000", read_part->data.name);
    TEST_ASSERT_EQUAL_INT32(0, read_part->data.model_index);
    TEST_ASSERT_EQUAL_INT32(400, read_part->data.entity_id);

    sf_msb3_destroy(read_msb3);
    sf_free(NULL, data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_msb3_synthetic_roundtrip);
    return UNITY_END();
}
