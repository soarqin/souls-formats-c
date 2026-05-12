/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 6 T15 — FLVER2 FaceSet + triangle-strip decode.
 */

#include "internal/flver2_internal.h"
#include "souls_formats/sf_flver2.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void put_u16_le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void put_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static sf_result_t read_faceset(const uint8_t *bytes, size_t size,
                                sf_flver2_face_set_t *out) {
    sf_istream_t *is = NULL;
    sf_binary_reader_t *br = NULL;
    sf_result_t r = sf_istream_open_memory(&is, bytes, size, NULL);
    if (r == SF_OK) r = sf_binary_reader_create(&br, is, false, NULL);
    if (r == SF_OK) {
        sf_flver2_header_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        r = sfi_flver2_face_set_read(br, &hdr, 16, 24, out, NULL);
    }
    sf_binary_reader_destroy(br);
    sf_istream_close(is);
    return r;
}

static sf_result_t write_faceset(const sf_flver2_face_set_t *fs,
                                 uint8_t **out,
                                 size_t *out_size) {
    sf_ostream_t *os = NULL;
    sf_binary_writer_t *bw = NULL;
    sf_result_t r = sf_ostream_open_memory(&os, NULL);
    if (r == SF_OK) r = sf_binary_writer_create(&bw, os, false, NULL);
    if (r == SF_OK) {
        sf_flver2_header_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        r = sfi_flver2_face_set_write(bw, &hdr, fs, 16, 0);
    }
    if (r == SF_OK) r = sfi_flver2_face_set_write_indices(bw, fs, 0, 24);
    if (r == SF_OK) r = sf_binary_writer_finish_bytes(bw, out, out_size);
    sf_binary_writer_destroy(bw);
    sf_ostream_close(os);
    return r;
}

static void make_faceset_fixture(uint8_t out[32]) {
    memset(out, 0, 32);
    put_u32_le(&out[0], SF_FLVER2_FS_FLAGS_NONE);
    out[4] = 0;    /* triangle strip */
    out[5] = 1;    /* cull backfaces */
    out[6] = 0xAA; /* Unk06 */
    out[7] = 0xBB; /* Unk07 */
    put_u32_le(&out[8], 4u);
    put_u32_le(&out[12], 0u);
    out[16] = 16;
    put_u16_le(&out[24], 0u);
    put_u16_le(&out[26], 1u);
    put_u16_le(&out[28], 2u);
    put_u16_le(&out[30], 3u);
}

static void test_faceset_round_trip_byte_identical(void) {
    uint8_t bytes[32];
    make_faceset_fixture(bytes);

    sf_flver2_face_set_t fs;
    TEST_ASSERT_EQUAL_INT(SF_OK, read_faceset(bytes, sizeof(bytes), &fs));
    TEST_ASSERT_EQUAL_HEX32(SF_FLVER2_FS_FLAGS_NONE, sf_flver2_face_set_flags(&fs));
    TEST_ASSERT_FALSE(sf_flver2_face_set_triangle_strip(&fs));
    TEST_ASSERT_TRUE(sf_flver2_face_set_cull_backfaces(&fs));
    TEST_ASSERT_EQUAL_UINT8(0xAAu, sf_flver2_face_set_unk06(&fs));
    TEST_ASSERT_EQUAL_UINT8(0xBBu, sf_flver2_face_set_unk07(&fs));
    TEST_ASSERT_EQUAL_UINT8(16u, sf_flver2_face_set_index_size(&fs));
    TEST_ASSERT_EQUAL_UINT64(4u, sf_flver2_face_set_index_count(&fs));
    TEST_ASSERT_EQUAL_UINT32(3u, sf_flver2_face_set_index(&fs, 3));

    uint8_t *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, write_faceset(&fs, &written, &written_size));
    TEST_ASSERT_EQUAL_UINT64(sizeof(bytes), written_size);
    TEST_ASSERT_EQUAL_MEMORY(bytes, written, sizeof(bytes));

    sf_free(NULL, written);
    sfi_flver2_face_set_destroy_inplace(&fs, NULL);
}

static void assert_triangulates(const uint32_t *indices, size_t index_count,
                                const uint32_t *expected, size_t expected_count) {
    sf_flver2_face_set_t fs;
    memset(&fs, 0, sizeof(fs));
    fs.triangle_strip = true;
    fs.indices = (uint32_t *)indices;
    fs.index_count = index_count;
    fs.index_size = 16;

    uint32_t *triangles = NULL;
    size_t triangle_count = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sfi_flver2_face_set_triangulate(&fs, true,
                                                                 &triangles,
                                                                 &triangle_count,
                                                                 NULL));
    TEST_ASSERT_EQUAL_UINT64(expected_count, triangle_count);
    TEST_ASSERT_EQUAL_UINT32_ARRAY(expected, triangles, expected_count);
    sf_free(NULL, triangles);
}

static void test_triangle_strip_four_indices_to_two_triangles(void) {
    const uint32_t indices[] = { 0, 1, 2, 3 };
    const uint32_t expected[] = { 0, 1, 2, 2, 1, 3 };
    assert_triangulates(indices, 4, expected, 6);
}

static void test_triangle_strip_restart_splits_independent_strips(void) {
    const uint32_t indices[] = { 0, 1, 2, 0xFFFFu, 3, 4, 5 };
    const uint32_t expected[] = { 0, 1, 2, 3, 4, 5 };
    assert_triangulates(indices, 7, expected, 6);
}

static void test_degenerate_filter_enabled_skips_repeated_vertices(void) {
    const uint32_t indices[] = { 0, 0, 1, 2, 2, 3 };
    const uint32_t expected[] = { 1, 0, 2 };
    assert_triangulates(indices, 6, expected, 3);
}

static void test_edge_compression_rejected(void) {
    uint8_t bytes[24];
    memset(bytes, 0, sizeof(bytes));
    put_u32_le(&bytes[0], SF_FLVER2_FS_FLAGS_EDGE_COMPRESSED);

    sf_flver2_face_set_t fs;
    TEST_ASSERT_EQUAL_INT(SF_ERR_UNSUPPORTED_VERSION,
                          read_faceset(bytes, sizeof(bytes), &fs));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_faceset_round_trip_byte_identical);
    RUN_TEST(test_triangle_strip_four_indices_to_two_triangles);
    RUN_TEST(test_triangle_strip_restart_splits_independent_strips);
    RUN_TEST(test_degenerate_filter_enabled_skips_repeated_vertices);
    RUN_TEST(test_edge_compression_rejected);
    return UNITY_END();
}
