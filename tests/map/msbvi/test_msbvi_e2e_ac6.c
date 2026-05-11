/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T40 — Phase 5 e2e: MSBVI against a real Armored Core VI mapstudio MSB.
 *
 * Walks one of AC6's mapstudio `.msb.dcx` entries from Data0:
 *   1. ac6_extract_from_data0 yields plaintext MSB bytes (outer DCX gone).
 *   2. sf_msbvi_read_from_memory parses them.
 *   3. The parsed MSBVI has model_count >= 0 (parse-only sanity).
 *
 * SKIPs gracefully when AC6 is not installed, Oodle is missing, or no
 * candidate mapstudio path is present in this shard.
 */

#include "ac6_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbvi.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static bool env_ok;

static const char *const k_candidates[] = {
    "/map/mapstudio/m01_00_00_00.msb.dcx",
    "/map/mapstudio/m10_00_00_00.msb.dcx",
    "/map/mapstudio/m20_00_00_00.msb.dcx",
    NULL,
};

static sf_result_t extract_first_ac6_msbvi(void **out, size_t *out_size,
                                           const char **out_path)
{
    *out      = NULL;
    *out_size = 0;
    *out_path = NULL;

    sf_result_t last_status = SF_ERR_NOT_FOUND;
    for (size_t i = 0; k_candidates[i] != NULL; ++i) {
        void       *buf      = NULL;
        size_t      buf_size = 0;
        sf_result_t r        = ac6_extract_from_data0(k_candidates[i], &buf, &buf_size);
        if (r == SF_OK && buf && buf_size > 0) {
            *out      = buf;
            *out_size = buf_size;
            *out_path = k_candidates[i];
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

static void test_msbvi_e2e_ac6(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("AC6 copy or Oodle DLL not available");
    }

    void       *bytes     = NULL;
    size_t      size      = 0;
    const char *used_path = NULL;
    sf_result_t r         = extract_first_ac6_msbvi(&bytes, &size, &used_path);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("AC6 MSBVI not accessible from Data0 (mapstudio entries may live in another shard)");
    }
    TEST_ASSERT_NOT_NULL(used_path);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN(0, (int)size);

    sf_msbvi_t  *msbvi = NULL;
    sf_result_t pr     = sf_msbvi_read_from_memory(&msbvi, (const uint8_t *)bytes, size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, pr);
    TEST_ASSERT_NOT_NULL(msbvi);

    const int32_t model_count = sf_msbvi_model_count(msbvi);
    TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, model_count,
                                             "AC6 MSBVI must report model_count >= 0");

    sf_msbvi_destroy(msbvi);
    sf_free(NULL, bytes);
}

int main(void)
{
    UNITY_BEGIN();
    env_ok = ac6_helper_is_available();
    if (env_ok) {
        env_ok = ac6_helper_init() == SF_OK;
    }
    RUN_TEST(test_msbvi_e2e_ac6);
    return UNITY_END();
}
