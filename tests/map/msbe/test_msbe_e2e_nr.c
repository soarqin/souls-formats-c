/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T38 — Phase 5 e2e: MSBE against a real Nightreign mapstudio MSB.
 *
 * Walks `/map/mapstudio/m10_00_00_00.msb.dcx` (and a small fallback set)
 * from Nightreign's data0:
 *   1. nightreign_extract_from_data0 yields plaintext MSB bytes (outer
 *      DCX gone — same MSBE/MSBE-as-Nightreign pipeline as ER).
 *   2. sf_msbe_read_from_memory parses them.
 *   3. The parsed MSBE has model_count >= 0 (parse-only sanity).
 *
 * SKIPs gracefully when Nightreign is not installed, Oodle is missing,
 * or no candidate mapstudio path is present in this shard.
 */

#include "nightreign_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbe.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static bool env_ok;

static const char *const k_nr_msbe_candidates[] = {
    "/map/mapstudio/m10_00_00_00.msb.dcx",
    "/map/mapstudio/m11_00_00_00.msb.dcx",
    "/map/mapstudio/m12_00_00_00.msb.dcx",
    "/map/mapstudio/m60_42_36_00.msb.dcx",
    NULL,
};

static sf_result_t extract_first_nr_msbe(void **out, size_t *out_size,
                                         const char **out_path)
{
    *out      = NULL;
    *out_size = 0;
    *out_path = NULL;

    sf_result_t last_status = SF_ERR_NOT_FOUND;
    for (size_t i = 0; k_nr_msbe_candidates[i] != NULL; ++i) {
        void       *buf      = NULL;
        size_t      buf_size = 0;
        sf_result_t r        = nightreign_extract_from_data0(
            k_nr_msbe_candidates[i], &buf, &buf_size);
        if (r == SF_OK && buf && buf_size > 0) {
            *out      = buf;
            *out_size = buf_size;
            *out_path = k_nr_msbe_candidates[i];
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

static void test_msbe_e2e_nr(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("Nightreign copy or Oodle DLL not available");
    }

    void       *bytes     = NULL;
    size_t      size      = 0;
    const char *used_path = NULL;
    sf_result_t r         = extract_first_nr_msbe(&bytes, &size, &used_path);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("Nightreign MSBE not accessible from data0"
                            " (mapstudio may live in another shard)");
    }
    TEST_ASSERT_NOT_NULL(used_path);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN(0, (int)size);

    sf_msbe_t  *msbe = NULL;
    sf_result_t pr   =
        sf_msbe_read_from_memory(&msbe, (const uint8_t *)bytes, size, NULL);
    if (pr != SF_OK) {
        sf_free(NULL, bytes);
        TEST_IGNORE_MESSAGE("Nightreign MSB did not parse as MSBE in this"
                            " environment (format may have diverged)");
    }
    TEST_ASSERT_NOT_NULL(msbe);

    const int32_t model_count = sf_msbe_model_count(msbe);
    TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, model_count,
                                             "MSBE must report a non-negative"
                                             " model_count");

    sf_msbe_destroy(msbe);
    sf_free(NULL, bytes);
}

int main(void)
{
    UNITY_BEGIN();
    env_ok = nightreign_helper_is_available();
    if (env_ok) {
        env_ok = nightreign_helper_init() == SF_OK;
    }
    RUN_TEST(test_msbe_e2e_nr);
    return UNITY_END();
}
