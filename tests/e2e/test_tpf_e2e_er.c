/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T18 — Phase-3 e2e: TPF nested inside an ER split archive.
 *
 * The layered scenario is BHD5 → DCX_KRAK → BXF4 → TPF → DDS magic.
 * The test scans several known Data0 split-archive candidates, locates the
 * first `.tpf` entry inside a parsed BXF4, and then validates the texture
 * payload.
 *
 * SKIPs gracefully when ER copy / Oodle DLL is missing or no candidate is
 * present in this ER build.
 */

#include "er_test_helper.h"

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bxf4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_tpf.h"

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

static sf_result_t load_first_tpf_entry(sf_bxf4_t **out_bxf,
                                        const sf_binder_file_t **out_tpf_entry)
{
    *out_bxf       = NULL;
    *out_tpf_entry = NULL;

    for (size_t i = 0; k_tpfbhd_candidates[i] != NULL; ++i) {
        const char *bhd_path = k_tpfbhd_candidates[i];

        void  *bhd_bytes = NULL;
        size_t bhd_size  = 0;
        sf_result_t r    = er_extract_from_data0(bhd_path, &bhd_bytes, &bhd_size);
        if (r == SF_ERR_OODLE_NOT_FOUND) {
            if (bhd_bytes != NULL) {
                sf_free(NULL, bhd_bytes);
            }
            return r;
        }
        if (r != SF_OK) {
            if (bhd_bytes != NULL) {
                sf_free(NULL, bhd_bytes);
            }
            continue;
        }

        char *bdt_path = tpfbhd_to_tpfbdt(bhd_path);
        if (!bdt_path) {
            sf_free(NULL, bhd_bytes);
            continue;
        }

        void  *bdt_bytes = NULL;
        size_t bdt_size  = 0;
        sf_result_t br    = er_extract_from_data0(bdt_path, &bdt_bytes, &bdt_size);
        free(bdt_path);
        if (br != SF_OK) {
            sf_free(NULL, bhd_bytes);
            if (bdt_bytes != NULL) {
                sf_free(NULL, bdt_bytes);
            }
            continue;
        }

        sf_bxf4_t  *bxf = NULL;
        sf_result_t pr  = sf_bxf4_read_from_memory(&bxf, (const uint8_t *)bhd_bytes,
                                                   bhd_size, (const uint8_t *)bdt_bytes,
                                                   bdt_size, NULL);
        sf_free(NULL, bhd_bytes);
        sf_free(NULL, bdt_bytes);
        if (pr != SF_OK || bxf == NULL) {
            if (bxf != NULL) {
                sf_bxf4_destroy(bxf);
            }
            continue;
        }

        const size_t entry_count = sf_bxf4_file_count(bxf);
        for (size_t j = 0; j < entry_count; ++j) {
            const sf_binder_file_t *file = sf_bxf4_get_file(bxf, j);
            if (file != NULL && file->name_utf8 != NULL) {
                const char *dot = strrchr(file->name_utf8, '.');
                if (dot != NULL && strcmp(dot, ".tpf") == 0) {
                    *out_bxf       = bxf;
                    *out_tpf_entry = file;
                    return SF_OK;
                }
            }
        }

        sf_bxf4_destroy(bxf);
    }

    return SF_ERR_NOT_FOUND;
}

/* Sub-test 1 — locate a candidate that actually contains a TPF entry. */
static void test_find_tpf_entry(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    sf_bxf4_t *bxf = NULL;
    const sf_binder_file_t *tpf_entry = NULL;
    sf_result_t r = load_first_tpf_entry(&bxf, &tpf_entry);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("no TPF-containing BXF4 candidate found in Data0");
    }

    TEST_ASSERT_NOT_NULL(bxf);
    TEST_ASSERT_NOT_NULL(tpf_entry);
    TEST_ASSERT_TRUE(tpf_entry->name_utf8 != NULL);
    TEST_ASSERT_NOT_NULL(strrchr(tpf_entry->name_utf8, '.'));

    sf_bxf4_destroy(bxf);
}

/* Sub-test 2 — parse the .tpf entry and confirm at least one texture. */
static void test_tpf_has_textures(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    sf_bxf4_t *bxf = NULL;
    const sf_binder_file_t *tpf_entry = NULL;
    sf_result_t r = load_first_tpf_entry(&bxf, &tpf_entry);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("no TPF-containing BXF4 candidate found in Data0");
    }

    sf_tpf_t *tpf = NULL;
    sf_result_t pr = sf_tpf_read_from_memory(&tpf, tpf_entry->data, tpf_entry->size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, pr);
    TEST_ASSERT_NOT_NULL(tpf);
    TEST_ASSERT_GREATER_THAN(0, (int)sf_tpf_texture_count(tpf));

    sf_tpf_destroy(tpf);
    sf_bxf4_destroy(bxf);
}

/* Sub-test 3 — first texture has a non-empty payload. */
static void test_first_texture_has_bytes(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    sf_bxf4_t *bxf = NULL;
    const sf_binder_file_t *tpf_entry = NULL;
    sf_result_t r = load_first_tpf_entry(&bxf, &tpf_entry);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("no TPF-containing BXF4 candidate found in Data0");
    }

    sf_tpf_t *tpf = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
                      sf_tpf_read_from_memory(&tpf, tpf_entry->data, tpf_entry->size, NULL));
    TEST_ASSERT_GREATER_THAN(0, (int)sf_tpf_texture_count(tpf));

    const sf_tpf_texture_t *first = sf_tpf_get_texture(tpf, 0);
    TEST_ASSERT_NOT_NULL(first);

    size_t         tex_size = 0;
    const uint8_t *tex      = sf_tpf_texture_get_bytes(first, &tex_size);
    TEST_ASSERT_NOT_NULL(tex);
    TEST_ASSERT_GREATER_THAN(0, (int)tex_size);

    sf_tpf_destroy(tpf);
    sf_bxf4_destroy(bxf);
}

/* Sub-test 4 — first texture's bytes start with DDS magic ("DDS "). */
static void test_first_texture_is_dds(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    sf_bxf4_t *bxf = NULL;
    const sf_binder_file_t *tpf_entry = NULL;
    sf_result_t r = load_first_tpf_entry(&bxf, &tpf_entry);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("no TPF-containing BXF4 candidate found in Data0");
    }

    sf_tpf_t *tpf = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
                      sf_tpf_read_from_memory(&tpf, tpf_entry->data, tpf_entry->size, NULL));
    TEST_ASSERT_GREATER_THAN(0, (int)sf_tpf_texture_count(tpf));

    const sf_tpf_texture_t *first = sf_tpf_get_texture(tpf, 0);
    TEST_ASSERT_NOT_NULL(first);

    size_t         tex_size = 0;
    const uint8_t *tex      = sf_tpf_texture_get_bytes(first, &tex_size);
    TEST_ASSERT_NOT_NULL(tex);
    TEST_ASSERT_GREATER_OR_EQUAL_size_t((size_t)4, tex_size);

    TEST_ASSERT_EQUAL_UINT8('D', tex[0]);
    TEST_ASSERT_EQUAL_UINT8('D', tex[1]);
    TEST_ASSERT_EQUAL_UINT8('S', tex[2]);
    TEST_ASSERT_EQUAL_UINT8(' ', tex[3]);

    sf_tpf_destroy(tpf);
    sf_bxf4_destroy(bxf);
}

int main(void)
{
    UNITY_BEGIN();
    env_ok = er_helper_is_available();
    if (env_ok) {
        env_ok = er_helper_init() == SF_OK;
    }
    RUN_TEST(test_find_tpf_entry);
    RUN_TEST(test_tpf_has_textures);
    RUN_TEST(test_first_texture_has_bytes);
    RUN_TEST(test_first_texture_is_dds);
    return UNITY_END();
}
