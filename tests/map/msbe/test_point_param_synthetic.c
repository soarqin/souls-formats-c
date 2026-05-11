/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "map/msbe/msbe_internal.h"

#include "souls_formats/sf_msbe.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_msbe_point_param_roundtrip(void) {
    sf_msbe_region_t regions[2];
    memset(regions, 0, sizeof(regions));
    regions[0].data.type = 1;
    regions[0].data.name = "InvasionPoint_000";
    regions[1].data.type = 18;
    regions[1].data.name = "WindArea_000";

    sf_msbe_t msbe;
    memset(&msbe, 0, sizeof(msbe));
    msbe.regions = regions;
    msbe.region_count = 2;

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_write_to_memory(&msbe, &data, &size, NULL));

    sf_msbe_t *read_msbe = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_read_from_memory(&read_msbe, data, size, NULL));
    TEST_ASSERT_EQUAL_INT32(2, sf_msbe_region_count(read_msbe));
    TEST_ASSERT_EQUAL_UINT32(1, read_msbe->regions[0].data.type);
    TEST_ASSERT_EQUAL_STRING("InvasionPoint_000", read_msbe->regions[0].data.name);
    TEST_ASSERT_EQUAL_UINT32(18, read_msbe->regions[1].data.type);

    sf_msbe_destroy(read_msbe);
    sf_free(NULL, data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_msbe_point_param_roundtrip);
    return UNITY_END();
}
