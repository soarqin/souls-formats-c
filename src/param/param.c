/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — PARAM binary reader / writer.
 *
 * Mirrors pinned upstream:
 *   SoulsFormats/Formats/PARAM/PARAM/PARAM.cs:79-302
 *   SoulsFormats/Formats/PARAM/PARAM/Row.cs:74-116,283-455
 */

#include "param/param_internal.h"
#include "internal/sf_internal.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool has_flag1(sf_param_format_flags1_t flags, sf_param_format_flags1_t flag) {
    return (flags & flag) != 0;
}

static bool has_flag2(sf_param_format_flags2_t flags, sf_param_format_flags2_t flag) {
    return (flags & flag) != 0;
}

static void cell_free(sf_param_cell_t *cell, const sf_allocator_t *alloc) {
    if (!cell) return;
    if (cell->owns_internal_name) sf_xfree(alloc, cell->internal_name);
    switch (cell->value.kind) {
    case SF_PARAM_CELL_KIND_U8_ARRAY:
    case SF_PARAM_CELL_KIND_DUMMY8_ARRAY:
        sf_xfree(alloc, (void *)cell->value.v.bytes.data);
        break;
    case SF_PARAM_CELL_KIND_FIXSTR:
    case SF_PARAM_CELL_KIND_FIXSTR_W:
        sf_xfree(alloc, (void *)cell->value.v.str_utf8);
        break;
    default:
        break;
    }
    memset(cell, 0, sizeof(*cell));
}

void sfi_param_row_clear_cells(sf_param_row_t *row, const sf_allocator_t *alloc) {
    if (!row) return;
    for (size_t i = 0; i < row->cell_count; i++) cell_free(&row->cells[i], alloc);
    sf_xfree(alloc, row->cells);
    row->cells = NULL;
    row->cell_count = 0;
    row->cell_data_size = 0;
    row->cells_applied = false;
}

enum { PARAM_ROW_ARENA_MIN_ROWS = 1000 };

static void row_free(sf_param_row_t *row, const sf_allocator_t *alloc, bool free_data) {
    if (!row) return;
    sfi_param_row_clear_cells(row, alloc);
    if (free_data) sf_xfree(alloc, row->data);
    sf_xfree(alloc, row->name);
    memset(row, 0, sizeof(*row));
}

void sf_param_destroy(sf_param_t *param) {
    if (!param) return;
    const sf_allocator_t *alloc = param->alloc;
    bool rows_own_data = param->row_data_arena == NULL;
    for (size_t i = 0; i < param->row_count; i++) row_free(&param->rows[i], alloc, rows_own_data);
    sf_xfree(alloc, param->row_data_arena);
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

    size_t total_size = 0;
    for (size_t i = 0; i < param->row_count; i++) {
        sf_param_row_t *row = &param->rows[i];
        int64_t end = (i + 1 < param->row_count) ? param->rows[i + 1].data_offset : strings_offset;
        if (row->data_offset < 0 || row->data_offset > length || end < row->data_offset || end > length) {
            return SF_ERR_TRUNCATED;
        }

        int64_t size64 = end - row->data_offset;
        if ((uint64_t)size64 > (uint64_t)SIZE_MAX) return SF_ERR_OUT_OF_RANGE;
        row->data_size = (size_t)size64;
        if (row->data_size > SIZE_MAX - total_size) return SF_ERR_OUT_OF_RANGE;
        total_size += row->data_size;
    }

    if (param->row_count >= PARAM_ROW_ARENA_MIN_ROWS && total_size > 0) {
        param->row_data_arena = (uint8_t *)sf_xalloc(param->alloc, total_size);
        if (!param->row_data_arena) return SF_ERR_OOM;
        param->row_data_arena_size = total_size;

        uint8_t *cursor = param->row_data_arena;
        for (size_t i = 0; i < param->row_count; i++) {
            sf_param_row_t *row = &param->rows[i];
            if (row->data_size == 0) continue;

            row->data = cursor;
            cursor += row->data_size;

            sf_result_t r = seek_abs(br, row->data_offset);
            if (r != SF_OK) return r;
            r = sf_binary_reader_read_bytes(br, row->data, row->data_size);
            if (r != SF_OK) return r;
        }
        return SF_OK;
    }

    for (size_t i = 0; i < param->row_count; i++) {
        sf_param_row_t *row = &param->rows[i];
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
        const sf_param_row_t *row = &param->rows[i];
        if (row->cells_applied) {
            uint8_t *row_bytes = NULL;
            size_t row_size = 0;
            r = sfi_param_row_cells_to_bytes(param, row, &row_bytes, &row_size);
            if (r != SF_OK) return r;
            if (row_size > 0) {
                r = sf_binary_writer_write_bytes(bw, row_bytes, row_size);
            }
            sf_xfree(param->alloc, row_bytes);
            if (r != SF_OK) return r;
        } else if (row->data_size > 0) {
            if (!row->data) return SF_ERR_INVALID_ARG;
            r = sf_binary_writer_write_bytes(bw, row->data, row->data_size);
            if (r != SF_OK) return r;
        }
    }
    return SF_OK;
}

static int64_t find_previous_name_offset(const sf_param_t *param, size_t row_index,
                                         const int64_t *offsets) {
    const char *name = param->rows[row_index].name ? param->rows[row_index].name : "";
    if (name[0] == '\0') return -1;
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
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i64(bw, "ParamTypeOffset"), return r);
        r = sf_binary_writer_write_pattern(bw, 0x14, 0); if (r != SF_OK) return r;
    } else if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET) ||
               has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_u32(bw, "ParamTypeOffset32"), return r);
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
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_u32(bw, "DataStart"), return r);
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
    } else if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i64(bw, "DataStart"), return r);
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
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_u32(bw, "StringsOffset", strings_offset32), return r);

    if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_OFFSET_PARAM_TYPE)) {
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i64(bw, "ParamTypeOffset", sf_binary_writer_position(bw)), return r);
        r = sf_binary_writer_write_ascii(bw, param->param_type ? param->param_type : "", true);
        if (r != SF_OK) return r;
    } else if (has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_INT_DATA_OFFSET) ||
               has_flag1(param->format2d, SF_PARAM_FORMAT_FLAGS1_LONG_DATA_OFFSET)) {
        uint32_t param_type_offset32 = 0;
        r = checked_u32(sf_binary_writer_position(bw), &param_type_offset32);
        if (r != SF_OK) return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_u32(bw, "ParamTypeOffset32", param_type_offset32), return r);
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

sf_param_row_t *sf_param_get_row_mut(sf_param_t *param, size_t index) {
    return (sf_param_row_t *)sf_param_get_row(param, index);
}

sf_param_row_t *sf_param_find_row_by_id_mut(sf_param_t *param, int64_t id) {
    if (!param || id < INT32_MIN || id > INT32_MAX) return NULL;
    return (sf_param_row_t *)sf_param_find_row_by_id(param, (int32_t)id);
}

static sf_result_t checked_add_size(size_t a, size_t b, size_t *out) {
    if (a > SIZE_MAX - b) return SF_ERR_OUT_OF_RANGE;
    *out = a + b;
    return SF_OK;
}

static void repair_cell_parent_links(sf_param_t *param) {
    for (size_t i = 0; i < param->row_count; i++) {
        sf_param_row_t *row = &param->rows[i];
        for (size_t j = 0; j < row->cell_count; j++) {
            row->cells[j].parent_param = param;
            row->cells[j].parent_row = row;
        }
    }
}

SF_API sf_result_t sf_param_add_row_by_id(sf_param_t *param, int32_t id,
                                          const char *name_optional,
                                          sf_param_row_t **out_row) {
    SF_CHECK_ARG(param != NULL && out_row != NULL);
    *out_row = NULL;
    if (!param->applied_paramdef) return SF_ERR_INVALID_STATE;

    /* Use the param's detected (file-derived) row size when available so that
     * the new row's data_size matches existing rows.  Fall back to the paramdef
     * row size only when the param has no rows yet (detected_size == -1). */
    int32_t row_size_i32;
    if (param->detected_size > 0) {
        if (param->detected_size > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
        row_size_i32 = (int32_t)param->detected_size;
    } else {
        row_size_i32 = sf_paramdef_get_row_size(param->applied_paramdef);
    }
    if (row_size_i32 <= 0) return SF_ERR_INVALID_STATE;
    size_t row_size = (size_t)row_size_i32;

    size_t existing_data_size = 0;
    for (size_t i = 0; i < param->row_count; i++) {
        sf_result_t r = checked_add_size(existing_data_size, param->rows[i].data_size,
                                         &existing_data_size);
        if (r != SF_OK) return r;
    }

    size_t new_arena_size = 0;
    sf_result_t r = checked_add_size(existing_data_size, row_size, &new_arena_size);
    if (r != SF_OK) return r;
    if (param->row_count > (SIZE_MAX / sizeof(*param->rows)) - 1u) return SF_ERR_OUT_OF_RANGE;

    char *name_copy = NULL;
    if (name_optional) {
        name_copy = sf_strdup(param->alloc, name_optional);
        if (!name_copy) return SF_ERR_OOM;
    }

    sf_param_row_t *new_rows = (sf_param_row_t *)sf_xalloc(
        param->alloc, (param->row_count + 1u) * sizeof(*new_rows));
    if (!new_rows) {
        sf_xfree(param->alloc, name_copy);
        return SF_ERR_OOM;
    }
    memset(new_rows, 0, (param->row_count + 1u) * sizeof(*new_rows));
    if (param->row_count > 0) memcpy(new_rows, param->rows, param->row_count * sizeof(*new_rows));

    uint8_t *new_arena = NULL;
    if (new_arena_size > 0) {
        new_arena = (uint8_t *)sf_xalloc(param->alloc, new_arena_size);
        if (!new_arena) {
            sf_xfree(param->alloc, new_rows);
            sf_xfree(param->alloc, name_copy);
            return SF_ERR_OOM;
        }
    }

    uint8_t *cursor = new_arena;
    for (size_t i = 0; i < param->row_count; i++) {
        if (param->rows[i].data_size > 0) {
            if (!param->rows[i].data) {
                sf_xfree(param->alloc, new_arena);
                sf_xfree(param->alloc, new_rows);
                sf_xfree(param->alloc, name_copy);
                return SF_ERR_INVALID_ARG;
            }
            memcpy(cursor, param->rows[i].data, param->rows[i].data_size);
            new_rows[i].data = cursor;
            cursor += param->rows[i].data_size;
        } else {
            new_rows[i].data = NULL;
        }
    }

    sf_param_row_t *new_row = &new_rows[param->row_count];
    new_row->id = id;
    new_row->data_offset = (int64_t)existing_data_size;
    new_row->data = new_arena ? new_arena + existing_data_size : NULL;
    new_row->data_size = row_size;
    new_row->name = name_copy;
    if (new_row->data && row_size > 0) memset(new_row->data, 0, row_size);

    bool rows_own_data = param->row_data_arena == NULL;
    if (rows_own_data) {
        for (size_t i = 0; i < param->row_count; i++) sf_xfree(param->alloc, param->rows[i].data);
    } else {
        sf_xfree(param->alloc, param->row_data_arena);
    }
    sf_xfree(param->alloc, param->rows);

    param->rows = new_rows;
    param->row_data_arena = new_arena;
    param->row_data_arena_size = new_arena_size;
    param->row_count++;
    if (param->detected_size == -1) param->detected_size = (int64_t)row_size;
    repair_cell_parent_links(param);

    r = sfi_param_apply_paramdef_to_row(new_row, param, param->applied_paramdef);
    if (r != SF_OK) return r;
    *out_row = new_row;
    return SF_OK;
}

static int param_row_cmp(const void *a, const void *b) {
    const sf_param_row_t *row_a = (const sf_param_row_t *)a;
    const sf_param_row_t *row_b = (const sf_param_row_t *)b;
    if (row_a->id < row_b->id) return -1;
    if (row_a->id > row_b->id) return 1;
    return 0;
}

SF_API sf_result_t sf_param_sort_rows_by_id(sf_param_t *param) {
    SF_CHECK_ARG(param != NULL);
    if (param->row_count <= 1) return SF_OK;

    qsort(param->rows, param->row_count, sizeof(*param->rows), param_row_cmp);
    return SF_OK;
}

int32_t sf_param_row_get_id(const sf_param_row_t *row) {
    return row ? row->id : 0;
}

const char *sf_param_row_get_name(const sf_param_row_t *row) {
    return (row && row->name) ? row->name : "";
}

size_t sf_param_row_get_cell_count(const sf_param_row_t *row) {
    return row ? row->cell_count : 0;
}

const sf_param_cell_t *sf_param_row_get_cell(const sf_param_row_t *row, size_t index) {
    if (!row || index >= row->cell_count) return NULL;
    return &row->cells[index];
}

const sf_param_cell_t *sf_param_row_find_cell(const sf_param_row_t *row,
                                              const char *internal_name) {
    if (!row || !internal_name) return NULL;
    for (size_t i = 0; i < row->cell_count; i++) {
        const char *name = row->cells[i].internal_name ? row->cells[i].internal_name : "";
        if (strcmp(name, internal_name) == 0) return &row->cells[i];
    }
    return NULL;
}

sf_param_cell_t *sf_param_row_get_cell_mut(sf_param_row_t *row, size_t index) {
    return (sf_param_cell_t *)sf_param_row_get_cell(row, index);
}

sf_param_cell_t *sf_param_row_find_cell_mut(sf_param_row_t *row,
                                            const char *internal_name) {
    return (sf_param_cell_t *)sf_param_row_find_cell(row, internal_name);
}

SF_API sf_result_t sf_param_row_copy(sf_param_row_t *dst, const sf_param_row_t *src) {
    SF_CHECK_ARG(dst != NULL && src != NULL);
    if (dst == src) return SF_OK;
    if (dst->cell_count != src->cell_count || dst->data_size != src->data_size) return SF_ERR_INVALID_ARG;
    memcpy(dst->data, src->data, src->data_size);
    for (size_t i = 0; i < dst->cell_count; i++) {
        (void)sf_param_cell_copy(&dst->cells[i], &src->cells[i]);
    }
    return SF_OK;
}

sf_param_cell_value_t sf_param_cell_get_value(const sf_param_cell_t *cell) {
    sf_param_cell_value_t value;
    memset(&value, 0, sizeof(value));
    return cell ? cell->value : value;
}

uint8_t sf_param_cell_get_u8(const sf_param_cell_t *cell) {
    if (!cell) return 0;
    if (cell->value.kind == SF_PARAM_CELL_KIND_U8 ||
        cell->value.kind == SF_PARAM_CELL_KIND_DUMMY8_BIT) {
        return cell->value.v.u8;
    }
    return 0;
}

int8_t sf_param_cell_get_s8(const sf_param_cell_t *cell) {
    return (cell && cell->value.kind == SF_PARAM_CELL_KIND_S8) ? cell->value.v.s8 : 0;
}

uint16_t sf_param_cell_get_u16(const sf_param_cell_t *cell) {
    return (cell && cell->value.kind == SF_PARAM_CELL_KIND_U16) ? cell->value.v.u16 : 0;
}

int16_t sf_param_cell_get_s16(const sf_param_cell_t *cell) {
    return (cell && cell->value.kind == SF_PARAM_CELL_KIND_S16) ? cell->value.v.s16 : 0;
}

uint32_t sf_param_cell_get_u32(const sf_param_cell_t *cell) {
    return (cell && cell->value.kind == SF_PARAM_CELL_KIND_U32) ? cell->value.v.u32 : 0;
}

int32_t sf_param_cell_get_s32(const sf_param_cell_t *cell) {
    return (cell && cell->value.kind == SF_PARAM_CELL_KIND_S32) ? cell->value.v.s32 : 0;
}

uint32_t sf_param_cell_get_b32(const sf_param_cell_t *cell) {
    return (cell && cell->value.kind == SF_PARAM_CELL_KIND_B32) ? cell->value.v.b32 : 0;
}

static const sf_allocator_t *cell_alloc(const sf_param_cell_t *cell) {
    return (cell && cell->alloc) ? cell->alloc : NULL;
}

static size_t cell_declared_size(const sf_param_cell_t *cell) {
    if (!cell) return 0;
    if (cell->byte_count > 0) return (size_t)cell->byte_count;
    if (cell->array_length > 0) return (size_t)cell->array_length;
    return 0;
}

static sf_result_t reject_cell_kind(sf_param_cell_t *cell, const char *setter_name);
static sf_result_t check_cell_backing(sf_param_cell_t *cell, size_t bytes_needed);
static sf_result_t write_cell_backing(sf_param_cell_t *cell, uint64_t value,
                                      size_t byte_count);

float sf_param_cell_get_f32(const sf_param_cell_t *cell) {
    return (cell && cell->value.kind == SF_PARAM_CELL_KIND_F32) ? cell->value.v.f32 : 0.0f;
}

float sf_param_cell_get_angle32(const sf_param_cell_t *cell) {
    return (cell && cell->value.kind == SF_PARAM_CELL_KIND_ANGLE32) ? cell->value.v.angle32 : 0.0f;
}

double sf_param_cell_get_f64(const sf_param_cell_t *cell) {
    return (cell && cell->value.kind == SF_PARAM_CELL_KIND_F64) ? cell->value.v.f64 : 0.0;
}

sf_result_t sf_param_cell_get_bytes(const sf_param_cell_t *cell,
                                    const uint8_t **out_data, size_t *out_size) {
    SF_CHECK_ARG(out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    if (!cell || (cell->value.kind != SF_PARAM_CELL_KIND_U8_ARRAY &&
                  cell->value.kind != SF_PARAM_CELL_KIND_DUMMY8_ARRAY)) {
        return SF_ERR_INVALID_ARG;
    }
    *out_data = cell->value.v.bytes.data;
    *out_size = cell->value.v.bytes.size;
    return SF_OK;
}

const char *sf_param_cell_get_string(const sf_param_cell_t *cell) {
    if (!cell || (cell->value.kind != SF_PARAM_CELL_KIND_FIXSTR &&
                  cell->value.kind != SF_PARAM_CELL_KIND_FIXSTR_W)) {
        return "";
    }
    return cell->value.v.str_utf8 ? cell->value.v.str_utf8 : "";
}

bool sf_param_cell_get_bool(const sf_param_cell_t *cell) {
    return cell && cell->value.kind == SF_PARAM_CELL_KIND_B32 && cell->value.v.b32 != 0;
}

sf_result_t sf_param_cell_set_s64(sf_param_cell_t *cell, int64_t value) {
    if (!cell) {
        sfi_set_last_error_detail("sf_param_cell_set_s64 requires a non-NULL cell");
        return SF_ERR_INVALID_ARG;
    }
    if (cell->value.kind != SF_PARAM_CELL_KIND_S64) return reject_cell_kind(cell, "s64");
    sf_result_t r = write_cell_backing(cell, (uint64_t)value, 8);
    if (r != SF_OK) return r;
    cell->value.v.s64 = value;
    sfi_clear_last_error_detail();
    return SF_OK;
}

sf_result_t sf_param_cell_set_u64(sf_param_cell_t *cell, uint64_t value) {
    if (!cell) {
        sfi_set_last_error_detail("sf_param_cell_set_u64 requires a non-NULL cell");
        return SF_ERR_INVALID_ARG;
    }
    if (cell->value.kind != SF_PARAM_CELL_KIND_U64) return reject_cell_kind(cell, "u64");
    sf_result_t r = write_cell_backing(cell, value, 8);
    if (r != SF_OK) return r;
    cell->value.v.u64 = value;
    sfi_clear_last_error_detail();
    return SF_OK;
}

sf_result_t sf_param_cell_set_f32(sf_param_cell_t *cell, float value) {
    if (!cell) {
        sfi_set_last_error_detail("sf_param_cell_set_f32 requires a non-NULL cell");
        return SF_ERR_INVALID_ARG;
    }
    if (cell->value.kind != SF_PARAM_CELL_KIND_F32) return reject_cell_kind(cell, "f32");
    uint32_t raw = 0;
    memcpy(&raw, &value, sizeof(raw));
    sf_result_t r = write_cell_backing(cell, raw, 4);
    if (r != SF_OK) return r;
    cell->value.v.f32 = value;
    sfi_clear_last_error_detail();
    return SF_OK;
}

SF_API sf_result_t sf_param_cell_set_angle32(sf_param_cell_t *cell, float value) {
    if (!cell) {
        sfi_set_last_error_detail("sf_param_cell_set_angle32 requires a non-NULL cell");
        return SF_ERR_INVALID_ARG;
    }
    if (cell->value.kind != SF_PARAM_CELL_KIND_ANGLE32) return reject_cell_kind(cell, "angle32");
    uint32_t raw = 0;
    memcpy(&raw, &value, sizeof(raw));
    sf_result_t r = write_cell_backing(cell, raw, 4);
    if (r != SF_OK) return r;
    cell->value.v.angle32 = value;
    sfi_clear_last_error_detail();
    return SF_OK;
}

sf_result_t sf_param_cell_set_f64(sf_param_cell_t *cell, double value) {
    if (!cell) {
        sfi_set_last_error_detail("sf_param_cell_set_f64 requires a non-NULL cell");
        return SF_ERR_INVALID_ARG;
    }
    if (cell->value.kind != SF_PARAM_CELL_KIND_F64) return reject_cell_kind(cell, "f64");
    uint64_t raw = 0;
    memcpy(&raw, &value, sizeof(raw));
    sf_result_t r = write_cell_backing(cell, raw, 8);
    if (r != SF_OK) return r;
    cell->value.v.f64 = value;
    sfi_clear_last_error_detail();
    return SF_OK;
}

sf_result_t sf_param_cell_set_bool(sf_param_cell_t *cell, bool value) {
    if (!cell) {
        sfi_set_last_error_detail("sf_param_cell_set_bool requires a non-NULL cell");
        return SF_ERR_INVALID_ARG;
    }
    if (cell->value.kind != SF_PARAM_CELL_KIND_B32) return reject_cell_kind(cell, "bool");
    uint32_t raw = value ? 1u : 0u;
    sf_result_t r = write_cell_backing(cell, raw, 4);
    if (r != SF_OK) return r;
    cell->value.v.b32 = raw;
    sfi_clear_last_error_detail();
    return SF_OK;
}

sf_result_t sf_param_cell_set_byte(sf_param_cell_t *cell, uint8_t value) {
    if (!cell) {
        sfi_set_last_error_detail("sf_param_cell_set_byte requires a non-NULL cell");
        return SF_ERR_INVALID_ARG;
    }
    if (cell->value.kind != SF_PARAM_CELL_KIND_U8) return reject_cell_kind(cell, "byte");
    sf_result_t r = write_cell_backing(cell, value, 1);
    if (r != SF_OK) return r;
    cell->value.v.u8 = value;
    sfi_clear_last_error_detail();
    return SF_OK;
}

sf_result_t sf_param_cell_set_fixstr(sf_param_cell_t *cell, const char *value, size_t value_len) {
    if (!cell || (!value && value_len != 0)) {
        sfi_set_last_error_detail("sf_param_cell_set_fixstr requires valid cell/value arguments");
        return SF_ERR_INVALID_ARG;
    }
    if (cell->value.kind != SF_PARAM_CELL_KIND_FIXSTR) return reject_cell_kind(cell, "fixstr");
    size_t capacity = cell_declared_size(cell);
    if (capacity == 0) {
        sfi_set_last_error_detail("fixstr cell has zero capacity");
        return SF_ERR_INVALID_ARG;
    }
    if (value_len > capacity) {
        sfi_set_last_error_detail("value length %zu > cell capacity %zu", value_len, capacity);
        return SF_ERR_OUT_OF_RANGE;
    }
    sf_result_t r = check_cell_backing(cell, capacity);
    if (r != SF_OK) return r;
    uint8_t *p = cell->parent_row->data + cell->byte_offset;
    memset(p, 0, capacity);
    if (value_len > 0) memcpy(p, value, value_len);
    char *copy = (char *)sf_xalloc(cell_alloc(cell), value_len + 1u);
    if (!copy) return SF_ERR_OOM;
    if (value_len > 0) memcpy(copy, value, value_len);
    copy[value_len] = '\0';
    sf_xfree(cell_alloc(cell), (void *)cell->value.v.str_utf8);
    cell->value.v.str_utf8 = copy;
    sfi_clear_last_error_detail();
    return SF_OK;
}

SF_API sf_result_t sf_param_cell_set_fixstr_w(sf_param_cell_t *cell, const wchar_t *value, size_t value_len) {
    if (!cell || (!value && value_len != 0)) {
        sfi_set_last_error_detail("sf_param_cell_set_fixstr_w requires valid cell/value arguments");
        return SF_ERR_INVALID_ARG;
    }
    if (cell->value.kind != SF_PARAM_CELL_KIND_FIXSTR_W) return reject_cell_kind(cell, "fixstr_w");
    size_t capacity = cell_declared_size(cell);
    if (capacity == 0) {
        sfi_set_last_error_detail("fixstr_w cell has zero capacity");
        return SF_ERR_INVALID_ARG;
    }
    if (value_len > capacity) {
        sfi_set_last_error_detail("value length %zu > cell capacity %zu", value_len, capacity);
        return SF_ERR_OUT_OF_RANGE;
    }
    sf_result_t r = check_cell_backing(cell, capacity * 2);
    if (r != SF_OK) return r;
    uint8_t *p = cell->parent_row->data + cell->byte_offset;
    memset(p, 0, capacity * 2);
    for (size_t i = 0; i < value_len; i++) {
        uint16_t ch = (uint16_t)value[i];
        p[i * 2] = (uint8_t)ch;
        p[i * 2 + 1] = (uint8_t)(ch >> 8);
    }
    char *copy = (char *)sf_xalloc(cell_alloc(cell), (value_len * 4) + 1u);
    if (!copy) return SF_ERR_OOM;
    size_t copy_index = 0;
    for (size_t i = 0; i < value_len; i++) {
        uint32_t codepoint = (uint32_t)value[i];
        if (codepoint <= 0x7F) {
            copy[copy_index++] = (char)codepoint;
        } else if (codepoint <= 0x7FF) {
            copy[copy_index++] = (char)(0xC0 | ((codepoint >> 6) & 0x1F));
            copy[copy_index++] = (char)(0x80 | (codepoint & 0x3F));
        } else {
            copy[copy_index++] = (char)(0xE0 | ((codepoint >> 12) & 0x0F));
            copy[copy_index++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
            copy[copy_index++] = (char)(0x80 | (codepoint & 0x3F));
        }
    }
    copy[copy_index] = '\0';
    sf_xfree(cell_alloc(cell), (void *)cell->value.v.str_utf8);
    cell->value.v.str_utf8 = copy;
    sfi_clear_last_error_detail();
    return SF_OK;
}

SF_API sf_result_t sf_param_cell_set_bytes(sf_param_cell_t *cell, const uint8_t *data, size_t size) {
    if (!cell || (size > 0 && !data)) {
        sfi_set_last_error_detail("sf_param_cell_set_bytes requires valid cell/data arguments");
        return SF_ERR_INVALID_ARG;
    }
    if (cell->value.kind != SF_PARAM_CELL_KIND_U8_ARRAY &&
        cell->value.kind != SF_PARAM_CELL_KIND_DUMMY8_ARRAY) {
        return reject_cell_kind(cell, "bytes");
    }
    size_t capacity = cell_declared_size(cell);
    if (capacity == 0) {
        sfi_set_last_error_detail("byte array cell has zero capacity");
        return SF_ERR_INVALID_ARG;
    }
    if (size > capacity) {
        sfi_set_last_error_detail("data size %zu > cell capacity %zu", size, capacity);
        return SF_ERR_OUT_OF_RANGE;
    }
    sf_result_t r = check_cell_backing(cell, size);
    if (r != SF_OK) return r;
    uint8_t *p = cell->parent_row->data + cell->byte_offset;
    memset(p, 0, capacity);
    if (size > 0) memcpy(p, data, size);
    uint8_t *copy = (uint8_t *)sf_xalloc(cell_alloc(cell), size);
    if (!copy && size > 0) return SF_ERR_OOM;
    if (size > 0) memcpy(copy, data, size);
    sf_xfree(cell_alloc(cell), (void *)cell->value.v.bytes.data);
    cell->value.v.bytes.data = copy;
    cell->value.v.bytes.size = size;
    sfi_clear_last_error_detail();
    return SF_OK;
}

static const char *cell_kind_name(sf_param_cell_kind_t kind) {
    switch (kind) {
    case SF_PARAM_CELL_KIND_U8: return "u8";
    case SF_PARAM_CELL_KIND_S8: return "s8";
    case SF_PARAM_CELL_KIND_U16: return "u16";
    case SF_PARAM_CELL_KIND_S16: return "s16";
    case SF_PARAM_CELL_KIND_U32: return "u32";
    case SF_PARAM_CELL_KIND_S32: return "s32";
    case SF_PARAM_CELL_KIND_U64: return "u64";
    case SF_PARAM_CELL_KIND_S64: return "s64";
    case SF_PARAM_CELL_KIND_B32: return "b32";
    case SF_PARAM_CELL_KIND_F32: return "f32";
    case SF_PARAM_CELL_KIND_ANGLE32: return "angle32";
    case SF_PARAM_CELL_KIND_F64: return "f64";
    case SF_PARAM_CELL_KIND_DUMMY8_BIT: return "dummy8_bit";
    case SF_PARAM_CELL_KIND_DUMMY8_ARRAY: return "dummy8_array";
    case SF_PARAM_CELL_KIND_U8_ARRAY: return "u8_array";
    case SF_PARAM_CELL_KIND_FIXSTR: return "fixstr";
    case SF_PARAM_CELL_KIND_FIXSTR_W: return "fixstr_w";
    default: return "unknown";
    }
}

static sf_result_t reject_cell_kind(sf_param_cell_t *cell, const char *setter_name) {
    sf_param_cell_kind_t kind = cell ? cell->value.kind : SF_PARAM_CELL_KIND_U8;
    sfi_set_last_error_detail("cell kind %s does not accept %s",
                              cell_kind_name(kind), setter_name);
    return SF_ERR_INVALID_ARG;
}

static void store_cell_u16(uint8_t *p, uint16_t value, bool big_endian) {
    if (big_endian) {
        p[0] = (uint8_t)(value >> 8);
        p[1] = (uint8_t)value;
    } else {
        p[0] = (uint8_t)value;
        p[1] = (uint8_t)(value >> 8);
    }
}

static void store_cell_u32(uint8_t *p, uint32_t value, bool big_endian) {
    if (big_endian) {
        p[0] = (uint8_t)(value >> 24);
        p[1] = (uint8_t)(value >> 16);
        p[2] = (uint8_t)(value >> 8);
        p[3] = (uint8_t)value;
    } else {
        p[0] = (uint8_t)value;
        p[1] = (uint8_t)(value >> 8);
        p[2] = (uint8_t)(value >> 16);
        p[3] = (uint8_t)(value >> 24);
    }
}

static void store_cell_u64(uint8_t *p, uint64_t value, bool big_endian) {
    if (big_endian) {
        for (size_t i = 0; i < 8; i++) p[i] = (uint8_t)(value >> ((7 - i) * 8));
    } else {
        for (size_t i = 0; i < 8; i++) p[i] = (uint8_t)(value >> (i * 8));
    }
}

static uint64_t load_cell_bit_window(const uint8_t *p, size_t byte_count, bool big_endian) {
    switch (byte_count) {
    case 1:
        return p[0];
    case 2:
        return big_endian ? ((uint16_t)p[0] << 8) | p[1]
                          : ((uint16_t)p[1] << 8) | p[0];
    case 4:
        return big_endian
            ? ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
              ((uint32_t)p[2] << 8) | p[3]
            : ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
              ((uint32_t)p[1] << 8) | p[0];
    default:
        return 0;
    }
}

static void store_cell_bit_window(uint8_t *p, size_t byte_count, uint64_t value,
                                  bool big_endian) {
    switch (byte_count) {
    case 1:
        p[0] = (uint8_t)value;
        break;
    case 2:
        store_cell_u16(p, (uint16_t)value, big_endian);
        break;
    case 4:
        store_cell_u32(p, (uint32_t)value, big_endian);
        break;
    case 8:
        store_cell_u64(p, value, big_endian);
        break;
    default:
        break;
    }
}

static sf_result_t check_cell_backing(sf_param_cell_t *cell, size_t bytes_needed) {
    if (!cell || !cell->parent_row || !cell->parent_param) {
        sfi_set_last_error_detail("cell has no mutable backing row");
        return SF_ERR_INVALID_ARG;
    }
    sf_param_row_t *row = cell->parent_row;
    if (!row->data || bytes_needed > row->data_size ||
        cell->byte_offset > row->data_size - bytes_needed) {
        sfi_set_last_error_detail("cell backing range %zu + %zu exceeds row size %zu",
                                  cell->byte_offset, bytes_needed, row->data_size);
        return SF_ERR_INVALID_ARG;
    }
    return SF_OK;
}

static sf_result_t write_cell_backing(sf_param_cell_t *cell, uint64_t value,
                                      size_t byte_count) {
    bool big_endian = cell->parent_param->big_endian;
    if (cell->is_bit_field) {
        if (cell->bit_size <= 0 || cell->bit_limit == 0) return SF_ERR_INVALID_ARG;
        size_t window_bytes = cell->bit_limit / 8;
        sf_result_t r = check_cell_backing(cell, window_bytes);
        if (r != SF_OK) return r;
        uint8_t *p = cell->parent_row->data + cell->byte_offset;
        uint64_t window = load_cell_bit_window(p, window_bytes, big_endian);
        uint64_t mask = ((uint64_t)1 << (size_t)cell->bit_size) - 1u;
        window = (window & ~(mask << cell->bit_offset)) |
                 ((value & mask) << cell->bit_offset);
        store_cell_bit_window(p, window_bytes, window, big_endian);
        return SF_OK;
    }

    sf_result_t r = check_cell_backing(cell, byte_count);
    if (r != SF_OK) return r;
    uint8_t *p = cell->parent_row->data + cell->byte_offset;
    switch (byte_count) {
    case 1:
        p[0] = (uint8_t)value;
        return SF_OK;
    case 2:
        store_cell_u16(p, (uint16_t)value, big_endian);
        return SF_OK;
    case 4:
        store_cell_u32(p, (uint32_t)value, big_endian);
        return SF_OK;
    case 8:
        store_cell_u64(p, value, big_endian);
        return SF_OK;
    default:
        return SF_ERR_INVALID_ARG;
    }
}

sf_result_t sf_param_cell_set_s8(sf_param_cell_t *cell, int8_t value) {
    if (!cell) return SF_ERR_INVALID_ARG;
    if (cell->value.kind != SF_PARAM_CELL_KIND_S8) return reject_cell_kind(cell, "s8");
    sf_result_t r = write_cell_backing(cell, (uint8_t)value, 1);
    if (r != SF_OK) return r;
    cell->value.v.s8 = value;
    return SF_OK;
}

sf_result_t sf_param_cell_set_u8(sf_param_cell_t *cell, uint8_t value) {
    if (!cell) return SF_ERR_INVALID_ARG;
    if (cell->value.kind != SF_PARAM_CELL_KIND_U8) return reject_cell_kind(cell, "u8");
    sf_result_t r = write_cell_backing(cell, value, 1);
    if (r != SF_OK) return r;
    cell->value.v.u8 = value;
    return SF_OK;
}

sf_result_t sf_param_cell_set_s16(sf_param_cell_t *cell, int16_t value) {
    if (!cell) return SF_ERR_INVALID_ARG;
    if (cell->value.kind != SF_PARAM_CELL_KIND_S16) return reject_cell_kind(cell, "s16");
    sf_result_t r = write_cell_backing(cell, (uint16_t)value, 2);
    if (r != SF_OK) return r;
    cell->value.v.s16 = value;
    return SF_OK;
}

sf_result_t sf_param_cell_set_u16(sf_param_cell_t *cell, uint16_t value) {
    if (!cell) return SF_ERR_INVALID_ARG;
    if (cell->value.kind != SF_PARAM_CELL_KIND_U16) return reject_cell_kind(cell, "u16");
    sf_result_t r = write_cell_backing(cell, value, 2);
    if (r != SF_OK) return r;
    cell->value.v.u16 = value;
    return SF_OK;
}

sf_result_t sf_param_cell_set_s32(sf_param_cell_t *cell, int32_t value) {
    if (!cell) return SF_ERR_INVALID_ARG;
    if (cell->value.kind != SF_PARAM_CELL_KIND_S32) return reject_cell_kind(cell, "s32");
    sf_result_t r = write_cell_backing(cell, (uint32_t)value, 4);
    if (r != SF_OK) return r;
    cell->value.v.s32 = value;
    return SF_OK;
}

sf_result_t sf_param_cell_set_u32(sf_param_cell_t *cell, uint32_t value) {
    if (!cell) return SF_ERR_INVALID_ARG;
    if (cell->value.kind != SF_PARAM_CELL_KIND_U32) return reject_cell_kind(cell, "u32");
    sf_result_t r = write_cell_backing(cell, value, 4);
    if (r != SF_OK) return r;
    cell->value.v.u32 = value;
    return SF_OK;
}

SF_API sf_result_t sf_param_cell_copy(sf_param_cell_t *dst, const sf_param_cell_t *src) {
    if (!dst || !src) {
        sfi_set_last_error_detail("sf_param_cell_copy requires non-NULL dst and src");
        return SF_ERR_INVALID_ARG;
    }
    if (dst->value.kind != src->value.kind) {
        sfi_set_last_error_detail("cannot copy cell of kind %s to cell of kind %s",
                                  cell_kind_name(src->value.kind),
                                  cell_kind_name(dst->value.kind));
        return SF_ERR_INVALID_ARG;
    }
    switch (src->value.kind) {
    case SF_PARAM_CELL_KIND_U8:
    case SF_PARAM_CELL_KIND_DUMMY8_BIT:
        return sf_param_cell_set_u8(dst, src->value.v.u8);
    case SF_PARAM_CELL_KIND_S8:
        return sf_param_cell_set_s8(dst, src->value.v.s8);
    case SF_PARAM_CELL_KIND_U16:
        return sf_param_cell_set_u16(dst, src->value.v.u16);
    case SF_PARAM_CELL_KIND_S16:
        return sf_param_cell_set_s16(dst, src->value.v.s16);
    case SF_PARAM_CELL_KIND_U32:
        return sf_param_cell_set_u32(dst, src->value.v.u32);
    case SF_PARAM_CELL_KIND_S32:
        return sf_param_cell_set_s32(dst, src->value.v.s32);
    case SF_PARAM_CELL_KIND_U64:
        return sf_param_cell_set_u64(dst, src->value.v.u64);
    case SF_PARAM_CELL_KIND_S64:
        return sf_param_cell_set_s64(dst, src->value.v.s64);
    case SF_PARAM_CELL_KIND_B32:
        return sf_param_cell_set_bool(dst, src->value.v.b32 != 0);
    case SF_PARAM_CELL_KIND_F32:
        return sf_param_cell_set_f32(dst, src->value.v.f32);
    case SF_PARAM_CELL_KIND_ANGLE32:
        return sf_param_cell_set_angle32(dst, src->value.v.angle32);
    case SF_PARAM_CELL_KIND_F64:
        return sf_param_cell_set_f64(dst, src->value.v.f64);
    case SF_PARAM_CELL_KIND_DUMMY8_ARRAY:
    case SF_PARAM_CELL_KIND_U8_ARRAY:
        return sf_param_cell_set_bytes(dst, src->value.v.bytes.data, src->value.v.bytes.size);
    case SF_PARAM_CELL_KIND_FIXSTR:
        return sf_param_cell_set_fixstr(dst, src->value.v.str_utf8,
                                       strlen(src->value.v.str_utf8));
    case SF_PARAM_CELL_KIND_FIXSTR_W:
        return sf_param_cell_set_fixstr(dst, src->value.v.str_utf8,
                                       strlen(src->value.v.str_utf8));
    default:
        sfi_set_last_error_detail("copying cells of kind %s is not supported",
                                  cell_kind_name(src->value.kind));
        return SF_ERR_UNSUPPORTED;
    }
}
