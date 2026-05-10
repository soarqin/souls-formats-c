/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "script/emevd_internal.h"

#include <string.h>

sf_result_t sfi_emevd_instruction_read(sf_binary_reader_t *br, sf_emevd_format_t format,
                                       const sf_emevd_offsets_t *offsets,
                                       sf_emevd_instruction_t *out,
                                       const sf_allocator_t *alloc) {
    SF_CHECK_ARG(br != NULL && offsets != NULL && out != NULL);
    memset(out, 0, sizeof(*out));

    sf_result_t r = sf_binary_reader_read_i32(br, &out->bank); if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &out->id); if (r != SF_OK) return r;
    int64_t args_length_i64 = 0;
    int64_t args_offset = 0;
    r = sf_binary_reader_read_varint(br, &args_length_i64); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &args_offset); if (r != SF_OK) return r;

    int64_t layer_offset = -1;
    if (sf_emevd_format_is_ds3_or_later(format)) {
        r = sf_binary_reader_read_i64(br, &layer_offset); if (r != SF_OK) return r;
    } else {
        int32_t layer_offset_i32 = 0;
        r = sf_binary_reader_read_i32(br, &layer_offset_i32); if (r != SF_OK) return r;
        layer_offset = layer_offset_i32;
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
    }

    r = sfi_emevd_i64_to_size(args_length_i64, &out->arg_data_size);
    if (r != SF_OK) return r;
    if (out->arg_data_size > 0) {
        out->arg_data = (uint8_t *)sf_xalloc(alloc, out->arg_data_size);
        if (!out->arg_data) return SF_ERR_OOM;
        r = sf_binary_reader_get_bytes(br, offsets->arguments + args_offset, out->arg_data,
                                       out->arg_data_size);
        if (r != SF_OK) {
            sf_xfree(alloc, out->arg_data);
            out->arg_data = NULL;
            out->arg_data_size = 0;
            return r;
        }
    }

    if (layer_offset != -1) {
        r = sf_binary_reader_step_in(br, offsets->layers + layer_offset);
        if (r != SF_OK) goto fail;
        r = sfi_emevd_layer_read(br, &out->layer);
        sf_result_t step_r = sf_binary_reader_step_out(br);
        if (r != SF_OK) goto fail;
        if (step_r != SF_OK) { r = step_r; goto fail; }
        out->has_layer = true;
    }

    return SF_OK;

fail:
    sf_xfree(alloc, out->arg_data);
    memset(out, 0, sizeof(*out));
    return r;
}

int32_t sf_emevd_instruction_get_bank(const sf_emevd_instruction_t *instr) {
    return instr ? instr->bank : 0;
}

int32_t sf_emevd_instruction_get_id(const sf_emevd_instruction_t *instr) {
    return instr ? instr->id : 0;
}

sf_result_t sf_emevd_instruction_get_arg_data(const sf_emevd_instruction_t *instr,
                                             const uint8_t **out_data, size_t *out_size) {
    SF_CHECK_ARG(out_data != NULL && out_size != NULL);
    *out_data = instr ? instr->arg_data : NULL;
    *out_size = instr ? instr->arg_data_size : 0;
    return instr ? SF_OK : SF_ERR_INVALID_ARG;
}

const sf_emevd_layer_t *sf_emevd_instruction_get_layer(const sf_emevd_instruction_t *instr) {
    if (!instr || !instr->has_layer) return NULL;
    return &instr->layer;
}
