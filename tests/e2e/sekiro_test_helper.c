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

/* Each Sekiro shard is signed with a distinct RSA private key, so the
 * default sfi_bhd5_get_pem_key(SEKIRO) only opens Data1; the four PEM
 * strings below cover Data2..Data5. Source: Nordgaren/UXM-Selective-
 * Unpack ArchiveKeys.cs SekiroKeys (GPL-3.0). */
static const char k_pem_data1[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBCwKCAQEA92l+AWx1aV7mzt+6r00bm/qnc4b6NH3VVr/v4UxMcfzushL8jsn9\n"
    "ZSP1ss95ot/quk8dOJsp0+/bvxH+C9DEezzNLSqqAGd2jq2PYosj/6FhYAKjjMlK\n"
    "jNxcVPsKQug0Zby+KYsENirmEXcmA1fzltrISf6d6LKB1UFHHN9NRkLCm3idE4Pu\n"
    "9852kPHbiL14EqfDCDgwm7kLeQdt3kUbcmdhu/6dvP42HGxBmAYLNFD3iAe7qLML\n"
    "MFzmKKHQD2fRQK/431Z3xPK6Jp245AdR0AwUYVvnXq+/97wMX0C6UKvAZ+b/1ytD\n"
    "Nu8vZt++lhJ01SjTc2A4hVPz7g1EEO5/TQIEKkj5Jw==\n"
    "-----END RSA PUBLIC KEY-----\n";
static const char k_pem_data2[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBDAKCAQEAqhjoThWX8VwsTKTI1kjp0JBloCXhV8i99P1KPTCTDBnmhVQPdu+7\n"
    "UQ5g4//eh0oqKaOUjet+0SP94QscjIIrhV91OzfIouIWgJJK/ROOP/A3sb5AlzPa\n"
    "6YPcN8ODxR+esyrWhc6rHCt4qGvXVXrgh6zpZM5h5VCTSaup4qqIWm44EF3+FeYS\n"
    "7faFg14rH0QEosieIIZFZmpI6SCJanlrVd+Zh13s4XcZfk0JdC2AEjxCQ2lKi3Un\n"
    "WAMOcJc+8uHoMuNNo1PMpYQ6Z8Nzg5Cii7EnwbCDmuJw58tFBmbOVHZpkY93VIeF\n"
    "maJXSE7ztTp0qTa05YZUsiU3g9HplkeTUwIFAP/xKZE=\n"
    "-----END RSA PUBLIC KEY-----\n";
static const char k_pem_data3[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBDAKCAQEAx5jlgIvoHQLwSFsAwKFZbNo3fgZ89C7tj4hwiZsQVg8QnNZohXl5\n"
    "S5Ep9pS2biOFsSkuZMXKmfYErh2CsdFbr7QR7kvPPianXNrkCI4xlfQwJvMmkLm9\n"
    "6/JmRIUzTWp0kKJUJZJH/UIrXNn7fmk8Vmx1bQIi8bumGSl3gxeMhutv/lC9khsY\n"
    "Tn0ABTJAbIbwNZ5GPXxzQZuQPXXDY52Gm+Fx7Yy1LiK/B6isIDJUN0xdgxdaXxGN\n"
    "f5pPocMJjng0Ob3cjhGvdkysll/jYFnRx0La3CGmtLcXMtHheEQxzGueGDa/lkkl\n"
    "AvvEXtcpKfyFQWcUheQZ8LngAh/UTJHtQwIFAOpVoU8=\n"
    "-----END RSA PUBLIC KEY-----\n";
static const char k_pem_data4[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBCwKCAQEAq8RyArk+eqMAcxLAHUDRYV7yScNKZpKSxGmgJZQ7y6Y8f5wdrNCt\n"
    "byXfmsdQECStIGlkwWjtfm8t/bRZuxxPciAYaFsWo0Ze2BB6uY6ZteNpLJn82qbL\n"
    "TXATf+af3kSrvICfvJwRzbfA/PRJRkHj2gJ6Tc7g6HK7S/4TiCZirq+c/zLY3gb8\n"
    "A8uIFNI4j0qxTzfoAlS7K6spZjfnhZ6l7pYFh+glz15wAbppC9Oy/u5vUacozf4v\n"
    "nacbUHD47ds9EZPZDHk3LfJbioHwtUzJfyBqZmIpI33yiwImPpb96zwvQU86TaXK\n"
    "sJrTmSs/48BeDsQwXuaqOg+6noETBx3pgQIEGM2Ohw==\n"
    "-----END RSA PUBLIC KEY-----\n";
static const char k_pem_data5[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBDAKCAQEAu75/UbXwHdvu/p49TwnY7Ou6DAuZYFAtLUkw/R4nvm0HWVlRsZiB\n"
    "LG3MOG6sPmK2Zc3JLBU2QK4uKazZ9VrmotM4OpYr03q2tiFnv3NfCvB1UeIJIKe3\n"
    "kVhHNZIbvrwEP9a5UCnrSHD+u+Fj5MQBr4yrEitwrNVvIC4J0Ez1Ppn3+D8ff8Xg\n"
    "QRP9qCVLI3X/wdQDea+B5o8PWaYEL9MKnnL1Tq4h+4PRYHcQR8/GXBTrc3x9q3cP\n"
    "QRDWHbRYhIfWSP9urtagjcsmcuG+p34fp+KyWOwkil3FJqwH1KgSTbk9Tb0oBPzq\n"
    "TCJKeE/wgu6hY++lBi5T3ArHZZcsbXzV6wIFAPlRTMc=\n"
    "-----END RSA PUBLIC KEY-----\n";
static const char *const k_pem_keys[SEKIRO_BHD_COUNT] = {
    k_pem_data1, k_pem_data2, k_pem_data3, k_pem_data4, k_pem_data5,
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
        sf_result_t r = sf_bhd5_open_with_key(&g_bhd[i], k_bhd_paths[i], k_bdt_paths[i],
                                              SF_BHD5_GAME_SEKIRO, k_pem_keys[i], NULL);
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
