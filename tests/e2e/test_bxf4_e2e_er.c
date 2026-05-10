/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T17 — Phase-3 e2e: BXF4 against ER `.tpfbhd` / `.tpfbdt` pairs.
 *
 * The exact set of split archives shipped inside ER's Data0 varies between
 * patches, so we attempt several known candidate paths and SKIP if none
 * are present. For every candidate that resolves we extract the matching
 * `.tpfbdt` companion (same path with a swapped extension) and parse the
 * pair via sf_bxf4_read_from_memory.
 *
 * SKIPs gracefully when the ER copy / Oodle DLL is missing or no candidate
 * pair is present in this build of Data0.
 */

#include "er_test_helper.h"

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bxf4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static bool env_ok;

static const char *const k_tpfbhd_candidates[] = {
    "/parts/wp_a_0010.tpfbhd",
    "/parts/common_body.tpfbhd",
    "/asset/aeg/aeg007/aeg007_002.tpfbhd",
    "/asset/aeg/aeg099/aeg099_010.tpfbhd",
    "/parts/am_m_2000.tpfbhd",
    NULL,
};

static char *tpfbhd_to_tpfbdt(const char *in)
{
    const size_t len = strlen(in);
    const char  *suf = ".tpfbhd";
    const size_t sl  = strlen(suf);
    if (len < sl || strcmp(in + len - sl, suf) != 0) {
        return NULL;
    }
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, in, len + 1);
    memcpy(out + len - sl, ".tpfbdt", sl);
    return out;
}

/* Attempts each candidate. On the first successful pair the BXF4 magic
 * "BHF4" must appear at the head of the BHD bytes. Returns true if a
 * candidate was processed; SKIP otherwise. */
static void test_bxf4_candidate_scan(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    bool   found      = false;
    int    attempts   = 0;
    size_t found_size = 0;

    for (size_t i = 0; k_tpfbhd_candidates[i] != NULL; i++) {
        const char *bhd_path = k_tpfbhd_candidates[i];
        attempts++;

        void  *bhd_bytes = NULL;
        size_t bhd_size  = 0;
        sf_result_t r =
            er_extract_from_data0(bhd_path, &bhd_bytes, &bhd_size);
        if (r == SF_ERR_OODLE_NOT_FOUND) {
            TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
        }
        if (r != SF_OK) {
            continue;
        }
        if (bhd_size < 4) {
            sf_free(NULL, bhd_bytes);
            continue;
        }
        const uint8_t *p = (const uint8_t *)bhd_bytes;
        if (!(p[0] == 'B' && p[1] == 'H' && p[2] == 'F' && p[3] == '4')) {
            sf_free(NULL, bhd_bytes);
            continue;
        }

        char *bdt_path = tpfbhd_to_tpfbdt(bhd_path);
        TEST_ASSERT_NOT_NULL(bdt_path);

        void  *bdt_bytes = NULL;
        size_t bdt_size  = 0;
        sf_result_t br =
            er_extract_from_data0(bdt_path, &bdt_bytes, &bdt_size);
        free(bdt_path);
        if (br != SF_OK) {
            sf_free(NULL, bhd_bytes);
            continue;
        }

        sf_bxf4_t  *bxf = NULL;
        sf_result_t pr  = sf_bxf4_read_from_memory(&bxf,
                                                   (const uint8_t *)bhd_bytes, bhd_size,
                                                   (const uint8_t *)bdt_bytes, bdt_size,
                                                   NULL);
        sf_free(NULL, bhd_bytes);
        sf_free(NULL, bdt_bytes);

        TEST_ASSERT_EQUAL_INT(SF_OK, pr);
        TEST_ASSERT_NOT_NULL(bxf);
        const size_t fc = sf_bxf4_file_count(bxf);
        TEST_ASSERT_GREATER_THAN(0, (int)fc);
        found      = true;
        found_size = fc;
        sf_bxf4_destroy(bxf);
        break;
    }

    if (!found) {
        TEST_ASSERT_GREATER_OR_EQUAL_INT(2, attempts);
        TEST_IGNORE_MESSAGE("no .tpfbhd/.tpfbdt pair found in this ER version");
    }
    TEST_ASSERT_GREATER_THAN(0, (int)found_size);
}

/* Pairs that succeeded in the candidate scan must also expose at least
 * one entry whose payload contains a positive number of bytes. We re-run
 * the scan instead of caching a global handle so each Unity sub-test stays
 * independent (matches the surrounding tests' style). */
static void test_bxf4_entry_has_data(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    for (size_t i = 0; k_tpfbhd_candidates[i] != NULL; i++) {
        const char *bhd_path = k_tpfbhd_candidates[i];

        void  *bhd_bytes = NULL;
        size_t bhd_size  = 0;
        sf_result_t r =
            er_extract_from_data0(bhd_path, &bhd_bytes, &bhd_size);
        if (r == SF_ERR_OODLE_NOT_FOUND) {
            TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
        }
        if (r != SF_OK || bhd_size < 4) {
            if (r == SF_OK) sf_free(NULL, bhd_bytes);
            continue;
        }
        const uint8_t *p = (const uint8_t *)bhd_bytes;
        if (!(p[0] == 'B' && p[1] == 'H' && p[2] == 'F' && p[3] == '4')) {
            sf_free(NULL, bhd_bytes);
            continue;
        }

        char *bdt_path = tpfbhd_to_tpfbdt(bhd_path);
        TEST_ASSERT_NOT_NULL(bdt_path);
        void  *bdt_bytes = NULL;
        size_t bdt_size  = 0;
        sf_result_t br = er_extract_from_data0(bdt_path, &bdt_bytes, &bdt_size);
        free(bdt_path);
        if (br != SF_OK) {
            sf_free(NULL, bhd_bytes);
            continue;
        }

        sf_bxf4_t  *bxf = NULL;
        sf_result_t pr  = sf_bxf4_read_from_memory(&bxf,
                                                   (const uint8_t *)bhd_bytes, bhd_size,
                                                   (const uint8_t *)bdt_bytes, bdt_size,
                                                   NULL);
        sf_free(NULL, bhd_bytes);
        sf_free(NULL, bdt_bytes);
        TEST_ASSERT_EQUAL_INT(SF_OK, pr);

        const size_t fc = sf_bxf4_file_count(bxf);
        TEST_ASSERT_GREATER_THAN(0, (int)fc);

        bool any_positive = false;
        for (size_t k = 0; k < fc; k++) {
            const sf_binder_file_t *f = sf_bxf4_get_file(bxf, k);
            if (f && f->size > 0) { any_positive = true; break; }
        }
        sf_bxf4_destroy(bxf);
        TEST_ASSERT_TRUE(any_positive);
        return;
    }

    TEST_IGNORE_MESSAGE("no .tpfbhd/.tpfbdt pair found in this ER version");
}

/* Sanity: confirm the candidate list itself is well-formed. Always runs;
 * does not depend on env_ok. */
static void test_candidate_list_terminated(void)
{
    int count = 0;
    while (k_tpfbhd_candidates[count] != NULL) count++;
    TEST_ASSERT_GREATER_OR_EQUAL_INT(2, count);
}

int main(void)
{
    UNITY_BEGIN();
    env_ok = er_helper_is_available();
    if (env_ok) {
        env_ok = er_helper_init() == SF_OK;
    }
    RUN_TEST(test_candidate_list_terminated);
    RUN_TEST(test_bxf4_candidate_scan);
    RUN_TEST(test_bxf4_entry_has_data);
    return UNITY_END();
}
