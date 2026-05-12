/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds2s_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include <stdint.h>
#include <windows.h>

#ifndef SF_E2E_DS2S_DIR
#define SF_E2E_DS2S_DIR L"C:/Games/Dark Souls II Scholar of the First Sin/Game"
#endif

static bool ds2s_join_path(const char *relative_path, wchar_t *out, size_t out_count)
{
    static const wchar_t root[] = SF_E2E_DS2S_DIR;
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

bool ds2s_helper_is_available(void)
{
    return GetFileAttributesW(SF_E2E_DS2S_DIR L"/Param/KeyConfigParam.param")
        != INVALID_FILE_ATTRIBUTES;
}

sf_result_t ds2s_read_loose_param(const char *param_path, void **out, size_t *out_size)
{
    if (!param_path || !out || !out_size) {
        return SF_ERR_INVALID_ARG;
    }
    *out      = NULL;
    *out_size = 0;

    wchar_t full_path[1024];
    if (!ds2s_join_path(param_path, full_path, sizeof(full_path) / sizeof(full_path[0]))) {
        return SF_ERR_OUT_OF_RANGE;
    }

    const sf_allocator_t *alloc  = sf_default_allocator();
    sf_istream_t         *stream = NULL;
    sf_result_t           r      = sf_istream_open_wfile(&stream, full_path, alloc);
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
        bytes = alloc->alloc((size_t)len, alloc->user);
        if (!bytes) {
            sf_istream_close(stream);
            return SF_ERR_OOM;
        }
        r = sf_istream_read(stream, bytes, (size_t)len);
        if (r != SF_OK) {
            sf_free(NULL, bytes);
            sf_istream_close(stream);
            return r;
        }
    }
    sf_istream_close(stream);
    *out      = bytes;
    *out_size = (size_t)len;
    return SF_OK;
}
