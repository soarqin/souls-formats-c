/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

#include <stdint.h>
#include <string.h>

#ifndef WINAPI
#define WINAPI __stdcall
#endif
__declspec(dllimport) int WINAPI DeleteFileA(const char *path);

void setUp(void) {}
void tearDown(void) {}

static void fill(uint8_t *p, size_t n, int kind) {
    uint32_t s = 0x12345678u;
    for (size_t i = 0; i < n; i++) {
        s = s * 1664525u + 1013904223u;
        p[i] = kind == 0 ? 0u : (kind == 1 ? (uint8_t)(s >> 24) : 0xFFu);
    }
}

static void expect_dflt_roundtrip(const uint8_t *buf, size_t size,
                                  const sf_dcx_compression_info_t *info) {
    uint8_t *cx = NULL;
    size_t cxn = 0;
    uint8_t *dx = NULL;
    size_t dxn = 0;
    sf_dcx_compression_info_t got;

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compress_to_buffer(buf, size, info,
                                                           &cx, &cxn, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_decompress_from_buffer(cx, cxn, &dx, &dxn,
                                                               &got, NULL));
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_DFLT, got.type);
    TEST_ASSERT_EQUAL_INT(info->u.dcx_dflt.unk04, got.u.dcx_dflt.unk04);
    TEST_ASSERT_EQUAL_INT(info->u.dcx_dflt.unk10, got.u.dcx_dflt.unk10);
    TEST_ASSERT_EQUAL_INT(info->u.dcx_dflt.unk14, got.u.dcx_dflt.unk14);
    TEST_ASSERT_EQUAL_UINT8(info->u.dcx_dflt.unk30, got.u.dcx_dflt.unk30);
    TEST_ASSERT_EQUAL_UINT8(info->u.dcx_dflt.unk38, got.u.dcx_dflt.unk38);
    TEST_ASSERT_EQUAL_UINT(size, dxn);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(buf, dx, size);

    sf_free(NULL, cx);
    sf_free(NULL, dx);
}

static void test_dcx_dflt_roundtrip_presets(void) {
    static const sf_dcx_dflt_compression_preset_t presets[] = {
        SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_10000_24_9,
        SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_10000_44_9,
        SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_8,
        SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_9,
        SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_9_15,
    };
    const size_t sizes[] = {1024u, 16384u, 1048576u};

    uint8_t *buf = sf_default_allocator()->alloc(1048576u, NULL);
    TEST_ASSERT_NOT_NULL(buf);
    for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
        sf_dcx_compression_info_t info;
        TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compression_info_from_dflt_preset(presets[i],
                                                                              &info));
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            for (int k = 0; k < 3; k++) {
                fill(buf, sizes[s], k);
                expect_dflt_roundtrip(buf, sizes[s], &info);
            }
        }
    }
    sf_free(NULL, buf);
}

static void test_dcx_stream_overloads(void) {
    uint8_t buf[4096];
    fill(buf, sizeof(buf), 1);

    sf_dcx_compression_info_t info;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compression_info_from_dflt_preset(
                                     SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_9,
                                     &info));

    sf_ostream_t *os = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ostream_open_memory(&os, NULL));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compress_to_stream(buf, sizeof(buf), &info, os, NULL));

    void *bytes = NULL;
    size_t n = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ostream_detach_buffer(os, &bytes, &n));
    sf_ostream_close(os);

    sf_istream_t *is = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_istream_open_memory(&is, bytes, n, NULL));
    bool is_dcx = false;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_is_from_stream(is, &is_dcx));
    TEST_ASSERT_TRUE(is_dcx);

    uint8_t *dx = NULL;
    size_t dxn = 0;
    sf_dcx_compression_info_t got;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_decompress_from_stream(is, &dx, &dxn, &got, NULL));
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_DFLT, got.type);
    TEST_ASSERT_EQUAL_UINT(sizeof(buf), dxn);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(buf, dx, sizeof(buf));

    sf_istream_close(is);
    sf_free(NULL, bytes);
    sf_free(NULL, dx);
}

static void test_dcx_path_overloads(void) {
    uint8_t buf[2048];
    fill(buf, sizeof(buf), 2);

    const char *path = "sf_test_dcx_dflt.dcx";
    (void)DeleteFileA(path);

    sf_dcx_compression_info_t info;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compression_info_from_dflt_preset(
                                     SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_9,
                                     &info));

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compress_to_path(buf, sizeof(buf), &info, path, NULL));

    bool is_dcx = false;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_is_from_path(path, &is_dcx));
    TEST_ASSERT_TRUE(is_dcx);

    uint8_t *dx = NULL;
    size_t dxn = 0;
    sf_dcx_compression_info_t got;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_decompress_from_path(path, &dx, &dxn, &got, NULL));
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_DFLT, got.type);
    TEST_ASSERT_EQUAL_UINT(sizeof(buf), dxn);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(buf, dx, sizeof(buf));

    sf_free(NULL, dx);
    (void)DeleteFileA(path);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dcx_dflt_roundtrip_presets);
    RUN_TEST(test_dcx_stream_overloads);
    RUN_TEST(test_dcx_path_overloads);
    return UNITY_END();
}
