/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_param.h"
#include "souls_formats/sf_paramdef.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static int to_wide(const char *src, wchar_t *dst, size_t dst_count) {
    size_t converted = mbstowcs(dst, src, dst_count);
    return converted != (size_t)-1 && converted < dst_count;
}

int main(int argc, char **argv) {
    if (argc != 6) {
        puts("usage: sf_param_set_demo_int <param> <paramdef.xml> <row-index> <s32-cell> <value>");
        return 0;
    }

    wchar_t param_path[1024];
    wchar_t def_path[1024];
    if (!to_wide(argv[1], param_path, sizeof(param_path) / sizeof(param_path[0])) ||
        !to_wide(argv[2], def_path, sizeof(def_path) / sizeof(def_path[0]))) {
        fputs("failed to convert path\n", stderr);
        return 2;
    }

    sf_param_t *param = NULL;
    sf_result_t r = sf_param_read_from_path(&param, param_path, NULL);
    if (r != SF_OK) {
        fprintf(stderr, "sf_param_read_from_path: %s\n", sf_result_str(r));
        return 3;
    }

    sf_paramdef_t *def = NULL;
    r = sf_paramdef_read_xml_from_path(&def, def_path, NULL);
    if (r != SF_OK) {
        fprintf(stderr, "sf_paramdef_read_xml_from_path: %s\n", sf_result_str(r));
        sf_param_destroy(param);
        return 4;
    }

    r = sf_param_apply_paramdef(param, def, SF_PARAM_APPLY_CAREFUL);
    if (r != SF_OK) {
        fprintf(stderr, "sf_param_apply_paramdef: %s\n", sf_result_str(r));
        sf_paramdef_destroy(def);
        sf_param_destroy(param);
        return 5;
    }

    size_t row_index = (size_t)strtoull(argv[3], NULL, 10);
    int32_t value = (int32_t)strtol(argv[5], NULL, 10);
    sf_param_row_t *row = sf_param_get_row_mut(param, row_index);
    sf_param_cell_t *cell = sf_param_row_find_cell_mut(row, argv[4]);
    r = sf_param_cell_set_s32(cell, value);
    if (r != SF_OK) {
        fprintf(stderr, "sf_param_cell_set_s32: %s (%s)\n", sf_result_str(r),
                sf_last_error_detail() ? sf_last_error_detail() : "no detail");
        sf_paramdef_destroy(def);
        sf_param_destroy(param);
        return 6;
    }

    const sf_param_row_t *const_row = sf_param_get_row(param, row_index);
    const sf_param_cell_t *const_cell = sf_param_row_find_cell(const_row, argv[4]);
    if (sf_param_cell_get_s32(const_cell) != value) {
        fputs("const getter did not observe the updated value\n", stderr);
        sf_paramdef_destroy(def);
        sf_param_destroy(param);
        return 7;
    }
    printf("%s = %ld\n", argv[4], (long)value);

    sf_paramdef_destroy(def);
    sf_param_destroy(param);
    return 0;
}
