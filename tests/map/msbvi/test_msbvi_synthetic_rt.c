/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "map/msbvi/msbvi_internal.h"

#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_msbvi_synthetic_roundtrip(void) {
    sf_msbvi_t msbvi;
    memset(&msbvi, 0, sizeof(msbvi));

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbvi_write_to_memory(&msbvi, &data, &size, NULL));

    sf_msbvi_t *read_msbvi = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbvi_read_from_memory(&read_msbvi, data, size, NULL));
    TEST_ASSERT_EQUAL_INT32(0, sf_msbvi_model_count(read_msbvi));
    TEST_ASSERT_EQUAL_INT32(0, sf_msbvi_part_count(read_msbvi));
    TEST_ASSERT_EQUAL_INT32(0, sf_msbvi_layer_count(read_msbvi));
    TEST_ASSERT_NULL(sf_msbvi_model_at(read_msbvi, 0));
    TEST_ASSERT_NULL(sf_msbvi_part_at(read_msbvi, 0));
    TEST_ASSERT_NULL(sf_msbvi_layer_at(read_msbvi, 0));

    sf_msbvi_destroy(read_msbvi);
    sf_free(NULL, data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_msbvi_synthetic_roundtrip);
    return UNITY_END();
}
