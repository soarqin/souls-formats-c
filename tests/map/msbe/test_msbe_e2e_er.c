/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T37 — Phase 5 e2e: MSBE against a real Elden Ring mapstudio MSB.
 *
 * Walks `/map/mapstudio/m60_42_36_00.msb.dcx` from Data0:
 *   1. er_extract_from_data0 yields plaintext MSB bytes (outer DCX gone).
 *   2. sf_msbe_read_from_memory parses them.
 *   3. The parsed MSBE has model_count > 0 && part_count > 0.
 *
 * SKIPs gracefully when the ER copy, Oodle DLL, or mapstudio entry is
 * unavailable. Per the phase-5 roadmap mapstudio MSBs may live in a
 * different shard on some installs; this test trusts the helper and
 * IGNOREs whenever extraction returns non-OK.
 */

#include "er_test_helper.h"

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

static const char *const k_msbe_candidates[] = {
    "/map/mapstudio/m60_42_36_00.msb.dcx",
    "/map/mapstudio/m60_44_52_00.msb.dcx",
    "/map/mapstudio/m11_00_00_00.msb.dcx",
    "/map/mapstudio/m10_00_00_00.msb.dcx",
    NULL,
};

static sf_result_t extract_first_msbe(void **out, size_t *out_size,
                                      const char **out_path)
{
    *out      = NULL;
    *out_size = 0;
    *out_path = NULL;

    sf_result_t last_status = SF_ERR_NOT_FOUND;
    for (size_t i = 0; k_msbe_candidates[i] != NULL; ++i) {
        void       *buf      = NULL;
        size_t      buf_size = 0;
        sf_result_t r        =
            er_extract_from_data0(k_msbe_candidates[i], &buf, &buf_size);
        if (r == SF_OK && buf && buf_size > 0) {
            *out      = buf;
            *out_size = buf_size;
            *out_path = k_msbe_candidates[i];
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

static void test_msbe_e2e_er(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    void       *bytes     = NULL;
    size_t      size      = 0;
    const char *used_path = NULL;
    sf_result_t r         = extract_first_msbe(&bytes, &size, &used_path);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("ER MSBE not accessible from Data0 (mapstudio"
                            " entries may live in another shard)");
    }
    TEST_ASSERT_NOT_NULL(used_path);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN(0, (int)size);

    sf_msbe_t  *msbe = NULL;
    sf_result_t pr   =
        sf_msbe_read_from_memory(&msbe, (const uint8_t *)bytes, size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, pr);
    TEST_ASSERT_NOT_NULL(msbe);

    const int32_t model_count = sf_msbe_model_count(msbe);
    const int32_t part_count  = sf_msbe_part_count(msbe);

    TEST_ASSERT_GREATER_THAN_MESSAGE(0, model_count,
                                     "ER MSBE must report model_count > 0");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, part_count,
                                     "ER MSBE must report part_count > 0");

    sf_msbe_destroy(msbe);
    sf_free(NULL, bytes);
}

int main(void)
{
    UNITY_BEGIN();
    env_ok = er_helper_is_available();
    if (env_ok) {
        env_ok = er_helper_init() == SF_OK;
    }
    RUN_TEST(test_msbe_e2e_er);
    return UNITY_END();
}
