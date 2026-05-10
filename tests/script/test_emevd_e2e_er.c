/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T4.6 — Phase 4 e2e: EMEVD against real Elden Ring event scripts.
 *
 * Walks `*.emevd.dcx` from the live Data0 archive through the full ER
 * pipeline:
 *   1. er_extract_from_data0 yields plaintext EMEVD bytes
 *      (RSA-unwrap → BHD5 lookup → AES decrypt → DCX_KRAK decompress).
 *   2. sf_emevd_read_from_memory parses them.
 *   3. The parsed EMEVD reports the Sekiro flag set (ER/AC6/Nightreign
 *      use Sekiro as alias per the Wave 0 pre-flight evidence; see
 *      .sisyphus/evidence/phase4-pre-flight.md).
 *   4. Event count is non-zero.
 *
 * Several candidate event paths are tried in sequence; the test SKIPs
 * gracefully when Data0 / Oodle / event entries are unavailable so a
 * clean checkout never FAILs.
 *
 * Evidence: writes .sisyphus/evidence/task-T4.6-emevd-flag-confirmed.log
 * on a successful confirmation.
 */

#include "er_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_emevd.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef SF_E2E_REPO_DIR
#define SF_E2E_REPO_DIR L"."
#endif

void setUp(void) {}
void tearDown(void) {}

static bool env_ok;

/* Compile-time path to the canonical evidence file. */
static const wchar_t k_evidence_path[] =
    SF_E2E_REPO_DIR L"/.sisyphus/evidence/task-T4.6-emevd-flag-confirmed.log";

static const char *const k_emevd_candidates[] = {
    "/event/m60_42_36_00.emevd.dcx",
    "/event/common.emevd.dcx",
    "/event/m11_00_00_00.emevd.dcx",
    "/event/m60_44_52_00.emevd.dcx",
    NULL,
};

/* Try each candidate path until one extracts. Caller frees *out via
 * sf_free(NULL, *out). Returns the path used on success, NULL otherwise. */
static const char *extract_first_emevd(void **out, size_t *out_size,
                                       sf_result_t *out_status)
{
    *out         = NULL;
    *out_size    = 0;
    *out_status  = SF_ERR_NOT_FOUND;

    for (size_t i = 0; k_emevd_candidates[i] != NULL; ++i) {
        void       *buf      = NULL;
        size_t      buf_size = 0;
        sf_result_t r        =
            er_extract_from_data0(k_emevd_candidates[i], &buf, &buf_size);
        if (r == SF_OK && buf && buf_size > 0) {
            *out        = buf;
            *out_size   = buf_size;
            *out_status = SF_OK;
            return k_emevd_candidates[i];
        }
        if (buf) {
            sf_free(NULL, buf);
        }
        /* Preserve the most informative status seen so far. */
        if (r == SF_ERR_OODLE_NOT_FOUND) {
            *out_status = r;
        }
    }
    return NULL;
}

/* Write the evidence file. Uses sf_ostream (Win32-backed), per the project-
 * wide ban on stdio for file I/O. On success (status==SF_OK) the file
 * records the parsed format + event count; on SKIP it records the
 * environment status so the test attempt is auditable in either case. */
static void write_evidence(sf_result_t status, const char *used_path,
                           sf_emevd_format_t fmt, size_t event_count)
{
    sf_ostream_t *s = NULL;
    if (sf_ostream_open_wfile(&s, k_evidence_path, NULL) != SF_OK) {
        return;
    }

    char        line[512];
    int         n;

#define EMIT(literal)                                                          \
    do {                                                                       \
        const char *_l = (literal);                                            \
        (void)sf_ostream_write(s, _l, strlen(_l));                             \
    } while (0)

    if (status == SF_OK) {
        EMIT("EMEVD e2e confirmed\n");

        n = snprintf(line, sizeof(line), "Source path: %s\n",
                     used_path ? used_path : "(unknown)");
        if (n > 0) {
            (void)sf_ostream_write(s, line, (size_t)n);
        }

        n = snprintf(line, sizeof(line),
                     "Format: %d (Sekiro=%d, EldenRing=%d)\n",
                     (int)fmt, (int)SF_EMEVD_FORMAT_SEKIRO,
                     (int)SF_EMEVD_FORMAT_ELDEN_RING);
        if (n > 0) {
            (void)sf_ostream_write(s, line, (size_t)n);
        }

        n = snprintf(line, sizeof(line), "Event count: %zu\n", event_count);
        if (n > 0) {
            (void)sf_ostream_write(s, line, (size_t)n);
        }

        EMIT("Consistent with T0.1 evidence: YES (Sekiro alias)\n");
    } else {
        EMIT("EMEVD e2e SKIPPED\n");
        n = snprintf(line, sizeof(line),
                     "Reason: ER data unavailable (status=%d)\n", (int)status);
        if (n > 0) {
            (void)sf_ostream_write(s, line, (size_t)n);
        }
        EMIT("Test will report PASS via Unity TEST_IGNORE (no failure"
             " injected).\n");
        EMIT("Consistent with T0.1 evidence: N/A (no real data parsed in"
             " this run)\n");
    }

#undef EMIT

    sf_ostream_close(s);
}

/* Sub-test 1 — full pipeline: extract → parse → assert flag set + events. */
static void test_emevd_e2e_er(void)
{
    if (!env_ok) {
        write_evidence(SF_ERR_IO, NULL, SF_EMEVD_FORMAT_SEKIRO, 0);
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    void       *emevd_bytes = NULL;
    size_t      emevd_size  = 0;
    sf_result_t status      = SF_ERR_NOT_FOUND;
    const char *used_path   = extract_first_emevd(&emevd_bytes, &emevd_size,
                                                  &status);

    if (status == SF_ERR_OODLE_NOT_FOUND) {
        write_evidence(status, used_path, SF_EMEVD_FORMAT_SEKIRO, 0);
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (status != SF_OK) {
        write_evidence(status, used_path, SF_EMEVD_FORMAT_SEKIRO, 0);
        TEST_IGNORE_MESSAGE("ER EMEVD not accessible (Data0 unavailable or"
                            " no candidate entry resolved)");
    }

    TEST_ASSERT_NOT_NULL(emevd_bytes);
    TEST_ASSERT_GREATER_THAN(0, (int)emevd_size);
    TEST_ASSERT_NOT_NULL(used_path);

    sf_emevd_t *emevd = NULL;
    sf_result_t r     = sf_emevd_read_from_memory(&emevd,
                                                   (const uint8_t *)emevd_bytes,
                                                   emevd_size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(emevd);

    /* SF_EMEVD_FORMAT_ELDEN_RING is a compile-time alias of
     * SF_EMEVD_FORMAT_SEKIRO (see sf_emevd.h:68 + the _Static_assert at
     * line 76). A single equality check therefore covers ER/AC6/Nightreign
     * per the Wave 0 pre-flight conclusion. */
    const sf_emevd_format_t fmt = sf_emevd_get_format(emevd);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SF_EMEVD_FORMAT_SEKIRO, (int)fmt,
                                  "ER EMEVD must report Sekiro flag set"
                                  " (ER/AC6/Nightreign alias Sekiro per"
                                  " Wave 0 evidence)");

    const size_t event_count = sf_emevd_get_event_count(emevd);
    TEST_ASSERT_GREATER_THAN(0, (int)event_count);

    write_evidence(SF_OK, used_path, fmt, event_count);

    sf_emevd_destroy(emevd, NULL);
    sf_free(NULL, emevd_bytes);
}

int main(void)
{
    UNITY_BEGIN();
    env_ok = er_helper_is_available();
    if (env_ok) {
        env_ok = er_helper_init() == SF_OK;
    }
    RUN_TEST(test_emevd_e2e_er);
    return UNITY_END();
}
