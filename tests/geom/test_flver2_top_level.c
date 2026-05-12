/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 6 T12 — FLVER2 top-level dispatch smoke tests.
 */

#include "souls_formats/sf_flver2.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void put_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void make_minimal_flver(uint8_t out[128], uint32_t version) {
    memset(out, 0, 128);
    memcpy(out, "FLVER\0", 6);
    out[6] = 'L';
    out[7] = 0;
    put_u32_le(&out[8], version);
    put_u32_le(&out[12], 128u); /* data offset */
    put_u32_le(&out[16], 0u);   /* data size */
    out[72] = 0;                /* vertex index size */
    out[73] = 1;                /* unicode */
    put_u32_le(&out[0x68], 0u); /* i16 Unk68 + i16 SpecialModifier */
}

static void test_bad_magic_returns_bad_magic(void) {
    uint8_t bytes[128];
    make_minimal_flver(bytes, 0x20014u);
    bytes[0] = 'X';

    sf_flver2_t *f = NULL;
    TEST_ASSERT_EQUAL_INT(SF_ERR_BAD_MAGIC,
                          sf_flver2_read_from_memory(&f, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NULL(f);
}

static void test_big_endian_marker_returns_unsupported(void) {
    uint8_t bytes[128];
    make_minimal_flver(bytes, 0x20014u);
    bytes[6] = 'B';

    sf_flver2_t *f = NULL;
    TEST_ASSERT_EQUAL_INT(SF_ERR_UNSUPPORTED_VERSION,
                          sf_flver2_read_from_memory(&f, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NULL(f);
}

static void test_unknown_version_returns_unsupported(void) {
    uint8_t bytes[128];
    make_minimal_flver(bytes, 0x99999999u);

    sf_flver2_t *f = NULL;
    TEST_ASSERT_EQUAL_INT(SF_ERR_UNSUPPORTED_VERSION,
                          sf_flver2_read_from_memory(&f, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NULL(f);
}

static void test_minimal_header_read_accessors_and_write(void) {
    uint8_t bytes[128];
    make_minimal_flver(bytes, 0x20014u);

    sf_flver2_t *f = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_flver2_read_from_memory(&f, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_HEX32(0x20014u, sf_flver2_header_version(f));
    TEST_ASSERT_TRUE(sf_flver2_header_unicode(f));
    TEST_ASSERT_EQUAL_UINT64(0u, sf_flver2_dummy_count(f));
    TEST_ASSERT_EQUAL_UINT64(0u, sf_flver2_material_count(f));
    TEST_ASSERT_EQUAL_UINT64(0u, sf_flver2_node_count(f));
    TEST_ASSERT_EQUAL_UINT64(0u, sf_flver2_mesh_count(f));
    TEST_ASSERT_EQUAL_UINT64(0u, sf_flver2_face_set_count(f));
    TEST_ASSERT_EQUAL_UINT64(0u, sf_flver2_vertex_buffer_count(f));
    TEST_ASSERT_EQUAL_UINT64(0u, sf_flver2_buffer_layout_count(f));
    TEST_ASSERT_EQUAL_UINT64(0u, sf_flver2_texture_count(f));

    void *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_flver2_write_to_memory(f, &written, &written_size, NULL));
    TEST_ASSERT_NOT_NULL(written);
    TEST_ASSERT_TRUE(written_size >= 128u);
    TEST_ASSERT_EQUAL_MEMORY("FLVER\0L\0", written, 8);

    sf_free(NULL, written);
    sf_flver2_destroy(f);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bad_magic_returns_bad_magic);
    RUN_TEST(test_big_endian_marker_returns_unsupported);
    RUN_TEST(test_unknown_version_returns_unsupported);
    RUN_TEST(test_minimal_header_read_accessors_and_write);
    return UNITY_END();
}
