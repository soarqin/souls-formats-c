/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "map/msbe/msbe_internal.h"

#include "souls_formats/sf_msbe.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_msbe_parts_param_roundtrip(void) {
    sf_msbe_part_t parts[2];
    memset(parts, 0, sizeof(parts));
    parts[0].data.type = 0;
    parts[0].data.name = "m10_00_00_00";
    parts[0].data.model_index = 0;
    parts[0].data.other_id = -1;
    parts[1].data.type = 13;
    parts[1].data.name = "AEG099_000_1000";
    parts[1].data.model_index = 1;
    parts[1].data.other_id = -1;

    sf_msbe_t msbe;
    memset(&msbe, 0, sizeof(msbe));
    msbe.parts = parts;
    msbe.part_count = 2;

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_write_to_memory(&msbe, &data, &size, NULL));

    sf_msbe_t *read_msbe = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_read_from_memory(&read_msbe, data, size, NULL));
    TEST_ASSERT_EQUAL_INT32(2, sf_msbe_part_count(read_msbe));
    TEST_ASSERT_EQUAL_UINT32(0, read_msbe->parts[0].data.type);
    TEST_ASSERT_EQUAL_STRING("m10_00_00_00", read_msbe->parts[0].data.name);
    TEST_ASSERT_EQUAL_UINT32(13, read_msbe->parts[1].data.type);

    sf_msbe_destroy(read_msbe);
    sf_free(NULL, data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_msbe_parts_param_roundtrip);
    return UNITY_END();
}
