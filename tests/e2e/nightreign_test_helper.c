/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * nightreign_test_helper.c — process-wide singleton that opens Nightreign's
 * data0.bhd / data0.bdt pair once per test process and exposes a single
 * extract-by-path entry point that auto-unwraps any outer DCX layer.
 */

#include "nightreign_test_helper.h"

#include "souls_formats/sf_bhd5.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_hash.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#ifndef SF_E2E_NIGHTREIGN_DIR
#define SF_E2E_NIGHTREIGN_DIR L"C:/Games/ELDEN RING NIGHTREIGN/Game"
#endif

static const wchar_t k_oodle_dir[] = L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle";
static const wchar_t k_bhd_path[]  = SF_E2E_NIGHTREIGN_DIR L"/data0.bhd";
static const wchar_t k_bdt_path[]  = SF_E2E_NIGHTREIGN_DIR L"/data0.bdt";

static sf_bhd5_t  *g_data0          = NULL;
static bool        g_init_attempted = false;
static sf_result_t g_init_result    = SF_ERR_INTERNAL;

static void nr_helper_shutdown_internal(void);

static sf_result_t nr_helper_init_internal(void)
{
    if (g_init_attempted) {
        return g_init_result;
    }
    g_init_attempted = true;

    if (GetFileAttributesW(k_bhd_path) == INVALID_FILE_ATTRIBUTES) {
        g_init_result = SF_ERR_IO;
        return g_init_result;
    }
    if (GetFileAttributesW(k_bdt_path) == INVALID_FILE_ATTRIBUTES) {
        g_init_result = SF_ERR_IO;
        return g_init_result;
    }

    (void)sf_oodle_set_search_path(k_oodle_dir);

    sf_result_t r = sf_bhd5_open(&g_data0, k_bhd_path, k_bdt_path,
                                 SF_BHD5_GAME_NIGHTREIGN, NULL);
    if (r != SF_OK) {
        g_init_result = r;
        return r;
    }

    atexit(nr_helper_shutdown_internal);
    g_init_result = SF_OK;
    return SF_OK;
}

static void nr_helper_shutdown_internal(void)
{
    if (g_data0) {
        sf_bhd5_close(g_data0);
        g_data0 = NULL;
    }
}

void nightreign_helper_shutdown(void)
{
    nr_helper_shutdown_internal();
}

sf_result_t nightreign_helper_init(void)
{
    return nr_helper_init_internal();
}

sf_result_t nightreign_extract_from_data0(const char *bhd5_path_utf8,
                                          void **out, size_t *out_size)
{
    if (!bhd5_path_utf8 || !out || !out_size) {
        return SF_ERR_INVALID_ARG;
    }

    sf_result_t r = nr_helper_init_internal();
    if (r != SF_OK) {
        return r;
    }

    const uint64_t hash = sf_path_hash_64(bhd5_path_utf8);
    void          *raw  = NULL;
    size_t         raw_size = 0;
    r = sf_bhd5_extract_by_hash_64(g_data0, hash, &raw, &raw_size, NULL);
    if (r != SF_OK) {
        return r;
    }

    sf_dcx_type_t     type    = SF_DCX_TYPE_UNKNOWN;
    const sf_result_t sniff_r = sf_dcx_sniff(raw, raw_size, &type);
    const bool        is_dcx  = sniff_r == SF_OK && type != SF_DCX_TYPE_NONE
                                && type != SF_DCX_TYPE_UNKNOWN;

    if (is_dcx) {
        void         *decompressed = NULL;
        size_t        decomp_size  = 0;
        sf_dcx_type_t out_type     = SF_DCX_TYPE_UNKNOWN;
        r = sf_dcx_decompress(raw, raw_size, &decompressed, &decomp_size,
                              &out_type, NULL);
        sf_free(NULL, raw);
        if (r != SF_OK) {
            return r;
        }
        *out      = decompressed;
        *out_size = decomp_size;
    } else {
        *out      = raw;
        *out_size = raw_size;
    }
    return SF_OK;
}

bool nightreign_helper_is_available(void)
{
    if (g_init_attempted) {
        return g_init_result == SF_OK;
    }

    return GetFileAttributesW(k_bhd_path) != INVALID_FILE_ATTRIBUTES
           && GetFileAttributesW(k_bdt_path) != INVALID_FILE_ATTRIBUTES;
}
