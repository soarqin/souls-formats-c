/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds1r_test_helper.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_mcp.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_mcp_e2e_ds1r(void) {
    if (!ds1r_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS1R copy not available");
    }
    void *bytes = NULL;
    size_t size = 0;
    sf_result_t r = ds1r_read_file("map/m10_00_00_00/m10_00_00_00.mcp", &bytes, &size);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R MCP not accessible");
    }
    sf_mcp_t *mcp = NULL;
    r = sf_mcp_read_from_memory(&mcp, bytes, size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(mcp);
    TEST_ASSERT_FALSE(sf_mcp_big_endian(mcp));
    TEST_ASSERT_GREATER_THAN(0, (int)sf_mcp_room_count(mcp));

    void *bytes2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mcp_write_to_memory(mcp, &bytes2, &size2, NULL));
    TEST_ASSERT_EQUAL_size_t(size, size2);
    TEST_ASSERT_EQUAL_MEMORY(bytes, bytes2, size);

    sf_free(NULL, bytes);
    sf_free(NULL, bytes2);
    sf_mcp_destroy(mcp);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mcp_e2e_ds1r);
    return UNITY_END();
}
