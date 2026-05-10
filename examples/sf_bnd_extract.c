/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sf_bnd_extract — extract a BND4 archive (with optional outer DCX wrap)
 * to a directory.
 *
 *     sf_bnd_extract <input.bnd[.dcx]> <output_dir>
 *
 * Each entry is written as <output_dir>/<sanitized_name>. Forward and
 * back-slashes inside an entry name are folded to '_' to flatten the
 * binder's internal directory structure into a single output directory,
 * matching the de-facto convention of Yabber and similar tools.
 *
 * stderr is used for all human-facing messages (status, warnings, errors).
 * stdout is reserved so callers can pipe the tool's future scriptable
 * output without contamination. Exit codes:
 *
 *   0  one or more entries written successfully
 *   1  argv parse / encoding failure
 *   2  failed to read input BND
 *   3  read OK but produced zero output entries
 */

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static wchar_t *utf8_to_wide(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) {
        return NULL;
    }
    wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!w) {
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static void sanitize_name(const char *name, char *buf, size_t buf_size)
{
    /* Strip leading separators to defeat path traversal via crafted
     * binder entry names ("/etc/passwd" → "etc_passwd"). */
    const char *src = name;
    while (*src == '/' || *src == '\\') {
        src++;
    }
    size_t i = 0;
    for (; *src && i + 1 < buf_size; src++, i++) {
        char c = *src;
        if (c == '/' || c == '\\' || c == ':') {
            c = '_';
        }
        buf[i] = c;
    }
    buf[i] = '\0';
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr,
                "Usage: sf_bnd_extract <input.bnd[.dcx]> <output_dir>\n");
        return 1;
    }

    wchar_t *input_w = utf8_to_wide(argv[1]);
    if (!input_w) {
        fprintf(stderr, "Error: bad input path encoding\n");
        return 1;
    }

    sf_bnd4_t        *bnd = NULL;
    const sf_result_t r_read =
        sf_bnd4_read_from_path(&bnd, input_w, NULL);
    free(input_w);
    if (r_read != SF_OK) {
        fprintf(stderr, "Error: failed to read BND4: %d\n", (int)r_read);
        return 2;
    }

    wchar_t *outdir_w = utf8_to_wide(argv[2]);
    if (!outdir_w) {
        sf_bnd4_destroy(bnd);
        fprintf(stderr, "Error: bad output path encoding\n");
        return 1;
    }
    /* CreateDirectoryW returns 0 on failure; ERROR_ALREADY_EXISTS is OK. */
    (void)CreateDirectoryW(outdir_w, NULL);

    const size_t count     = sf_bnd4_file_count(bnd);
    size_t       extracted = 0;
    for (size_t i = 0; i < count; i++) {
        const sf_binder_file_t *f = sf_bnd4_get_file(bnd, i);
        if (!f || !f->data || f->size == 0) {
            continue;
        }

        char sanitized[512];
        sanitize_name(f->name_utf8 ? f->name_utf8 : "unnamed",
                      sanitized, sizeof sanitized);

        wchar_t *sanitized_w = utf8_to_wide(sanitized);
        if (!sanitized_w) {
            fprintf(stderr, "Warning: bad name encoding for entry %zu\n", i);
            continue;
        }

        wchar_t   out_path[1024];
        const int n = swprintf(out_path,
                               sizeof out_path / sizeof out_path[0],
                               L"%ls\\%ls", outdir_w, sanitized_w);
        free(sanitized_w);
        if (n < 0) {
            fprintf(stderr, "Warning: output path too long for entry %zu\n", i);
            continue;
        }

        sf_ostream_t     *out_stream = NULL;
        const sf_result_t r_open =
            sf_ostream_open_wfile(&out_stream, out_path, NULL);
        if (r_open != SF_OK) {
            fprintf(stderr, "Warning: failed to create %s: %d\n", sanitized,
                    (int)r_open);
            continue;
        }
        const sf_result_t r_write =
            sf_ostream_write(out_stream, f->data, f->size);
        sf_ostream_close(out_stream);
        if (r_write == SF_OK) {
            extracted++;
        } else {
            fprintf(stderr, "Warning: failed to write %s: %d\n", sanitized,
                    (int)r_write);
        }
    }

    sf_bnd4_destroy(bnd);
    free(outdir_w);
    fprintf(stderr, "Extracted %zu/%zu files.\n", extracted, count);
    return extracted > 0 ? 0 : 3;
}
