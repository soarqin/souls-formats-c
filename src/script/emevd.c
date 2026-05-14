/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * EMEVD reader. Mirrors SoulsFormats/Formats/EMEVD/EMEVD.cs Read().
 */

#include "script/emevd_internal.h"

#include <limits.h>
#include <string.h>

sf_result_t sfi_emevd_i64_to_size(int64_t value, size_t *out) {
    SF_CHECK_ARG(out != NULL);
    if (value < 0) return SF_ERR_OUT_OF_RANGE;
#if SIZE_MAX < INT64_MAX
    if ((uint64_t)value > (uint64_t)SIZE_MAX) return SF_ERR_OUT_OF_RANGE;
#endif
    *out = (size_t)value;
    return SF_OK;
}

static sf_result_t emevd_mul_size(size_t count, size_t elem_size, size_t *out) {
    SF_CHECK_ARG(out != NULL && elem_size > 0);
    if (count > SIZE_MAX / elem_size) return SF_ERR_OUT_OF_RANGE;
    *out = count * elem_size;
    return SF_OK;
}

static sf_result_t emevd_count_to_i64(size_t count, int64_t *out) {
    SF_CHECK_ARG(out != NULL);
#if SIZE_MAX > INT64_MAX
    if (count > (size_t)INT64_MAX) return SF_ERR_OUT_OF_RANGE;
#endif
    *out = (int64_t)count;
    return SF_OK;
}

static sf_result_t emevd_position_to_i32(int64_t value, int32_t *out) {
    SF_CHECK_ARG(out != NULL);
    if (value < INT32_MIN || value > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    *out = (int32_t)value;
    return SF_OK;
}

static sf_result_t emevd_read_header(sf_binary_reader_t *br, sf_emevd_t *emevd,
                                     sf_emevd_offsets_t *offsets,
                                     int64_t *out_event_count,
                                     int64_t *out_linked_file_count,
                                     int64_t *out_string_data_size) {
    SF_CHECK_ARG(br != NULL && emevd != NULL && offsets != NULL && out_event_count != NULL &&
                 out_linked_file_count != NULL && out_string_data_size != NULL);

    uint8_t magic[4] = {0, 0, 0, 0};
    sf_result_t r = sf_binary_reader_read_bytes(br, magic, sizeof(magic));
    if (r != SF_OK) return r;
    static const uint8_t expected_magic[4] = {'E', 'V', 'D', 0};
    if (memcmp(magic, expected_magic, sizeof(expected_magic)) != 0) {
        return SF_ERR_BAD_MAGIC;
    }

    bool big_endian = false;
    r = sf_binary_reader_read_bool(br, &big_endian); if (r != SF_OK) return r;
    int8_t is64_flag = 0;
    const int8_t varint_flags[2] = {0, -1};
    r = sf_binary_reader_assert_i8(br, 2, varint_flags, &is64_flag); if (r != SF_OK) return r;
    bool unk06 = false;
    r = sf_binary_reader_read_bool(br, &unk06); if (r != SF_OK) return r;
    int8_t unk07_flag = 0;
    r = sf_binary_reader_assert_i8(br, 2, varint_flags, &unk07_flag); if (r != SF_OK) return r;

    const bool is64_bit = (is64_flag == -1);
    const bool unk07 = (unk07_flag == -1);
    sf_binary_reader_set_big_endian(br, big_endian);
    sf_binary_reader_set_varint_long(br, is64_bit);

    int32_t version = 0;
    const int32_t versions[2] = {0xCC, 0xCD};
    r = sf_binary_reader_assert_i32(br, 2, versions, &version); if (r != SF_OK) return r;
    int32_t file_size = 0;
    r = sf_binary_reader_read_i32(br, &file_size); if (r != SF_OK) return r;
    (void)file_size;

    if (!big_endian && !is64_bit && !unk06 && !unk07 && version == 0xCC) {
        emevd->format = SF_EMEVD_FORMAT_DARK_SOULS_1;
    } else if (big_endian && !is64_bit && !unk06 && !unk07 && version == 0xCC) {
        emevd->format = SF_EMEVD_FORMAT_DARK_SOULS_1_BE;
    } else if (!big_endian && is64_bit && !unk06 && !unk07 && version == 0xCC) {
        emevd->format = SF_EMEVD_FORMAT_BLOODBORNE;
    } else if (!big_endian && is64_bit && unk06 && !unk07 &&
               (version == 0xCC || version == 0xCD)) {
        emevd->format = SF_EMEVD_FORMAT_DARK_SOULS_3;
    } else if (!big_endian && is64_bit && unk06 && unk07 && version == 0xCD) {
        emevd->format = SF_EMEVD_FORMAT_SEKIRO;
    } else {
        return SF_ERR_UNSUPPORTED_VERSION;
    }
    emevd->big_endian = big_endian;
    emevd->is_64_bit = is64_bit;

    int64_t ignored = 0;
    r = sf_binary_reader_read_varint(br, out_event_count); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &offsets->events); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &ignored); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &offsets->instructions); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_varint_one(br, 0); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &ignored); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &ignored); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &offsets->layers); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &ignored); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &offsets->parameters); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, out_linked_file_count); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &offsets->linked_files); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &ignored); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &offsets->arguments); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, out_string_data_size); if (r != SF_OK) return r;
    r = sf_binary_reader_read_varint(br, &offsets->strings); if (r != SF_OK) return r;
    if (!is64_bit) {
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
    }

    return SF_OK;
}

static sf_result_t emevd_read(sf_binary_reader_t *br, sf_emevd_t **out,
                              const sf_allocator_t *alloc) {
    SF_CHECK_ARG(br != NULL && out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_emevd_t *emevd = (sf_emevd_t *)sf_xalloc(alloc, sizeof(*emevd));
    if (!emevd) return SF_ERR_OOM;
    memset(emevd, 0, sizeof(*emevd));
    emevd->alloc = alloc;

    sf_emevd_offsets_t offsets;
    memset(&offsets, 0, sizeof(offsets));
    int64_t event_count_i64 = 0;
    int64_t linked_file_count_i64 = 0;
    int64_t string_size_i64 = 0;
    sf_result_t r = emevd_read_header(br, emevd, &offsets, &event_count_i64,
                                      &linked_file_count_i64, &string_size_i64);
    if (r != SF_OK) goto fail;

    r = sfi_emevd_i64_to_size(event_count_i64, &emevd->event_count);
    if (r != SF_OK) goto fail;
    r = sfi_emevd_i64_to_size(linked_file_count_i64, &emevd->linked_file_count);
    if (r != SF_OK) goto fail;
    r = sfi_emevd_i64_to_size(string_size_i64, &emevd->string_data_size);
    if (r != SF_OK) goto fail;

    if (emevd->event_count > 0) {
        size_t bytes = 0;
        r = emevd_mul_size(emevd->event_count, sizeof(*emevd->events), &bytes);
        if (r != SF_OK) goto fail;
        emevd->events = (sf_emevd_event_t *)sf_xalloc(alloc, bytes);
        if (!emevd->events) { r = SF_ERR_OOM; goto fail; }
        memset(emevd->events, 0, bytes);
        r = sf_binary_reader_step_in(br, offsets.events); if (r != SF_OK) goto fail;
        for (size_t i = 0; i < emevd->event_count; i++) {
            r = sfi_emevd_event_read(br, emevd->format, &offsets, &emevd->events[i], alloc);
            if (r != SF_OK) {
                (void)sf_binary_reader_step_out(br);
                goto fail;
            }
        }
        r = sf_binary_reader_step_out(br); if (r != SF_OK) goto fail;
    }

    if (emevd->linked_file_count > 0) {
        size_t bytes = 0;
        r = emevd_mul_size(emevd->linked_file_count, sizeof(*emevd->linked_file_offsets), &bytes);
        if (r != SF_OK) goto fail;
        emevd->linked_file_offsets = (int64_t *)sf_xalloc(alloc, bytes);
        if (!emevd->linked_file_offsets) { r = SF_ERR_OOM; goto fail; }
        r = sf_binary_reader_get_varints(br, offsets.linked_files, emevd->linked_file_count,
                                         emevd->linked_file_offsets);
        if (r != SF_OK) goto fail;
    }

    if (emevd->string_data_size > 0) {
        emevd->string_data = (uint8_t *)sf_xalloc(alloc, emevd->string_data_size);
        if (!emevd->string_data) { r = SF_ERR_OOM; goto fail; }
        r = sf_binary_reader_get_bytes(br, offsets.strings, emevd->string_data,
                                       emevd->string_data_size);
        if (r != SF_OK) goto fail;
    }

    *out = emevd;
    return SF_OK;

fail:
    sf_emevd_destroy(emevd, NULL);
    return r;
}

sf_result_t sf_emevd_read_from_stream(sf_emevd_t **out, sf_istream_t *stream,
                                      const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && stream != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_binary_reader_t *br = NULL;
    sf_result_t r = sf_binary_reader_create(&br, stream, false, alloc);
    if (r != SF_OK) return r;
    r = emevd_read(br, out, alloc);
    sf_binary_reader_destroy(br);
    return r;
}

sf_result_t sf_emevd_read_from_memory(sf_emevd_t **out, const uint8_t *data, size_t size,
                                      const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || data != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_memory(&stream, data, size, alloc);
    if (r != SF_OK) return r;
    r = sf_emevd_read_from_stream(out, stream, alloc);
    sf_istream_close(stream);
    return r;
}

sf_result_t sf_emevd_read_from_path(sf_emevd_t **out, const wchar_t *path,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && path != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_wfile(&stream, path, alloc);
    if (r != SF_OK) return r;
    r = sf_emevd_read_from_stream(out, stream, alloc);
    sf_istream_close(stream);
    return r;
}

sf_result_t sf_emevd_create(const sf_allocator_t *alloc, sf_emevd_format_t format,
                            sf_emevd_t **out) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    if (format < SF_EMEVD_FORMAT_DARK_SOULS_1 || format > SF_EMEVD_FORMAT_SEKIRO) {
        return SF_ERR_INVALID_ARG;
    }
    alloc = sf_alloc_or_default(alloc);
    sf_emevd_t *emevd = (sf_emevd_t *)sf_xalloc(alloc, sizeof(*emevd));
    if (!emevd) return SF_ERR_OOM;
    memset(emevd, 0, sizeof(*emevd));
    emevd->alloc = alloc;
    emevd->format = format;
    emevd->big_endian = (format == SF_EMEVD_FORMAT_DARK_SOULS_1_BE);
    emevd->is_64_bit = sf_emevd_format_is_64_bit(format);
    *out = emevd;
    return SF_OK;
}

void sf_emevd_destroy(sf_emevd_t *emevd, const sf_allocator_t *alloc) {
    (void)alloc;
    if (!emevd) return;
    const sf_allocator_t *a = emevd->alloc;
    for (size_t i = 0; i < emevd->event_count; i++) {
        sfi_emevd_event_free(&emevd->events[i], a);
    }
    sf_xfree(a, emevd->events);
    sf_xfree(a, emevd->linked_file_offsets);
    sf_xfree(a, emevd->string_data);
    sf_xfree(a, emevd);
}

typedef struct emevd_write_props {
    bool big_endian;
    bool is_64_bit;
    bool unk06;
    bool unk07;
    int32_t version;
} emevd_write_props_t;

static sf_result_t emevd_write_props(sf_emevd_format_t format, emevd_write_props_t *out) {
    SF_CHECK_ARG(out != NULL);
    switch (format) {
        case SF_EMEVD_FORMAT_DARK_SOULS_1:
            *out = (emevd_write_props_t){false, false, false, false, 0xCC};
            return SF_OK;
        case SF_EMEVD_FORMAT_DARK_SOULS_1_BE:
            *out = (emevd_write_props_t){true, false, false, false, 0xCC};
            return SF_OK;
        case SF_EMEVD_FORMAT_BLOODBORNE:
            *out = (emevd_write_props_t){false, true, false, false, 0xCC};
            return SF_OK;
        case SF_EMEVD_FORMAT_DARK_SOULS_3:
            *out = (emevd_write_props_t){false, true, true, false, 0xCD};
            return SF_OK;
        case SF_EMEVD_FORMAT_SEKIRO:
            *out = (emevd_write_props_t){false, true, true, true, 0xCD};
            return SF_OK;
    }
    return SF_ERR_UNSUPPORTED_VERSION;
}

static sf_result_t emevd_total_instruction_count(const sf_emevd_t *emevd, int64_t *out) {
    SF_CHECK_ARG(emevd != NULL && out != NULL);
    size_t total = 0;
    for (size_t i = 0; i < emevd->event_count; i++) {
        if (emevd->events[i].instruction_count > SIZE_MAX - total) return SF_ERR_OUT_OF_RANGE;
        total += emevd->events[i].instruction_count;
    }
    return emevd_count_to_i64(total, out);
}

static sf_result_t emevd_total_parameter_count(const sf_emevd_t *emevd, int64_t *out) {
    SF_CHECK_ARG(emevd != NULL && out != NULL);
    size_t total = 0;
    for (size_t i = 0; i < emevd->event_count; i++) {
        if (emevd->events[i].parameter_count > SIZE_MAX - total) return SF_ERR_OUT_OF_RANGE;
        total += emevd->events[i].parameter_count;
    }
    return emevd_count_to_i64(total, out);
}

static sf_result_t emevd_collect_layers(const sf_emevd_t *emevd, uint32_t **out_layers,
                                        size_t *out_count, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(emevd != NULL && out_layers != NULL && out_count != NULL);
    *out_layers = NULL;
    *out_count = 0;

    for (size_t i = 0; i < emevd->event_count; i++) {
        const sf_emevd_event_t *event = &emevd->events[i];
        for (size_t j = 0; j < event->instruction_count; j++) {
            const sf_emevd_instruction_t *instr = &event->instructions[j];
            if (!instr->has_layer) continue;
            bool exists = false;
            for (size_t k = 0; k < *out_count; k++) {
                if ((*out_layers)[k] == instr->layer.mask) {
                    exists = true;
                    break;
                }
            }
            if (exists) continue;
            if (*out_count == SIZE_MAX / sizeof(**out_layers)) return SF_ERR_OUT_OF_RANGE;
            size_t old_bytes = *out_count * sizeof(**out_layers);
            size_t new_count = *out_count + 1;
            uint32_t *p = (uint32_t *)sf_xrealloc(alloc, *out_layers, old_bytes,
                                                  new_count * sizeof(**out_layers));
            if (!p) return SF_ERR_OOM;
            p[*out_count] = instr->layer.mask;
            *out_layers = p;
            *out_count = new_count;
        }
    }
    return SF_OK;
}

static sf_result_t emevd_validate_for_write(const sf_emevd_t *emevd) {
    SF_CHECK_ARG(emevd != NULL);
    if (emevd->event_count > 0 && !emevd->events) return SF_ERR_INVALID_ARG;
    if (emevd->linked_file_count > 0 && !emevd->linked_file_offsets) return SF_ERR_INVALID_ARG;
    if (emevd->string_data_size > 0 && !emevd->string_data) return SF_ERR_INVALID_ARG;
    for (size_t i = 0; i < emevd->event_count; i++) {
        const sf_emevd_event_t *event = &emevd->events[i];
        if (event->instruction_count > 0 && !event->instructions) return SF_ERR_INVALID_ARG;
        if (event->parameter_count > 0 && !event->parameters) return SF_ERR_INVALID_ARG;
    }
    return SF_OK;
}

sf_result_t sfi_emevd_write(sf_binary_writer_t *bw, const sf_emevd_t *emevd,
                            const sf_allocator_t *alloc) {
    SF_CHECK_ARG(bw != NULL && emevd != NULL);
    alloc = sf_alloc_or_default(alloc);

    emevd_write_props_t props;
    sf_result_t r = emevd_write_props(emevd->format, &props); if (r != SF_OK) return r;
    r = emevd_validate_for_write(emevd); if (r != SF_OK) return r;

    uint32_t *layers = NULL;
    int64_t *layer_offsets = NULL;
    size_t layer_count = 0;
    r = emevd_collect_layers(emevd, &layers, &layer_count, alloc);
    if (r != SF_OK) goto cleanup;
    if (layer_count > 0) {
        if (layer_count > SIZE_MAX / sizeof(*layer_offsets)) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
        layer_offsets = (int64_t *)sf_xalloc(alloc, layer_count * sizeof(*layer_offsets));
        if (!layer_offsets) { r = SF_ERR_OOM; goto cleanup; }
        memset(layer_offsets, 0, layer_count * sizeof(*layer_offsets));
    }

    int64_t event_count = 0;
    int64_t instruction_count = 0;
    int64_t parameter_count = 0;
    int64_t linked_file_count = 0;
    int64_t string_data_size = 0;
    int64_t layer_count_i64 = 0;
    r = emevd_count_to_i64(emevd->event_count, &event_count); if (r != SF_OK) goto cleanup;
    r = emevd_total_instruction_count(emevd, &instruction_count); if (r != SF_OK) goto cleanup;
    r = emevd_total_parameter_count(emevd, &parameter_count); if (r != SF_OK) goto cleanup;
    r = emevd_count_to_i64(emevd->linked_file_count, &linked_file_count); if (r != SF_OK) goto cleanup;
    r = emevd_count_to_i64(emevd->string_data_size, &string_data_size); if (r != SF_OK) goto cleanup;
    r = emevd_count_to_i64(layer_count, &layer_count_i64); if (r != SF_OK) goto cleanup;

    r = sf_binary_writer_write_bytes(bw, "EVD\0", 4); if (r != SF_OK) goto cleanup;
    r = sf_binary_writer_write_bool(bw, props.big_endian); if (r != SF_OK) goto cleanup;
    r = sf_binary_writer_write_i8(bw, props.is_64_bit ? (int8_t)-1 : (int8_t)0);
    if (r != SF_OK) goto cleanup;
    r = sf_binary_writer_write_bool(bw, props.unk06); if (r != SF_OK) goto cleanup;
    r = sf_binary_writer_write_i8(bw, props.unk07 ? (int8_t)-1 : (int8_t)0);
    if (r != SF_OK) goto cleanup;
    sf_binary_writer_set_big_endian(bw, props.big_endian);
    sf_binary_writer_set_varint_long(bw, props.is_64_bit);

    r = sf_binary_writer_write_i32(bw, props.version); if (r != SF_OK) goto cleanup;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, "FileSize"), goto cleanup);
    r = sf_binary_writer_write_varint(bw, event_count); if (r != SF_OK) goto cleanup;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "EventsOffset"), goto cleanup);
    r = sf_binary_writer_write_varint(bw, instruction_count); if (r != SF_OK) goto cleanup;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "InstructionsOffset"), goto cleanup);
    r = sf_binary_writer_write_varint(bw, 0); if (r != SF_OK) goto cleanup;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "Offset3"), goto cleanup);
    r = sf_binary_writer_write_varint(bw, layer_count_i64); if (r != SF_OK) goto cleanup;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "LayersOffset"), goto cleanup);
    r = sf_binary_writer_write_varint(bw, parameter_count); if (r != SF_OK) goto cleanup;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "ParametersOffset"), goto cleanup);
    r = sf_binary_writer_write_varint(bw, linked_file_count); if (r != SF_OK) goto cleanup;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "LinkedFilesOffset"), goto cleanup);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "ArgumentsLength"), goto cleanup);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "ArgumentsOffset"), goto cleanup);
    r = sf_binary_writer_write_varint(bw, string_data_size); if (r != SF_OK) goto cleanup;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "StringsOffset"), goto cleanup);
    if (!props.is_64_bit) {
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) goto cleanup;
    }

    sf_emevd_offsets_t offsets;
    memset(&offsets, 0, sizeof(offsets));
    offsets.events = sf_binary_writer_position(bw);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "EventsOffset", offsets.events), goto cleanup);
    for (size_t i = 0; i < emevd->event_count; i++) {
        r = sfi_emevd_event_write(bw, emevd->format, &emevd->events[i], i);
        if (r != SF_OK) goto cleanup;
    }

    offsets.instructions = sf_binary_writer_position(bw);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "InstructionsOffset", offsets.instructions), goto cleanup);
    for (size_t i = 0; i < emevd->event_count; i++) {
        r = sfi_emevd_event_write_instructions(bw, emevd->format, &offsets,
                                               &emevd->events[i], i);
        if (r != SF_OK) goto cleanup;
    }

    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "Offset3", sf_binary_writer_position(bw)), goto cleanup);

    offsets.layers = sf_binary_writer_position(bw);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "LayersOffset", offsets.layers), goto cleanup);
    for (size_t i = 0; i < layer_count; i++) {
        layer_offsets[i] = sf_binary_writer_position(bw) - offsets.layers;
        sf_emevd_layer_t layer = {.mask = layers[i]};
        r = sfi_emevd_layer_write(bw, emevd->format, &layer);
        if (r != SF_OK) goto cleanup;
    }
    for (size_t i = 0; i < emevd->event_count; i++) {
        const sf_emevd_event_t *event = &emevd->events[i];
        for (size_t j = 0; j < event->instruction_count; j++) {
            r = sfi_emevd_instruction_fill_layer_offset(bw, emevd->format,
                                                        &event->instructions[j], layers,
                                                        layer_offsets, layer_count, i, j);
            if (r != SF_OK) goto cleanup;
        }
    }

    offsets.arguments = sf_binary_writer_position(bw);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "ArgumentsOffset", offsets.arguments), goto cleanup);
    for (size_t i = 0; i < emevd->event_count; i++) {
        const sf_emevd_event_t *event = &emevd->events[i];
        for (size_t j = 0; j < event->instruction_count; j++) {
            r = sfi_emevd_instruction_write_args(bw, emevd->format, &offsets,
                                                 &event->instructions[j], i, j);
            if (r != SF_OK) goto cleanup;
        }
    }
    r = sf_binary_writer_pad_relative(bw, offsets.arguments, 0x10); if (r != SF_OK) goto cleanup;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "ArgumentsLength", sf_binary_writer_position(bw) - offsets.arguments), goto cleanup);

    offsets.parameters = sf_binary_writer_position(bw);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "ParametersOffset", offsets.parameters), goto cleanup);
    for (size_t i = 0; i < emevd->event_count; i++) {
        r = sfi_emevd_event_write_parameters(bw, emevd->format, &offsets,
                                             &emevd->events[i], i);
        if (r != SF_OK) goto cleanup;
    }

    offsets.linked_files = sf_binary_writer_position(bw);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "LinkedFilesOffset", offsets.linked_files), goto cleanup);
    for (size_t i = 0; i < emevd->linked_file_count; i++) {
        r = sf_binary_writer_write_varint(bw, emevd->linked_file_offsets[i]);
        if (r != SF_OK) goto cleanup;
    }

    offsets.strings = sf_binary_writer_position(bw);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "StringsOffset", offsets.strings), goto cleanup);
    if (emevd->string_data_size > 0) {
        r = sf_binary_writer_write_bytes(bw, emevd->string_data, emevd->string_data_size);
        if (r != SF_OK) goto cleanup;
    }

    int32_t file_size = 0;
    r = emevd_position_to_i32(sf_binary_writer_position(bw), &file_size);
    if (r != SF_OK) goto cleanup;
    r = sf_binary_writer_fill_i32(bw, "FileSize", file_size);

cleanup:
    sf_xfree(alloc, layer_offsets);
    sf_xfree(alloc, layers);
    return r;
}

sf_result_t sf_emevd_write_to_memory(const sf_emevd_t *emevd, uint8_t **out,
                                     size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(emevd != NULL && out != NULL && out_size != NULL);
    *out = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_memory(&stream, alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, stream, false, alloc);
    if (r != SF_OK) { sf_ostream_close(stream); return r; }

    r = sfi_emevd_write(bw, emevd, alloc);
    if (r == SF_OK) r = sf_binary_writer_finish_bytes(bw, out, out_size);
    sf_binary_writer_destroy(bw);
    sf_ostream_close(stream);
    return r;
}

sf_result_t sf_emevd_write_to_stream(const sf_emevd_t *emevd, sf_ostream_t *stream,
                                     const sf_allocator_t *alloc) {
    SF_CHECK_ARG(emevd != NULL && stream != NULL);
    alloc = sf_alloc_or_default(alloc);

    sf_binary_writer_t *bw = NULL;
    sf_result_t r = sf_binary_writer_create(&bw, stream, false, alloc);
    if (r != SF_OK) return r;
    r = sfi_emevd_write(bw, emevd, alloc);
    if (r == SF_OK) r = sf_binary_writer_finish(bw);
    sf_binary_writer_destroy(bw);
    return r;
}

sf_result_t sf_emevd_write_to_path(const sf_emevd_t *emevd, const wchar_t *path,
                                   const sf_allocator_t *alloc) {
    SF_CHECK_ARG(emevd != NULL && path != NULL);
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_wfile(&stream, path, alloc);
    if (r != SF_OK) return r;
    r = sf_emevd_write_to_stream(emevd, stream, alloc);
    sf_ostream_close(stream);
    return r;
}

sf_emevd_format_t sf_emevd_get_format(const sf_emevd_t *emevd) {
    return emevd ? emevd->format : SF_EMEVD_FORMAT_DARK_SOULS_1;
}

size_t sf_emevd_get_event_count(const sf_emevd_t *emevd) {
    return emevd ? emevd->event_count : 0;
}

const sf_emevd_event_t *sf_emevd_get_event(const sf_emevd_t *emevd, size_t index) {
    if (!emevd || index >= emevd->event_count) return NULL;
    return &emevd->events[index];
}

sf_result_t sf_emevd_event_find_by_id(sf_emevd_t *emevd, int64_t event_id,
                                      sf_emevd_event_t **out) {
    SF_CHECK_ARG(emevd != NULL && out != NULL);
    *out = NULL;
    for (size_t i = 0; i < emevd->event_count; i++) {
        if (emevd->events[i].id == event_id) {
            *out = &emevd->events[i];
            return SF_OK;
        }
    }
    return SF_ERR_NOT_FOUND;
}

sf_result_t sf_emevd_add_event(sf_emevd_t *emevd, int64_t event_id,
                               sf_emevd_rest_behavior_t rest, sf_emevd_event_t **out) {
    SF_CHECK_ARG(emevd != NULL && out != NULL);
    *out = NULL;
    if (rest < SF_EMEVD_REST_BEHAVIOR_DEFAULT || rest > SF_EMEVD_REST_BEHAVIOR_END) {
        return SF_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < emevd->event_count; i++) {
        if (emevd->events[i].id == event_id) return SF_ERR_ALREADY_EXISTS;
    }
    if (emevd->event_count == SIZE_MAX) return SF_ERR_OUT_OF_RANGE;
    if (emevd->event_count > SIZE_MAX / sizeof(*emevd->events) - 1) {
        return SF_ERR_OUT_OF_RANGE;
    }
    size_t old_bytes = emevd->event_count * sizeof(*emevd->events);
    size_t new_bytes = (emevd->event_count + 1) * sizeof(*emevd->events);
    sf_emevd_event_t *events = (sf_emevd_event_t *)sf_xrealloc(emevd->alloc, emevd->events,
                                                               old_bytes, new_bytes);
    if (!events) return SF_ERR_OOM;
    emevd->events = events;
    sf_emevd_event_t *event = &emevd->events[emevd->event_count];
    memset(event, 0, sizeof(*event));
    event->alloc = emevd->alloc;
    event->id = event_id;
    event->rest_behavior = rest;
    emevd->event_count++;
    *out = event;
    return SF_OK;
}

size_t sf_emevd_get_linked_file_count(const sf_emevd_t *emevd) {
    return emevd ? emevd->linked_file_count : 0;
}

int64_t sf_emevd_get_linked_file_offset(const sf_emevd_t *emevd, size_t index) {
    if (!emevd || index >= emevd->linked_file_count) return -1;
    return emevd->linked_file_offsets[index];
}

const uint8_t *sf_emevd_get_string_data(const sf_emevd_t *emevd) {
    return emevd ? emevd->string_data : NULL;
}

size_t sf_emevd_get_string_data_size(const sf_emevd_t *emevd) {
    return emevd ? emevd->string_data_size : 0;
}
