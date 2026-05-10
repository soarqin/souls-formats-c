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

sf_result_t sf_emevd_write_to_memory(const sf_emevd_t *emevd, uint8_t **out,
                                     size_t *out_size, const sf_allocator_t *alloc) {
    (void)emevd;
    (void)alloc;
    SF_CHECK_ARG(out != NULL && out_size != NULL);
    *out = NULL;
    *out_size = 0;
    return SF_ERR_UNSUPPORTED_VERSION;
}

sf_result_t sf_emevd_write_to_stream(const sf_emevd_t *emevd, sf_ostream_t *stream,
                                     const sf_allocator_t *alloc) {
    (void)emevd;
    (void)alloc;
    SF_CHECK_ARG(stream != NULL);
    return SF_ERR_UNSUPPORTED_VERSION;
}

sf_result_t sf_emevd_write_to_path(const sf_emevd_t *emevd, const wchar_t *path,
                                   const sf_allocator_t *alloc) {
    (void)emevd;
    (void)alloc;
    SF_CHECK_ARG(path != NULL);
    return SF_ERR_UNSUPPORTED_VERSION;
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
