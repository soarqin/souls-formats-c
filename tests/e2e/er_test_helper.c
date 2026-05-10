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
#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_hash.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_regulation.h"
#include "souls_formats/sf_oodle.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
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

static const sf_allocator_t *er_resolve_allocator(const sf_allocator_t *alloc)
{
    return alloc ? alloc : sf_default_allocator();
}

static sf_result_t er_copy_entry_data(const sf_binder_file_t *file, void **out_bytes,
                                      size_t *out_size, const sf_allocator_t *alloc)
{
    if (!file || !out_bytes || !out_size) {
        return SF_ERR_INVALID_ARG;
    }
    if (!file->data && file->size != 0) {
        return SF_ERR_INTERNAL;
    }

    const sf_allocator_t *use_alloc = er_resolve_allocator(alloc);
    void                 *copy      = NULL;
    if (file->size > 0) {
        copy = use_alloc->alloc(file->size, use_alloc->user);
        if (!copy) {
            return SF_ERR_OOM;
        }
        memcpy(copy, file->data, file->size);
    }

    *out_bytes = copy;
    *out_size  = file->size;
    return SF_OK;
}

static bool er_name_ends_with(const char *name, const char *suffix)
{
    if (!name || !suffix) {
        return false;
    }

    const size_t name_len   = strlen(name);
    const size_t suffix_len = strlen(suffix);
    if (suffix_len > name_len) {
        return false;
    }
    return memcmp(name + (name_len - suffix_len), suffix, suffix_len) == 0;
}

static sf_result_t er_find_bnd4_entry_copy(const sf_bnd4_t *bnd, const char *needle,
                                           bool suffix_match, void **out_bytes,
                                           size_t *out_size, const sf_allocator_t *alloc)
{
    if (!bnd || !needle || !out_bytes || !out_size) {
        return SF_ERR_INVALID_ARG;
    }

    const size_t count = sf_bnd4_file_count(bnd);
    for (size_t i = 0; i < count; ++i) {
        const sf_binder_file_t *file = sf_bnd4_get_file(bnd, i);
        if (!file || !file->name_utf8) {
            continue;
        }

        const bool match = suffix_match ? er_name_ends_with(file->name_utf8, needle)
                                        : strstr(file->name_utf8, needle) != NULL;
        if (!match) {
            continue;
        }

        return er_copy_entry_data(file, out_bytes, out_size, alloc);
    }

    return SF_ERR_NOT_FOUND;
}

static sf_result_t er_read_file_bytes(const wchar_t *path, void **out_bytes,
                                     size_t *out_size, const sf_allocator_t *alloc)
{
    if (!path || !out_bytes || !out_size) {
        return SF_ERR_INVALID_ARG;
    }

    const sf_allocator_t *use_alloc = er_resolve_allocator(alloc);
    sf_istream_t         *s         = NULL;
    sf_result_t           r         = sf_istream_open_wfile(&s, path, use_alloc);
    if (r != SF_OK) {
        return r;
    }

    const int64_t len64 = sf_istream_length(s);
    if (len64 < 0 || (uint64_t)len64 > SIZE_MAX) {
        sf_istream_close(s);
        return SF_ERR_OUT_OF_RANGE;
    }

    void  *bytes = NULL;
    size_t size  = (size_t)len64;
    if (size > 0) {
        bytes = use_alloc->alloc(size, use_alloc->user);
        if (!bytes) {
            sf_istream_close(s);
            return SF_ERR_OOM;
        }
        r = sf_istream_read(s, bytes, size);
        if (r != SF_OK) {
            sf_free(use_alloc, bytes);
            sf_istream_close(s);
            return r;
        }
    }

    sf_istream_close(s);
    *out_bytes = bytes;
    *out_size  = size;
    return SF_OK;
}

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

sf_result_t er_load_param(const char *param_name, void **out_bytes, size_t *out_size,
                          const sf_allocator_t *alloc)
{
    if (!param_name || !out_bytes || !out_size) {
        return SF_ERR_INVALID_ARG;
    }

    const sf_allocator_t *use_alloc = er_resolve_allocator(alloc);
    const size_t          suffix_len = strlen(param_name) + sizeof(".param");
    char                 *suffix     = use_alloc->alloc(suffix_len, use_alloc->user);
    if (!suffix) {
        return SF_ERR_OOM;
    }
    const size_t param_len = strlen(param_name);
    memcpy(suffix, param_name, param_len);
    memcpy(suffix + param_len, ".param", sizeof(".param"));

    void   *reg_bytes = NULL;
    size_t  reg_size  = 0;
    sf_result_t r = er_read_file_bytes(L"/mnt/c/Games/ELDEN RING/Game/regulation.bin",
                                       &reg_bytes, &reg_size, use_alloc);
    if (r != SF_OK) {
        sf_free(use_alloc, suffix);
        return r;
    }

    uint8_t    *plain_bytes = NULL;
    size_t      plain_size  = 0;
    r = sf_regulation_decrypt_er(reg_bytes, reg_size, &plain_bytes, &plain_size,
                                 use_alloc);
    sf_free(use_alloc, reg_bytes);
    if (r != SF_OK) {
        sf_free(use_alloc, suffix);
        return r;
    }

    sf_bnd4_t  *bnd = NULL;
    r = sf_bnd4_read_from_memory(&bnd, plain_bytes, plain_size, use_alloc);
    sf_free(use_alloc, plain_bytes);
    if (r != SF_OK) {
        sf_free(use_alloc, suffix);
        return r;
    }

    r = er_find_bnd4_entry_copy(bnd, suffix, true, out_bytes, out_size, use_alloc);
    sf_bnd4_destroy(bnd);
    sf_free(use_alloc, suffix);
    return r;
}

sf_result_t er_load_msgbnd_entry(const char *msgbnd_path, const char *entry_name,
                                 void **out_bytes, size_t *out_size,
                                 const sf_allocator_t *alloc)
{
    if (!msgbnd_path || !entry_name || !out_bytes || !out_size) {
        return SF_ERR_INVALID_ARG;
    }

    const sf_allocator_t *use_alloc = er_resolve_allocator(alloc);
    void                 *msg_bytes = NULL;
    size_t                msg_size  = 0;
    sf_result_t           r         = er_extract_from_data0(msgbnd_path, &msg_bytes,
                                                            &msg_size);
    if (r != SF_OK) {
        return r;
    }

    sf_bnd4_t *bnd = NULL;
    r = sf_bnd4_read_from_memory(&bnd, msg_bytes, msg_size, use_alloc);
    sf_free(NULL, msg_bytes);
    if (r != SF_OK) {
        return r;
    }

    r = er_find_bnd4_entry_copy(bnd, entry_name, false, out_bytes, out_size, use_alloc);
    sf_bnd4_destroy(bnd);
    return r;
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
