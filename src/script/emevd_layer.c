/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "script/emevd_internal.h"

sf_result_t sfi_emevd_layer_read(sf_binary_reader_t *br, sf_emevd_layer_t *out) {
    SF_CHECK_ARG(br != NULL && out != NULL);
    sf_result_t r = sf_binary_reader_assert_i32_one(br, 2); if (r != SF_OK) return r;
    r = sf_binary_reader_read_u32(br, &out->mask); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_varint_one(br, 0); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_varint_one(br, -1); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_varint_one(br, 1); if (r != SF_OK) return r;
    return SF_OK;
}

uint32_t sf_emevd_layer_get_mask(const sf_emevd_layer_t *layer) {
    return layer ? layer->mask : 0;
}

sf_result_t sfi_emevd_layer_write(sf_binary_writer_t *bw, sf_emevd_format_t format,
                                  const sf_emevd_layer_t *layer) {
    (void)format;
    SF_CHECK_ARG(bw != NULL && layer != NULL);
    sf_result_t r = sf_binary_writer_write_i32(bw, 2); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u32(bw, layer->mask); if (r != SF_OK) return r;
    r = sf_binary_writer_write_varint(bw, 0); if (r != SF_OK) return r;
    r = sf_binary_writer_write_varint(bw, -1); if (r != SF_OK) return r;
    return sf_binary_writer_write_varint(bw, 1);
}
