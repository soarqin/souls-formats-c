/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const uint8_t k_payload[] = {
    'S', 'F', 'U', 't', 'i', 'l', '_', 'p',
    'a', 'y', 'l', 'o', 'a', 'd', '_', '_',
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0xFE, 0xED, 0xFA, 0xCE, 0xCA, 0xFE, 0xBA, 0xBE,
};

static void test_get_decompressed_reader_dcx_path_creates_new_reader(void) {
    sf_dcx_compression_info_t comp_info;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_dcx_compression_info_from_dflt_preset(
                              SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_9,
                              &comp_info));

    uint8_t *compressed = NULL;
    size_t compressed_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_dcx_compress_to_buffer(k_payload, sizeof(k_payload),
                                                    &comp_info, &compressed,
                                                    &compressed_size, NULL));
    TEST_ASSERT_NOT_NULL(compressed);
    TEST_ASSERT_TRUE(compressed_size > 4u);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((const uint8_t *)"DCX\0", compressed, 4u);

    sf_istream_t *in_stream = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_istream_open_memory(&in_stream, compressed,
                                                 compressed_size, NULL));

    sf_binary_reader_t *in_reader = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_binary_reader_create(&in_reader, in_stream,
                                                  /*big_endian=*/false, NULL));

    sf_binary_reader_t        *new_reader = NULL;
    sf_dcx_compression_info_t  out_info;
    memset(&out_info, 0, sizeof(out_info));

    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_get_decompressed_reader(in_reader, &new_reader,
                                                     &out_info, NULL));
    TEST_ASSERT_NOT_NULL(new_reader);
    TEST_ASSERT_TRUE(new_reader != in_reader);
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_DFLT, out_info.type);
    TEST_ASSERT_EQUAL_INT64((int64_t)sizeof(k_payload),
                            sf_binary_reader_length(new_reader));
    TEST_ASSERT_EQUAL_INT64(0, sf_binary_reader_position(new_reader));

    uint8_t got[sizeof(k_payload)];
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_binary_reader_read_u8s(new_reader, sizeof(k_payload),
                                                    got));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(k_payload, got, sizeof(k_payload));

    sf_binary_reader_destroy(new_reader);
    sf_binary_reader_destroy(in_reader);
    sf_istream_close(in_stream);
    sf_free(NULL, compressed);
}

static void test_get_decompressed_reader_raw_path_borrows_input(void) {
    sf_istream_t *in_stream = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_istream_open_memory(&in_stream, k_payload,
                                                 sizeof(k_payload), NULL));

    sf_binary_reader_t *in_reader = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_binary_reader_create(&in_reader, in_stream,
                                                  /*big_endian=*/false, NULL));

    sf_binary_reader_t        *new_reader = (sf_binary_reader_t *)(uintptr_t)0xdeadbeef;
    sf_dcx_compression_info_t  out_info;
    memset(&out_info, 0xff, sizeof(out_info));

    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_get_decompressed_reader(in_reader, &new_reader,
                                                     &out_info, NULL));
    TEST_ASSERT_EQUAL_PTR(in_reader, new_reader);
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_NONE, out_info.type);

    sf_binary_reader_destroy(in_reader);
    sf_istream_close(in_stream);
}

static void test_get_decompressed_reader_short_stream_treated_as_raw(void) {
    const uint8_t tiny[] = { 0x01, 0x02 };
    sf_istream_t *in_stream = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_istream_open_memory(&in_stream, tiny, sizeof(tiny),
                                                 NULL));

    sf_binary_reader_t *in_reader = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_binary_reader_create(&in_reader, in_stream,
                                                  /*big_endian=*/false, NULL));

    sf_binary_reader_t        *new_reader = NULL;
    sf_dcx_compression_info_t  out_info;
    memset(&out_info, 0xff, sizeof(out_info));

    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_get_decompressed_reader(in_reader, &new_reader,
                                                     &out_info, NULL));
    TEST_ASSERT_EQUAL_PTR(in_reader, new_reader);
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_NONE, out_info.type);

    sf_binary_reader_destroy(in_reader);
    sf_istream_close(in_stream);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_get_decompressed_reader_dcx_path_creates_new_reader);
    RUN_TEST(test_get_decompressed_reader_raw_path_borrows_input);
    RUN_TEST(test_get_decompressed_reader_short_stream_treated_as_raw);
    return UNITY_END();
}
