/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 6 T23 — MTD synthetic round-trip: 3 params (Int, Float, Bool) +
 * 2 non-extended textures (g_DiffuseTexture, g_NormalTexture). Byte
 * stream is constructed via sf_binary_writer mirroring exactly the block
 * layout in src/geom/mtd.c::mtd_write_to_writer().
 */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_mtd.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

typedef struct mtd_block {
    int64_t start;
    char    name[32];
} mtd_block_t;

static sf_result_t mtd_w_marker(sf_binary_writer_t *bw, uint8_t marker) {
    sf_result_t r = sf_binary_writer_write_u8(bw, marker);
    if (r != SF_OK) return r;
    return sf_binary_writer_pad(bw, 4);
}

static sf_result_t mtd_w_marked_string(sf_binary_writer_t *bw, uint8_t marker,
                                       const char *s) {
    size_t len = strlen(s);
    sf_result_t r = sf_binary_writer_write_i32(bw, (int32_t)len);
    if (r == SF_OK && len > 0) r = sf_binary_writer_write_bytes(bw, s, len);
    if (r != SF_OK) return r;
    return mtd_w_marker(bw, marker);
}

static sf_result_t mtd_b_open(sf_binary_writer_t *bw, int32_t type, int32_t version,
                              uint8_t marker, mtd_block_t *blk) {
    sf_result_t r = sf_binary_writer_write_i32(bw, 0);
    if (r != SF_OK) return r;
    blk->start = sf_binary_writer_position(bw) + 4;
    (void)snprintf(blk->name, sizeof(blk->name), "Block%llX",
                   (unsigned long long)blk->start);
    r = sf_binary_writer_reserve_u32(bw, blk->name);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, type);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, version);
    if (r == SF_OK) r = mtd_w_marker(bw, marker);
    return r;
}

static sf_result_t mtd_b_close(sf_binary_writer_t *bw, mtd_block_t *blk) {
    int64_t current = sf_binary_writer_position(bw);
    uint32_t length = (uint32_t)(current - blk->start);
    return sf_binary_writer_fill_u32(bw, blk->name, length);
}

static sf_result_t write_param_block_int(sf_binary_writer_t *bw,
                                         const char *name, int32_t value) {
    mtd_block_t param;
    sf_result_t r = mtd_b_open(bw, 4, 4, 0xA3, &param);
    if (r == SF_OK) r = mtd_w_marked_string(bw, 0xA3, name);
    if (r == SF_OK) r = mtd_w_marked_string(bw, 0x04, "int");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 1);
    mtd_block_t val;
    if (r == SF_OK) r = mtd_b_open(bw, 0x1001, 1, 0xC5, &val);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 1);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, value);
    if (r == SF_OK) r = mtd_b_close(bw, &val);
    if (r == SF_OK) r = mtd_w_marker(bw, 0x04);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = mtd_b_close(bw, &param);
    return r;
}

static sf_result_t write_param_block_float(sf_binary_writer_t *bw,
                                           const char *name, float value) {
    mtd_block_t param;
    sf_result_t r = mtd_b_open(bw, 4, 4, 0xA3, &param);
    if (r == SF_OK) r = mtd_w_marked_string(bw, 0xA3, name);
    if (r == SF_OK) r = mtd_w_marked_string(bw, 0x04, "float");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 1);
    mtd_block_t val;
    if (r == SF_OK) r = mtd_b_open(bw, 0x1002, 1, 0xCA, &val);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 1);
    if (r == SF_OK) r = sf_binary_writer_write_f32(bw, value);
    if (r == SF_OK) r = mtd_b_close(bw, &val);
    if (r == SF_OK) r = mtd_w_marker(bw, 0x04);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = mtd_b_close(bw, &param);
    return r;
}

static sf_result_t write_param_block_bool(sf_binary_writer_t *bw,
                                          const char *name, bool value) {
    mtd_block_t param;
    sf_result_t r = mtd_b_open(bw, 4, 4, 0xA3, &param);
    if (r == SF_OK) r = mtd_w_marked_string(bw, 0xA3, name);
    if (r == SF_OK) r = mtd_w_marked_string(bw, 0x04, "bool");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 1);
    mtd_block_t val;
    if (r == SF_OK) r = mtd_b_open(bw, 0x1000, 1, 0xC0, &val);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 1);
    if (r == SF_OK) r = sf_binary_writer_write_bool(bw, value);
    if (r == SF_OK) r = mtd_b_close(bw, &val);
    if (r == SF_OK) r = mtd_w_marker(bw, 0x04);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = mtd_b_close(bw, &param);
    return r;
}

static sf_result_t write_texture_block_simple(sf_binary_writer_t *bw,
                                              const char *type, int32_t uv_number,
                                              int32_t shader_data_index) {
    mtd_block_t tex;
    sf_result_t r = mtd_b_open(bw, 0x2000, 3, 0xA3, &tex);
    if (r == SF_OK) r = mtd_w_marked_string(bw, 0x35, type);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, uv_number);
    if (r == SF_OK) r = mtd_w_marker(bw, 0x35);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, shader_data_index);
    if (r == SF_OK) r = mtd_b_close(bw, &tex);
    return r;
}

static sf_result_t build_canonical_mtd_bytes(uint8_t **out_bytes, size_t *out_size) {
    sf_ostream_t *os = NULL;
    sf_binary_writer_t *bw = NULL;
    sf_result_t r = sf_ostream_open_memory(&os, NULL);
    if (r != SF_OK) return r;
    r = sf_binary_writer_create(&bw, os, false, NULL);
    if (r != SF_OK) { sf_ostream_close(os); return r; }

    mtd_block_t file_block;
    if (r == SF_OK) r = mtd_b_open(bw, 0, 3, 0x01, &file_block);

    mtd_block_t header_block;
    if (r == SF_OK) r = mtd_b_open(bw, 1, 2, 0xB0, &header_block);
    if (r == SF_OK) r = mtd_w_marked_string(bw, 0x34, "MTD ");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 1000);
    if (r == SF_OK) r = mtd_b_close(bw, &header_block);
    if (r == SF_OK) r = mtd_w_marker(bw, 0x01);

    mtd_block_t data_block;
    if (r == SF_OK) r = mtd_b_open(bw, 2, 4, 0xA3, &data_block);
    if (r == SF_OK) r = mtd_w_marked_string(bw, 0xA3, "shaders/test.spx");
    if (r == SF_OK) r = mtd_w_marked_string(bw, 0x03, "Synthetic MTD");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 1);

    mtd_block_t lists_block;
    if (r == SF_OK) r = mtd_b_open(bw, 3, 4, 0xA3, &lists_block);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = mtd_w_marker(bw, 0x03);

    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 3);
    if (r == SF_OK) r = write_param_block_int(bw, "g_BlendMode", 0);
    if (r == SF_OK) r = write_param_block_float(bw, "g_Roughness", 0.5f);
    if (r == SF_OK) r = write_param_block_bool(bw, "g_DoubleSided", true);

    if (r == SF_OK) r = mtd_w_marker(bw, 0x03);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 2);
    if (r == SF_OK) r = write_texture_block_simple(bw, "g_DiffuseTexture", 1, 0);
    if (r == SF_OK) r = write_texture_block_simple(bw, "g_NormalTexture",  1, 0);

    if (r == SF_OK) r = mtd_w_marker(bw, 0x04);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = mtd_b_close(bw, &lists_block);
    if (r == SF_OK) r = mtd_w_marker(bw, 0x04);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = mtd_b_close(bw, &data_block);
    if (r == SF_OK) r = mtd_w_marker(bw, 0x04);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = mtd_b_close(bw, &file_block);

    if (r == SF_OK) r = sf_binary_writer_finish_bytes(bw, out_bytes, out_size);
    sf_binary_writer_destroy(bw);
    sf_ostream_close(os);
    return r;
}

static void verify_mtd_fields(const sf_mtd_t *mtd) {
    TEST_ASSERT_NOT_NULL(mtd);
    TEST_ASSERT_EQUAL_STRING("shaders/test.spx", sf_mtd_shader_path(mtd));
    TEST_ASSERT_EQUAL_STRING("Synthetic MTD",    sf_mtd_description(mtd));
    TEST_ASSERT_EQUAL_size_t(3u, sf_mtd_param_count(mtd));
    TEST_ASSERT_EQUAL_size_t(2u, sf_mtd_texture_count(mtd));

    const sf_mtd_param_t *p0 = sf_mtd_param(mtd, 0);
    TEST_ASSERT_NOT_NULL(p0);
    TEST_ASSERT_EQUAL_STRING("g_BlendMode", sf_mtd_param_name(p0));
    TEST_ASSERT_EQUAL_UINT32(SF_MTD_PARAM_TYPE_INT,
                             (uint32_t)sf_mtd_param_type(p0));
    int32_t int_value = 999;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mtd_param_value_int(p0, &int_value));
    TEST_ASSERT_EQUAL_INT32(0, int_value);

    const sf_mtd_param_t *p1 = sf_mtd_param(mtd, 1);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_EQUAL_STRING("g_Roughness", sf_mtd_param_name(p1));
    TEST_ASSERT_EQUAL_UINT32(SF_MTD_PARAM_TYPE_FLOAT,
                             (uint32_t)sf_mtd_param_type(p1));
    float float_value = 0.f;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mtd_param_value_float(p1, &float_value));
    TEST_ASSERT_EQUAL_FLOAT(0.5f, float_value);

    const sf_mtd_param_t *p2 = sf_mtd_param(mtd, 2);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT_EQUAL_STRING("g_DoubleSided", sf_mtd_param_name(p2));
    TEST_ASSERT_EQUAL_UINT32(SF_MTD_PARAM_TYPE_BOOL,
                             (uint32_t)sf_mtd_param_type(p2));
    bool bool_value = false;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mtd_param_value_bool(p2, &bool_value));
    TEST_ASSERT_TRUE(bool_value);

    const sf_mtd_texture_t *t0 = sf_mtd_texture(mtd, 0);
    TEST_ASSERT_NOT_NULL(t0);
    TEST_ASSERT_EQUAL_STRING("g_DiffuseTexture", sf_mtd_texture_type(t0));
    TEST_ASSERT_EQUAL_INT32(1, sf_mtd_texture_uv_number(t0));
    TEST_ASSERT_EQUAL_INT32(0, sf_mtd_texture_shader_data_index(t0));
    TEST_ASSERT_FALSE(sf_mtd_texture_has_extended(t0));

    const sf_mtd_texture_t *t1 = sf_mtd_texture(mtd, 1);
    TEST_ASSERT_NOT_NULL(t1);
    TEST_ASSERT_EQUAL_STRING("g_NormalTexture", sf_mtd_texture_type(t1));
    TEST_ASSERT_EQUAL_INT32(1, sf_mtd_texture_uv_number(t1));
    TEST_ASSERT_EQUAL_INT32(0, sf_mtd_texture_shader_data_index(t1));
    TEST_ASSERT_FALSE(sf_mtd_texture_has_extended(t1));
}

static void test_mtd_synthetic_round_trip_byte_identical(void) {
    uint8_t *in_bytes = NULL;
    size_t   in_size  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, build_canonical_mtd_bytes(&in_bytes, &in_size));
    TEST_ASSERT_NOT_NULL(in_bytes);
    TEST_ASSERT_TRUE(in_size > 0u);

    sf_mtd_t *mtd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mtd_read_from_memory(&mtd, in_bytes, in_size, NULL));
    verify_mtd_fields(mtd);

    uint8_t *out_bytes = NULL;
    size_t   out_size  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mtd_write_to_memory(mtd, &out_bytes, &out_size, NULL));
    TEST_ASSERT_NOT_NULL(out_bytes);
    TEST_ASSERT_EQUAL_size_t(in_size, out_size);
    TEST_ASSERT_EQUAL_MEMORY(in_bytes, out_bytes, in_size);

    sf_free(NULL, out_bytes);
    sf_mtd_destroy(mtd);
    sf_free(NULL, in_bytes);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mtd_synthetic_round_trip_byte_identical);
    return UNITY_END();
}
