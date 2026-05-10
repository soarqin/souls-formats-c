/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — PARAM binary reader / writer.
 *
 * Mirrors pinned upstream:
 *   SoulsFormats/Formats/PARAM/PARAM/PARAM.cs:79-302
 *   SoulsFormats/Formats/PARAM/PARAM/Row.cs:74-116,283-455
 */

#include "souls_formats/sf_param.h"

#include "internal/sf_internal.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sf_param_cell {
    int unused;
};

struct sf_param_row {
    int32_t id;
    int64_t data_offset;
    uint8_t *data;
    size_t data_size;
    char   *name;
};

struct sf_param {
    const sf_allocator_t *alloc;

    sf_param_row_t *rows;
    size_t row_count;

    char *param_type;

    bool big_endian;
    sf_param_format_flags1_t format2d;
    sf_param_format_flags2_t format2e;
    uint8_t paramdef_format_version;
    int16_t paramdef_data_version;
    int16_t unk06;
    int64_t detected_size;
};

static bool has_flag1(sf_param_format_flags1_t flags, sf_param_format_flags1_t flag) {
    return (flags & flag) != 0;
}

static bool has_flag2(sf_param_format_flags2_t flags, sf_param_format_flags2_t flag) {
    return (flags & flag) != 0;
}

static void row_free(sf_param_row_t *row, const sf_allocator_t *alloc) {
    if (!row) return;
    sf_xfree(alloc, row->data);
    sf_xfree(alloc, row->name);
    memset(row, 0, sizeof(*row));
}

void sf_param_destroy(sf_param_t *param) {
    if (!param) return;
    const sf_allocator_t *alloc = param->alloc;
    for (size_t i = 0; i < param->row_count; i++) row_free(&param->rows[i], alloc);
    sf_xfree(alloc, param->rows);
    sf_xfree(alloc, param->param_type);
    sf_xfree(alloc, param);
}

static sf_result_t seek_abs(sf_binary_reader_t *br, int64_t offset) {
    return sf_istream_seek(sf_binary_reader_stream(br), offset);
}

static sf_result_t read_param_type_at(sf_binary_reader_t *br, int64_t offset, char **out) {
    *out = NULL;
    if (offset <= 0 || offset >= sf_binary_reader_length(br)) return SF_OK;
    return sf_binary_reader_get_shift_jis(br, offset, out, NULL);
}

static sf_result_t read_param_type(sf_binary_reader_t *br, sf_param_t *param,
                                   int64_t *actual_strings_offset) {
    sf_result_t r;
    const int64_t length = sf_binary_reader_length(br);

    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_OFFSET_PARAM_TYPE)) {
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
        int64_t param_type_offset = 0;
        r = sf_binary_reader_read_i64(br, &param_type_offset); if (r != SF_OK) return r;
        for (int i = 0; i < 0x14; i++) {
            r = sf_binary_reader_assert_u8_one(br, 0); if (r != SF_OK) return r;
        }
        r = read_param_type_at(br, param_type_offset, &param->param_type);
        if (r != SF_OK) return r;
        if (param->param_type && param_type_offset < length) {
            *actual_strings_offset = param_type_offset;
        }
        return SF_OK;
    }

    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET) ||
        has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        int64_t field_pos = sf_binary_reader_position(br);
        uint32_t param_type_offset = 0;
        r = sf_binary_reader_read_u32(br, &param_type_offset); if (r != SF_OK) return r;
        if (param_type_offset > 0 && (int64_t)param_type_offset < length) {
            r = read_param_type_at(br, (int64_t)param_type_offset, &param->param_type);
            if (r != SF_OK) return r;
            if (param->param_type) {
                *actual_strings_offset = (int64_t)param_type_offset;
                return seek_abs(br, 0x2C);
            }
        }

        r = seek_abs(br, field_pos); if (r != SF_OK) return r;
    }

    return sf_binary_reader_read_fix_str(br, 0x20, &param->param_type, NULL);
}

static sf_result_t read_data_start(sf_binary_reader_t *br, sf_param_t *param,
                                   int64_t *data_start) {
    sf_result_t r = seek_abs(br, 0x2C);
    if (r != SF_OK) return r;

    r = sf_binary_reader_skip(br, 4); if (r != SF_OK) return r;

    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET)) {
        int32_t value = 0;
        r = sf_binary_reader_read_i32(br, &value); if (r != SF_OK) return r;
        *data_start = value;
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
    } else if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        int64_t value = 0;
        r = sf_binary_reader_read_i64(br, &value); if (r != SF_OK) return r;
        *data_start = value;
        r = sf_binary_reader_assert_i64_one(br, 0); if (r != SF_OK) return r;
    }

    return SF_OK;
}

static size_t row_header_size(sf_param_format_flags1_t format2d) {
    if (has_flag1(format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) return 24;
    return 12;
}

static sf_result_t checked_u16(int64_t value, uint16_t *out) {
    if (value < 0 || value > UINT16_MAX) return SF_ERR_OUT_OF_RANGE;
    *out = (uint16_t)value;
    return SF_OK;
}

static sf_result_t checked_u32(int64_t value, uint32_t *out) {
    if (value < 0 || value > UINT32_MAX) return SF_ERR_OUT_OF_RANGE;
    *out = (uint32_t)value;
    return SF_OK;
}

static sf_result_t reject_unnamed_or_headerless_rows(sf_binary_reader_t *br,
                                                     int64_t data_start,
                                                     size_t row_count,
                                                     size_t header_size) {
    if (row_count == 0) return SF_OK;
    if (data_start < 0) return SF_OK;

    int64_t rows_start = sf_binary_reader_position(br);
    int64_t rows_size = data_start - rows_start;
    if (rows_size < 0) return SF_ERR_UNSUPPORTED_VERSION;

    if (row_count > (size_t)(INT64_MAX / (int64_t)header_size)) return SF_ERR_OUT_OF_RANGE;
    int64_t minimum_rows_size = (int64_t)(row_count * header_size);
    if (rows_size < minimum_rows_size) return SF_ERR_UNSUPPORTED_VERSION;

    return SF_OK;
}

static sf_result_t read_row_name(sf_binary_reader_t *br, sf_param_t *param,
                                 int64_t name_offset, int64_t *actual_strings_offset,
                                 sf_param_row_t *row) {
    if (name_offset == 0 || name_offset == sf_binary_reader_length(br)) return SF_OK;
    if (name_offset < 0 || name_offset > sf_binary_reader_length(br)) return SF_ERR_TRUNCATED;

    if (*actual_strings_offset == 0 || name_offset < *actual_strings_offset) {
        *actual_strings_offset = name_offset;
    }

    return has_flag2(param->format2e, SF_PARAM_FORMAT_FLAGS2_UNICODE_ROW_NAMES)
        ? sf_binary_reader_get_utf16(br, name_offset, &row->name, NULL)
        : sf_binary_reader_get_shift_jis(br, name_offset, &row->name, NULL);
}

static sf_result_t read_row(sf_binary_reader_t *br, sf_param_t *param,
                            int64_t *actual_strings_offset, sf_param_row_t *row) {
    sf_result_t r = sf_binary_reader_read_i32(br, &row->id);
    if (r != SF_OK) return r;

    int64_t name_offset = 0;
    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        r = sf_binary_reader_skip(br, 4); if (r != SF_OK) return r;
        r = sf_binary_reader_read_i64(br, &row->data_offset); if (r != SF_OK) return r;
        r = sf_binary_reader_read_i64(br, &name_offset); if (r != SF_OK) return r;
    } else if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET)) {
        int32_t data_offset = 0;
        uint32_t name_u32 = 0;
        r = sf_binary_reader_read_i32(br, &data_offset); if (r != SF_OK) return r;
        r = sf_binary_reader_read_u32(br, &name_u32); if (r != SF_OK) return r;
        row->data_offset = data_offset;
        name_offset = name_u32;
    } else {
        uint16_t data_offset = 0;
        uint32_t name_u32 = 0;
        r = sf_binary_reader_read_u16(br, &data_offset); if (r != SF_OK) return r;
        r = sf_binary_reader_skip(br, 2); if (r != SF_OK) return r;
        r = sf_binary_reader_read_u32(br, &name_u32); if (r != SF_OK) return r;
        row->data_offset = data_offset;
        name_offset = name_u32;
    }

    return read_row_name(br, param, name_offset, actual_strings_offset, row);
}

static sf_result_t capture_row_data(sf_binary_reader_t *br, sf_param_t *param,
                                    int64_t strings_offset) {
    const int64_t length = sf_binary_reader_length(br);
    if (strings_offset <= 0 || strings_offset > length) return SF_ERR_TRUNCATED;

    for (size_t i = 0; i < param->row_count; i++) {
        sf_param_row_t *row = &param->rows[i];
        int64_t end = (i + 1 < param->row_count) ? param->rows[i + 1].data_offset : strings_offset;
        if (row->data_offset < 0 || row->data_offset > length || end < row->data_offset || end > length) {
            return SF_ERR_TRUNCATED;
        }

        int64_t size64 = end - row->data_offset;
        if ((uint64_t)size64 > (uint64_t)SIZE_MAX) return SF_ERR_OUT_OF_RANGE;
        row->data_size = (size_t)size64;
        if (row->data_size == 0) continue;

        row->data = (uint8_t *)sf_xalloc(param->alloc, row->data_size);
        if (!row->data) return SF_ERR_OOM;
        sf_result_t r = seek_abs(br, row->data_offset);
        if (r != SF_OK) return r;
        r = sf_binary_reader_read_bytes(br, row->data, row->data_size);
        if (r != SF_OK) return r;
    }

    return SF_OK;
}

static sf_result_t param_read(sf_binary_reader_t *br, sf_param_t **out,
                              const sf_allocator_t *alloc) {
    SF_CHECK_ARG(br != NULL && out != NULL);
    *out = NULL;

    alloc = sf_alloc_or_default(alloc);

    sf_result_t r = seek_abs(br, 0x2C);
    if (r != SF_OK) return r;
    uint8_t endian_byte = 0;
    r = sf_binary_reader_read_u8(br, &endian_byte); if (r != SF_OK) return r;
    if (endian_byte != 0 && endian_byte != 0xFF) return SF_ERR_BAD_MAGIC;
    bool big_endian = endian_byte == 0xFF;
    sf_binary_reader_set_big_endian(br, big_endian);

    sf_param_format_flags1_t format2d = 0;
    sf_param_format_flags2_t format2e = 0;
    uint8_t paramdef_format_version = 0;
    r = sf_binary_reader_read_u8(br, &format2d); if (r != SF_OK) return r;
    r = sf_binary_reader_read_u8(br, &format2e); if (r != SF_OK) return r;
    r = sf_binary_reader_read_u8(br, &paramdef_format_version); if (r != SF_OK) return r;

    sf_param_t *param = (sf_param_t *)sf_xalloc(alloc, sizeof(*param));
    if (!param) return SF_ERR_OOM;
    memset(param, 0, sizeof(*param));
    param->alloc = alloc;
    param->big_endian = big_endian;
    param->format2d = format2d;
    param->format2e = format2e;
    param->paramdef_format_version = paramdef_format_version;
    param->detected_size = -1;

    r = seek_abs(br, 0); if (r != SF_OK) goto fail;
    uint32_t strings_offset = 0;
    r = sf_binary_reader_read_u32(br, &strings_offset); if (r != SF_OK) goto fail;
    int64_t data_start = -1;
    uint16_t data_start_u16 = 0;
    r = sf_binary_reader_read_u16(br, &data_start_u16); if (r != SF_OK) goto fail;
    data_start = data_start_u16;
    r = sf_binary_reader_read_i16(br, &param->unk06); if (r != SF_OK) goto fail;
    r = sf_binary_reader_read_i16(br, &param->paramdef_data_version); if (r != SF_OK) goto fail;
    uint16_t row_count_u16 = 0;
    r = sf_binary_reader_read_u16(br, &row_count_u16); if (r != SF_OK) goto fail;

    int64_t actual_strings_offset = 0;
    r = read_param_type(br, param, &actual_strings_offset); if (r != SF_OK) goto fail;
    if (!param->param_type) {
        param->param_type = sf_strdup(alloc, "");
        if (!param->param_type) { r = SF_ERR_OOM; goto fail; }
    }

    r = read_data_start(br, param, &data_start); if (r != SF_OK) goto fail;
    size_t rows_start = (size_t)sf_binary_reader_position(br);
    size_t header_size = row_header_size(param->format2d);
    r = reject_unnamed_or_headerless_rows(br, data_start, row_count_u16, header_size);
    if (r != SF_OK) goto fail;

    param->row_count = row_count_u16;
    if (param->row_count > 0) {
        param->rows = (sf_param_row_t *)sf_xalloc(alloc, param->row_count * sizeof(*param->rows));
        if (!param->rows) { r = SF_ERR_OOM; goto fail; }
        memset(param->rows, 0, param->row_count * sizeof(*param->rows));
    }

    r = seek_abs(br, (int64_t)rows_start); if (r != SF_OK) goto fail;
    for (size_t i = 0; i < param->row_count; i++) {
        r = read_row(br, param, &actual_strings_offset, &param->rows[i]);
        if (r != SF_OK) goto fail;
    }

    if (param->row_count > 1) {
        param->detected_size = param->rows[1].data_offset - param->rows[0].data_offset;
    } else if (param->row_count == 1) {
        param->detected_size = (int64_t)strings_offset - param->rows[0].data_offset;
    } else {
        param->detected_size = -1;
    }

    r = capture_row_data(br, param, (int64_t)strings_offset);
    if (r != SF_OK) goto fail;

    *out = param;
    return SF_OK;

fail:
    sf_param_destroy(param);
    return r;
}

sf_result_t sf_param_read_from_stream(sf_param_t **out, sf_istream_t *stream,
                                      const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && stream != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_binary_reader_t *br = NULL;
    sf_result_t r = sf_binary_reader_create(&br, stream, false, alloc);
    if (r != SF_OK) return r;
    r = param_read(br, out, alloc);
    sf_binary_reader_destroy(br);
    return r;
}

sf_result_t sf_param_read_from_memory(sf_param_t **out, const uint8_t *data,
                                      size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || data != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_memory(&stream, data, size, alloc);
    if (r != SF_OK) return r;
    r = sf_param_read_from_stream(out, stream, alloc);
    sf_istream_close(stream);
    return r;
}

sf_result_t sf_param_read_from_path(sf_param_t **out, const wchar_t *path,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && path != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_wfile(&stream, path, alloc);
    if (r != SF_OK) return r;
    r = sf_param_read_from_stream(out, stream, alloc);
    sf_istream_close(stream);
    return r;
}

static sf_result_t reserve_row_offset(sf_binary_writer_t *bw, const sf_param_t *param,
                                      size_t index) {
    char name[32];
    int n = snprintf(name, sizeof(name), "RowOffset%zu", index);
    if (n < 0 || (size_t)n >= sizeof(name)) return SF_ERR_OUT_OF_RANGE;

    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        return sf_binary_writer_reserve_i64(bw, name);
    }
    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET)) {
        return sf_binary_writer_reserve_i32(bw, name);
    }
    return sf_binary_writer_reserve_u16(bw, name);
}

static sf_result_t fill_row_offset(sf_binary_writer_t *bw, const sf_param_t *param,
                                   size_t index, int64_t value) {
    char name[32];
    int n = snprintf(name, sizeof(name), "RowOffset%zu", index);
    if (n < 0 || (size_t)n >= sizeof(name)) return SF_ERR_OUT_OF_RANGE;

    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        return sf_binary_writer_fill_i64(bw, name, value);
    }
    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET)) {
        if (value < INT32_MIN || value > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
        return sf_binary_writer_fill_i32(bw, name, (int32_t)value);
    }

    uint16_t value16 = 0;
    sf_result_t r = checked_u16(value, &value16);
    if (r != SF_OK) return r;
    return sf_binary_writer_fill_u16(bw, name, value16);
}

static sf_result_t reserve_name_offset(sf_binary_writer_t *bw, const sf_param_t *param,
                                       size_t index) {
    char name[32];
    int n = snprintf(name, sizeof(name), "NameOffset%zu", index);
    if (n < 0 || (size_t)n >= sizeof(name)) return SF_ERR_OUT_OF_RANGE;
    return has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)
        ? sf_binary_writer_reserve_i64(bw, name)
        : sf_binary_writer_reserve_u32(bw, name);
}

static sf_result_t fill_name_offset(sf_binary_writer_t *bw, const sf_param_t *param,
                                    size_t index, int64_t value) {
    char name[32];
    int n = snprintf(name, sizeof(name), "NameOffset%zu", index);
    if (n < 0 || (size_t)n >= sizeof(name)) return SF_ERR_OUT_OF_RANGE;

    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        return sf_binary_writer_fill_i64(bw, name, value);
    }

    uint32_t value32 = 0;
    sf_result_t r = checked_u32(value, &value32);
    if (r != SF_OK) return r;
    return sf_binary_writer_fill_u32(bw, name, value32);
}

static sf_result_t write_row_header(sf_binary_writer_t *bw, const sf_param_t *param,
                                    size_t index) {
    const sf_param_row_t *row = &param->rows[index];
    sf_result_t r = sf_binary_writer_write_i32(bw, row->id);
    if (r != SF_OK) return r;

    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        r = sf_binary_writer_write_i32(bw, 0);
        if (r != SF_OK) return r;
    }

    r = reserve_row_offset(bw, param, index);
    if (r != SF_OK) return r;
    if (!has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET) &&
        !has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET)) {
        r = sf_binary_writer_write_u16(bw, 0);
        if (r != SF_OK) return r;
    }

    return reserve_name_offset(bw, param, index);
}

static sf_result_t write_rows(sf_binary_writer_t *bw, const sf_param_t *param) {
    for (size_t i = 0; i < param->row_count; i++) {
        sf_result_t r = fill_row_offset(bw, param, i, sf_binary_writer_position(bw));
        if (r != SF_OK) return r;
        if (param->rows[i].data_size > 0) {
            if (!param->rows[i].data) return SF_ERR_INVALID_ARG;
            r = sf_binary_writer_write_bytes(bw, param->rows[i].data, param->rows[i].data_size);
            if (r != SF_OK) return r;
        }
    }
    return SF_OK;
}

static int64_t find_previous_name_offset(const sf_param_t *param, size_t row_index,
                                         const int64_t *offsets) {
    const char *name = param->rows[row_index].name ? param->rows[row_index].name : "";
    if (name[0] == '\0') return offsets[0];
    for (size_t i = 0; i < row_index; i++) {
        const char *other = param->rows[i].name ? param->rows[i].name : "";
        if (strcmp(name, other) == 0) return offsets[i + 1];
    }
    return 0;
}

static sf_result_t write_row_names(sf_binary_writer_t *bw, const sf_param_t *param,
                                   int64_t strings_offset) {
    int64_t *offsets = (int64_t *)sf_xalloc(param->alloc, (param->row_count + 1) * sizeof(*offsets));
    if (!offsets) return SF_ERR_OOM;
    memset(offsets, 0, (param->row_count + 1) * sizeof(*offsets));
    offsets[0] = strings_offset;

    sf_result_t r = sf_binary_writer_write_i16(bw, 0);
    if (r != SF_OK) goto cleanup;

    for (size_t i = 0; i < param->row_count; i++) {
        int64_t name_offset = find_previous_name_offset(param, i, offsets);
        if (name_offset == 0) {
            name_offset = sf_binary_writer_position(bw);
            const char *name = param->rows[i].name ? param->rows[i].name : "";
            r = has_flag2(param->format2e, SF_PARAM_FORMAT_FLAGS2_UNICODE_ROW_NAMES)
                ? sf_binary_writer_write_utf16(bw, name, true)
                : sf_binary_writer_write_shift_jis(bw, name, true);
            if (r != SF_OK) goto cleanup;
        }
        offsets[i + 1] = name_offset;
        r = fill_name_offset(bw, param, i, name_offset);
        if (r != SF_OK) goto cleanup;
    }

    r = sf_binary_writer_write_i16(bw, 0);

cleanup:
    sf_xfree(param->alloc, offsets);
    return r;
}

static sf_result_t fill_data_start(sf_binary_writer_t *bw, const sf_param_t *param,
                                   int64_t data_start) {
    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_FLAG01) &&
        has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET)) {
        uint32_t value32 = 0;
        sf_result_t r = checked_u32(data_start, &value32);
        if (r != SF_OK) return r;
        return sf_binary_writer_fill_u32(bw, "DataStart", value32);
    }
    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        return sf_binary_writer_fill_i64(bw, "DataStart", data_start);
    }

    uint16_t value16 = 0;
    sf_result_t r = checked_u16(data_start, &value16);
    if (r != SF_OK) return r;
    return sf_binary_writer_fill_u16(bw, "DataStart", value16);
}

static sf_result_t param_write(sf_binary_writer_t *bw, const sf_param_t *param) {
    SF_CHECK_ARG(bw != NULL && param != NULL);
    if (param->row_count > UINT16_MAX) return SF_ERR_OUT_OF_RANGE;

    sf_binary_writer_set_big_endian(bw, param->big_endian);
    sf_result_t r = sf_binary_writer_reserve_u32(bw, "StringsOffset");
    if (r != SF_OK) return r;

    if ((has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_FLAG01) &&
         has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET)) ||
        has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        r = sf_binary_writer_write_i16(bw, 0);
    } else {
        r = sf_binary_writer_reserve_u16(bw, "DataStart");
    }
    if (r != SF_OK) return r;

    r = sf_binary_writer_write_i16(bw, param->unk06); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i16(bw, param->paramdef_data_version); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u16(bw, (uint16_t)param->row_count); if (r != SF_OK) return r;

    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_OFFSET_PARAM_TYPE)) {
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
        r = sf_binary_writer_reserve_i64(bw, "ParamTypeOffset"); if (r != SF_OK) return r;
        r = sf_binary_writer_write_pattern(bw, 0x14, 0); if (r != SF_OK) return r;
    } else if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET) ||
               has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        r = sf_binary_writer_reserve_u32(bw, "ParamTypeOffset32"); if (r != SF_OK) return r;
        r = sf_binary_writer_write_pattern(bw, 0x1C, 0); if (r != SF_OK) return r;
    } else {
        uint8_t padding = has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_FLAG01) ? 0x20 : 0x00;
        r = sf_binary_writer_write_fix_str(bw, param->param_type ? param->param_type : "", 0x20,
                                           padding);
        if (r != SF_OK) return r;
    }

    r = sf_binary_writer_write_u8(bw, param->big_endian ? 0xFFu : 0x00u); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, param->format2d); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, param->format2e); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, param->paramdef_format_version); if (r != SF_OK) return r;

    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_FLAG01) &&
        has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET)) {
        r = sf_binary_writer_reserve_u32(bw, "DataStart"); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
    } else if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        r = sf_binary_writer_reserve_i64(bw, "DataStart"); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i64(bw, 0); if (r != SF_OK) return r;
    }

    for (size_t i = 0; i < param->row_count; i++) {
        r = write_row_header(bw, param, i);
        if (r != SF_OK) return r;
    }

    if (param->format2d == SF_PARAM_FORMAT_FLAGS1_FLAG01) {
        r = sf_binary_writer_write_pattern(bw, 0x20, 0);
        if (r != SF_OK) return r;
    }

    r = fill_data_start(bw, param, sf_binary_writer_position(bw));
    if (r != SF_OK) return r;
    r = write_rows(bw, param);
    if (r != SF_OK) return r;

    int64_t strings_offset = sf_binary_writer_position(bw);
    uint32_t strings_offset32 = 0;
    r = checked_u32(strings_offset, &strings_offset32);
    if (r != SF_OK) return r;
    r = sf_binary_writer_fill_u32(bw, "StringsOffset", strings_offset32);
    if (r != SF_OK) return r;

    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_OFFSET_PARAM_TYPE)) {
        r = sf_binary_writer_fill_i64(bw, "ParamTypeOffset", sf_binary_writer_position(bw));
        if (r != SF_OK) return r;
        r = sf_binary_writer_write_ascii(bw, param->param_type ? param->param_type : "", true);
        if (r != SF_OK) return r;
    } else if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET) ||
               has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        uint32_t param_type_offset32 = 0;
        r = checked_u32(sf_binary_writer_position(bw), &param_type_offset32);
        if (r != SF_OK) return r;
        r = sf_binary_writer_fill_u32(bw, "ParamTypeOffset32", param_type_offset32);
        if (r != SF_OK) return r;
        r = sf_binary_writer_write_shift_jis(bw, param->param_type ? param->param_type : "", true);
        if (r != SF_OK) return r;
    }

    return write_row_names(bw, param, strings_offset);
}

sf_result_t sf_param_write_to_memory(const sf_param_t *param, uint8_t **out,
                                     size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(param != NULL && out != NULL && out_size != NULL);
    *out = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_memory(&stream, alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, stream, param->big_endian, alloc);
    if (r != SF_OK) { sf_ostream_close(stream); return r; }

    r = param_write(bw, param);
    if (r == SF_OK) {
        r = sf_binary_writer_finish_bytes(bw, out, out_size);
    } else {
        sf_binary_writer_destroy(bw);
    }
    sf_ostream_close(stream);
    return r;
}

sf_result_t sf_param_write_to_stream(const sf_param_t *param, sf_ostream_t *stream,
                                     const sf_allocator_t *alloc) {
    SF_CHECK_ARG(param != NULL && stream != NULL);
    alloc = sf_alloc_or_default(alloc);

    sf_binary_writer_t *bw = NULL;
    sf_result_t r = sf_binary_writer_create(&bw, stream, param->big_endian, alloc);
    if (r != SF_OK) return r;
    r = param_write(bw, param);
    if (r == SF_OK) {
        r = sf_binary_writer_finish(bw);
    } else {
        sf_binary_writer_destroy(bw);
    }
    return r;
}

sf_result_t sf_param_write_to_path(const sf_param_t *param, const wchar_t *path,
                                   const sf_allocator_t *alloc) {
    SF_CHECK_ARG(param != NULL && path != NULL);
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_wfile(&stream, path, alloc);
    if (r != SF_OK) return r;

    r = sf_param_write_to_stream(param, stream, alloc);
    sf_ostream_close(stream);
    return r;
}

sf_result_t sf_param_apply_paramdef(sf_param_t *param, const sf_paramdef_t *paramdef,
                                    sf_param_apply_mode_t mode) {
    (void)param;
    (void)paramdef;
    (void)mode;
    return SF_ERR_UNSUPPORTED_VERSION;
}

sf_result_t sf_param_apply_paramdef_multi(sf_param_t *param,
                                          const sf_paramdef_t *const *paramdefs,
                                          size_t paramdef_count,
                                          sf_param_apply_mode_t mode) {
    (void)param;
    (void)paramdefs;
    (void)paramdef_count;
    (void)mode;
    return SF_ERR_UNSUPPORTED_VERSION;
}

const char *sf_param_get_param_type(const sf_param_t *param) {
    return (param && param->param_type) ? param->param_type : "";
}

bool sf_param_is_big_endian(const sf_param_t *param) {
    return param ? param->big_endian : false;
}

sf_param_format_flags1_t sf_param_get_format_flags1(const sf_param_t *param) {
    return param ? param->format2d : SF_PARAM_FORMAT_FLAGS1_NONE;
}

sf_param_format_flags2_t sf_param_get_format_flags2(const sf_param_t *param) {
    return param ? param->format2e : SF_PARAM_FORMAT_FLAGS2_NONE;
}

uint8_t sf_param_get_paramdef_format_version(const sf_param_t *param) {
    return param ? param->paramdef_format_version : 0;
}

int16_t sf_param_get_paramdef_data_version(const sf_param_t *param) {
    return param ? param->paramdef_data_version : 0;
}

size_t sf_param_get_row_count(const sf_param_t *param) {
    return param ? param->row_count : 0;
}

const sf_param_row_t *sf_param_get_row(const sf_param_t *param, size_t index) {
    if (!param || index >= param->row_count) return NULL;
    return &param->rows[index];
}

const sf_param_row_t *sf_param_find_row_by_id(const sf_param_t *param, int32_t id) {
    if (!param) return NULL;
    for (size_t i = 0; i < param->row_count; i++) {
        if (param->rows[i].id == id) return &param->rows[i];
    }
    return NULL;
}

int32_t sf_param_row_get_id(const sf_param_row_t *row) {
    return row ? row->id : 0;
}

const char *sf_param_row_get_name(const sf_param_row_t *row) {
    return (row && row->name) ? row->name : "";
}

size_t sf_param_row_get_cell_count(const sf_param_row_t *row) {
    (void)row;
    return 0;
}

const sf_param_cell_t *sf_param_row_get_cell(const sf_param_row_t *row, size_t index) {
    (void)row;
    (void)index;
    return NULL;
}

const sf_param_cell_t *sf_param_row_find_cell(const sf_param_row_t *row,
                                             const char *internal_name) {
    (void)row;
    (void)internal_name;
    return NULL;
}

sf_param_cell_value_t sf_param_cell_get_value(const sf_param_cell_t *cell) {
    (void)cell;
    sf_param_cell_value_t value;
    memset(&value, 0, sizeof(value));
    return value;
}

uint8_t sf_param_cell_get_u8(const sf_param_cell_t *cell) {
    (void)cell;
    return 0;
}

int8_t sf_param_cell_get_s8(const sf_param_cell_t *cell) {
    (void)cell;
    return 0;
}

uint16_t sf_param_cell_get_u16(const sf_param_cell_t *cell) {
    (void)cell;
    return 0;
}

int16_t sf_param_cell_get_s16(const sf_param_cell_t *cell) {
    (void)cell;
    return 0;
}

uint32_t sf_param_cell_get_u32(const sf_param_cell_t *cell) {
    (void)cell;
    return 0;
}

int32_t sf_param_cell_get_s32(const sf_param_cell_t *cell) {
    (void)cell;
    return 0;
}

uint32_t sf_param_cell_get_b32(const sf_param_cell_t *cell) {
    (void)cell;
    return 0;
}

float sf_param_cell_get_f32(const sf_param_cell_t *cell) {
    (void)cell;
    return 0.0f;
}

float sf_param_cell_get_angle32(const sf_param_cell_t *cell) {
    (void)cell;
    return 0.0f;
}

double sf_param_cell_get_f64(const sf_param_cell_t *cell) {
    (void)cell;
    return 0.0;
}

sf_result_t sf_param_cell_get_bytes(const sf_param_cell_t *cell,
                                    const uint8_t **out_data, size_t *out_size) {
    (void)cell;
    SF_CHECK_ARG(out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    return SF_ERR_INVALID_ARG;
}

const char *sf_param_cell_get_string(const sf_param_cell_t *cell) {
    (void)cell;
    return "";
}

bool sf_param_cell_get_bool(const sf_param_cell_t *cell) {
    (void)cell;
    return false;
}
