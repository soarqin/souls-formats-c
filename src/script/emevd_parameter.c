/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "script/emevd_internal.h"

#include <string.h>

sf_result_t sfi_emevd_parameter_read(sf_binary_reader_t *br, sf_emevd_parameter_t *out) {
    SF_CHECK_ARG(br != NULL && out != NULL);
    memset(out, 0, sizeof(*out));
    sf_result_t r = sf_binary_reader_read_varint(br, &out->instruction_index);
    if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &out->target_start_byte); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &out->source_start_byte); if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &out->byte_count); if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &out->unk_id); if (r != SF_OK) return r;
    return SF_OK;
}

int64_t sf_emevd_parameter_get_instruction_index(const sf_emevd_parameter_t *parameter) {
    return parameter ? parameter->instruction_index : 0;
}

int64_t sf_emevd_parameter_get_target_start_byte(const sf_emevd_parameter_t *parameter) {
    return parameter ? parameter->target_start_byte : 0;
}

int64_t sf_emevd_parameter_get_source_start_byte(const sf_emevd_parameter_t *parameter) {
    return parameter ? parameter->source_start_byte : 0;
}

int32_t sf_emevd_parameter_get_byte_count(const sf_emevd_parameter_t *parameter) {
    return parameter ? parameter->byte_count : 0;
}

int32_t sf_emevd_parameter_get_unk_id(const sf_emevd_parameter_t *parameter) {
    return parameter ? parameter->unk_id : 0;
}

sf_result_t sfi_emevd_parameter_write(sf_binary_writer_t *bw,
                                      const sf_emevd_parameter_t *parameter) {
    SF_CHECK_ARG(bw != NULL && parameter != NULL);
    sf_result_t r = sf_binary_writer_write_varint(bw, parameter->instruction_index);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_varint(bw, parameter->target_start_byte); if (r != SF_OK) return r;
    r = sf_binary_writer_write_varint(bw, parameter->source_start_byte); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, parameter->byte_count); if (r != SF_OK) return r;
    return sf_binary_writer_write_i32(bw, parameter->unk_id);
}
