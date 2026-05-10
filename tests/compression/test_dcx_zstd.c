/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static void fill(uint8_t *p, size_t n, int kind) {
    uint32_t s = 0x12345678u;
    for (size_t i = 0; i < n; i++) {
        s = s * 1664525u + 1013904223u;
        p[i] = kind == 0 ? 0u : (kind == 1 ? (uint8_t)(s >> 24) : 0xFFu);
    }
}

static void test_zstd_roundtrip(void) {
    const size_t sizes[] = {1024u, 16384u, 1048576u};
    const uint8_t levels[] = {1u, 15u};

    uint8_t *buf = sf_default_allocator()->alloc(1048576u, NULL);
    TEST_ASSERT_NOT_NULL(buf);
    for (size_t level = 0; level < sizeof(levels) / sizeof(levels[0]); level++) {
        sf_dcx_compression_info_t info;
        info.type = SF_DCX_TYPE_DCX_ZSTD;
        info.u.dcx_zstd.compression_level = levels[level];

        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            for (int k = 0; k < 3; k++) {
                fill(buf, sizes[s], k);
                uint8_t *cx = NULL;
                size_t cxn = 0;
                uint8_t *dx = NULL;
                size_t dxn = 0;
                sf_dcx_compression_info_t got;

                TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compress_to_buffer(buf, sizes[s], &info,
                                                                       &cx, &cxn, NULL));
                TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_decompress_from_buffer(cx, cxn, &dx,
                                                                           &dxn, &got, NULL));
                TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_ZSTD, got.type);
                TEST_ASSERT_EQUAL_UINT8(levels[level], got.u.dcx_zstd.compression_level);
                TEST_ASSERT_EQUAL_UINT(sizes[s], dxn);
                TEST_ASSERT_EQUAL_UINT8_ARRAY(buf, dx, sizes[s]);
                sf_free(NULL, cx);
                sf_free(NULL, dx);
            }
        }
    }
    sf_free(NULL, buf);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zstd_roundtrip);
    return UNITY_END();
}
