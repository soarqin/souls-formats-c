/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * QA — sfi_dds_parse_header extracts texture metadata from raw DDS headers.
 */

#include "internal/dds_header.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void put_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)(v >> 24);
}

static void build_bc1_header(uint8_t buf[128]) {
    memset(buf, 0, 128);
    buf[0] = 0x44; buf[1] = 0x44; buf[2] = 0x53; buf[3] = 0x20; /* 'DDS ' */
    put_u32_le(&buf[4],  124);                                  /* dwSize */
    put_u32_le(&buf[8],  0x000A1007u);                          /* dwFlags */
    put_u32_le(&buf[12], 8);                                    /* height */
    put_u32_le(&buf[16], 8);                                    /* width */
    put_u32_le(&buf[20], 8);                                    /* dwPitchOrLinearSize */
    put_u32_le(&buf[24], 0);                                    /* dwDepth */
    put_u32_le(&buf[28], 1);                                    /* dwMipMapCount */
    /* bytes 32-75 dwReserved1[11] zeroed by memset */
    put_u32_le(&buf[76], 32);                                   /* PIXELFORMAT.dwSize */
    put_u32_le(&buf[80], 0x00000004u);                          /* PIXELFORMAT.dwFlags = DDPF_FOURCC */
    put_u32_le(&buf[84], 0x31545844u);                          /* PIXELFORMAT.dwFourCC = 'DXT1' */
    /* bytes 88-107 PIXELFORMAT remainder zeroed */
    put_u32_le(&buf[108], 0x1000u);                             /* dwCaps */
    put_u32_le(&buf[112], 0);                                   /* dwCaps2 (no cubemap) */
    put_u32_le(&buf[116], 0);                                   /* dwCaps3 */
    put_u32_le(&buf[120], 0);                                   /* dwCaps4 */
    put_u32_le(&buf[124], 0);                                   /* dwReserved2 */
}

static void test_dds_parse_8x8_bc1(void) {
    uint8_t buf[128];
    build_bc1_header(buf);

    sfi_dds_metadata_t meta = {0};
    TEST_ASSERT_EQUAL(SF_OK, sfi_dds_parse_header(buf, sizeof(buf), &meta));
    TEST_ASSERT_FALSE(meta.cubemap);
    TEST_ASSERT_EQUAL_UINT32(1u, meta.mipmap_count);
    TEST_ASSERT_EQUAL_UINT32(0u, meta.depth);
    TEST_ASSERT_EQUAL_UINT32(0u, meta.dxgi_format);
}

static void test_dds_parse_dx10_format(void) {
    uint8_t buf[148];
    memset(buf, 0, sizeof(buf));
    build_bc1_header(buf);
    put_u32_le(&buf[84],  0x30315844u);   /* dwFourCC = 'DX10' */
    put_u32_le(&buf[128], 98u);           /* dxgiFormat = DXGI_FORMAT_BC7_UNORM */
    put_u32_le(&buf[132], 3u);            /* resourceDimension = TEXTURE2D */
    put_u32_le(&buf[136], 0u);            /* miscFlag */
    put_u32_le(&buf[140], 1u);            /* arraySize */
    put_u32_le(&buf[144], 0u);            /* miscFlags2 */

    sfi_dds_metadata_t meta = {0};
    TEST_ASSERT_EQUAL(SF_OK, sfi_dds_parse_header(buf, sizeof(buf), &meta));
    TEST_ASSERT_EQUAL_UINT32(98u, meta.dxgi_format);
}

static void test_dds_parse_bad_magic(void) {
    uint8_t buf[128];
    build_bc1_header(buf);
    buf[0] = 'X';   /* corrupt magic */

    sfi_dds_metadata_t meta = {0};
    TEST_ASSERT_EQUAL(SF_ERR_BAD_MAGIC,
                      sfi_dds_parse_header(buf, sizeof(buf), &meta));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dds_parse_8x8_bc1);
    RUN_TEST(test_dds_parse_dx10_format);
    RUN_TEST(test_dds_parse_bad_magic);
    return UNITY_END();
}
