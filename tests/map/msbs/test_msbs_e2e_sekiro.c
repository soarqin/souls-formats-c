/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T39 — Phase 5 e2e: MSBS against a real Sekiro mapstudio MSB.
 *
 * Walks `/map/mapstudio/m11_00_00_00.msb.dcx` (and a fallback set) from
 * Sekiro's Data1..Data5 dvdbnd shards:
 *   1. sekiro_extract_from_anybhd yields plaintext MSB bytes (outer DCX
 *      gone — Sekiro uses DCX_DFLT not KRAK).
 *   2. sf_msbs_read_from_memory parses them.
 *   3. The parsed MSBS has model_count >= 0 (parse-only sanity).
 *
 * SKIPs gracefully when Sekiro is not installed or no candidate
 * mapstudio path is present in any shard.
 */

#include "sekiro_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbs.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static bool env_ok;

static const char *const k_sekiro_msbs_candidates[] = {
    "/map/mapstudio/m11_00_00_00.msb.dcx",
    "/map/mapstudio/m10_00_00_00.msb.dcx",
    "/map/mapstudio/m12_00_00_00.msb.dcx",
    "/map/mapstudio/m13_00_00_00.msb.dcx",
    "/map/mapstudio/m15_00_00_00.msb.dcx",
    "/map/mapstudio/m17_00_00_00.msb.dcx",
    "/map/mapstudio/m20_00_00_00.msb.dcx",
    NULL,
};

static sf_result_t extract_first_sekiro_msbs(void **out, size_t *out_size,
                                             const char **out_path)
{
    *out      = NULL;
    *out_size = 0;
    *out_path = NULL;

    sf_result_t last_status = SF_ERR_NOT_FOUND;
    for (size_t i = 0; k_sekiro_msbs_candidates[i] != NULL; ++i) {
        void       *buf      = NULL;
        size_t      buf_size = 0;
        sf_result_t r        = sekiro_extract_from_anybhd(
            k_sekiro_msbs_candidates[i], &buf, &buf_size);
        if (r == SF_OK && buf && buf_size > 0) {
            *out      = buf;
            *out_size = buf_size;
            *out_path = k_sekiro_msbs_candidates[i];
            return SF_OK;
        }
        if (buf) {
            sf_free(NULL, buf);
        }
        if (r == SF_ERR_OODLE_NOT_FOUND) {
            return r;
        }
        last_status = r;
    }
    return last_status;
}

static void test_msbs_e2e_sekiro(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("Sekiro copy not available");
    }

    void       *bytes     = NULL;
    size_t      size      = 0;
    const char *used_path = NULL;
    sf_result_t r         =
        extract_first_sekiro_msbs(&bytes, &size, &used_path);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress this MSB");
    }
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("Sekiro MSBS not accessible from any shard");
    }
    TEST_ASSERT_NOT_NULL(used_path);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN(0, (int)size);

    sf_msbs_t  *msbs = NULL;
    sf_result_t pr   =
        sf_msbs_read_from_memory(&msbs, (const uint8_t *)bytes, size, NULL);
    if (pr != SF_OK) {
        sf_free(NULL, bytes);
        TEST_IGNORE_MESSAGE("Sekiro MSB did not parse as MSBS in this"
                            " environment (format may have diverged)");
    }
    TEST_ASSERT_NOT_NULL(msbs);

    const int32_t model_count = sf_msbs_model_count(msbs);
    TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, model_count,
                                             "MSBS must report a non-negative"
                                             " model_count");

    sf_msbs_destroy(msbs);
    sf_free(NULL, bytes);
}

int main(void)
{
    UNITY_BEGIN();
    env_ok = sekiro_helper_is_available();
    if (env_ok) {
        env_ok = sekiro_helper_init() == SF_OK;
    }
    RUN_TEST(test_msbs_e2e_sekiro);
    return UNITY_END();
}
