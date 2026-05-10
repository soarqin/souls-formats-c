/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "script/emevd_internal.h"

#include <limits.h>
#include <stdio.h>
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

static sf_result_t emevd_instr_name(char *name, size_t name_size, const char *suffix,
                                    size_t event_index, size_t instr_index) {
    int n = snprintf(name, name_size, "Event%zuInstr%zu%s", event_index, instr_index, suffix);
    if (n < 0 || (size_t)n >= name_size) return SF_ERR_OUT_OF_RANGE;
    return SF_OK;
}

sf_result_t sfi_emevd_instruction_write(sf_binary_writer_t *bw, sf_emevd_format_t format,
                                        const sf_emevd_instruction_t *instr,
                                        size_t event_index, size_t instr_index) {
    SF_CHECK_ARG(bw != NULL && instr != NULL);
    char name[80];

    sf_result_t r = sf_binary_writer_write_i32(bw, instr->bank); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, instr->id); if (r != SF_OK) return r;
    r = sf_binary_writer_write_varint(bw, (int64_t)instr->arg_data_size);
    if (r != SF_OK) return r;

    r = emevd_instr_name(name, sizeof(name), "ArgsOffset", event_index, instr_index);
    if (r != SF_OK) return r;
    if (format < SF_EMEVD_FORMAT_BLOODBORNE) {
        r = sf_binary_writer_reserve_i32(bw, name);
    } else if (format < SF_EMEVD_FORMAT_SEKIRO) {
        r = sf_binary_writer_reserve_i32(bw, name); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, 0);
    } else {
        r = sf_binary_writer_reserve_i64(bw, name);
    }
    if (r != SF_OK) return r;

    r = emevd_instr_name(name, sizeof(name), "LayerOffset", event_index, instr_index);
    if (r != SF_OK) return r;
    if (format < SF_EMEVD_FORMAT_DARK_SOULS_3) {
        r = sf_binary_writer_reserve_i32(bw, name); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, 0);
    } else {
        r = sf_binary_writer_reserve_i64(bw, name);
    }
    return r;
}

sf_result_t sfi_emevd_instruction_write_args(sf_binary_writer_t *bw,
                                             sf_emevd_format_t format,
                                             const sf_emevd_offsets_t *offsets,
                                             const sf_emevd_instruction_t *instr,
                                             size_t event_index, size_t instr_index) {
    SF_CHECK_ARG(bw != NULL && offsets != NULL && instr != NULL);
    char name[80];
    sf_result_t r = emevd_instr_name(name, sizeof(name), "ArgsOffset", event_index,
                                    instr_index);
    if (r != SF_OK) return r;
    int64_t offset = instr->arg_data_size > 0
        ? sf_binary_writer_position(bw) - offsets->arguments
        : -1;
    if (format < SF_EMEVD_FORMAT_SEKIRO) {
        if (offset < INT32_MIN || offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
        r = sf_binary_writer_fill_i32(bw, name, (int32_t)offset);
    } else {
        r = sf_binary_writer_fill_i64(bw, name, offset);
    }
    if (r != SF_OK) return r;
    if (instr->arg_data_size > 0) {
        if (!instr->arg_data) return SF_ERR_INVALID_ARG;
        r = sf_binary_writer_write_bytes(bw, instr->arg_data, instr->arg_data_size);
        if (r != SF_OK) return r;
    }
    return sf_binary_writer_pad(bw, 4);
}

sf_result_t sfi_emevd_instruction_fill_layer_offset(sf_binary_writer_t *bw,
                                                    sf_emevd_format_t format,
                                                    const sf_emevd_instruction_t *instr,
                                                    const uint32_t *layers,
                                                    const int64_t *layer_offsets,
                                                    size_t layer_count,
                                                    size_t event_index,
                                                    size_t instr_index) {
    SF_CHECK_ARG(bw != NULL && instr != NULL);
    char name[80];
    sf_result_t r = emevd_instr_name(name, sizeof(name), "LayerOffset", event_index,
                                    instr_index);
    if (r != SF_OK) return r;

    int64_t offset = -1;
    if (instr->has_layer) {
        bool found = false;
        for (size_t i = 0; i < layer_count; i++) {
            if (layers[i] == instr->layer.mask) {
                offset = layer_offsets[i];
                found = true;
                break;
            }
        }
        if (!found) return SF_ERR_INTERNAL;
    }

    if (format < SF_EMEVD_FORMAT_DARK_SOULS_3) {
        if (offset < INT32_MIN || offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
        return sf_binary_writer_fill_i32(bw, name, (int32_t)offset);
    }
    return sf_binary_writer_fill_i64(bw, name, offset);
}
