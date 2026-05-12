/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — internal PARAM structures shared by PARAM modules.
 */

#ifndef SF_PARAM_INTERNAL_H
#define SF_PARAM_INTERNAL_H

#include "souls_formats/sf_param.h"
#include "souls_formats/sf_paramdef.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct sf_param_cell {
    char *internal_name;

    sf_paramdef_def_type_t display_type;
    int32_t bit_size;
    int32_t array_length;
    int32_t byte_count;

    sf_param_cell_value_t value;
};

struct sf_param_row {
    int32_t id;
    int64_t data_offset;
    uint8_t *data;
    size_t data_size;
    char *name;

    sf_param_cell_t *cells;
    size_t cell_count;
    size_t cell_data_size;
    bool cells_applied;
};

struct sf_param {
    const sf_allocator_t *alloc;

    sf_param_row_t *rows;
    size_t row_count;
    uint8_t *row_data_arena;
    size_t row_data_arena_size;

    char *param_type;

    bool big_endian;
    bool headerless_rows;
    sf_param_format_flags1_t format2d;
    sf_param_format_flags2_t format2e;
    uint8_t paramdef_format_version;
    int16_t paramdef_data_version;
    int16_t unk06;
    int64_t detected_size;
};

void sfi_param_row_clear_cells(sf_param_row_t *row, const sf_allocator_t *alloc);

sf_result_t sfi_param_row_cells_to_bytes(const sf_param_t *param,
                                         const sf_param_row_t *row,
                                         uint8_t **out,
                                         size_t *out_size);

#endif /* SF_PARAM_INTERNAL_H */
