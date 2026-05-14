/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_param.h"
#include "param/param_internal.h"

#include <stdio.h>
#include <string.h>

static void init_cell(sf_param_cell_t *cell, const sf_param_t *param, sf_param_row_t *row,
                      sf_param_cell_kind_t kind, int32_t byte_count, size_t byte_offset) {
    memset(cell, 0, sizeof(*cell));
    cell->parent_param = param;
    cell->parent_row = row;
    cell->value.kind = kind;
    cell->byte_count = byte_count;
    cell->array_length = byte_count;
    cell->byte_offset = byte_offset;
}

int main(void) {
    sf_param_cell_t s64_cell;
    sf_param_cell_t u64_cell;
    sf_param_cell_t f32_cell;
    sf_param_cell_t f64_cell;
    sf_param_cell_t bool_cell;
    sf_param_cell_t byte_cell;
    sf_param_cell_t fixstr_cell;
    uint8_t row_data[41];
    sf_param_t param;
    sf_param_row_t row;

    memset(row_data, 0, sizeof(row_data));
    memset(&param, 0, sizeof(param));
    memset(&row, 0, sizeof(row));
    row.data = row_data;
    row.data_size = sizeof(row_data);

    init_cell(&s64_cell, &param, &row, SF_PARAM_CELL_KIND_S64, 8, 0);
    init_cell(&u64_cell, &param, &row, SF_PARAM_CELL_KIND_U64, 8, 8);
    init_cell(&f32_cell, &param, &row, SF_PARAM_CELL_KIND_F32, 4, 16);
    init_cell(&f64_cell, &param, &row, SF_PARAM_CELL_KIND_F64, 8, 20);
    init_cell(&bool_cell, &param, &row, SF_PARAM_CELL_KIND_B32, 4, 28);
    init_cell(&byte_cell, &param, &row, SF_PARAM_CELL_KIND_U8, 1, 32);
    init_cell(&fixstr_cell, &param, &row, SF_PARAM_CELL_KIND_FIXSTR, 8, 33);

    if (sf_param_cell_set_s64(&s64_cell, -42) != SF_OK ||
        sf_param_cell_set_u64(&u64_cell, 42) != SF_OK ||
        sf_param_cell_set_f32(&f32_cell, 1.25f) != SF_OK ||
        sf_param_cell_set_f64(&f64_cell, 2.5) != SF_OK ||
        sf_param_cell_set_bool(&bool_cell, true) != SF_OK ||
        sf_param_cell_set_byte(&byte_cell, 0x7Fu) != SF_OK ||
        sf_param_cell_set_fixstr(&fixstr_cell, "demo", 4) != SF_OK) {
        const char *detail = sf_last_error_detail();
        fprintf(stderr, "setter failed: %s\n", detail ? detail : sf_result_str(SF_ERR_INVALID_ARG));
        return 1;
    }

    printf("s64=%lld\n", (long long)sf_param_cell_get_value(&s64_cell).v.s64);
    printf("u64=%llu\n", (unsigned long long)sf_param_cell_get_value(&u64_cell).v.u64);
    printf("f32=%.2f\n", sf_param_cell_get_f32(&f32_cell));
    printf("f64=%.2f\n", sf_param_cell_get_f64(&f64_cell));
    printf("bool=%s\n", sf_param_cell_get_bool(&bool_cell) ? "true" : "false");
    printf("byte=0x%02X\n", (unsigned)sf_param_cell_get_u8(&byte_cell));
    printf("fixstr=%s\n", sf_param_cell_get_string(&fixstr_cell));

    return 0;
}
