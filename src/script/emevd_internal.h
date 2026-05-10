/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_SCRIPT_EMEVD_INTERNAL_H
#define SF_SCRIPT_EMEVD_INTERNAL_H

#include "internal/sf_internal.h"
#include "souls_formats/sf_emevd.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct sf_emevd_offsets {
    int64_t events;
    int64_t instructions;
    int64_t layers;
    int64_t parameters;
    int64_t linked_files;
    int64_t arguments;
    int64_t strings;
} sf_emevd_offsets_t;

struct sf_emevd_layer {
    uint32_t mask;
};

struct sf_emevd_parameter {
    int64_t instruction_index;
    int64_t target_start_byte;
    int64_t source_start_byte;
    int32_t byte_count;
    int32_t unk_id;
};

struct sf_emevd_instruction {
    int32_t bank;
    int32_t id;
    uint8_t *arg_data;
    size_t arg_data_size;
    bool has_layer;
    sf_emevd_layer_t layer;
};

struct sf_emevd_event {
    int64_t id;
    sf_emevd_rest_behavior_t rest_behavior;
    sf_emevd_instruction_t *instructions;
    size_t instruction_count;
    sf_emevd_parameter_t *parameters;
    size_t parameter_count;
};

struct sf_emevd {
    const sf_allocator_t *alloc;
    sf_emevd_format_t format;
    bool big_endian;
    bool is_64_bit;
    sf_emevd_event_t *events;
    size_t event_count;
    int64_t *linked_file_offsets;
    size_t linked_file_count;
    uint8_t *string_data;
    size_t string_data_size;
};

static inline bool sf_emevd_format_is_64_bit(sf_emevd_format_t format) {
    return format >= SF_EMEVD_FORMAT_BLOODBORNE;
}

static inline bool sf_emevd_format_is_ds3_or_later(sf_emevd_format_t format) {
    return format >= SF_EMEVD_FORMAT_DARK_SOULS_3;
}

sf_result_t sfi_emevd_i64_to_size(int64_t value, size_t *out);
void sfi_emevd_event_free(sf_emevd_event_t *event, const sf_allocator_t *alloc);
sf_result_t sfi_emevd_event_read(sf_binary_reader_t *br, sf_emevd_format_t format,
                                 const sf_emevd_offsets_t *offsets,
                                 sf_emevd_event_t *out, const sf_allocator_t *alloc);
sf_result_t sfi_emevd_instruction_read(sf_binary_reader_t *br, sf_emevd_format_t format,
                                       const sf_emevd_offsets_t *offsets,
                                       sf_emevd_instruction_t *out,
                                       const sf_allocator_t *alloc);
sf_result_t sfi_emevd_layer_read(sf_binary_reader_t *br, sf_emevd_layer_t *out);
sf_result_t sfi_emevd_parameter_read(sf_binary_reader_t *br, sf_emevd_parameter_t *out);

#endif /* SF_SCRIPT_EMEVD_INTERNAL_H */
