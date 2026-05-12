/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds1r_test_helper.h"

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd3.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"

#include <stdint.h>
#include <string.h>
#include <windows.h>

#ifndef SF_E2E_DS1R_DIR
#define SF_E2E_DS1R_DIR L"C:/Games/DARK SOULS REMASTERED"
#endif

static const sf_allocator_t *ds1r_alloc(const sf_allocator_t *alloc)
{
    return alloc ? alloc : sf_default_allocator();
}

static bool ds1r_join_path(const char *relative_path, wchar_t *out, size_t out_count)
{
    static const wchar_t root[] = SF_E2E_DS1R_DIR;
    if (!relative_path || !out || out_count == 0) {
        return false;
    }

    size_t pos = 0;
    while (root[pos] != L'\0') {
        if (pos + 1 >= out_count) {
            return false;
        }
        out[pos] = root[pos];
        ++pos;
    }
    if (relative_path[0] != '/' && relative_path[0] != '\\') {
        if (pos + 1 >= out_count) {
            return false;
        }
        out[pos++] = L'/';
    }
    for (size_t i = 0; relative_path[i] != '\0'; ++i) {
        if (pos + 1 >= out_count) {
            return false;
        }
        const unsigned char c = (unsigned char)relative_path[i];
        out[pos++]             = (c == '\\') ? L'/' : (wchar_t)c;
    }
    out[pos] = L'\0';
    return true;
}

static sf_result_t ds1r_read_raw_wfile(const wchar_t *path, void **out, size_t *out_size,
                                       const sf_allocator_t *alloc)
{
    if (!path || !out || !out_size) {
        return SF_ERR_INVALID_ARG;
    }
    *out      = NULL;
    *out_size = 0;

    const sf_allocator_t *use_alloc = ds1r_alloc(alloc);
    sf_istream_t         *stream    = NULL;
    sf_result_t           r         = sf_istream_open_wfile(&stream, path, use_alloc);
    if (r != SF_OK) {
        return r;
    }

    const int64_t len = sf_istream_length(stream);
    if (len < 0 || (uint64_t)len > SIZE_MAX) {
        sf_istream_close(stream);
        return SF_ERR_OUT_OF_RANGE;
    }

    void *bytes = NULL;
    if (len > 0) {
        bytes = use_alloc->alloc((size_t)len, use_alloc->user);
        if (!bytes) {
            sf_istream_close(stream);
            return SF_ERR_OOM;
        }
        r = sf_istream_read(stream, bytes, (size_t)len);
        if (r != SF_OK) {
            sf_free(use_alloc, bytes);
            sf_istream_close(stream);
            return r;
        }
    }

    sf_istream_close(stream);
    *out      = bytes;
    *out_size = (size_t)len;
    return SF_OK;
}

static sf_result_t ds1r_maybe_decompress(void *raw, size_t raw_size, void **out,
                                         size_t *out_size)
{
    sf_dcx_type_t type    = SF_DCX_TYPE_UNKNOWN;
    sf_result_t   sniff_r = sf_dcx_sniff(raw, raw_size, &type);
    const bool    is_dcx  = sniff_r == SF_OK && type != SF_DCX_TYPE_NONE
                         && type != SF_DCX_TYPE_UNKNOWN;
    if (!is_dcx) {
        *out      = raw;
        *out_size = raw_size;
        return SF_OK;
    }

    void         *decompressed = NULL;
    size_t        decomp_size  = 0;
    sf_dcx_type_t out_type     = SF_DCX_TYPE_UNKNOWN;
    sf_result_t   r            = sf_dcx_decompress(raw, raw_size, &decompressed,
                                                   &decomp_size, &out_type, NULL);
    sf_free(NULL, raw);
    if (r != SF_OK) {
        return r;
    }
    *out      = decompressed;
    *out_size = decomp_size;
    return SF_OK;
}

static bool name_ends_with(const char *name, const char *suffix)
{
    if (!name || !suffix) {
        return false;
    }
    const size_t name_len   = strlen(name);
    const size_t suffix_len = strlen(suffix);
    return suffix_len <= name_len
        && memcmp(name + name_len - suffix_len, suffix, suffix_len) == 0;
}

bool ds1r_helper_is_available(void)
{
    return GetFileAttributesW(SF_E2E_DS1R_DIR) != INVALID_FILE_ATTRIBUTES;
}

sf_result_t ds1r_read_file(const char *relative_path, void **out, size_t *out_size)
{
    if (!relative_path || !out || !out_size) {
        return SF_ERR_INVALID_ARG;
    }
    *out      = NULL;
    *out_size = 0;

    wchar_t full_path[1024];
    if (!ds1r_join_path(relative_path, full_path, sizeof(full_path) / sizeof(full_path[0]))) {
        return SF_ERR_OUT_OF_RANGE;
    }

    void       *raw      = NULL;
    size_t      raw_size = 0;
    sf_result_t r        = ds1r_read_raw_wfile(full_path, &raw, &raw_size, NULL);
    if (r != SF_OK) {
        return r;
    }
    return ds1r_maybe_decompress(raw, raw_size, out, out_size);
}

sf_result_t ds1r_extract_bnd3_entry(const char *bnd3_path, const char *entry_suffix,
                                    void **out, size_t *out_size)
{
    if (!bnd3_path || !entry_suffix || !out || !out_size) {
        return SF_ERR_INVALID_ARG;
    }
    *out      = NULL;
    *out_size = 0;

    void       *bnd_bytes = NULL;
    size_t      bnd_size  = 0;
    sf_result_t r         = ds1r_read_file(bnd3_path, &bnd_bytes, &bnd_size);
    if (r != SF_OK) {
        return r;
    }

    sf_bnd3_t *bnd = NULL;
    r = sf_bnd3_read_from_memory(&bnd, (const uint8_t *)bnd_bytes, bnd_size, NULL);
    sf_free(NULL, bnd_bytes);
    if (r != SF_OK) {
        return r;
    }

    const sf_allocator_t *alloc = sf_default_allocator();
    const size_t          count = sf_bnd3_file_count(bnd);
    for (size_t i = 0; i < count; ++i) {
        const sf_binder_file_t *file = sf_bnd3_get_file(bnd, i);
        if (!file || !file->name_utf8 || !file->data || file->size == 0) {
            continue;
        }
        if (!name_ends_with(file->name_utf8, entry_suffix)) {
            continue;
        }
        void *copy = alloc->alloc(file->size, alloc->user);
        if (!copy) {
            sf_bnd3_destroy(bnd);
            return SF_ERR_OOM;
        }
        memcpy(copy, file->data, file->size);
        *out      = copy;
        *out_size = file->size;
        sf_bnd3_destroy(bnd);
        return SF_OK;
    }

    sf_bnd3_destroy(bnd);
    return SF_ERR_NOT_FOUND;
}
