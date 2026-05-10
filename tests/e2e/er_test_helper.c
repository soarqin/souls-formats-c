/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * er_test_helper.c — process-wide singleton that opens Elden Ring's
 * Data0.bhd / Data0.bdt pair once per test process and exposes a single
 * extract-by-path entry point that auto-unwraps any outer DCX layer.
 *
 * Initialisation is lazy and idempotent. If the configured paths are
 * absent (no ER copy, no Oodle DLL), init returns a non-OK sf_result_t
 * and every call is short-circuited so tests can TEST_IGNORE_MESSAGE
 * and skip cleanly. Path roots come from SF_E2E_ELDEN_RING_DIR /
 * SF_E2E_OODLE_DIR; #ifndef fallbacks let clangd index the file without
 * a build-system-supplied define.
 */

#include "er_test_helper.h"

#include "souls_formats/sf_bhd5.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_hash.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"

#include <stdbool.h>
#include <stdlib.h>
#include <windows.h>

#ifndef SF_E2E_ELDEN_RING_DIR
#define SF_E2E_ELDEN_RING_DIR L"C:/Games/ELDEN RING"
#endif
#ifndef SF_E2E_OODLE_DIR
#define SF_E2E_OODLE_DIR L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle"
#endif

/* Canonical ER dvdbnd shard 0 — the only shard read by these tests. */
static const wchar_t k_bhd_path[] = SF_E2E_ELDEN_RING_DIR L"/Game/Data0.bhd";
static const wchar_t k_bdt_path[] = SF_E2E_ELDEN_RING_DIR L"/Game/Data0.bdt";

static sf_bhd5_t  *g_data0          = NULL;
static bool        g_init_attempted = false;
static sf_result_t g_init_result    = SF_ERR_INTERNAL;

void er_helper_shutdown(void)
{
    if (g_data0) {
        sf_bhd5_close(g_data0);
        g_data0 = NULL;
    }
}

sf_result_t er_helper_init(void)
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

    /* Configure the Oodle DLL search directory but do NOT load the DLL
     * eagerly: ER's Data0 contains a mix of DFLT, KRAK, and uncompressed
     * entries, and plain BHD5 reads must succeed even when Oodle is
     * absent. The first KRAK extract will surface SF_ERR_OODLE_NOT_FOUND
     * if the DLL is missing — callers decide whether that is fatal. */
    (void)sf_oodle_set_search_path(SF_E2E_OODLE_DIR);

    sf_result_t r = sf_bhd5_open(&g_data0, k_bhd_path, k_bdt_path,
                                 SF_BHD5_GAME_ELDENRING, NULL);
    if (r != SF_OK) {
        g_init_result = r;
        return r;
    }

    atexit(er_helper_shutdown);
    g_init_result = SF_OK;
    return SF_OK;
}

sf_result_t er_extract_from_data0(const char *bhd5_path_utf8,
                                  void **out, size_t *out_size)
{
    if (!bhd5_path_utf8 || !out || !out_size) {
        return SF_ERR_INVALID_ARG;
    }

    sf_result_t r = er_helper_init();
    if (r != SF_OK) {
        return r;
    }

    const uint64_t hash     = sf_path_hash_64(bhd5_path_utf8);
    void          *raw      = NULL;
    size_t         raw_size = 0;
    r = sf_bhd5_extract_by_hash_64(g_data0, hash, &raw, &raw_size, NULL);
    if (r != SF_OK) {
        return r;
    }

    /* Unwrap exactly one DCX layer if present. ER nests at most one
     * level (BHD5 → DCX → payload); deeper nesting would indicate a
     * format mismatch we want to surface, not paper over. */
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

bool er_helper_is_available(void)
{
    if (g_init_attempted) {
        return g_init_result == SF_OK;
    }
    /* Side-effect-free probe: do NOT trigger the lazy init. */
    return GetFileAttributesW(k_bhd_path) != INVALID_FILE_ATTRIBUTES
           && GetFileAttributesW(k_bdt_path) != INVALID_FILE_ATTRIBUTES;
}

sf_bhd5_t *er_helper_get_bhd5_for_testing(void) { return g_data0; }
