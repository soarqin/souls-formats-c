/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "map/msbe/msbe_internal.h"

#include "souls_formats/sf_msbe.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_msbe_event_param_roundtrip(void) {
    sf_msbe_event_t events[2];
    memset(events, 0, sizeof(events));
    events[0].data.type = 4;
    events[0].data.name = "Treasure_000";
    events[0].data.event_id = -1;
    events[0].data.other_id = -1;
    events[1].data.type = UINT32_MAX;
    events[1].data.name = "Other_000";
    events[1].data.event_id = -1;
    events[1].data.other_id = -1;

    sf_msbe_t msbe;
    memset(&msbe, 0, sizeof(msbe));
    msbe.events = events;
    msbe.event_count = 2;

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_write_to_memory(&msbe, &data, &size, NULL));

    sf_msbe_t *read_msbe = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_read_from_memory(&read_msbe, data, size, NULL));
    TEST_ASSERT_EQUAL_INT32(2, sf_msbe_event_count(read_msbe));
    TEST_ASSERT_EQUAL_UINT32(4, read_msbe->events[0].data.type);
    TEST_ASSERT_EQUAL_STRING("Treasure_000", read_msbe->events[0].data.name);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, read_msbe->events[1].data.type);
    TEST_ASSERT_EQUAL_STRING("Other_000", read_msbe->events[1].data.name);

    sf_msbe_destroy(read_msbe);
    sf_free(NULL, data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_msbe_event_param_roundtrip);
    return UNITY_END();
}
