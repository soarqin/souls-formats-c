/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "map/msbe/msbe_internal.h"

#include "souls_formats/sf_msbe.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_msbe_route_param_roundtrip(void) {
    sf_msbe_route_t routes[2];
    memset(routes, 0, sizeof(routes));
    routes[0].data.type = 3;
    routes[0].data.name = "MufflingPortalLink_000";
    routes[0].data.other_id = -1;
    routes[1].data.type = 4;
    routes[1].data.name = "MufflingBoxLink_000";
    routes[1].data.other_id = -1;

    sf_msbe_t msbe;
    memset(&msbe, 0, sizeof(msbe));
    msbe.routes = routes;
    msbe.route_count = 2;

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_write_to_memory(&msbe, &data, &size, NULL));

    sf_msbe_t *read_msbe = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_read_from_memory(&read_msbe, data, size, NULL));
    TEST_ASSERT_EQUAL_INT32(2, sf_msbe_route_count(read_msbe));
    TEST_ASSERT_EQUAL_UINT32(3, read_msbe->routes[0].data.type);
    TEST_ASSERT_EQUAL_STRING("MufflingPortalLink_000", read_msbe->routes[0].data.name);
    TEST_ASSERT_EQUAL_UINT32(4, read_msbe->routes[1].data.type);

    sf_msbe_destroy(read_msbe);
    sf_free(NULL, data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_msbe_route_param_roundtrip);
    return UNITY_END();
}
