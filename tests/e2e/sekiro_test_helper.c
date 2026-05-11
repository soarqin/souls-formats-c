/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sekiro_test_helper.c — Sekiro counterpart to er_test_helper.c.
 *
 * Sekiro's dvdbnd is split across five BHD5/BDT pairs (Data1..Data5).
 * The helper opens every pair lazily at first use and the extract entry
 * point fans the requested path hash across each open shard, returning
 * the first hit. SF_ERR_NOT_FOUND surfaces only when every shard rejects
 * the hash.
 *
 * Init treats individual shard failures as soft: it succeeds as long as
 * at least one shard opened cleanly so partial installs (or test rigs
 * with only a subset of shards present) keep working. is_available()
 * remains strict (all five shards) so smoke tests can skip cleanly when
 * the machine is missing Sekiro entirely.
 *
 * Root paths come from SF_E2E_SEKIRO_DIR / SF_E2E_OODLE_DIR; #ifndef
 * fallbacks let clangd index the file without build-system defines.
 */

#include "sekiro_test_helper.h"

#include "souls_formats/sf_bhd5.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"

#include <stdbool.h>
#include <stdlib.h>
#include <windows.h>

#ifndef SF_E2E_SEKIRO_DIR
#define SF_E2E_SEKIRO_DIR L"C:/Games/Sekiro"
#endif
#ifndef SF_E2E_OODLE_DIR
#define SF_E2E_OODLE_DIR L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle"
#endif

#define SEKIRO_BHD_COUNT 5

static const wchar_t *const k_bhd_paths[SEKIRO_BHD_COUNT] = {
    SF_E2E_SEKIRO_DIR L"/Data1.bhd",
    SF_E2E_SEKIRO_DIR L"/Data2.bhd",
    SF_E2E_SEKIRO_DIR L"/Data3.bhd",
    SF_E2E_SEKIRO_DIR L"/Data4.bhd",
    SF_E2E_SEKIRO_DIR L"/Data5.bhd",
};
static const wchar_t *const k_bdt_paths[SEKIRO_BHD_COUNT] = {
    SF_E2E_SEKIRO_DIR L"/Data1.bdt",
    SF_E2E_SEKIRO_DIR L"/Data2.bdt",
    SF_E2E_SEKIRO_DIR L"/Data3.bdt",
    SF_E2E_SEKIRO_DIR L"/Data4.bdt",
    SF_E2E_SEKIRO_DIR L"/Data5.bdt",
};

static sf_bhd5_t  *g_bhd[SEKIRO_BHD_COUNT] = {0};
static bool        g_init_attempted        = false;
static sf_result_t g_init_result           = SF_ERR_INTERNAL;

void sekiro_helper_shutdown(void)
{
    for (int i = 0; i < SEKIRO_BHD_COUNT; ++i) {
        if (g_bhd[i]) {
            sf_bhd5_close(g_bhd[i]);
            g_bhd[i] = NULL;
        }
    }
}

bool sekiro_helper_is_available(void)
{
    if (g_init_attempted) {
        return g_init_result == SF_OK;
    }
    for (int i = 0; i < SEKIRO_BHD_COUNT; ++i) {
        if (GetFileAttributesW(k_bhd_paths[i]) == INVALID_FILE_ATTRIBUTES) {
            return false;
        }
        if (GetFileAttributesW(k_bdt_paths[i]) == INVALID_FILE_ATTRIBUTES) {
            return false;
        }
    }
    return true;
}

sf_result_t sekiro_helper_init(void)
{
    if (g_init_attempted) {
        return g_init_result;
    }
    g_init_attempted = true;

    (void)sf_oodle_set_search_path(SF_E2E_OODLE_DIR);

    sf_result_t first_err = SF_OK;
    int         opened    = 0;
    for (int i = 0; i < SEKIRO_BHD_COUNT; ++i) {
        if (GetFileAttributesW(k_bhd_paths[i]) == INVALID_FILE_ATTRIBUTES
            || GetFileAttributesW(k_bdt_paths[i]) == INVALID_FILE_ATTRIBUTES) {
            if (first_err == SF_OK) {
                first_err = SF_ERR_IO;
            }
            continue;
        }
        sf_result_t r = sf_bhd5_open(&g_bhd[i], k_bhd_paths[i], k_bdt_paths[i],
                                     SF_BHD5_GAME_SEKIRO, NULL);
        if (r != SF_OK) {
            g_bhd[i] = NULL;
            if (first_err == SF_OK) {
                first_err = r;
            }
            continue;
        }
        ++opened;
    }

    if (opened == 0) {
        sekiro_helper_shutdown();
        g_init_result = (first_err != SF_OK) ? first_err : SF_ERR_IO;
        return g_init_result;
    }

    atexit(sekiro_helper_shutdown);
    g_init_result = SF_OK;
    return SF_OK;
}

sf_result_t sekiro_extract_from_anybhd(const char *utf8_path,
                                       void **out, size_t *out_size)
{
    if (!utf8_path || !out || !out_size) {
        return SF_ERR_INVALID_ARG;
    }

    sf_result_t r = sekiro_helper_init();
    if (r != SF_OK) {
        return r;
    }

    void   *raw      = NULL;
    size_t  raw_size = 0;
    bool    hit      = false;
    sf_result_t last_err = SF_ERR_NOT_FOUND;
    for (int i = 0; i < SEKIRO_BHD_COUNT; ++i) {
        if (!g_bhd[i]) {
            continue;
        }
        r = sf_bhd5_extract_by_path(g_bhd[i], utf8_path, &raw, &raw_size, NULL);
        if (r == SF_OK) {
            hit = true;
            break;
        }
        if (r != SF_ERR_NOT_FOUND) {
            last_err = r;
        }
    }
    if (!hit) {
        return last_err;
    }

    sf_dcx_type_t     type    = SF_DCX_TYPE_UNKNOWN;
    const sf_result_t sniff_r = sf_dcx_sniff(raw, raw_size, &type);
    const bool        is_dcx  =
        sniff_r == SF_OK && type != SF_DCX_TYPE_NONE
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
