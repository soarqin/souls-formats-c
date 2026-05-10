/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_edge_roundtrip(void) {
    uint8_t buf[16384];
    for (size_t i = 0; i < sizeof(buf); i++) buf[i] = (uint8_t)(i * 13u);

    const sf_dcx_type_t types[] = {SF_DCX_TYPE_DCX_EDGE, SF_DCX_TYPE_DCP_EDGE};
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        sf_dcx_compression_info_t info;
        memset(&info, 0, sizeof(info));
        info.type = types[i];

        uint8_t *cx = NULL;
        size_t cxn = 0;
        uint8_t *dx = NULL;
        size_t dxn = 0;
        sf_dcx_compression_info_t got;

        TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compress_to_buffer(buf, sizeof(buf), &info,
                                                               &cx, &cxn, NULL));
        TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_decompress_from_buffer(cx, cxn, &dx, &dxn,
                                                                   &got, NULL));
        TEST_ASSERT_EQUAL_INT(types[i], got.type);
        TEST_ASSERT_EQUAL_UINT(sizeof(buf), dxn);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(buf, dx, sizeof(buf));
        sf_free(NULL, cx);
        sf_free(NULL, dx);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_edge_roundtrip);
    return UNITY_END();
}
