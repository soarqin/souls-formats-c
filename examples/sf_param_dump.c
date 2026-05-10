/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_param.h"
#include "souls_formats/sf_paramdef.h"
#include "souls_formats/sf_regulation.h"

#include <stdarg.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <windows.h>

static char *normalize_path_utf8(const char *path)
{
    if (!path) {
        return NULL;
    }

    if (path[0] == '\\' && path[1] == '\\') {
        return _strdup(path);
    }

    const size_t len = strlen(path);
    if (len >= 7 && path[0] == '/' && path[1] == 'm' && path[2] == 'n' && path[3] == 't' &&
        path[4] == '/' && path[5] != '\0' && path[6] == '/') {
        const char drive = path[5];
        if ((drive >= 'a' && drive <= 'z') || (drive >= 'A' && drive <= 'Z')) {
            const char *tail = path + 6;
            char *out = (char *)malloc(len + 3);
            if (!out) {
                return NULL;
            }
            out[0] = (char)((drive >= 'a' && drive <= 'z') ? (drive - 'a' + 'A') : drive);
            out[1] = ':';
            out[2] = '\\';
            size_t j = 3;
            for (const char *p = tail; *p; ++p) {
                out[j++] = (*p == '/') ? '\\' : *p;
            }
            out[j] = '\0';
            return out;
        }
    }

    if (path[0] == '/') {
        const char *distro = getenv("WSL_DISTRO_NAME");
        if (distro && distro[0] != '\0') {
            const size_t distro_len = strlen(distro);
            char *out = (char *)malloc(len + distro_len + 8);
            if (!out) {
                return NULL;
            }
            size_t j = (size_t)snprintf(out, len + distro_len + 8, "\\\\wsl$\\%s", distro);
            for (const char *p = path; *p; ++p) {
                out[j++] = (*p == '/') ? '\\' : *p;
            }
            out[j] = '\0';
            return out;
        }
    }

    char *out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    for (size_t i = 0; i < len; i++) {
        out[i] = (path[i] == '/') ? '\\' : path[i];
    }
    out[len] = '\0';
    return out;
}

static wchar_t *utf8_to_wide(const char *s)
{
    const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) {
        return NULL;
    }
    wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!w) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n) <= 0) {
        free(w);
        return NULL;
    }
    return w;
}

static sf_result_t read_whole_file(const wchar_t *path, uint8_t **out, size_t *out_size)
{
    sf_istream_t *in = NULL;
    sf_result_t   r  = sf_istream_open_wfile(&in, path, NULL);
    if (r != SF_OK) {
        return r;
    }

    const int64_t len = sf_istream_length(in);
    if (len <= 0) {
        sf_istream_close(in);
        return SF_ERR_IO;
    }

    uint8_t *buf = (uint8_t *)malloc((size_t)len);
    if (!buf) {
        sf_istream_close(in);
        return SF_ERR_OOM;
    }

    r = sf_istream_read(in, buf, (size_t)len);
    sf_istream_close(in);
    if (r != SF_OK) {
        free(buf);
        return r;
    }

    *out      = buf;
    *out_size = (size_t)len;
    return SF_OK;
}

static bool contains_icase(const char *haystack, const char *needle)
{
    if (!haystack || !needle || !needle[0]) {
        return false;
    }
    for (const char *h = haystack; *h; ++h) {
        const char *hp = h;
        const char *np = needle;
        while (*hp && *np && tolower((unsigned char)*hp) == tolower((unsigned char)*np)) {
            ++hp;
            ++np;
        }
        if (*np == '\0') {
            return true;
        }
    }
    return false;
}

static sf_result_t try_read_param_bytes(const uint8_t *data, size_t size,
                                        sf_param_t **out)
{
    sf_result_t r = sf_param_read_from_memory(out, data, size, NULL);
    if (r != SF_ERR_TRUNCATED) {
        return r;
    }

    sf_dcx_type_t type = SF_DCX_TYPE_UNKNOWN;
    r = sf_dcx_sniff(data, size, &type);
    if (r != SF_OK || type == SF_DCX_TYPE_NONE || type == SF_DCX_TYPE_UNKNOWN) {
        return SF_ERR_TRUNCATED;
    }

    void    *decoded = NULL;
    size_t   decoded_size = 0;
    sf_dcx_type_t out_type = SF_DCX_TYPE_UNKNOWN;
    r = sf_dcx_decompress(data, size, &decoded, &decoded_size, &out_type, NULL);
    if (r != SF_OK) {
        return r;
    }

    r = sf_param_read_from_memory(out, (const uint8_t *)decoded, decoded_size, NULL);
    sf_free(NULL, decoded);
    return r;
}

static sf_result_t load_param_from_bnd(const sf_bnd4_t *bnd, const char *param_name,
                                       const sf_paramdef_t *def, bool debug,
                                       sf_param_t **out)
{
    if (!bnd || !param_name || !out) {
        return SF_ERR_INVALID_ARG;
    }
    (void)def;

    const size_t count = sf_bnd4_file_count(bnd);
    if (getenv("SF_PARAM_DUMP_DEBUG") != NULL) {
        fprintf(stderr, "Debug: archive has %zu files\n", count);
        for (size_t i = 0; i < count && i < 40; i++) {
            const sf_binder_file_t *probe = sf_bnd4_get_file(bnd, i);
            fprintf(stderr, "Debug: [%zu] %s\n", i,
                    (probe && probe->name_utf8) ? probe->name_utf8 : "<no name>");
        }
    }

    char suffix[256];
    (void)snprintf(suffix, sizeof suffix, "%s.param", param_name);

    for (size_t i = 0; i < count; i++) {
        const sf_binder_file_t *file = sf_bnd4_get_file(bnd, i);
        if (!file || !file->name_utf8 || !contains_icase(file->name_utf8, suffix)) {
            continue;
        }

        if (debug) {
            fprintf(stderr, "Debug: suffix candidate[%zu] %s (%zu bytes)\n", i,
                    file->name_utf8, file->size);
        }

        sf_param_t *candidate = NULL;
        sf_result_t r = try_read_param_bytes(file->data, file->size, &candidate);
        if (r != SF_OK) {
            if (debug) {
                fprintf(stderr, "Debug: suffix candidate[%zu] read failed: %s\n", i,
                        sf_result_str(r));
            }
            continue;
        }

        if (debug) {
            fprintf(stderr, "Debug: suffix candidate[%zu] parse ok\n", i);
        }
        *out = candidate;
        return SF_OK;
    }

    for (size_t i = 0; i < count; i++) {
        const sf_binder_file_t *file = sf_bnd4_get_file(bnd, i);
        if (!file || !file->name_utf8 || !contains_icase(file->name_utf8, param_name)) {
            continue;
        }

        if (debug) {
            fprintf(stderr, "Debug: loose candidate[%zu] %s (%zu bytes)\n", i,
                    file->name_utf8, file->size);
        }

        sf_param_t *candidate = NULL;
        sf_result_t r = try_read_param_bytes(file->data, file->size, &candidate);
        if (r != SF_OK) {
            if (debug) {
                fprintf(stderr, "Debug: loose candidate[%zu] read failed: %s\n", i,
                        sf_result_str(r));
            }
            continue;
        }

        if (debug) {
            fprintf(stderr, "Debug: loose candidate[%zu] parse ok\n", i);
        }
        *out = candidate;
        return SF_OK;
    }

    return SF_ERR_NOT_FOUND;
}

static sf_result_t emitf(sf_ostream_t *file_out, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list ap_copy;
    va_copy(ap_copy, ap);
    const int needed = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);
    if (needed < 0) {
        va_end(ap);
        return SF_ERR_IO;
    }

    char *buf = (char *)malloc((size_t)needed + 1);
    if (!buf) {
        va_end(ap);
        return SF_ERR_OOM;
    }
    (void)vsnprintf(buf, (size_t)needed + 1, fmt, ap);
    va_end(ap);

    sf_result_t r = SF_OK;
    if (file_out) {
        r = sf_ostream_write(file_out, buf, (size_t)needed);
    } else {
        if (fwrite(buf, 1, (size_t)needed, stdout) != (size_t)needed) {
            r = SF_ERR_IO;
        }
    }
    free(buf);
    return r;
}

static sf_result_t emit_cell_value(sf_ostream_t *out, const sf_paramdef_field_t *field,
                                   const sf_param_cell_t *cell)
{
    const sf_paramdef_def_type_t type = sf_paramdef_field_get_display_type(field);
    const int32_t array_len = sf_paramdef_field_get_array_length(field);

    if (type == SF_PARAMDEF_DEF_TYPE_DUMMY8 || array_len > 1) {
        return emitf(out, "<bytes>");
    }

    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:
        return emitf(out, "%d", (int)sf_param_cell_get_s8(cell));
    case SF_PARAMDEF_DEF_TYPE_U8:
        return emitf(out, "%u", (unsigned)sf_param_cell_get_u8(cell));
    case SF_PARAMDEF_DEF_TYPE_S16:
        return emitf(out, "%d", (int)sf_param_cell_get_s16(cell));
    case SF_PARAMDEF_DEF_TYPE_U16:
        return emitf(out, "%u", (unsigned)sf_param_cell_get_u16(cell));
    case SF_PARAMDEF_DEF_TYPE_S32:
        return emitf(out, "%d", sf_param_cell_get_s32(cell));
    case SF_PARAMDEF_DEF_TYPE_U32:
        return emitf(out, "%u", sf_param_cell_get_u32(cell));
    case SF_PARAMDEF_DEF_TYPE_B32:
        return emitf(out, "%u", sf_param_cell_get_b32(cell));
    case SF_PARAMDEF_DEF_TYPE_F32:
        return emitf(out, "%g", (double)sf_param_cell_get_f32(cell));
    case SF_PARAMDEF_DEF_TYPE_ANGLE32:
        return emitf(out, "%g", (double)sf_param_cell_get_angle32(cell));
    case SF_PARAMDEF_DEF_TYPE_F64:
        return emitf(out, "%g", sf_param_cell_get_f64(cell));
    case SF_PARAMDEF_DEF_TYPE_FIXSTR:
    case SF_PARAMDEF_DEF_TYPE_FIXSTR_W:
        return emitf(out, "%s", sf_param_cell_get_string(cell));
    default:
        return emitf(out, "<bytes>");
    }
}

static sf_result_t write_tsv(sf_ostream_t *out, const sf_param_t *param, const sf_paramdef_t *def)
{
    const size_t field_count = sf_paramdef_get_field_count(def);
    const size_t row_count   = sf_param_get_row_count(param);

    sf_result_t r = emitf(out, "id\tname");
    if (r != SF_OK) {
        return r;
    }
    for (size_t i = 0; i < field_count; i++) {
        const sf_paramdef_field_t *field = sf_paramdef_get_field(def, i);
        r = emitf(out, "\t%s", sf_paramdef_field_get_internal_name(field));
        if (r != SF_OK) {
            return r;
        }
    }
    r = emitf(out, "\n");
    if (r != SF_OK) {
        return r;
    }

    for (size_t row_idx = 0; row_idx < row_count; row_idx++) {
        const sf_param_row_t *row = sf_param_get_row(param, row_idx);
        if (!row) {
            continue;
        }

        r = emitf(out, "%d\t%s", sf_param_row_get_id(row), sf_param_row_get_name(row));
        if (r != SF_OK) {
            return r;
        }

        for (size_t field_idx = 0; field_idx < field_count; field_idx++) {
            const sf_paramdef_field_t *field = sf_paramdef_get_field(def, field_idx);
            const char *name = sf_paramdef_field_get_internal_name(field);
            const sf_param_cell_t *cell = sf_param_row_find_cell(row, name);
            if (!cell) {
                cell = sf_param_row_get_cell(row, field_idx);
            }
            r = emitf(out, "\t");
            if (r != SF_OK) {
                return r;
            }
            r = emit_cell_value(out, field, cell);
            if (r != SF_OK) {
                return r;
            }
        }

        r = emitf(out, "\n");
        if (r != SF_OK) {
            return r;
        }
    }

    return SF_OK;
}

int main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Usage: sf_param_dump <regulation.bin> <paramdef.xml> <param-name> [out.tsv]\n");
        return 1;
    }

    const char *regulation_path    = argv[1];
    const char *paramdef_xml_path   = argv[2];
    const char *param_name          = argv[3];
    const char *out_tsv_path        = (argc >= 5) ? argv[4] : NULL;
    const bool   debug              = (out_tsv_path && strcmp(out_tsv_path, "--debug") == 0)
                                    || (argc >= 6 && strcmp(argv[5], "--debug") == 0);
    if (out_tsv_path && strcmp(out_tsv_path, "--debug") == 0) {
        out_tsv_path = NULL;
    }

    char *regulation_norm = normalize_path_utf8(regulation_path);
    wchar_t *wregulation = regulation_norm ? utf8_to_wide(regulation_norm) : NULL;
    free(regulation_norm);
    if (!wregulation) {
        fprintf(stderr, "Error: bad regulation path encoding\n");
        return 1;
    }
    char *paramdef_norm = normalize_path_utf8(paramdef_xml_path);
    wchar_t *wparamdef = paramdef_norm ? utf8_to_wide(paramdef_norm) : NULL;
    free(paramdef_norm);
    if (!wparamdef) {
        free(wregulation);
        fprintf(stderr, "Error: bad paramdef path encoding\n");
        return 1;
    }

    uint8_t *reg_bytes = NULL;
    size_t   reg_size  = 0;
    if (debug) fprintf(stderr, "Debug: reading regulation\n");
    sf_result_t r = read_whole_file(wregulation, &reg_bytes, &reg_size);
    free(wregulation);
    if (r != SF_OK) {
        free(wparamdef);
        fprintf(stderr, "Error: %s\n", sf_result_str(r));
        return 1;
    }

    uint8_t *plain_bytes = NULL;
    size_t   plain_size  = 0;
    if (debug) fprintf(stderr, "Debug: decrypting regulation\n");
    r = sf_regulation_decrypt_er(reg_bytes, reg_size, &plain_bytes, &plain_size, NULL);
    sf_free(NULL, reg_bytes);
    if (r != SF_OK) {
        free(wparamdef);
        fprintf(stderr, "Error: %s\n", sf_result_str(r));
        return 1;
    }

    sf_bnd4_t *bnd = NULL;
    if (debug) fprintf(stderr, "Debug: reading bnd4\n");
    r = sf_bnd4_read_from_memory(&bnd, plain_bytes, plain_size, NULL);
    sf_free(NULL, plain_bytes);
    if (r != SF_OK) {
        free(wparamdef);
        fprintf(stderr, "Error: %s\n", sf_result_str(r));
        return 1;
    }

    sf_param_t *param = NULL;
    sf_paramdef_t *def = NULL;
    if (debug) fprintf(stderr, "Debug: reading paramdef xml\n");
    uint8_t *xml_bytes = NULL;
    size_t   xml_size  = 0;
    r = read_whole_file(wparamdef, &xml_bytes, &xml_size);
    if (r == SF_OK) {
        r = sf_paramdef_read_xml_from_memory(&def, (const char *)xml_bytes, xml_size, NULL);
    }
    sf_free(NULL, xml_bytes);
    free(wparamdef);
    if (r != SF_OK) {
        sf_param_destroy(param);
        sf_bnd4_destroy(bnd);
        fprintf(stderr, "Error: %s\n", sf_result_str(r));
        return 1;
    }

    if (debug) fprintf(stderr, "Debug: loading param\n");
    r = load_param_from_bnd(bnd, param_name, def, debug, &param);
    if (r != SF_OK) {
        sf_paramdef_destroy(def);
        sf_bnd4_destroy(bnd);
        if (r == SF_ERR_NOT_FOUND) {
            fprintf(stderr, "Error: param '%s' not found\n", param_name);
        } else {
            fprintf(stderr, "Error: %s\n", sf_result_str(r));
        }
        return 1;
    }

    sf_ostream_t *out = NULL;
    if (out_tsv_path) {
        char *out_norm = normalize_path_utf8(out_tsv_path);
        wchar_t *wout = out_norm ? utf8_to_wide(out_norm) : NULL;
        free(out_norm);
        if (!wout) {
            sf_paramdef_destroy(def);
            sf_param_destroy(param);
            sf_bnd4_destroy(bnd);
            fprintf(stderr, "Error: bad output path encoding\n");
            return 1;
        }
        r = sf_ostream_open_wfile(&out, wout, NULL);
        free(wout);
        if (r != SF_OK) {
            sf_paramdef_destroy(def);
            sf_param_destroy(param);
            sf_bnd4_destroy(bnd);
            fprintf(stderr, "Error: %s\n", sf_result_str(r));
            return 1;
        }
    }

    r = write_tsv(out, param, def);
    if (out) {
        sf_ostream_close(out);
    }

    sf_paramdef_destroy(def);
    sf_param_destroy(param);
    sf_bnd4_destroy(bnd);

    if (r != SF_OK) {
        fprintf(stderr, "Error: %s\n", sf_result_str(r));
        return 1;
    }

    return 0;
}
