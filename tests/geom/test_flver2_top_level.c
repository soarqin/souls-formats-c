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

static void put_i32_le(uint8_t *p, int32_t v) {
    put_u32_le(p, (uint32_t)v);
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

static void make_flver_with_vertex_buffer(uint8_t out[240]) {
    memset(out, 0, 240);
    memcpy(out, "FLVER\0", 6);
    out[6] = 'L';
    out[7] = 0;
    put_u32_le(&out[8], 0x20014u);
    put_u32_le(&out[12], 0xE0u);       /* data offset */
    put_u32_le(&out[16], 0x10u);       /* data size */
    put_i32_le(&out[36], 1);           /* vertex buffer count */
    out[72] = 0;                       /* vertex index size */
    out[73] = 1;                       /* unicode */
    put_i32_le(&out[84], 1);           /* buffer layout count */
    put_u32_le(&out[0x68], 0x80000000u); /* i16 Unk68 + i16 SpecialModifier */

    /* VertexBuffer at 0x80. */
    put_i32_le(&out[0x80], 3);  /* BufferIndex */
    put_i32_le(&out[0x84], 0);  /* LayoutIndex */
    put_i32_le(&out[0x88], 8);  /* VertexSize: intentionally != layout.Size */
    put_i32_le(&out[0x8C], 2);  /* VertexCount */
    put_i32_le(&out[0x98], 16); /* BufferLength */
    put_i32_le(&out[0x9C], 0);  /* BufferOffset relative to data section */

    /* BufferLayout at 0xA0; members at 0xB0. */
    put_i32_le(&out[0xA0], 2);
    put_i32_le(&out[0xAC], 0xB0);

    put_i32_le(&out[0xB0], 0); /* stream + specialModifier (SpeedTree split) */
    put_i32_le(&out[0xB4], 0);
    put_u32_le(&out[0xB8], SF_FLVER_LAYOUT_TYPE_FLOAT3);
    put_u32_le(&out[0xBC], SF_FLVER_LAYOUT_SEMANTIC_POSITION);
    put_i32_le(&out[0xC0], 0);

    put_u32_le(&out[0xC4], 0x80000000u); /* specialModifier == -32768 */
    put_i32_le(&out[0xC8], 12);
    put_u32_le(&out[0xCC], SF_FLVER_LAYOUT_TYPE_HALF4);
    put_u32_le(&out[0xD0], SF_FLVER_LAYOUT_SEMANTIC_UV);
    put_i32_le(&out[0xD4], 0);

    for (size_t i = 0; i < 16; i++) {
        out[0xE0 + i] = (uint8_t)(0xA0u + i);
    }
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

static void test_vertex_buffer_and_buffer_layout_roundtrip(void) {
    uint8_t bytes[240];
    make_flver_with_vertex_buffer(bytes);

    sf_flver2_t *f = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_flver2_read_from_memory(&f, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_UINT64(1u, sf_flver2_vertex_buffer_count(f));
    TEST_ASSERT_EQUAL_UINT64(1u, sf_flver2_buffer_layout_count(f));

    const sf_flver2_vertex_buffer_t *vb = sf_flver2_vertex_buffer(f, 0);
    TEST_ASSERT_NOT_NULL(vb);
    TEST_ASSERT_EQUAL_INT32(3, sf_flver2_vertex_buffer_buffer_index(vb));
    TEST_ASSERT_EQUAL_INT32(0, sf_flver2_vertex_buffer_layout_index(vb));
    TEST_ASSERT_EQUAL_INT32(8, sf_flver2_vertex_buffer_vertex_size(vb));
    TEST_ASSERT_EQUAL_INT32(2, sf_flver2_vertex_buffer_vertex_count(vb));
    size_t raw_size = 0;
    const uint8_t *raw = sf_flver2_vertex_buffer_bytes(vb, &raw_size);
    TEST_ASSERT_EQUAL_UINT64(16u, raw_size);
    TEST_ASSERT_EQUAL_MEMORY(&bytes[0xE0], raw, 16);

    const sf_flver2_buffer_layout_t *bl = sf_flver2_buffer_layout(f, 0);
    TEST_ASSERT_NOT_NULL(bl);
    TEST_ASSERT_EQUAL_UINT64(2u, sf_flver2_buffer_layout_member_count(bl));
    TEST_ASSERT_EQUAL_UINT32(12u, sf_flver2_buffer_layout_size(bl));
    TEST_ASSERT_EQUAL_INT16(-32768, sf_flver2_buffer_layout_member_special_modifier(bl, 1));
    TEST_ASSERT_EQUAL_UINT32(SF_FLVER_LAYOUT_TYPE_HALF4,
                             sf_flver2_buffer_layout_member_type(bl, 1));

    void *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_flver2_write_to_memory(f, &written, &written_size, NULL));
    TEST_ASSERT_EQUAL_UINT64(sizeof(bytes), written_size);
    TEST_ASSERT_EQUAL_MEMORY(bytes, written, sizeof(bytes));

    sf_free(NULL, written);
    sf_flver2_destroy(f);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bad_magic_returns_bad_magic);
    RUN_TEST(test_big_endian_marker_returns_unsupported);
    RUN_TEST(test_unknown_version_returns_unsupported);
    RUN_TEST(test_minimal_header_read_accessors_and_write);
    RUN_TEST(test_vertex_buffer_and_buffer_layout_roundtrip);
    return UNITY_END();
}
