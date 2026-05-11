/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Probe: compare ER vs NR MSB headers to determine MSBE compatibility
 * Usage: probe_nightreign_msb.exe
 * Output: .sisyphus/evidence/task-4-nightreign-probe.md
 */

#include "er_test_helper.h"

#include "souls_formats/sf_bhd5.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PROBE_EVIDENCE_PATH ".sisyphus/evidence/task-4-nightreign-probe.md"
#define PROBE_HEADER_SIZE 64u

typedef struct probe_msb_header {
    const char *label;
    const char *path;
    uint8_t    *bytes;
    size_t      size;
    uint8_t     header[PROBE_HEADER_SIZE];
    char        magic[5];
    int32_t     version;
    int32_t     entry_list_count;
} probe_msb_header_t;

static int g_exit_code = 0;

static uint32_t read_u32_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void parse_header(probe_msb_header_t *msb)
{
    memcpy(msb->header, msb->bytes, PROBE_HEADER_SIZE);
    memcpy(msb->magic, msb->bytes, 4);
    msb->magic[4]         = '\0';
    msb->version          = (int32_t)read_u32_le(msb->bytes + 4);
    msb->entry_list_count = (int32_t)read_u32_le(msb->bytes + 8);
}

static void print_hex_dump(FILE *out, const uint8_t bytes[PROBE_HEADER_SIZE])
{
    for (size_t row = 0; row < PROBE_HEADER_SIZE; row += 16) {
        fprintf(out, "%04zX:", row);
        for (size_t col = 0; col < 16; ++col) {
            fprintf(out, " %02X", bytes[row + col]);
        }
        fputc('\n', out);
    }
}

static sf_result_t extract_er_msb(probe_msb_header_t *out)
{
    void  *bytes = NULL;
    size_t size  = 0;
    sf_result_t r = er_extract_from_data0("/map/mapstudio/m60_42_36_00.msb.dcx", &bytes, &size);
    if (r != SF_OK) {
        return r;
    }
    if (size < PROBE_HEADER_SIZE) {
        sf_free(NULL, bytes);
        return SF_ERR_TRUNCATED;
    }

    out->path  = "/map/mapstudio/m60_42_36_00.msb.dcx";
    out->bytes = (uint8_t *)bytes;
    out->size  = size;
    parse_header(out);
    return SF_OK;
}

static sf_result_t extract_and_decompress_nr(sf_bhd5_t *bhd, const char *path,
                                             probe_msb_header_t *out)
{
    void  *dcx      = NULL;
    size_t dcx_size = 0;
    sf_result_t r = sf_bhd5_extract_by_path(bhd, path, &dcx, &dcx_size, NULL);
    if (r != SF_OK) {
        return r;
    }

    void         *msb      = NULL;
    size_t        msb_size = 0;
    sf_dcx_type_t dcx_type = SF_DCX_TYPE_UNKNOWN;
    r = sf_dcx_decompress(dcx, dcx_size, &msb, &msb_size, &dcx_type, NULL);
    sf_free(NULL, dcx);
    if (r != SF_OK) {
        return r;
    }
    if (msb_size < PROBE_HEADER_SIZE) {
        sf_free(NULL, msb);
        return SF_ERR_TRUNCATED;
    }

    out->path  = path;
    out->bytes = (uint8_t *)msb;
    out->size  = msb_size;
    parse_header(out);
    return SF_OK;
}

static sf_result_t find_nightreign_msb(sf_bhd5_t *bhd, probe_msb_header_t *out)
{
    static char path[64];

    /* BHD5 stores path hashes, not names. The probe therefore walks the known
     * mapstudio naming space until a hashed path resolves to an NR MSB. */
    for (int area = 0; area < 100; ++area) {
        for (int block = 0; block < 100; ++block) {
            for (int tile = 0; tile < 100; ++tile) {
                snprintf(path, sizeof(path), "/map/mapstudio/m%02d_%02d_%02d_00.msb.dcx", area,
                         block, tile);
                const sf_result_t r = extract_and_decompress_nr(bhd, path, out);
                if (r == SF_OK) {
                    return SF_OK;
                }
                if (r != SF_ERR_NOT_FOUND) {
                    return r;
                }
            }
        }
    }
    return SF_ERR_NOT_FOUND;
}

static const char *decide_verdict(const probe_msb_header_t *er, const probe_msb_header_t *nr)
{
    const bool magic_equal   = memcmp(er->magic, nr->magic, 4) == 0;
    const bool version_equal = er->version == nr->version;

    if (magic_equal && version_equal) {
        return "COMPATIBLE (A)";
    }
    if (magic_equal) {
        return "MOSTLY_COMPATIBLE (B)";
    }
    return "DIVERGED (C)";
}

static void print_summary(FILE *out, const probe_msb_header_t *msb)
{
    fprintf(out, "- Path: `%s`\n", msb->path ? msb->path : "(none)");
    fprintf(out, "- Decompressed size: %zu bytes\n", msb->size);
    fprintf(out, "- Magic: `%s`\n", msb->magic);
    fprintf(out, "- Version field: %d\n", msb->version);
    fprintf(out, "- Entry list count field: %d\n\n", msb->entry_list_count);
}

static void write_report(FILE *out, const probe_msb_header_t *er, const probe_msb_header_t *nr,
                         const char *verdict)
{
    fprintf(out, "# Nightreign MSB Header Probe\n\n");
    fprintf(out, "## ER MSB header hex dump\n\n");
    print_summary(out, er);
    fprintf(out, "```text\n");
    print_hex_dump(out, er->header);
    fprintf(out, "```\n\n");

    fprintf(out, "## NR MSB header hex dump\n\n");
    print_summary(out, nr);
    fprintf(out, "```text\n");
    print_hex_dump(out, nr->header);
    fprintf(out, "```\n\n");

    fprintf(out, "## Diff + decision\n\n");
    fprintf(out, "- Magic bytes: %s (`%s` vs `%s`)\n",
            memcmp(er->magic, nr->magic, 4) == 0 ? "match" : "DIFF", er->magic, nr->magic);
    fprintf(out, "- Version field: %s (%d vs %d)\n",
            er->version == nr->version ? "match" : "DIFF", er->version, nr->version);
    fprintf(out, "- Entry list count field: %s (%d vs %d)\n",
            er->entry_list_count == nr->entry_list_count ? "match" : "DIFF",
            er->entry_list_count, nr->entry_list_count);
    fprintf(out, "- First 64 bytes: %s\n", memcmp(er->header, nr->header, PROBE_HEADER_SIZE) == 0
                                                ? "identical"
                                                : "differ");
    fprintf(out, "\nVERDICT: %s\n", verdict);
}

static void write_literal(sf_ostream_t *out, const char *text)
{
    if (g_exit_code == 0 && sf_ostream_write(out, text, strlen(text)) != SF_OK) {
        g_exit_code = 1;
    }
}

static void write_hex_dump(sf_ostream_t *out, const uint8_t bytes[PROBE_HEADER_SIZE])
{
    char line[80];
    for (size_t row = 0; row < PROBE_HEADER_SIZE; row += 16) {
        int n = snprintf(line, sizeof(line), "%04zX:", row);
        if (n <= 0 || (size_t)n >= sizeof(line)) {
            g_exit_code = 1;
            return;
        }
        size_t used = (size_t)n;
        for (size_t col = 0; col < 16; ++col) {
            n = snprintf(line + used, sizeof(line) - used, " %02X", bytes[row + col]);
            if (n <= 0 || (size_t)n >= sizeof(line) - used) {
                g_exit_code = 1;
                return;
            }
            used += (size_t)n;
        }
        n = snprintf(line + used, sizeof(line) - used, "\n");
        if (n <= 0 || (size_t)n >= sizeof(line) - used) {
            g_exit_code = 1;
            return;
        }
        write_literal(out, line);
    }
}

static void write_summary(sf_ostream_t *out, const probe_msb_header_t *msb)
{
    char line[256];
    snprintf(line, sizeof(line), "- Path: `%s`\n", msb->path ? msb->path : "(none)");
    write_literal(out, line);
    snprintf(line, sizeof(line), "- Decompressed size: %zu bytes\n", msb->size);
    write_literal(out, line);
    snprintf(line, sizeof(line), "- Magic: `%s`\n", msb->magic);
    write_literal(out, line);
    snprintf(line, sizeof(line), "- Version field: %d\n", msb->version);
    write_literal(out, line);
    snprintf(line, sizeof(line), "- Entry list count field: %d\n\n", msb->entry_list_count);
    write_literal(out, line);
}

static sf_result_t write_evidence_file(const probe_msb_header_t *er, const probe_msb_header_t *nr,
                                       const char *verdict)
{
    sf_ostream_t *out = NULL;
    sf_result_t   r   = sf_ostream_open_file(&out, PROBE_EVIDENCE_PATH, NULL);
    if (r != SF_OK) {
        return r;
    }

    char line[256];
    write_literal(out, "# Nightreign MSB Header Probe\n\n");
    write_literal(out, "## ER MSB header hex dump\n\n");
    write_summary(out, er);
    write_literal(out, "```text\n");
    write_hex_dump(out, er->header);
    write_literal(out, "```\n\n");

    write_literal(out, "## NR MSB header hex dump\n\n");
    write_summary(out, nr);
    write_literal(out, "```text\n");
    write_hex_dump(out, nr->header);
    write_literal(out, "```\n\n");

    write_literal(out, "## Diff + decision\n\n");
    snprintf(line, sizeof(line), "- Magic bytes: %s (`%s` vs `%s`)\n",
             memcmp(er->magic, nr->magic, 4) == 0 ? "match" : "DIFF", er->magic, nr->magic);
    write_literal(out, line);
    snprintf(line, sizeof(line), "- Version field: %s (%d vs %d)\n",
             er->version == nr->version ? "match" : "DIFF", er->version, nr->version);
    write_literal(out, line);
    snprintf(line, sizeof(line), "- Entry list count field: %s (%d vs %d)\n",
             er->entry_list_count == nr->entry_list_count ? "match" : "DIFF",
             er->entry_list_count, nr->entry_list_count);
    write_literal(out, line);
    snprintf(line, sizeof(line), "- First 64 bytes: %s\n",
             memcmp(er->header, nr->header, PROBE_HEADER_SIZE) == 0 ? "identical" : "differ");
    write_literal(out, line);
    snprintf(line, sizeof(line), "\nVERDICT: %s\n", verdict);
    write_literal(out, line);

    sf_ostream_close(out);
    return g_exit_code == 0 ? SF_OK : SF_ERR_IO;
}

static sf_result_t open_nightreign_data0(sf_bhd5_t **out)
{
    static const sf_bhd5_game_t games[] = {
        SF_BHD5_GAME_NIGHTREIGN,
        SF_BHD5_GAME_ELDENRING,
        SF_BHD5_GAME_ARMOREDCORE6,
    };

    sf_result_t first_error = SF_ERR_INTERNAL;
    for (size_t i = 0; i < sizeof(games) / sizeof(games[0]); ++i) {
        sf_result_t r = sf_bhd5_open(out, L"C:/Games/ELDEN RING NIGHTREIGN/Game/data0.bhd",
                                    L"C:/Games/ELDEN RING NIGHTREIGN/Game/data0.bdt",
                                    games[i], NULL);
        if (r == SF_OK) {
            return SF_OK;
        }
        if (i == 0) {
            first_error = r;
        }
    }
    return first_error;
}

int main(void)
{
    sf_oodle_set_search_path(L"C:/Games/ELDEN RING NIGHTREIGN/Game");

    sf_bhd5_t *nr_bhd = NULL;
    sf_result_t r = open_nightreign_data0(&nr_bhd);
    if (r != SF_OK) {
        probe_msb_header_t er = {.label = "ER"};
        probe_msb_header_t nr = {.label = "NR", .path = "BHD5 open failed before MSB extraction"};
        memcpy(nr.magic, "FAIL", 5);
        nr.version = -1;
        nr.entry_list_count = -1;

        const sf_result_t er_r = extract_er_msb(&er);
        if (er_r != SF_OK) {
            er.path = "ER BHD5 extraction failed before MSB header read";
            memcpy(er.magic, "FAIL", 5);
            er.version = -1;
            er.entry_list_count = -1;
            fprintf(stderr, "failed to open NR data0: %s; also failed to extract ER MSB: %s\n",
                    sf_result_str(r), sf_result_str(er_r));
        }

        const char *verdict = "DIVERGED (C)";
        write_report(stdout, &er, &nr, verdict);
        const sf_result_t wr = write_evidence_file(&er, &nr, verdict);
        if (er.bytes) {
            sf_free(NULL, er.bytes);
        }
        if (wr != SF_OK) {
            fprintf(stderr, "failed to write evidence file after NR open failure: %s\n",
                    sf_result_str(wr));
            return 1;
        }
        return 0;
    }

    probe_msb_header_t er = {.label = "ER"};
    probe_msb_header_t nr = {.label = "NR"};
    r = extract_er_msb(&er);
    if (r != SF_OK) {
        fprintf(stderr, "failed to extract ER MSB: %s\n", sf_result_str(r));
        sf_bhd5_close(nr_bhd);
        return 1;
    }

    r = find_nightreign_msb(nr_bhd, &nr);
    if (r != SF_OK) {
        fprintf(stderr, "failed to find/extract NR MSB: %s\n", sf_result_str(r));
        sf_free(NULL, er.bytes);
        sf_bhd5_close(nr_bhd);
        return 1;
    }

    const char *verdict = decide_verdict(&er, &nr);
    write_report(stdout, &er, &nr, verdict);

    r = write_evidence_file(&er, &nr, verdict);
    if (r != SF_OK) {
        fprintf(stderr, "failed to write evidence file: %s\n", sf_result_str(r));
        sf_free(NULL, nr.bytes);
        sf_free(NULL, er.bytes);
        sf_bhd5_close(nr_bhd);
        return 1;
    }

    sf_free(NULL, nr.bytes);
    sf_free(NULL, er.bytes);
    sf_bhd5_close(nr_bhd);
    return 0;
}
