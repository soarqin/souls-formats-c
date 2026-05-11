/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * ac6_test_helper.c — process-wide singleton for AC6 e2e tests.
 *
 * AC6's dvdbnd is split across four BHD5/BDT pairs (Data0..Data3).
 * The helper opens every pair at init and fans the requested path hash
 * across each open shard, returning the first hit.
 *
 * Root paths come from SF_E2E_AC6_DIR / SF_E2E_OODLE_DIR.
 */

#include "ac6_test_helper.h"

#include "souls_formats/sf_bhd5.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"

#include <stdbool.h>
#include <stdlib.h>
#include <windows.h>

#ifndef SF_E2E_AC6_DIR
#define SF_E2E_AC6_DIR L"C:/Games/ARMORED CORE VI FIRES OF RUBICON/Game"
#endif
#ifndef SF_E2E_OODLE_DIR
#define SF_E2E_OODLE_DIR L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle"
#endif

#define AC6_BHD_COUNT 4

static const wchar_t *const k_bhd_paths[AC6_BHD_COUNT] = {
    SF_E2E_AC6_DIR L"/Data0.bhd",
    SF_E2E_AC6_DIR L"/Data1.bhd",
    SF_E2E_AC6_DIR L"/Data2.bhd",
    SF_E2E_AC6_DIR L"/Data3.bhd",
};
static const wchar_t *const k_bdt_paths[AC6_BHD_COUNT] = {
    SF_E2E_AC6_DIR L"/Data0.bdt",
    SF_E2E_AC6_DIR L"/Data1.bdt",
    SF_E2E_AC6_DIR L"/Data2.bdt",
    SF_E2E_AC6_DIR L"/Data3.bdt",
};

static sf_bhd5_t  *g_bhd[AC6_BHD_COUNT] = {0};
static bool        g_init_attempted      = false;
static sf_result_t g_init_result         = SF_ERR_INTERNAL;

void ac6_helper_shutdown(void)
{
    for (int i = 0; i < AC6_BHD_COUNT; ++i) {
        if (g_bhd[i]) {
            sf_bhd5_close(g_bhd[i]);
            g_bhd[i] = NULL;
        }
    }
}

bool ac6_helper_is_available(void)
{
    if (g_init_attempted) {
        return g_init_result == SF_OK;
    }
    /* Check all 4 shards exist */
    for (int i = 0; i < AC6_BHD_COUNT; ++i) {
        if (GetFileAttributesW(k_bhd_paths[i]) == INVALID_FILE_ATTRIBUTES) {
            return false;
        }
        if (GetFileAttributesW(k_bdt_paths[i]) == INVALID_FILE_ATTRIBUTES) {
            return false;
        }
    }
    return true;
}

sf_result_t ac6_helper_init(void)
{
    if (g_init_attempted) {
        return g_init_result;
    }
    g_init_attempted = true;

    (void)sf_oodle_set_search_path(SF_E2E_OODLE_DIR);

    int opened = 0;
    for (int i = 0; i < AC6_BHD_COUNT; ++i) {
        if (GetFileAttributesW(k_bhd_paths[i]) == INVALID_FILE_ATTRIBUTES ||
            GetFileAttributesW(k_bdt_paths[i]) == INVALID_FILE_ATTRIBUTES) {
            continue;
        }
        sf_result_t r = sf_bhd5_open(&g_bhd[i], k_bhd_paths[i], k_bdt_paths[i],
                                      SF_BHD5_GAME_ARMOREDCORE6, NULL);
        if (r == SF_OK) {
            ++opened;
        }
    }

    if (opened == 0) {
        g_init_result = SF_ERR_IO;
        return g_init_result;
    }

    atexit(ac6_helper_shutdown);
    g_init_result = SF_OK;
    return SF_OK;
}

sf_result_t ac6_extract_from_data0(const char *bhd5_path_utf8,
                                    void **out, size_t *out_size)
{
    if (!bhd5_path_utf8 || !out || !out_size) {
        return SF_ERR_INVALID_ARG;
    }

    sf_result_t r = ac6_helper_init();
    if (r != SF_OK) {
        return r;
    }

    /* Fan across all open shards */
    for (int i = 0; i < AC6_BHD_COUNT; ++i) {
        if (!g_bhd[i]) continue;

        void  *raw      = NULL;
        size_t raw_size = 0;
        r = sf_bhd5_extract_by_path(g_bhd[i], bhd5_path_utf8, &raw, &raw_size, NULL);
        if (r != SF_OK) continue;

        /* Auto-decompress outer DCX wrapper if present */
        uint8_t      *plain      = NULL;
        size_t        plain_size = 0;
        sf_dcx_type_t out_type   = SF_DCX_TYPE_UNKNOWN;
        sf_result_t dr = sf_dcx_decompress(raw, raw_size, (void **)&plain,
                                            &plain_size, &out_type, NULL);
        sf_free(NULL, raw);

        if (dr == SF_OK) {
            *out      = plain;
            *out_size = plain_size;
        } else {
            *out      = NULL;
            *out_size = 0;
            return dr;
        }
        return SF_OK;
    }

    return SF_ERR_NOT_FOUND;
}
