/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"
#include "unity.h"

#include <stdint.h>
#include <string.h>

#ifndef SF_E2E_OODLE_DIR
#define SF_E2E_OODLE_DIR L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle"
#endif

void setUp(void) {
    if (sf_oodle_set_search_path(SF_E2E_OODLE_DIR) != SF_OK) {
        TEST_IGNORE_MESSAGE("sf_oodle_set_search_path failed");
    }
    if (sf_oodle_load() != SF_OK) TEST_IGNORE_MESSAGE("oodle dll missing — skipping KRAK tests");
    sf_oodle_version_t version = sf_oodle_version();
    TEST_ASSERT_TRUE(version == SF_OODLE_VERSION_9 || version == SF_OODLE_VERSION_8 ||
                     version == SF_OODLE_VERSION_6);
}

void tearDown(void) { sf_oodle_unload(); }

static void fill(uint8_t *p, size_t n, int random) {
    uint32_t s = 0x12345678u;
    for (size_t i = 0; i < n; i++) {
        s = s * 1664525u + 1013904223u;
        p[i] = random ? (uint8_t)(s >> 24) : 0u;
    }
}

static void expect_krak_roundtrip(const uint8_t *buf, size_t size,
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
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_KRAK, got.type);
    TEST_ASSERT_EQUAL_UINT8(info->u.dcx_krak.compression_level,
                            got.u.dcx_krak.compression_level);
    TEST_ASSERT_EQUAL_INT(SF_OODLE_LZ_COMPRESSOR_KRAKEN,
                          got.u.dcx_krak.oodle_compressor_type);
    TEST_ASSERT_EQUAL_UINT(size, dxn);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(buf, dx, size);

    sf_free(NULL, cx);
    sf_free(NULL, dx);
}

static void test_krak_roundtrip_presets(void) {
    static const sf_dcx_krak_compression_preset_t presets[] = {
        SF_DCX_KRAK_COMPRESSION_PRESET_ELDEN_RING,
        SF_DCX_KRAK_COMPRESSION_PRESET_ARMORED_CORE_6,
    };
    const size_t sizes[] = {4096u, 65536u, 1048576u};

    uint8_t *buf = sf_default_allocator()->alloc(1048576u, NULL);
    TEST_ASSERT_NOT_NULL(buf);
    for (size_t preset = 0; preset < sizeof(presets) / sizeof(presets[0]); preset++) {
        sf_dcx_compression_info_t info;
        TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compression_info_from_krak_preset(presets[preset],
                                                                              &info));
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            for (int k = 0; k < 2; k++) {
                fill(buf, sizes[s], k);
                expect_krak_roundtrip(buf, sizes[s], &info);
            }
        }
    }
    sf_free(NULL, buf);
}

static void test_krak_manual_info_roundtrip(void) {
    uint8_t buf[4096];
    fill(buf, sizeof(buf), 1);

    sf_dcx_compression_info_t info;
    memset(&info, 0, sizeof(info));
    info.type = SF_DCX_TYPE_DCX_KRAK;
    info.u.dcx_krak.compression_level = 6;
    info.u.dcx_krak.oodle_compressor_type = SF_OODLE_LZ_COMPRESSOR_KRAKEN;

    expect_krak_roundtrip(buf, sizeof(buf), &info);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_krak_roundtrip_presets);
    RUN_TEST(test_krak_manual_info_roundtrip);
    return UNITY_END();
}
