/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "script/emevd_internal.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

void sfi_emevd_event_free(sf_emevd_event_t *event, const sf_allocator_t *alloc) {
    if (!event) return;
    for (size_t i = 0; i < event->instruction_count; i++) {
        sf_xfree(alloc, event->instructions[i].arg_data);
    }
    sf_xfree(alloc, event->instructions);
    sf_xfree(alloc, event->parameters);
    memset(event, 0, sizeof(*event));
}

static sf_result_t emevd_alloc_event_arrays(sf_emevd_event_t *event,
                                            const sf_allocator_t *alloc) {
    if (event->instruction_count > 0) {
        if (event->instruction_count > SIZE_MAX / sizeof(*event->instructions)) {
            return SF_ERR_OUT_OF_RANGE;
        }
        size_t bytes = event->instruction_count * sizeof(*event->instructions);
        event->instructions = (sf_emevd_instruction_t *)sf_xalloc(alloc, bytes);
        if (!event->instructions) return SF_ERR_OOM;
        memset(event->instructions, 0, bytes);
    }
    if (event->parameter_count > 0) {
        if (event->parameter_count > SIZE_MAX / sizeof(*event->parameters)) {
            return SF_ERR_OUT_OF_RANGE;
        }
        size_t bytes = event->parameter_count * sizeof(*event->parameters);
        event->parameters = (sf_emevd_parameter_t *)sf_xalloc(alloc, bytes);
        if (!event->parameters) return SF_ERR_OOM;
        memset(event->parameters, 0, bytes);
    }
    return SF_OK;
}

sf_result_t sfi_emevd_event_read(sf_binary_reader_t *br, sf_emevd_format_t format,
                                 const sf_emevd_offsets_t *offsets,
                                 sf_emevd_event_t *out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(br != NULL && offsets != NULL && out != NULL);
    memset(out, 0, sizeof(*out));

    int64_t instruction_count_i64 = 0;
    int64_t instruction_offset = 0;
    int64_t parameter_count_i64 = 0;
    int64_t parameter_offset = 0;
    sf_result_t r = sf_binary_reader_read_varint(br, &out->id); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &instruction_count_i64); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &instruction_offset); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &parameter_count_i64); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &parameter_offset); if (r != SF_OK) return r;
    uint32_t rest_behavior = 0;
    const uint32_t rest_options[3] = {SF_EMEVD_REST_BEHAVIOR_DEFAULT,
                                      SF_EMEVD_REST_BEHAVIOR_RESTART,
                                      SF_EMEVD_REST_BEHAVIOR_END};
    r = sf_binary_reader_read_enum_32(br, 3, rest_options, &rest_behavior);
    if (r != SF_OK) return r;
    out->rest_behavior = (sf_emevd_rest_behavior_t)rest_behavior;
    r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;

    r = sfi_emevd_i64_to_size(instruction_count_i64, &out->instruction_count);
    if (r != SF_OK) return r;
    r = sfi_emevd_i64_to_size(parameter_count_i64, &out->parameter_count);
    if (r != SF_OK) return r;
    r = emevd_alloc_event_arrays(out, alloc);
    if (r != SF_OK) goto fail;

    if (out->instruction_count > 0) {
        r = sf_binary_reader_step_in(br, offsets->instructions + instruction_offset);
        if (r != SF_OK) goto fail;
        for (size_t i = 0; i < out->instruction_count; i++) {
            r = sfi_emevd_instruction_read(br, format, offsets, &out->instructions[i], alloc);
            if (r != SF_OK) {
                (void)sf_binary_reader_step_out(br);
                goto fail;
            }
        }
        r = sf_binary_reader_step_out(br); if (r != SF_OK) goto fail;
    }

    if (out->parameter_count > 0) {
        r = sf_binary_reader_step_in(br, offsets->parameters + parameter_offset);
        if (r != SF_OK) goto fail;
        for (size_t i = 0; i < out->parameter_count; i++) {
            r = sfi_emevd_parameter_read(br, &out->parameters[i]);
            if (r != SF_OK) {
                (void)sf_binary_reader_step_out(br);
                goto fail;
            }
        }
        r = sf_binary_reader_step_out(br); if (r != SF_OK) goto fail;
    }

    return SF_OK;

fail:
    sfi_emevd_event_free(out, alloc);
    return r;
}

int64_t sf_emevd_event_get_id(const sf_emevd_event_t *event) {
    return event ? event->id : 0;
}

sf_emevd_rest_behavior_t sf_emevd_event_get_rest_behavior(const sf_emevd_event_t *event) {
    return event ? event->rest_behavior : SF_EMEVD_REST_BEHAVIOR_DEFAULT;
}

size_t sf_emevd_event_get_instruction_count(const sf_emevd_event_t *event) {
    return event ? event->instruction_count : 0;
}

const sf_emevd_instruction_t *sf_emevd_event_get_instruction(const sf_emevd_event_t *event,
                                                            size_t index) {
    if (!event || index >= event->instruction_count) return NULL;
    return &event->instructions[index];
}

size_t sf_emevd_event_get_parameter_count(const sf_emevd_event_t *event) {
    return event ? event->parameter_count : 0;
}

const sf_emevd_parameter_t *sf_emevd_event_get_parameter(const sf_emevd_event_t *event,
                                                         size_t index) {
    if (!event || index >= event->parameter_count) return NULL;
    return &event->parameters[index];
}

static sf_result_t emevd_event_name(char *name, size_t name_size, const char *suffix,
                                    size_t event_index) {
    int n = snprintf(name, name_size, "Event%zu%s", event_index, suffix);
    if (n < 0 || (size_t)n >= name_size) return SF_ERR_OUT_OF_RANGE;
    return SF_OK;
}

sf_result_t sfi_emevd_event_write(sf_binary_writer_t *bw, sf_emevd_format_t format,
                                  const sf_emevd_event_t *event, size_t event_index) {
    SF_CHECK_ARG(bw != NULL && event != NULL);
    char name[64];

    sf_result_t r = sf_binary_writer_write_varint(bw, event->id); if (r != SF_OK) return r;
    r = sf_binary_writer_write_varint(bw, (int64_t)event->instruction_count);
    if (r != SF_OK) return r;
    r = emevd_event_name(name, sizeof(name), "InstrsOffset", event_index);
    if (r != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
    r = sf_binary_writer_write_varint(bw, (int64_t)event->parameter_count);
    if (r != SF_OK) return r;

    r = emevd_event_name(name, sizeof(name), "ParamsOffset", event_index);
    if (r != SF_OK) return r;
    if (format < SF_EMEVD_FORMAT_BLOODBORNE) {
        r = sf_binary_writer_reserve_i32(bw, name);
    } else if (format < SF_EMEVD_FORMAT_DARK_SOULS_3) {
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, name), return r);
        r = sf_binary_writer_write_i32(bw, 0);
    } else {
        r = sf_binary_writer_reserve_i64(bw, name);
    }
    if (r != SF_OK) return r;

    r = sf_binary_writer_write_u32(bw, (uint32_t)event->rest_behavior); if (r != SF_OK) return r;
    return sf_binary_writer_write_i32(bw, 0);
}

sf_result_t sfi_emevd_event_write_instructions(sf_binary_writer_t *bw,
                                               sf_emevd_format_t format,
                                               const sf_emevd_offsets_t *offsets,
                                               const sf_emevd_event_t *event,
                                               size_t event_index) {
    SF_CHECK_ARG(bw != NULL && offsets != NULL && event != NULL);
    char name[64];
    sf_result_t r = emevd_event_name(name, sizeof(name), "InstrsOffset", event_index);
    if (r != SF_OK) return r;
    int64_t offset = event->instruction_count > 0
        ? sf_binary_writer_position(bw) - offsets->instructions
        : -1;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, name, offset), return r);

    for (size_t i = 0; i < event->instruction_count; i++) {
        r = sfi_emevd_instruction_write(bw, format, &event->instructions[i], event_index, i);
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

sf_result_t sfi_emevd_event_write_parameters(sf_binary_writer_t *bw,
                                             sf_emevd_format_t format,
                                             const sf_emevd_offsets_t *offsets,
                                             const sf_emevd_event_t *event,
                                             size_t event_index) {
    SF_CHECK_ARG(bw != NULL && offsets != NULL && event != NULL);
    char name[64];
    sf_result_t r = emevd_event_name(name, sizeof(name), "ParamsOffset", event_index);
    if (r != SF_OK) return r;
    int64_t offset = event->parameter_count > 0
        ? sf_binary_writer_position(bw) - offsets->parameters
        : -1;
    if (format < SF_EMEVD_FORMAT_DARK_SOULS_3) {
        if (offset < INT32_MIN || offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
        r = sf_binary_writer_fill_i32(bw, name, (int32_t)offset);
    } else {
        r = sf_binary_writer_fill_i64(bw, name, offset);
    }
    if (r != SF_OK) return r;

    for (size_t i = 0; i < event->parameter_count; i++) {
        r = sfi_emevd_parameter_write(bw, &event->parameters[i]);
        if (r != SF_OK) return r;
    }
    return SF_OK;
}
