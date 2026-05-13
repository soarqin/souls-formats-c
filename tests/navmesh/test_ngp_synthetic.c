/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_ngp.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Construct a minimum valid NGP file: 32-byte header + 4 zero varint
 * offsets (16 bytes for Vanilla version) + 0 meshes. All counts are 0 so no
 * data follows. */
static sf_result_t build_minimal_ngp_le_vanilla(uint8_t **out, size_t *out_size) {
    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_memory(&os, NULL);
    if (r != SF_OK) return r;
    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, false, NULL);
    if (r != SF_OK) { sf_ostream_close(os); return r; }

    r = sf_binary_writer_write_ascii(bw, "NVG2", false);
    if (r == SF_OK) r = sf_binary_writer_write_u16(bw, 1);
    if (r == SF_OK) r = sf_binary_writer_write_i16(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0xCAFE);
    if (r == SF_OK) sf_binary_writer_set_varint_long(bw, false);
    if (r == SF_OK) r = sf_binary_writer_write_varint(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_varint(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_varint(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_varint(bw, 0);
    if (r != SF_OK) {
        sf_binary_writer_destroy(bw);
        sf_ostream_close(os);
        return r;
    }
    uint8_t *bytes = NULL;
    size_t bytes_len = 0;
    r = sf_binary_writer_finish_bytes(bw, &bytes, &bytes_len);
    sf_ostream_close(os);
    if (r != SF_OK) return r;
    *out = bytes;
    *out_size = bytes_len;
    return SF_OK;
}

static void test_ngp_synthetic_roundtrip(void) {
    uint8_t *src_bytes = NULL;
    size_t   src_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, build_minimal_ngp_le_vanilla(&src_bytes, &src_size));
    TEST_ASSERT_NOT_NULL(src_bytes);

    sf_ngp_t *ngp = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ngp_read_from_memory(&ngp, src_bytes, src_size, NULL));
    TEST_ASSERT_NOT_NULL(ngp);
    TEST_ASSERT_FALSE(sf_ngp_big_endian(ngp));
    TEST_ASSERT_EQUAL_INT(SF_NGP_VERSION_VANILLA, sf_ngp_version(ngp));
    TEST_ASSERT_EQUAL_INT(0xCAFE, sf_ngp_unk1c(ngp));
    TEST_ASSERT_EQUAL_INT(0, sf_ngp_mesh_count(ngp));
    TEST_ASSERT_EQUAL_INT(0, sf_ngp_struct_a_count(ngp));

    void *bytes2 = NULL;
    size_t size2 = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ngp_write_to_memory(ngp, &bytes2, &size2, NULL));
    TEST_ASSERT_EQUAL_size_t(src_size, size2);
    TEST_ASSERT_EQUAL_MEMORY(src_bytes, bytes2, src_size);

    sf_free(NULL, src_bytes);
    sf_free(NULL, bytes2);
    sf_ngp_destroy(ngp);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ngp_synthetic_roundtrip);
    return UNITY_END();
}
