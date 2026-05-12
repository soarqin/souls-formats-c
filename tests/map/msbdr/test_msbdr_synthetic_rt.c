/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "map/msbdr/msbdr_internal.h"
#include "souls_formats/sf_msbdr.h"
#include "unity.h"
#include <string.h>
void setUp(void) {}
void tearDown(void) {}
static void test_msbdr_synthetic_roundtrip(void) {
    sf_msbdr_model_t models[1]; sf_msbdr_event_t events[1]; sf_msbdr_region_t regions[1]; sf_msbdr_part_t parts[1];
    memset(models, 0, sizeof(models)); memset(events, 0, sizeof(events)); memset(regions, 0, sizeof(regions)); memset(parts, 0, sizeof(parts));
    models[0].data.kind = SF_MSB_MODEL_MAP_PIECE; models[0].data.name = (char *)"m100000"; models[0].data.sib_path = (char *)"N:\\m100000.sib"; models[0].data.instance_count = 1;
    events[0].data.type = MSBDR_EVENT_LIGHT; events[0].data.name = (char *)"Light 0"; events[0].data.event_id = 10; events[0].data.part_index = 0; events[0].data.region_index = 0; events[0].data.entity_id = 100; events[0].data.type_value0 = 200;
    regions[0].data.type = MSBDR_REGION_LOGIC; regions[0].data.shape_type = MSBDR_REGION_SHAPE_POINT; regions[0].data.name = (char *)"Region 0"; regions[0].data.position = (sf_vec3_t){ 1.0f, 2.0f, 3.0f }; regions[0].data.rotation = (sf_vec3_t){ 4.0f, 5.0f, 6.0f }; regions[0].data.entity_id = 300;
    parts[0].data.type = MSBDR_PART_MAP_PIECE; parts[0].data.name = (char *)"m100000"; parts[0].data.model_index = 0; parts[0].data.sib_path = (char *)"N:\\m100000.sib"; parts[0].data.position = (sf_vec3_t){ 7.0f, 8.0f, 9.0f }; parts[0].data.rotation = (sf_vec3_t){ 10.0f, 11.0f, 12.0f }; parts[0].data.scale = (sf_vec3_t){ 1.0f, 1.0f, 1.0f }; parts[0].data.entity_id = 400;
    sf_msbdr_t msbdr; memset(&msbdr, 0, sizeof(msbdr)); msbdr.models = models; msbdr.model_count = 1; msbdr.events = events; msbdr.event_count = 1; msbdr.regions = regions; msbdr.region_count = 1; msbdr.parts = parts; msbdr.part_count = 1;
    uint8_t *data = NULL; size_t size = 0; TEST_ASSERT_EQUAL(SF_OK, sf_msbdr_write_to_memory(&msbdr, &data, &size, NULL));
    sf_msbdr_t *read_msbdr = NULL; TEST_ASSERT_EQUAL(SF_OK, sf_msbdr_read_from_memory(&read_msbdr, data, size, NULL));
    TEST_ASSERT_EQUAL_INT32(1, sf_msbdr_model_count(read_msbdr)); TEST_ASSERT_EQUAL_INT32(1, sf_msbdr_event_count(read_msbdr)); TEST_ASSERT_EQUAL_INT32(1, sf_msbdr_region_count(read_msbdr)); TEST_ASSERT_EQUAL_INT32(1, sf_msbdr_part_count(read_msbdr)); TEST_ASSERT_EQUAL_INT32(0, sf_msbdr_tree_count(read_msbdr));
    TEST_ASSERT_EQUAL_STRING("m100000", sf_msbdr_model_at(read_msbdr, 0)->data.name); TEST_ASSERT_EQUAL(MSBDR_EVENT_LIGHT, sf_msbdr_event_at(read_msbdr, 0)->data.type); TEST_ASSERT_EQUAL_INT32(300, sf_msbdr_region_at(read_msbdr, 0)->data.entity_id); TEST_ASSERT_EQUAL_INT32(400, sf_msbdr_part_at(read_msbdr, 0)->data.entity_id);
    sf_msbdr_destroy(read_msbdr); sf_free(NULL, data);
}
int main(void) { UNITY_BEGIN(); RUN_TEST(test_msbdr_synthetic_roundtrip); return UNITY_END(); }
