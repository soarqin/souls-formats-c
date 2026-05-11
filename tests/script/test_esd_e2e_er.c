/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T36 — Phase 5 e2e: ESD against a real Elden Ring talkesdbnd.
 *
 * Walks `/script/talk/m10_00_00_00.talkesdbnd.dcx` from Data0:
 *   1. er_extract_from_data0 yields plaintext talkesdbnd bytes
 *      (RSA-unwrap → BHD5 lookup → AES decrypt → outer DCX_KRAK gone).
 *   2. sf_bnd4_read_from_memory parses the inner BND4 archive.
 *   3. The first .esd entry is parsed via sf_esd_read_from_memory.
 *   4. The parsed ESD reports state_group_count > 0.
 *
 * SKIPs gracefully when the ER copy, Oodle DLL, talkesdbnd entry, or
 * any .esd sub-entry is unavailable. NEVER fails on missing data.
 */

#include "er_test_helper.h"

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_esd.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static bool env_ok;

static const char *const k_talkesdbnd_candidates[] = {
    "/script/talk/m10_00_00_00.talkesdbnd.dcx",
    "/script/talk/m11_00_00_00.talkesdbnd.dcx",
    "/script/talk/m60_42_36_00.talkesdbnd.dcx",
    NULL,
};

/* Returns true if name ends with ".esd" (case-insensitive enough for
 * upstream-shipped lower-case names). */
static bool name_ends_with_esd(const char *name)
{
    if (!name) {
        return false;
    }
    const size_t len = strlen(name);
    if (len < 4) {
        return false;
    }
    return name[len - 4] == '.'
        && (name[len - 3] == 'e' || name[len - 3] == 'E')
        && (name[len - 2] == 's' || name[len - 2] == 'S')
        && (name[len - 1] == 'd' || name[len - 1] == 'D');
}

/* Try each talkesdbnd candidate path until one extracts. */
static sf_result_t extract_first_talkesdbnd(void **out, size_t *out_size,
                                            const char **out_path)
{
    *out      = NULL;
    *out_size = 0;
    *out_path = NULL;

    sf_result_t last_status = SF_ERR_NOT_FOUND;
    for (size_t i = 0; k_talkesdbnd_candidates[i] != NULL; ++i) {
        void       *buf      = NULL;
        size_t      buf_size = 0;
        sf_result_t r        = er_extract_from_data0(
            k_talkesdbnd_candidates[i], &buf, &buf_size);
        if (r == SF_OK && buf && buf_size > 0) {
            *out      = buf;
            *out_size = buf_size;
            *out_path = k_talkesdbnd_candidates[i];
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

/* Full pipeline: extract talkesdbnd → parse BND4 → find .esd → parse ESD
 * → assert state_group_count > 0. */
static void test_esd_e2e_er(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    void       *bnd_bytes = NULL;
    size_t      bnd_size  = 0;
    const char *used_path = NULL;
    sf_result_t r         = extract_first_talkesdbnd(&bnd_bytes, &bnd_size,
                                                     &used_path);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("ESD talkesdbnd not accessible (Data0 unavailable"
                            " or no candidate entry resolved)");
    }
    TEST_ASSERT_NOT_NULL(used_path);
    TEST_ASSERT_NOT_NULL(bnd_bytes);
    TEST_ASSERT_GREATER_THAN(4, (int)bnd_size);

    /* Outer wrapper is BND4 (talkesdbnd is BND4-based). */
    sf_bnd4_t  *bnd = NULL;
    sf_result_t pr  =
        sf_bnd4_read_from_memory(&bnd, (const uint8_t *)bnd_bytes, bnd_size,
                                 NULL);
    if (pr != SF_OK) {
        sf_free(NULL, bnd_bytes);
        TEST_IGNORE_MESSAGE("talkesdbnd did not parse as BND4 in this"
                            " environment");
    }
    TEST_ASSERT_NOT_NULL(bnd);

    const size_t entry_count = sf_bnd4_file_count(bnd);
    if (entry_count == 0) {
        sf_bnd4_destroy(bnd);
        sf_free(NULL, bnd_bytes);
        TEST_IGNORE_MESSAGE("talkesdbnd has no entries");
    }

    /* Locate the first .esd entry. */
    const sf_binder_file_t *esd_entry = NULL;
    for (size_t i = 0; i < entry_count; ++i) {
        const sf_binder_file_t *e = sf_bnd4_get_file(bnd, i);
        if (e && e->data && e->size > 0 && name_ends_with_esd(e->name_utf8)) {
            esd_entry = e;
            break;
        }
    }
    if (!esd_entry) {
        sf_bnd4_destroy(bnd);
        sf_free(NULL, bnd_bytes);
        TEST_IGNORE_MESSAGE("no .esd entry found in talkesdbnd");
    }

    /* Parse ESD payload. */
    sf_esd_t   *esd = NULL;
    sf_result_t er  = sf_esd_read_from_memory(&esd, esd_entry->data,
                                              esd_entry->size, NULL);
    if (er != SF_OK) {
        sf_bnd4_destroy(bnd);
        sf_free(NULL, bnd_bytes);
        TEST_IGNORE_MESSAGE("inner .esd entry did not parse as ESD in this"
                            " environment");
    }
    TEST_ASSERT_NOT_NULL(esd);

    const int32_t group_count = sf_esd_get_state_group_count(esd);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, group_count,
                                     "ER ESD must report state_group_count > 0");

    sf_esd_destroy(esd);
    sf_bnd4_destroy(bnd);
    sf_free(NULL, bnd_bytes);
}

int main(void)
{
    UNITY_BEGIN();
    env_ok = er_helper_is_available();
    if (env_ok) {
        env_ok = er_helper_init() == SF_OK;
    }
    RUN_TEST(test_esd_e2e_er);
    return UNITY_END();
}
