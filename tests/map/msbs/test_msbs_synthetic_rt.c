/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "map/msbs/msbs_internal.h"

#include "souls_formats/sf_msbs.h"

#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_msbs_synthetic_roundtrip(void) {
    sf_msbs_model_t models[1];
    sf_msbs_part_t parts[1];
    memset(models, 0, sizeof(models));
    memset(parts, 0, sizeof(parts));

    models[0].data.kind = SF_MSB_MODEL_MAP_PIECE;
    models[0].data.name = (char *)"m100000";
    models[0].data.sib_path = (char *)"N:\\m100000.sib";
    models[0].data.instance_count = 1;

    parts[0].data.type = MSBS_PART_MAP_PIECE;
    parts[0].data.name = (char *)"m100000";
    parts[0].data.model_index = 0;
    parts[0].data.sib_path = (char *)"N:\\m100000.sib";
    parts[0].data.position = (sf_vec3_t){ 1.0f, 2.0f, 3.0f };
    parts[0].data.rotation = (sf_vec3_t){ 4.0f, 5.0f, 6.0f };
    parts[0].data.scale = (sf_vec3_t){ 1.0f, 1.0f, 1.0f };
    parts[0].data.entity_id = 1000;

    sf_msbs_t msbs;
    memset(&msbs, 0, sizeof(msbs));
    msbs.models = models;
    msbs.model_count = 1;
    msbs.parts = parts;
    msbs.part_count = 1;

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbs_write_to_memory(&msbs, &data, &size, NULL));

    sf_msbs_t *read_msbs = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbs_read_from_memory(&read_msbs, data, size, NULL));
    TEST_ASSERT_EQUAL_INT32(1, sf_msbs_model_count(read_msbs));
    TEST_ASSERT_EQUAL_INT32(1, sf_msbs_part_count(read_msbs));

    const sf_msbs_model_t *read_model = sf_msbs_model_at(read_msbs, 0);
    const sf_msbs_part_t *read_part = sf_msbs_part_at(read_msbs, 0);
    TEST_ASSERT_NOT_NULL(read_model);
    TEST_ASSERT_NOT_NULL(read_part);
    TEST_ASSERT_EQUAL(SF_MSB_MODEL_MAP_PIECE, read_model->data.kind);
    TEST_ASSERT_EQUAL_STRING("m100000", read_model->data.name);
    TEST_ASSERT_EQUAL_STRING("m100000", read_part->data.name);
    TEST_ASSERT_EQUAL_INT32(0, read_part->data.model_index);

    sf_msbs_destroy(read_msbs);
    sf_free(NULL, data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_msbs_synthetic_roundtrip);
    return UNITY_END();
}
