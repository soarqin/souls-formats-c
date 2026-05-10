/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 3 QA — TPF synthetic round-trip + Headerizer scope coverage.
 *
 * Goals:
 *   1. PC TPF with two DDS textures: read(write(b)) == b and the second
 *      write produces the byte-identical buffer.
 *   2. Headerize on a non-PC platform returns SF_ERR_UNSUPPORTED_VERSION.
 *   3. Per-texture DCP_EDGE compression (flags1 == 2) auto-decompresses on
 *      read and re-wraps on write.
 *   4. Empty TPF (zero textures) round-trips byte-identically.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_tpf.h"

#include "archive/tpf_headerizer.h"

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*===========================================================================
 * Helper: minimal 128-byte DDS buffer (DXT1, 8x8 single texture).
 *===========================================================================*/

static void make_minimal_dds_bytes(uint8_t buf[128]) {
    memset(buf, 0, 128);
    buf[0] = 0x44; buf[1] = 0x44; buf[2] = 0x53; buf[3] = 0x20; /* "DDS " */

    /* dwSize = 124 */
    buf[4]  = 0x7C; buf[5]  = 0x00; buf[6]  = 0x00; buf[7]  = 0x00;
    /* dwFlags = 0x1007 (CAPS|HEIGHT|WIDTH|PIXELFORMAT) */
    buf[8]  = 0x07; buf[9]  = 0x10; buf[10] = 0x00; buf[11] = 0x00;
    /* dwHeight = 8 */
    buf[12] = 0x08; buf[13] = 0x00; buf[14] = 0x00; buf[15] = 0x00;
    /* dwWidth = 8 */
    buf[16] = 0x08; buf[17] = 0x00; buf[18] = 0x00; buf[19] = 0x00;

    /* pixelformat at offset 76: dwSize = 32 */
    buf[76] = 0x20; buf[77] = 0x00; buf[78] = 0x00; buf[79] = 0x00;
    /* dwFlags = DDPF_FOURCC = 4 */
    buf[80] = 0x04; buf[81] = 0x00; buf[82] = 0x00; buf[83] = 0x00;
    /* fourCC = "DXT1" */
    buf[84] = 'D';  buf[85] = 'X';  buf[86] = 'T';  buf[87] = '1';

    /* dwCaps at 108: DDSCAPS_TEXTURE = 0x1000 */
    buf[108] = 0x00; buf[109] = 0x10; buf[110] = 0x00; buf[111] = 0x00;
}

/*===========================================================================
 * Round-trip helper: write → read → write → compare both buffers byte-equal.
 *===========================================================================*/

static void roundtrip_assert(const sf_tpf_t *t1) {
    uint8_t *b1 = NULL;
    size_t   n1 = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_write_to_memory(t1, &b1, &n1, NULL));

    sf_tpf_t *t2 = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_read_from_memory(&t2, b1, n1, NULL));

    TEST_ASSERT_EQUAL_size_t(sf_tpf_texture_count(t1), sf_tpf_texture_count(t2));
    TEST_ASSERT_EQUAL_INT((int)sf_tpf_get_platform(t1), (int)sf_tpf_get_platform(t2));
    TEST_ASSERT_EQUAL_UINT8(sf_tpf_get_encoding(t1), sf_tpf_get_encoding(t2));
    TEST_ASSERT_EQUAL_UINT8(sf_tpf_get_flag2(t1),    sf_tpf_get_flag2(t2));

    for (size_t i = 0; i < sf_tpf_texture_count(t1); i++) {
        const sf_tpf_texture_t *a = sf_tpf_get_texture(t1, i);
        const sf_tpf_texture_t *b = sf_tpf_get_texture(t2, i);
        TEST_ASSERT_NOT_NULL(a);
        TEST_ASSERT_NOT_NULL(b);
        TEST_ASSERT_EQUAL_STRING(sf_tpf_texture_get_name(a),
                                 sf_tpf_texture_get_name(b));
        TEST_ASSERT_EQUAL_UINT8(sf_tpf_texture_get_format(a),
                                sf_tpf_texture_get_format(b));
        TEST_ASSERT_EQUAL_UINT8(sf_tpf_texture_get_flags1(a),
                                sf_tpf_texture_get_flags1(b));

        size_t sa = 0, sb = 0;
        const uint8_t *ba = sf_tpf_texture_get_bytes(a, &sa);
        const uint8_t *bb = sf_tpf_texture_get_bytes(b, &sb);
        TEST_ASSERT_EQUAL_size_t(sa, sb);
        if (sa > 0) TEST_ASSERT_EQUAL_MEMORY(ba, bb, sa);
    }

    uint8_t *b2 = NULL;
    size_t   n2 = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_write_to_memory(t2, &b2, &n2, NULL));

    TEST_ASSERT_EQUAL_size_t(n1, n2);
    TEST_ASSERT_EQUAL_MEMORY(b1, b2, n1);

    sf_free(NULL, b1);
    sf_free(NULL, b2);
    sf_tpf_destroy(t2);
}

/*===========================================================================
 * Test 1: PC, two textures, byte-equal round-trip.
 *===========================================================================*/

static void test_tpf_pc_2tex(void) {
    sf_tpf_t *t = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_create(&t, NULL));

    uint8_t dds[128];
    make_minimal_dds_bytes(dds);

    sf_tpf_texture_t *tex1 = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_texture_create(&tex1, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_texture_set_name(tex1, "alpha"));
    sf_tpf_texture_set_format(tex1, 0);
    sf_tpf_texture_set_flags1(tex1, 0);
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_texture_set_bytes(tex1, dds, sizeof(dds)));
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_add_texture(t, tex1));
    sf_tpf_texture_destroy(tex1);

    sf_tpf_texture_t *tex2 = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_texture_create(&tex2, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_texture_set_name(tex2, "beta_tex"));
    sf_tpf_texture_set_format(tex2, 0);
    sf_tpf_texture_set_flags1(tex2, 0);
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_texture_set_bytes(tex2, dds, sizeof(dds)));
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_add_texture(t, tex2));
    sf_tpf_texture_destroy(tex2);

    TEST_ASSERT_EQUAL_size_t(2, sf_tpf_texture_count(t));
    roundtrip_assert(t);
    sf_tpf_destroy(t);
}

/*===========================================================================
 * Test 2: PS4 platform → Headerize returns SF_ERR_UNSUPPORTED_VERSION.
 *===========================================================================*/

static void test_tpf_ps4_unsupported(void) {
    sf_tpf_texture_t *tex = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_texture_create(&tex, NULL));
    uint8_t dds[128];
    make_minimal_dds_bytes(dds);
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_texture_set_bytes(tex, dds, sizeof(dds)));

    void  *out      = NULL;
    size_t out_size = 0;
    TEST_ASSERT_EQUAL(SF_ERR_UNSUPPORTED_VERSION,
                      sfi_tpf_headerize(tex, SF_TPF_PLATFORM_PS4,
                                        &out, &out_size, NULL));

    /* PC path: succeeds and returns a copy. */
    TEST_ASSERT_EQUAL(SF_OK,
                      sfi_tpf_headerize(tex, SF_TPF_PLATFORM_PC,
                                        &out, &out_size, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof(dds), out_size);
    TEST_ASSERT_EQUAL_MEMORY(dds, out, sizeof(dds));
    sf_free(NULL, out);

    sf_tpf_texture_destroy(tex);
}

/*===========================================================================
 * Test 3: flags1 == 2 (DCP_EDGE) round-trip.
 *
 * The on-disk bytes are DCP_EDGE-compressed; the reader must decompress
 * them back to the raw DDS. The writer must re-wrap them when it sees
 * flags1 == 2. We compare against the raw DDS bytes after read.
 *===========================================================================*/

static void test_tpf_dcp_edge_roundtrip(void) {
    uint8_t raw[128];
    make_minimal_dds_bytes(raw);

    sf_tpf_t *t = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_create(&t, NULL));

    sf_tpf_texture_t *tex = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_texture_create(&tex, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_texture_set_name(tex, "edged_tex"));
    sf_tpf_texture_set_format(tex, 0);
    sf_tpf_texture_set_flags1(tex, 2);
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_texture_set_bytes(tex, raw, sizeof(raw)));
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_add_texture(t, tex));
    sf_tpf_texture_destroy(tex);

    /* Write → read → confirm decompressed bytes match the raw DDS. */
    uint8_t *buf = NULL;
    size_t   n   = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_write_to_memory(t, &buf, &n, NULL));

    sf_tpf_t *t2 = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_read_from_memory(&t2, buf, n, NULL));
    TEST_ASSERT_EQUAL_size_t(1, sf_tpf_texture_count(t2));

    const sf_tpf_texture_t *got = sf_tpf_get_texture(t2, 0);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_UINT8(2, sf_tpf_texture_get_flags1(got));
    size_t got_size = 0;
    const uint8_t *got_bytes = sf_tpf_texture_get_bytes(got, &got_size);
    TEST_ASSERT_EQUAL_size_t(sizeof(raw), got_size);
    TEST_ASSERT_EQUAL_MEMORY(raw, got_bytes, sizeof(raw));

    /* Second write must equal the first (re-wrap is deterministic). */
    uint8_t *buf2 = NULL;
    size_t   n2   = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_write_to_memory(t2, &buf2, &n2, NULL));
    TEST_ASSERT_EQUAL_size_t(n, n2);
    TEST_ASSERT_EQUAL_MEMORY(buf, buf2, n);

    sf_free(NULL, buf);
    sf_free(NULL, buf2);
    sf_tpf_destroy(t);
    sf_tpf_destroy(t2);
}

/*===========================================================================
 * Test 4: empty TPF round-trip.
 *===========================================================================*/

static void test_tpf_empty(void) {
    sf_tpf_t *t = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_tpf_create(&t, NULL));
    TEST_ASSERT_EQUAL_size_t(0, sf_tpf_texture_count(t));
    roundtrip_assert(t);
    sf_tpf_destroy(t);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tpf_pc_2tex);
    RUN_TEST(test_tpf_ps4_unsupported);
    RUN_TEST(test_tpf_dcp_edge_roundtrip);
    RUN_TEST(test_tpf_empty);
    return UNITY_END();
}
