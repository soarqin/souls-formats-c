/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "map/msbn/msbn_internal.h"

#include "souls_formats/sf_msbn.h"

#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_msbn_synthetic_roundtrip(void) {
    sf_msbn_model_t models[1];
    sf_msbn_part_t parts[1];
    memset(models, 0, sizeof(models));
    memset(parts, 0, sizeof(parts));

    models[0].data.kind = SF_MSB_MODEL_MAP_PIECE;
    models[0].data.name = (char *)"m00_0000";

    parts[0].data.type = MSBN_PART_MAP_PIECE;
    parts[0].data.name = (char *)"m00_0000_0000";
    parts[0].data.model_index = 0;
    parts[0].data.position = (sf_vec3_t){ 1.0f, 2.0f, 3.0f };
    parts[0].data.rotation = (sf_vec3_t){ 4.0f, 5.0f, 6.0f };
    parts[0].data.scale = (sf_vec3_t){ 1.0f, 1.0f, 1.0f };

    sf_msbn_t msbn;
    memset(&msbn, 0, sizeof(msbn));
    msbn.models = models;
    msbn.model_count = 1;
    msbn.parts = parts;
    msbn.part_count = 1;

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbn_write_to_memory(&msbn, &data, &size, NULL));

    sf_msbn_t *read_msbn = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbn_read_from_memory(&read_msbn, data, size, NULL));
    TEST_ASSERT_EQUAL_INT32(1, sf_msbn_model_count(read_msbn));
    TEST_ASSERT_EQUAL_INT32(1, sf_msbn_part_count(read_msbn));

    const sf_msbn_model_t *read_model = sf_msbn_model_at(read_msbn, 0);
    const sf_msbn_part_t *read_part = sf_msbn_part_at(read_msbn, 0);
    TEST_ASSERT_NOT_NULL(read_model);
    TEST_ASSERT_NOT_NULL(read_part);
    TEST_ASSERT_EQUAL(SF_MSB_MODEL_MAP_PIECE, read_model->data.kind);
    TEST_ASSERT_EQUAL_STRING("m00_0000", read_model->data.name);
    TEST_ASSERT_EQUAL(MSBN_PART_MAP_PIECE, read_part->data.type);
    TEST_ASSERT_EQUAL_STRING("m00_0000_0000", read_part->data.name);
    TEST_ASSERT_EQUAL_INT32(0, read_part->data.model_index);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, read_part->data.position.x);

    sf_msbn_destroy(read_msbn);
    sf_free(NULL, data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_msbn_synthetic_roundtrip);
    return UNITY_END();
}
