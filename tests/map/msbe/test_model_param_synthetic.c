/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "map/msbe/msbe_internal.h"

#include "souls_formats/sf_msbe.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_msbe_model_param_roundtrip(void) {
    sf_msbe_model_t models[2];
    memset(models, 0, sizeof(models));
    models[0].data.type = 0;
    models[0].data.name = "m10_00_00_00";
    models[0].data.sib_path = "N:\\dummy.sib";
    models[1].data.type = 10;
    models[1].data.name = "AEG099_000";
    models[1].data.sib_path = "";

    sf_msbe_t msbe;
    memset(&msbe, 0, sizeof(msbe));
    msbe.models = models;
    msbe.model_count = 2;

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_write_to_memory(&msbe, &data, &size, NULL));

    sf_msbe_t *read_msbe = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_read_from_memory(&read_msbe, data, size, NULL));
    TEST_ASSERT_EQUAL_INT32(2, sf_msbe_model_count(read_msbe));
    TEST_ASSERT_EQUAL_STRING("m10_00_00_00", read_msbe->models[0].data.name);
    TEST_ASSERT_EQUAL_UINT32(10, read_msbe->models[1].data.type);

    sf_msbe_destroy(read_msbe);
    sf_free(NULL, data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_msbe_model_param_roundtrip);
    return UNITY_END();
}
