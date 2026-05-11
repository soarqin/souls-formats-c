/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Probe: sample Elden Ring allmaterial.matbinbnd.dcx MATBIN ParamType and
 * sampler type usage before implementing the production MATBIN reader.
 *
 * Upstream reference: SoulsFormats/Formats/MATBIN.cs
 */

#include "er_test_helper.h"

#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"

#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MATBIN_BND_PATH "/material/allmaterial.matbinbnd.dcx"
#define PROBE_EVIDENCE_PATH ".sisyphus/evidence/task-5-matbin-survey.md"
#define SAMPLE_LIMIT 10u
#define PARAM_ENTRY_SIZE 40u
#define SAMPLER_ENTRY_SIZE 48u

typedef struct string_set {
    char  **items;
    size_t  count;
    size_t  capacity;
} string_set_t;

typedef struct survey {
    size_t       total_entries;
    const char  *sample_paths[SAMPLE_LIMIT];
    size_t       sample_count;
    uint64_t     param_hist[8];
    uint64_t     unknown_values[32];
    size_t       unknown_count;
    string_set_t sampler_types;
    const char  *minimum_path;
    const uint8_t *minimum_bytes;
    size_t       minimum_size;
} survey_t;

typedef struct report_buffer {
    char  *data;
    size_t len;
    size_t capacity;
} report_buffer_t;

static uint32_t read_u32_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t read_u64_le(const uint8_t *p)
{
    return ((uint64_t)read_u32_le(p)) | ((uint64_t)read_u32_le(p + 4) << 32);
}

static uint64_t bhd5_path_hash_64_133(const char *path)
{
    uint64_t h = 0;
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
        unsigned char c = *p == '\\' ? '/' : *p;
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        h = (uint64_t)c + 133u * h;
    }
    return h;
}

static sf_result_t extract_candidate_path(const char *path, void **out, size_t *out_size)
{
    sf_result_t r = er_extract_from_data0(path, out, out_size);
    if (r == SF_OK) return r;

    r = er_helper_init();
    if (r != SF_OK) return r;

    void *raw = NULL;
    size_t raw_size = 0;
    r = sf_bhd5_extract_by_hash_64(er_helper_get_bhd5_for_testing(), bhd5_path_hash_64_133(path),
                                   &raw, &raw_size, NULL);
    if (r != SF_OK) return r;

    sf_dcx_type_t type = SF_DCX_TYPE_UNKNOWN;
    const sf_result_t sniff_r = sf_dcx_sniff(raw, raw_size, &type);
    if (sniff_r == SF_OK && type != SF_DCX_TYPE_NONE && type != SF_DCX_TYPE_UNKNOWN) {
        void *decompressed = NULL;
        size_t decompressed_size = 0;
        sf_dcx_type_t out_type = SF_DCX_TYPE_UNKNOWN;
        r = sf_dcx_decompress(raw, raw_size, &decompressed, &decompressed_size, &out_type, NULL);
        sf_free(NULL, raw);
        if (r != SF_OK) return r;
        *out = decompressed;
        *out_size = decompressed_size;
        return SF_OK;
    }

    *out = raw;
    *out_size = raw_size;
    return SF_OK;
}

static bool ends_with_matbin(const char *path)
{
    if (!path) return false;
    const size_t len = strlen(path);
    const char suffix[] = ".matbin";
    const size_t suffix_len = sizeof(suffix) - 1u;
    return len >= suffix_len && strcmp(path + len - suffix_len, suffix) == 0;
}

static int param_type_index(uint32_t raw)
{
    switch (raw) {
    case 0: return 0;
    case 4: return 1;
    case 5: return 2;
    case 8: return 3;
    case 9: return 4;
    case 10: return 5;
    case 11: return 6;
    case 12: return 7;
    default: return -1;
    }
}

static void remember_unknown(survey_t *survey, uint32_t raw)
{
    for (size_t i = 0; i < survey->unknown_count; ++i) {
        if (survey->unknown_values[i] == raw) return;
    }
    if (survey->unknown_count < sizeof(survey->unknown_values) / sizeof(survey->unknown_values[0])) {
        survey->unknown_values[survey->unknown_count++] = raw;
    }
}

static sf_result_t string_set_add(string_set_t *set, const char *value)
{
    if (!value || value[0] == '\0') return SF_OK;
    for (size_t i = 0; i < set->count; ++i) {
        if (strcmp(set->items[i], value) == 0) return SF_OK;
    }

    if (set->count == set->capacity) {
        const size_t new_capacity = set->capacity ? set->capacity * 2u : 16u;
        char **new_items = sf_default_allocator()->alloc(new_capacity * sizeof(*new_items),
                                                         sf_default_allocator()->user);
        if (!new_items) return SF_ERR_OOM;
        if (set->items) {
            memcpy(new_items, set->items, set->count * sizeof(*new_items));
            sf_free(NULL, set->items);
        }
        set->items = new_items;
        set->capacity = new_capacity;
    }

    const size_t len = strlen(value) + 1u;
    char *copy = sf_default_allocator()->alloc(len, sf_default_allocator()->user);
    if (!copy) return SF_ERR_OOM;
    memcpy(copy, value, len);
    set->items[set->count++] = copy;
    return SF_OK;
}

static void string_set_destroy(string_set_t *set)
{
    for (size_t i = 0; i < set->count; ++i) sf_free(NULL, set->items[i]);
    sf_free(NULL, set->items);
    set->items = NULL;
    set->count = 0;
    set->capacity = 0;
}

static sf_result_t read_utf16_ascii_z(const uint8_t *data, size_t size, uint64_t offset,
                                      char *out, size_t out_size)
{
    if (!out || out_size == 0) return SF_ERR_INVALID_ARG;
    out[0] = '\0';
    if (offset >= size) return SF_ERR_OUT_OF_RANGE;

    size_t src = (size_t)offset;
    size_t dst = 0;
    while (src + 1u < size) {
        const uint16_t ch = (uint16_t)data[src] | ((uint16_t)data[src + 1u] << 8);
        src += 2u;
        if (ch == 0) {
            out[dst] = '\0';
            return SF_OK;
        }
        if (dst + 1u >= out_size) return SF_ERR_OUT_OF_RANGE;
        out[dst++] = ch <= 0x7Fu ? (char)ch : '?';
    }
    return SF_ERR_TRUNCATED;
}

static sf_result_t parse_matbin(const sf_binder_file_t *file, survey_t *survey)
{
    if (!file || !file->data || file->size < 56u || !survey) return SF_ERR_INVALID_ARG;
    const uint8_t *data = file->data;
    const size_t size = file->size;

    if (memcmp(data, "MAB\0", 4u) != 0 || read_u32_le(data + 4u) != 2u) return SF_ERR_BAD_MAGIC;

    const uint32_t param_count = read_u32_le(data + 28u);
    const uint32_t sampler_count = read_u32_le(data + 32u);
    const size_t param_base = 56u;
    if (param_count > (SIZE_MAX - param_base) / PARAM_ENTRY_SIZE) {
        return SF_ERR_TRUNCATED;
    }
    const size_t sampler_base = param_base + (size_t)param_count * PARAM_ENTRY_SIZE;
    if (sampler_count > (SIZE_MAX - sampler_base) / SAMPLER_ENTRY_SIZE ||
        sampler_base + (size_t)sampler_count * SAMPLER_ENTRY_SIZE > size) {
        return SF_ERR_TRUNCATED;
    }

    for (uint32_t i = 0; i < param_count; ++i) {
        const size_t entry = param_base + (size_t)i * PARAM_ENTRY_SIZE;
        const uint32_t raw_type = read_u32_le(data + entry + 20u);
        const int idx = param_type_index(raw_type);
        if (idx >= 0) {
            survey->param_hist[idx]++;
        } else {
            remember_unknown(survey, raw_type);
        }
    }

    for (uint32_t i = 0; i < sampler_count; ++i) {
        const size_t entry = sampler_base + (size_t)i * SAMPLER_ENTRY_SIZE;
        const uint64_t type_offset = read_u64_le(data + entry);
        char type[256];
        sf_result_t r = read_utf16_ascii_z(data, size, type_offset, type, sizeof(type));
        if (r != SF_OK) return r;
        r = string_set_add(&survey->sampler_types, type);
        if (r != SF_OK) return r;
    }

    return SF_OK;
}

static sf_result_t report_append(report_buffer_t *buf, const char *text)
{
    const size_t add_len = strlen(text);
    if (buf->len + add_len + 1u > buf->capacity) {
        size_t new_capacity = buf->capacity ? buf->capacity * 2u : 4096u;
        while (new_capacity < buf->len + add_len + 1u) new_capacity *= 2u;
        char *new_data = sf_default_allocator()->alloc(new_capacity, sf_default_allocator()->user);
        if (!new_data) return SF_ERR_OOM;
        if (buf->data) {
            memcpy(new_data, buf->data, buf->len);
            sf_free(NULL, buf->data);
        }
        buf->data = new_data;
        buf->capacity = new_capacity;
    }
    memcpy(buf->data + buf->len, text, add_len);
    buf->len += add_len;
    buf->data[buf->len] = '\0';
    return SF_OK;
}

static sf_result_t report_appendf(report_buffer_t *buf, const char *fmt, ...)
{
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    const int written = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (written < 0) return SF_ERR_INTERNAL;
    if ((size_t)written < sizeof(tmp)) return report_append(buf, tmp);

    char *large = sf_default_allocator()->alloc((size_t)written + 1u,
                                                sf_default_allocator()->user);
    if (!large) return SF_ERR_OOM;
    va_start(ap, fmt);
    vsnprintf(large, (size_t)written + 1u, fmt, ap);
    va_end(ap);
    sf_result_t r = report_append(buf, large);
    sf_free(NULL, large);
    return r;
}

static sf_result_t build_report(const survey_t *survey, report_buffer_t *report)
{
    static const char *names[8] = {"BOOL(0)",   "INT(4)",    "INT2(5)",
                                   "FLOAT(8)",  "FLOAT2(9)", "FLOAT3(10)",
                                   "FLOAT4(11)", "FLOAT5(12)"};

    sf_result_t r = report_appendf(report, "TOTAL_ENTRIES: %zu\n", survey->total_entries);
    if (r != SF_OK) return r;
    r = report_append(report, "SAMPLE_PATHS:\n");
    if (r != SF_OK) return r;
    for (size_t i = 0; i < survey->sample_count; ++i) {
        r = report_appendf(report, "  %s\n", survey->sample_paths[i]);
        if (r != SF_OK) return r;
    }

    r = report_append(report, "\nPARAMTYPE_HIST:\n");
    if (r != SF_OK) return r;
    for (size_t i = 0; i < 8u; ++i) {
        r = report_appendf(report, "  %s: %llu\n", names[i],
                           (unsigned long long)survey->param_hist[i]);
        if (r != SF_OK) return r;
    }
    for (size_t i = 0; i < survey->unknown_count; ++i) {
        r = report_appendf(report, "UNKNOWN_PARAMTYPE: %llu\n",
                           (unsigned long long)survey->unknown_values[i]);
        if (r != SF_OK) return r;
    }

    r = report_append(report, "\nSAMPLER_TYPES:\n");
    if (r != SF_OK) return r;
    for (size_t i = 0; i < survey->sampler_types.count; ++i) {
        r = report_appendf(report, "  %s\n", survey->sampler_types.items[i]);
        if (r != SF_OK) return r;
    }

    r = report_appendf(report, "\nMINIMUM_MATBIN_HEX:\n  PATH: %s\n  SIZE: %zu\n",
                       survey->minimum_path ? survey->minimum_path : "(none)",
                       survey->minimum_size);
    if (r != SF_OK) return r;
    const size_t dump_size = survey->minimum_size < 500u ? survey->minimum_size : 64u;
    for (size_t row = 0; row < dump_size; row += 16u) {
        r = report_appendf(report, "  %04zX:", row);
        if (r != SF_OK) return r;
        const size_t row_end = row + 16u < dump_size ? row + 16u : dump_size;
        for (size_t col = row; col < row_end; ++col) {
            r = report_appendf(report, " %02X", survey->minimum_bytes[col]);
            if (r != SF_OK) return r;
        }
        r = report_append(report, "\n");
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

static sf_result_t write_evidence(const report_buffer_t *report)
{
    sf_ostream_t *out = NULL;
    sf_result_t r = sf_ostream_open_file(&out, PROBE_EVIDENCE_PATH, NULL);
    if (r != SF_OK) return r;
    r = sf_ostream_write(out, report->data, report->len);
    sf_ostream_close(out);
    return r;
}

int main(void)
{
    static const char *candidate_paths[] = {
        MATBIN_BND_PATH,
        "/material/allmaterial.matbinbnd",
    };
    void *bnd_bytes = NULL;
    size_t bnd_size = 0;
    sf_result_t r = SF_ERR_NOT_FOUND;
    const char *used_path = NULL;
    for (size_t i = 0; i < sizeof(candidate_paths) / sizeof(candidate_paths[0]); ++i) {
        r = extract_candidate_path(candidate_paths[i], &bnd_bytes, &bnd_size);
        if (r == SF_OK) {
            used_path = candidate_paths[i];
            break;
        }
    }
    if (r != SF_OK) {
        fprintf(stderr, "failed to extract %s: %s\n", MATBIN_BND_PATH, sf_result_str(r));
        return 1;
    }
    (void)used_path;

    sf_bnd4_t *bnd = NULL;
    r = sf_bnd4_read_from_memory(&bnd, bnd_bytes, bnd_size, NULL);
    sf_free(NULL, bnd_bytes);
    if (r != SF_OK) {
        fprintf(stderr, "failed to read BND4: %s\n", sf_result_str(r));
        return 1;
    }

    survey_t survey = {0};
    survey.total_entries = sf_bnd4_file_count(bnd);
    survey.minimum_size = SIZE_MAX;
    for (size_t i = 0; i < survey.total_entries; ++i) {
        const sf_binder_file_t *file = sf_bnd4_get_file(bnd, i);
        if (!file || !ends_with_matbin(file->name_utf8)) continue;
        if (file->size < survey.minimum_size) {
            survey.minimum_path = file->name_utf8;
            survey.minimum_bytes = file->data;
            survey.minimum_size = file->size;
        }
        if (survey.sample_count >= SAMPLE_LIMIT) continue;
        survey.sample_paths[survey.sample_count++] = file->name_utf8;
        r = parse_matbin(file, &survey);
        if (r != SF_OK) {
            fprintf(stderr, "failed to parse %s: %s\n", file->name_utf8, sf_result_str(r));
            string_set_destroy(&survey.sampler_types);
            sf_bnd4_destroy(bnd);
            return 1;
        }
    }

    report_buffer_t report = {0};
    r = build_report(&survey, &report);
    if (r == SF_OK) r = write_evidence(&report);
    if (r == SF_OK) fwrite(report.data, 1u, report.len, stdout);

    sf_free(NULL, report.data);
    string_set_destroy(&survey.sampler_types);
    sf_bnd4_destroy(bnd);
    if (r != SF_OK) {
        fprintf(stderr, "failed to write report: %s\n", sf_result_str(r));
        return 1;
    }
    return 0;
}
