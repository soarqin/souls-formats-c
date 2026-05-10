/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T0.1 — Phase 4 pre-flight empirical probe.
 *
 * This is a PROBE ONLY. It exists solely to read raw bytes from a real
 * Elden Ring install and write empirical findings to
 * `.sisyphus/evidence/phase4-pre-flight.md`. No format parsing logic is
 * implemented here; downstream phases (FMG, PARAM, EMEVD) use the findings
 * to confirm that the planned signatures match shipping data.
 *
 * Three probes run in sequence; each may report individually:
 *   1. EMEVD probe — extract an event file via er_extract_from_data0,
 *      capture the leading 16 bytes, decode bigEndian/is64Bit/unk06/
 *      unk07/version, classify against the Sekiro variant.
 *   2. PARAM probe — decrypt regulation.bin with sf_regulation_decrypt_er,
 *      parse the inner BND4 from memory, record the exact entry name of
 *      the SpEffectParam table.
 *   3. FMG probe — extract item.msgbnd.dcx via er_extract_from_data0,
 *      parse the inner BND4, record the path and size of ItemName.fmg.
 *
 * Graceful degradation: if ER game data is unavailable the test
 * TEST_IGNORE_MESSAGEs (Unity convention) and writes "SKIP: ER data
 * unavailable" to the evidence file. It NEVER fails the build in a clean
 * checkout.
 */

#include "er_test_helper.h"

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"
#include "souls_formats/sf_regulation.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#ifndef SF_E2E_REPO_DIR
#define SF_E2E_REPO_DIR L"."
#endif
#ifndef SF_E2E_ELDEN_RING_DIR
#define SF_E2E_ELDEN_RING_DIR L"C:/Games/ELDEN RING"
#endif

void setUp(void) {}
void tearDown(void) {}

/* Compile-time path to the canonical evidence file. */
static const wchar_t k_evidence_path[] =
    SF_E2E_REPO_DIR L"/.sisyphus/evidence/phase4-pre-flight.md";

/* Compile-time path to the encrypted regulation. */
static const wchar_t k_regulation_path[] =
    SF_E2E_ELDEN_RING_DIR L"/Game/regulation.bin";

/* Captured evidence — populated by each probe, flushed in main(). */
typedef struct evidence {
    char        emevd_source_path[256];
    char        emevd_magic_hex[64];
    char        emevd_flags_line[256];
    char        emevd_format_match[128];
    bool        emevd_ok;

    char        param_entry_name[512];
    bool        param_ok;

    char        fmg_msgbnd_path[128];
    char        fmg_entry_name[256];
    size_t      fmg_entry_size;
    bool        fmg_ok;

    bool        regulation_available;
    bool        data0_available;
} evidence_t;

static evidence_t g_ev;

/* Append a single hex byte (uppercase) to dest. */
static void append_hex_byte(char *dest, size_t dest_size, size_t *off, uint8_t b)
{
    if (*off + 3 < dest_size) {
        const char hex[] = "0123456789abcdef";
        dest[(*off)++] = hex[(b >> 4) & 0xF];
        dest[(*off)++] = hex[b & 0xF];
        dest[(*off)++] = ' ';
        dest[*off]     = '\0';
    }
}

static void hexdump_16(const uint8_t *data, char *out, size_t out_size)
{
    size_t off = 0;
    out[0]     = '\0';
    for (size_t i = 0; i < 16; ++i) {
        append_hex_byte(out, out_size, &off, data[i]);
    }
    /* Trim trailing space. */
    if (off > 0 && out[off - 1] == ' ') {
        out[off - 1] = '\0';
    }
}

/* Sekiro variant from EMEVD.cs:114-115. */
static bool is_sekiro_variant(uint8_t big_endian, uint8_t is64_raw,
                              uint8_t unk06, uint8_t unk07_raw,
                              uint32_t version)
{
    /* upstream `AssertSByte(0, -1) == -1` means raw byte 0xFF → is64Bit=true. */
    const bool is64bit = (int8_t)is64_raw == -1;
    const bool unk07   = (int8_t)unk07_raw == -1;
    return big_endian == 0 && is64bit && unk06 == 1 && unk07
           && version == 0xCDu;
}

/*=============================================================================
 * Probe 1 — EMEVD
 *
 * Try a sequence of candidate event paths until one resolves. For the first
 * successful extract, read magic + 8 header bytes and capture variant info.
 *===========================================================================*/
static void probe_emevd(void)
{
    static const char *const k_candidates[] = {
        "/event/m60_42_36_00.emevd.dcx",
        "/event/common.emevd.dcx",
        "/event/m11_00_00_00.emevd.dcx",
        "/event/m60_44_52_00.emevd.dcx",
    };
    const size_t k_n = sizeof(k_candidates) / sizeof(k_candidates[0]);

    for (size_t i = 0; i < k_n; ++i) {
        void       *buf       = NULL;
        size_t      buf_size  = 0;
        sf_result_t r         =
            er_extract_from_data0(k_candidates[i], &buf, &buf_size);
        if (r != SF_OK || !buf || buf_size < 16) {
            if (buf) {
                sf_free(NULL, buf);
            }
            continue;
        }

        const uint8_t *p = (const uint8_t *)buf;
        snprintf(g_ev.emevd_source_path, sizeof(g_ev.emevd_source_path),
                 "%s", k_candidates[i]);
        hexdump_16(p, g_ev.emevd_magic_hex, sizeof(g_ev.emevd_magic_hex));

        /* Verify magic == "EVD\0" = 0x45 0x56 0x44 0x00. */
        const bool magic_ok =
            p[0] == 0x45 && p[1] == 0x56 && p[2] == 0x44 && p[3] == 0x00;
        const uint8_t big_endian = p[4];
        const uint8_t is64_raw   = p[5];
        const uint8_t unk06      = p[6];
        const uint8_t unk07_raw  = p[7];
        const uint32_t version   = (uint32_t)p[8]
                                   | ((uint32_t)p[9] << 8)
                                   | ((uint32_t)p[10] << 16)
                                   | ((uint32_t)p[11] << 24);

        snprintf(g_ev.emevd_flags_line, sizeof(g_ev.emevd_flags_line),
                 "bigEndian=%u is64Bit=%u unk06=%u unk07=%u version=0x%02X",
                 (unsigned)big_endian,
                 (unsigned)((int8_t)is64_raw == -1 ? 1 : 0),
                 (unsigned)unk06,
                 (unsigned)((int8_t)unk07_raw == -1 ? 1 : 0),
                 (unsigned)version);

        if (magic_ok
            && is_sekiro_variant(big_endian, is64_raw, unk06, unk07_raw,
                                 version)) {
            snprintf(g_ev.emevd_format_match,
                     sizeof(g_ev.emevd_format_match),
                     "Sekiro");
        } else {
            snprintf(g_ev.emevd_format_match,
                     sizeof(g_ev.emevd_format_match),
                     "Novel: bigEndian=0x%02X is64Bit=0x%02X unk06=0x%02X"
                     " unk07=0x%02X version=0x%02X",
                     (unsigned)big_endian, (unsigned)is64_raw,
                     (unsigned)unk06, (unsigned)unk07_raw,
                     (unsigned)version);
        }

        g_ev.emevd_ok = true;
        sf_free(NULL, buf);
        return;
    }
}

/*=============================================================================
 * Probe 2 — PARAM (regulation.bin → BND4 → SpEffectParam entry name)
 *===========================================================================*/
static sf_result_t read_whole_file(const wchar_t *path, uint8_t **out,
                                   size_t *out_size)
{
    sf_istream_t *s = NULL;
    sf_result_t   r = sf_istream_open_wfile(&s, path, NULL);
    if (r != SF_OK) {
        return r;
    }
    int64_t len = sf_istream_length(s);
    if (len <= 0) {
        sf_istream_close(s);
        return SF_ERR_IO;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)len);
    if (!buf) {
        sf_istream_close(s);
        return SF_ERR_OOM;
    }
    r = sf_istream_read(s, buf, (size_t)len);
    sf_istream_close(s);
    if (r != SF_OK) {
        free(buf);
        return r;
    }
    *out      = buf;
    *out_size = (size_t)len;
    return SF_OK;
}

static void probe_param(void)
{
    uint8_t *encrypted = NULL;
    size_t   enc_size  = 0;
    if (read_whole_file(k_regulation_path, &encrypted, &enc_size) != SF_OK) {
        return;
    }

    uint8_t *plaintext = NULL;
    size_t   plain_size = 0;
    sf_result_t r = sf_regulation_decrypt_er(encrypted, enc_size, &plaintext,
                                             &plain_size, NULL);
    free(encrypted);
    if (r != SF_OK || !plaintext) {
        return;
    }

    sf_bnd4_t *bnd = NULL;
    r = sf_bnd4_read_from_memory(&bnd, plaintext, plain_size, NULL);
    sf_free(NULL, plaintext);
    if (r != SF_OK || !bnd) {
        return;
    }

    const size_t n = sf_bnd4_file_count(bnd);
    for (size_t i = 0; i < n; ++i) {
        const sf_binder_file_t *f = sf_bnd4_get_file(bnd, i);
        if (!f || !f->name_utf8) {
            continue;
        }
        if (strstr(f->name_utf8, "SpEffectParam.param") != NULL) {
            snprintf(g_ev.param_entry_name, sizeof(g_ev.param_entry_name),
                     "%s", f->name_utf8);
            g_ev.param_ok = true;
            break;
        }
    }
    sf_bnd4_destroy(bnd);
}

/*=============================================================================
 * Probe 3 — FMG (item.msgbnd.dcx → BND4 → ItemName.fmg)
 *===========================================================================*/
static void probe_fmg(void)
{
    static const char *const k_candidates[] = {
        "/msg/engus/item.msgbnd.dcx",
        "/msg/engUS/item.msgbnd.dcx",
        "/msg/en-US/item.msgbnd.dcx",
    };
    const size_t k_n = sizeof(k_candidates) / sizeof(k_candidates[0]);

    for (size_t i = 0; i < k_n; ++i) {
        void       *buf      = NULL;
        size_t      buf_size = 0;
        sf_result_t r        =
            er_extract_from_data0(k_candidates[i], &buf, &buf_size);
        if (r != SF_OK || !buf || buf_size < 4) {
            if (buf) {
                sf_free(NULL, buf);
            }
            continue;
        }

        sf_bnd4_t *bnd = NULL;
        r = sf_bnd4_read_from_memory(&bnd, (const uint8_t *)buf, buf_size,
                                     NULL);
        sf_free(NULL, buf);
        if (r != SF_OK || !bnd) {
            continue;
        }

        snprintf(g_ev.fmg_msgbnd_path, sizeof(g_ev.fmg_msgbnd_path), "%s",
                 k_candidates[i]);

        const size_t n = sf_bnd4_file_count(bnd);
        for (size_t j = 0; j < n; ++j) {
            const sf_binder_file_t *f = sf_bnd4_get_file(bnd, j);
            if (!f || !f->name_utf8) {
                continue;
            }
            if (strstr(f->name_utf8, "ItemName.fmg") != NULL) {
                snprintf(g_ev.fmg_entry_name, sizeof(g_ev.fmg_entry_name),
                         "%s", f->name_utf8);
                g_ev.fmg_entry_size = f->size;
                g_ev.fmg_ok         = true;
                break;
            }
        }
        sf_bnd4_destroy(bnd);
        if (g_ev.fmg_ok) {
            return;
        }
    }
}

/*=============================================================================
 * Evidence file writer
 *===========================================================================*/
static void write_evidence(void)
{
    sf_ostream_t *s = NULL;
    if (sf_ostream_open_wfile(&s, k_evidence_path, NULL) != SF_OK) {
        return;
    }

#define WRITE_LINE(line)                                                   \
    do {                                                                   \
        const char *_l = (line);                                           \
        (void)sf_ostream_write(s, _l, strlen(_l));                         \
        (void)sf_ostream_write(s, "\n", 1);                                \
    } while (0)

    char buf[1024];

    WRITE_LINE("# Phase 4 Pre-Flight Evidence");
    WRITE_LINE("");

    if (!g_ev.regulation_available && !g_ev.data0_available) {
        WRITE_LINE("SKIP: ER data unavailable");
        sf_ostream_close(s);
        return;
    }

    WRITE_LINE("## EMEVD Probe");
    if (g_ev.emevd_ok) {
        snprintf(buf, sizeof(buf), "- EMEVD source path: %s",
                 g_ev.emevd_source_path);
        WRITE_LINE(buf);
        snprintf(buf, sizeof(buf), "- EMEVD magic (hex): %s",
                 g_ev.emevd_magic_hex);
        WRITE_LINE(buf);
        snprintf(buf, sizeof(buf), "- EMEVD flags (5 bytes): %s",
                 g_ev.emevd_flags_line);
        WRITE_LINE(buf);
        snprintf(buf, sizeof(buf), "- EMEVD format match: %s",
                 g_ev.emevd_format_match);
        WRITE_LINE(buf);
    } else {
        WRITE_LINE("- EMEVD source path: (none — no candidate resolved)");
        WRITE_LINE("- EMEVD magic (hex): (unavailable)");
        WRITE_LINE("- EMEVD flags (5 bytes): (unavailable)");
        WRITE_LINE("- EMEVD format match: (unavailable)");
    }
    WRITE_LINE("");

    WRITE_LINE("## PARAM Probe (regulation.bin)");
    if (g_ev.param_ok) {
        snprintf(buf, sizeof(buf), "- SpEffectParam BND4 entry name: %s",
                 g_ev.param_entry_name);
        WRITE_LINE(buf);
    } else {
        WRITE_LINE("- SpEffectParam BND4 entry name: (not found)");
    }
    WRITE_LINE("");

    WRITE_LINE("## FMG Probe (msgbnd)");
    if (g_ev.fmg_ok) {
        snprintf(buf, sizeof(buf), "- ItemName.fmg msgbnd path: %s",
                 g_ev.fmg_msgbnd_path);
        WRITE_LINE(buf);
        snprintf(buf, sizeof(buf), "- ItemName.fmg entry size: %zu bytes",
                 g_ev.fmg_entry_size);
        WRITE_LINE(buf);
    } else {
        WRITE_LINE("- ItemName.fmg msgbnd path: (none — no candidate"
                   " resolved)");
        WRITE_LINE("- ItemName.fmg entry size: (unavailable)");
    }

#undef WRITE_LINE

    sf_ostream_close(s);
}

/*=============================================================================
 * Unity tests
 *===========================================================================*/

static void test_environment_or_skip(void)
{
    if (!g_ev.regulation_available && !g_ev.data0_available) {
        TEST_IGNORE_MESSAGE("ER game data unavailable; SKIP recorded in"
                            " evidence file");
    }
}

static void test_emevd_probe_recorded(void)
{
    if (!g_ev.data0_available) {
        TEST_IGNORE_MESSAGE("Data0 unavailable; EMEVD probe skipped");
    }
    /* The probe is empirical: failure to find ANY candidate is itself
     * data worth recording, but cannot be a hard test failure (event
     * filenames may differ across patches). The evidence file records
     * whichever candidate resolved. */
    TEST_PASS_MESSAGE("EMEVD probe attempted; see evidence file");
}

static void test_param_probe_recorded(void)
{
    if (!g_ev.regulation_available) {
        TEST_IGNORE_MESSAGE("regulation.bin unavailable; PARAM probe"
                            " skipped");
    }
    TEST_PASS_MESSAGE("PARAM probe attempted; see evidence file");
}

static void test_fmg_probe_recorded(void)
{
    if (!g_ev.data0_available) {
        TEST_IGNORE_MESSAGE("Data0 unavailable; FMG probe skipped");
    }
    TEST_PASS_MESSAGE("FMG probe attempted; see evidence file");
}

int main(void)
{
    memset(&g_ev, 0, sizeof(g_ev));

    g_ev.regulation_available =
        GetFileAttributesW(k_regulation_path) != INVALID_FILE_ATTRIBUTES;

    g_ev.data0_available = er_helper_is_available()
                           && er_helper_init() == SF_OK;

    if (g_ev.regulation_available) {
        probe_param();
    }
    if (g_ev.data0_available) {
        probe_emevd();
        probe_fmg();
    }

    write_evidence();

    UNITY_BEGIN();
    RUN_TEST(test_environment_or_skip);
    RUN_TEST(test_emevd_probe_recorded);
    RUN_TEST(test_param_probe_recorded);
    RUN_TEST(test_fmg_probe_recorded);
    return UNITY_END();
}
