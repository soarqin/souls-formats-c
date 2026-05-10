/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const uint8_t k_payload[] = {
    0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13,
    0x20, 0x21, 0x22, 0x23, 0x30, 0x31, 0x32, 0x33,
};

static void expect_type(const uint8_t *buf, size_t n, sf_dcx_type_t expected) {
    sf_dcx_type_t got = SF_DCX_TYPE_UNKNOWN;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_sniff(buf, n, &got));
    TEST_ASSERT_EQUAL_INT(expected, got);
}

static void test_sniff_headers(void) {
    uint8_t b[0x2C];

    memset(b, 0, sizeof(b));
    memcpy(b, "DCP\0DFLT", 8);
    expect_type(b, 8, SF_DCX_TYPE_DCP_DFLT);

    memset(b, 0, sizeof(b));
    memcpy(b, "DCP\0EDGE", 8);
    expect_type(b, 8, SF_DCX_TYPE_DCP_EDGE);

    memset(b, 0, sizeof(b));
    memcpy(b, "DCX\0", 4);
    memcpy(b + 0x28, "EDGE", 4);
    expect_type(b, sizeof(b), SF_DCX_TYPE_DCX_EDGE);

    memcpy(b + 0x28, "DFLT", 4);
    expect_type(b, sizeof(b), SF_DCX_TYPE_DCX_DFLT);

    memcpy(b + 0x28, "KRAK", 4);
    expect_type(b, sizeof(b), SF_DCX_TYPE_DCX_KRAK);

    memcpy(b + 0x28, "ZSTD", 4);
    expect_type(b, sizeof(b), SF_DCX_TYPE_DCX_ZSTD);

    memset(b, 0, sizeof(b));
    b[0] = 0x78;
    b[1] = 0xDA;
    expect_type(b, 4, SF_DCX_TYPE_ZLIB);

    memcpy(b, "XXXX", 4);
    expect_type(b, 4, SF_DCX_TYPE_UNKNOWN);
}

static void test_is_from_buffer_matches_upstream_dcx_magic_only(void) {
    uint8_t b[0x2C];
    bool is_dcx = true;

    memset(b, 0, sizeof(b));
    memcpy(b, "DCX\0", 4);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_is_from_buffer(b, sizeof(b), &is_dcx));
    TEST_ASSERT_TRUE(is_dcx);

    memset(b, 0, sizeof(b));
    memcpy(b, "DCP\0", 4);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_is_from_buffer(b, sizeof(b), &is_dcx));
    TEST_ASSERT_TRUE(is_dcx);

    memset(b, 0, sizeof(b));
    b[0] = 0x78;
    b[1] = 0xDA;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_is_from_buffer(b, 4, &is_dcx));
    TEST_ASSERT_FALSE(is_dcx);
}

static void expect_roundtrip(const sf_dcx_compression_info_t *info) {
    uint8_t *cx = NULL;
    size_t cxn = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compress_to_buffer(k_payload, sizeof(k_payload),
                                                           info, &cx, &cxn, NULL));
    TEST_ASSERT_NOT_NULL(cx);

    if (info->type == SF_DCX_TYPE_NONE) {
        TEST_ASSERT_EQUAL_size_t(sizeof(k_payload), cxn);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(k_payload, cx, sizeof(k_payload));
        sf_free(NULL, cx);
        return;
    }

    uint8_t *dx = NULL;
    size_t dxn = 0;
    sf_dcx_compression_info_t got;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_decompress_from_buffer(cx, cxn, &dx, &dxn,
                                                               &got, NULL));
    TEST_ASSERT_EQUAL_INT(info->type, got.type);
    TEST_ASSERT_EQUAL_size_t(sizeof(k_payload), dxn);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(k_payload, dx, sizeof(k_payload));

    if (info->type == SF_DCX_TYPE_DCX_DFLT) {
        TEST_ASSERT_EQUAL_INT(info->u.dcx_dflt.unk04, got.u.dcx_dflt.unk04);
        TEST_ASSERT_EQUAL_INT(info->u.dcx_dflt.unk10, got.u.dcx_dflt.unk10);
        TEST_ASSERT_EQUAL_INT(info->u.dcx_dflt.unk14, got.u.dcx_dflt.unk14);
        TEST_ASSERT_EQUAL_UINT8(info->u.dcx_dflt.unk30, got.u.dcx_dflt.unk30);
        TEST_ASSERT_EQUAL_UINT8(info->u.dcx_dflt.unk38, got.u.dcx_dflt.unk38);
    } else if (info->type == SF_DCX_TYPE_DCX_ZSTD) {
        TEST_ASSERT_EQUAL_UINT8(info->u.dcx_zstd.compression_level,
                                got.u.dcx_zstd.compression_level);
    }

    sf_free(NULL, cx);
    sf_free(NULL, dx);
}

static void test_compression_info_variants_without_oodle(void) {
    sf_dcx_compression_info_t info;

    memset(&info, 0, sizeof(info));
    info.type = SF_DCX_TYPE_UNKNOWN;
    uint8_t *cx = NULL;
    size_t cxn = 0;
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG,
                          sf_dcx_compress_to_buffer(k_payload, sizeof(k_payload), &info,
                                                    &cx, &cxn, NULL));
    TEST_ASSERT_NULL(cx);

    memset(&info, 0, sizeof(info));
    info.type = SF_DCX_TYPE_NONE;
    expect_roundtrip(&info);

    memset(&info, 0, sizeof(info));
    info.type = SF_DCX_TYPE_ZLIB;
    expect_roundtrip(&info);

    memset(&info, 0, sizeof(info));
    info.type = SF_DCX_TYPE_DCP_EDGE;
    expect_roundtrip(&info);

    memset(&info, 0, sizeof(info));
    info.type = SF_DCX_TYPE_DCP_DFLT;
    expect_roundtrip(&info);

    memset(&info, 0, sizeof(info));
    info.type = SF_DCX_TYPE_DCX_EDGE;
    expect_roundtrip(&info);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compression_info_from_dflt_preset(
                                     SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_9_15,
                                     &info));
    expect_roundtrip(&info);

    memset(&info, 0, sizeof(info));
    info.type = SF_DCX_TYPE_DCX_ZSTD;
    info.u.dcx_zstd.compression_level = 15;
    expect_roundtrip(&info);
}

static void test_preset_factories(void) {
    sf_dcx_compression_info_t info;

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compression_info_from_default_type(
                                     SF_DCX_DEFAULT_TYPE_DEMONS_SOULS, &info));
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_EDGE, info.type);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compression_info_from_default_type(
                                     SF_DCX_DEFAULT_TYPE_DARK_SOULS_1, &info));
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_DFLT, info.type);
    TEST_ASSERT_EQUAL_INT(0x10000, info.u.dcx_dflt.unk04);
    TEST_ASSERT_EQUAL_INT(0x24, info.u.dcx_dflt.unk10);
    TEST_ASSERT_EQUAL_INT(0x2C, info.u.dcx_dflt.unk14);
    TEST_ASSERT_EQUAL_UINT8(9, info.u.dcx_dflt.unk30);
    TEST_ASSERT_EQUAL_UINT8(0, info.u.dcx_dflt.unk38);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compression_info_from_default_type(
                                     SF_DCX_DEFAULT_TYPE_BLOODBORNE, &info));
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_DFLT, info.type);
    TEST_ASSERT_EQUAL_INT(0x10000, info.u.dcx_dflt.unk04);
    TEST_ASSERT_EQUAL_INT(0x44, info.u.dcx_dflt.unk10);
    TEST_ASSERT_EQUAL_INT(0x4C, info.u.dcx_dflt.unk14);

    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compression_info_from_default_type(
                                     SF_DCX_DEFAULT_TYPE_AC6, &info));
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_KRAK, info.type);
    TEST_ASSERT_EQUAL_UINT8(9, info.u.dcx_krak.compression_level);
    TEST_ASSERT_EQUAL_INT(SF_OODLE_LZ_COMPRESSOR_KRAKEN,
                          info.u.dcx_krak.oodle_compressor_type);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sniff_headers);
    RUN_TEST(test_is_from_buffer_matches_upstream_dcx_magic_only);
    RUN_TEST(test_compression_info_variants_without_oodle);
    RUN_TEST(test_preset_factories);
    return UNITY_END();
}
