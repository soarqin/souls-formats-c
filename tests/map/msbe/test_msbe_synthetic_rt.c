/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "map/msbe/msbe_internal.h"

#include "souls_formats/sf_msbe.h"

#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_msbe_synthetic_roundtrip(void) {
    sf_msbe_model_t models[1];
    sf_msbe_part_t parts[1];
    memset(models, 0, sizeof(models));
    memset(parts, 0, sizeof(parts));

    models[0].data.type = 0;
    models[0].data.name = (char *)"m10_00_00_00";
    models[0].data.sib_path = (char *)"N:\\dummy.sib";
    models[0].data.instance_count = 1;

    parts[0].data.type = 0;
    parts[0].data.name = (char *)"m10_00_00_00";
    parts[0].data.model_index = 0;
    parts[0].data.other_id = -1;

    sf_msbe_t msbe;
    memset(&msbe, 0, sizeof(msbe));
    msbe.models = models;
    msbe.model_count = 1;
    msbe.parts = parts;
    msbe.part_count = 1;

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_write_to_memory(&msbe, &data, &size, NULL));

    sf_msbe_t *read_msbe = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_read_from_memory(&read_msbe, data, size, NULL));
    TEST_ASSERT_EQUAL_INT32(1, sf_msbe_model_count(read_msbe));
    TEST_ASSERT_EQUAL_INT32(1, sf_msbe_part_count(read_msbe));

    const sf_msbe_model_t *read_model = sf_msbe_model_at(read_msbe, 0);
    const sf_msbe_part_t *read_part = sf_msbe_part_at(read_msbe, 0);
    TEST_ASSERT_NOT_NULL(read_model);
    TEST_ASSERT_NOT_NULL(read_part);
    TEST_ASSERT_EQUAL_UINT32(0, read_model->data.type);
    TEST_ASSERT_EQUAL_STRING("m10_00_00_00", read_model->data.name);
    TEST_ASSERT_EQUAL_STRING("m10_00_00_00", read_part->data.name);
    TEST_ASSERT_EQUAL_INT32(0, read_part->data.model_index);

    sf_msbe_destroy(read_msbe);
    sf_free(NULL, data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_msbe_synthetic_roundtrip);
    return UNITY_END();
}
