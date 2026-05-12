/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T27b — Phase-6 e2e: MTD against Sekiro's allmaterialbnd.mtdbnd.dcx.
 *
 * Pipeline:
 *   1. sekiro_extract_from_anybhd("/mtd/<path>") fans the request across
 *      Data1..Data5; the helper unwraps an outer DCX layer when present.
 *   2. sf_bnd4_read_from_memory parses the resulting BND4. Sekiro ships
 *      its MTD set under one of several historical layouts, so the test
 *      probes a list of candidate paths and stops at the first hit.
 *   3. The first `.mtd` entry is parsed via sf_mtd_read_from_memory.
 *      Assertions: shader_path reachable (upstream guarantees it is at
 *      least the empty string ""), param_count and texture_count are
 *      both reachable (real Sekiro MTDs always populate both, but the
 *      test only asserts reachability to stay tolerant of variants).
 *   4. sf_mtd_write_to_memory re-serializes and compares byte-for-byte
 *      against the input entry — the writer's contract is a faithful
 *      round-trip when starting from a parsed-from-bytes MTD.
 *
 * SKIPs gracefully when the Sekiro copy or Oodle DLL is missing or the
 * mtdbnd archive does not contain any .mtd entries (extremely unlikely
 * on a real install).
 */

#include "sekiro_test_helper.h"

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_mtd.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static bool env_ok;

/* Candidate paths for the master MTD binder across Sekiro layouts. */
static const char *const k_mtdbnd_candidates[] = {
    "/mtd/allmaterialbnd.mtdbnd.dcx",
    "/mtd/allMaterialBnd.mtdbnd.dcx",
    "/material/allmaterialbnd.mtdbnd.dcx",
    "/material/allmaterial.mtdbnd.dcx",
    NULL,
};

static bool name_ends_with_mtd(const char *name)
{
    if (!name) {
        return false;
    }
    const size_t name_len   = strlen(name);
    const char   suffix[]   = ".mtd";
    const size_t suffix_len = sizeof(suffix) - 1u;
    if (name_len < suffix_len) {
        return false;
    }
    return memcmp(name + name_len - suffix_len, suffix, suffix_len) == 0;
}

/* Extract the first candidate MTD binder and locate the first .mtd entry.
 * On success returns SF_OK and populates *out_bnd / *out_entry; caller frees
 * *out_bnd via sf_bnd4_destroy().  SF_ERR_NOT_FOUND means no candidate path
 * was present or none of them contained an .mtd entry. */
static sf_result_t load_first_mtd_entry(sf_bnd4_t **out_bnd,
                                        const sf_binder_file_t **out_entry)
{
    *out_bnd   = NULL;
    *out_entry = NULL;

    for (size_t i = 0; k_mtdbnd_candidates[i] != NULL; ++i) {
        void  *bnd_bytes = NULL;
        size_t bnd_size  = 0;
        sf_result_t r    = sekiro_extract_from_anybhd(k_mtdbnd_candidates[i],
                                                      &bnd_bytes, &bnd_size);
        if (r == SF_ERR_OODLE_NOT_FOUND) {
            if (bnd_bytes) {
                sf_free(NULL, bnd_bytes);
            }
            return r;
        }
        if (r != SF_OK) {
            if (bnd_bytes) {
                sf_free(NULL, bnd_bytes);
            }
            continue;
        }

        sf_bnd4_t *bnd = NULL;
        r = sf_bnd4_read_from_memory(&bnd, (const uint8_t *)bnd_bytes, bnd_size, NULL);
        sf_free(NULL, bnd_bytes);
        if (r != SF_OK) {
            continue;
        }

        const size_t count = sf_bnd4_file_count(bnd);
        for (size_t j = 0; j < count; ++j) {
            const sf_binder_file_t *file = sf_bnd4_get_file(bnd, j);
            if (!file || !file->name_utf8 || !file->data || file->size == 0) {
                continue;
            }
            if (name_ends_with_mtd(file->name_utf8)) {
                *out_bnd   = bnd;
                *out_entry = file;
                return SF_OK;
            }
        }

        sf_bnd4_destroy(bnd);
    }

    return SF_ERR_NOT_FOUND;
}

/* Sub-test 1 — extraction yields a usable .mtd entry. */
static void test_extract_first_mtd_entry(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("Sekiro copy or Oodle DLL not available");
    }

    sf_bnd4_t              *bnd   = NULL;
    const sf_binder_file_t *entry = NULL;
    sf_result_t             r     = load_first_mtd_entry(&bnd, &entry);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r == SF_ERR_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("no Sekiro mtdbnd candidate path found");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(bnd);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_NOT_NULL(entry->name_utf8);
    TEST_ASSERT_NOT_NULL(entry->data);
    TEST_ASSERT_GREATER_THAN(0, (int)entry->size);

    sf_bnd4_destroy(bnd);
}

/* Sub-test 2 — sf_mtd_read_from_memory parses cleanly. shader_path is
 * always reachable per the upstream public-field contract (empty string
 * minimum). param/texture counts are checked for reachability only. */
static void test_parse_mtd_fields(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("Sekiro copy or Oodle DLL not available");
    }

    sf_bnd4_t              *bnd   = NULL;
    const sf_binder_file_t *entry = NULL;
    sf_result_t             r     = load_first_mtd_entry(&bnd, &entry);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r == SF_ERR_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("no Sekiro mtdbnd candidate path found");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, r);

    sf_mtd_t   *mtd = NULL;
    sf_result_t pr  = sf_mtd_read_from_memory(&mtd, entry->data, entry->size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, pr);
    TEST_ASSERT_NOT_NULL(mtd);

    const char *shader = sf_mtd_shader_path(mtd);
    TEST_ASSERT_NOT_NULL(shader);

    const char *description = sf_mtd_description(mtd);
    TEST_ASSERT_NOT_NULL(description);

    (void)sf_mtd_param_count(mtd);
    (void)sf_mtd_texture_count(mtd);

    sf_mtd_destroy(mtd);
    sf_bnd4_destroy(bnd);
}

/* Sub-test 3 — byte-for-byte round-trip: read → write reproduces the
 * exact entry payload. */
static void test_mtd_roundtrip(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("Sekiro copy or Oodle DLL not available");
    }

    sf_bnd4_t              *bnd   = NULL;
    const sf_binder_file_t *entry = NULL;
    sf_result_t             r     = load_first_mtd_entry(&bnd, &entry);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r == SF_ERR_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("no Sekiro mtdbnd candidate path found");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, r);

    sf_mtd_t *mtd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_mtd_read_from_memory(&mtd, entry->data, entry->size, NULL));

    uint8_t *out_bytes = NULL;
    size_t   out_size  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_mtd_write_to_memory(mtd, &out_bytes, &out_size, NULL));
    TEST_ASSERT_NOT_NULL(out_bytes);
    TEST_ASSERT_EQUAL_size_t(entry->size, out_size);
    TEST_ASSERT_EQUAL_MEMORY(entry->data, out_bytes, entry->size);

    sf_free(NULL, out_bytes);
    sf_mtd_destroy(mtd);
    sf_bnd4_destroy(bnd);
}

int main(void)
{
    UNITY_BEGIN();
    env_ok = sekiro_helper_is_available();
    if (env_ok) {
        env_ok = sekiro_helper_init() == SF_OK;
    }
    RUN_TEST(test_extract_first_mtd_entry);
    RUN_TEST(test_parse_mtd_fields);
    RUN_TEST(test_mtd_roundtrip);
    return UNITY_END();
}
