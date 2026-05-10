/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T18 — Phase-3 e2e: TPF nested inside an ER chrbnd BND4.
 *
 * The deepest layered scenario: BHD5 → DCX_KRAK → BND4 → TPF → DDS magic.
 * Walks `/chr/c0000.chrbnd.dcx`, scans for a `.tpf`-named entry, parses it
 * via sf_tpf_read_from_memory, and asserts that the first texture's bytes
 * begin with the DDS magic ("DDS ").
 *
 * SKIPs gracefully when ER copy / Oodle DLL is missing.
 */

#include "er_test_helper.h"

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_tpf.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static bool env_ok;

static const char *k_chrbnd_path = "/chr/c0000.chrbnd.dcx";

static bool name_has_tpf_suffix(const char *name)
{
    if (!name) return false;
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    return strcmp(dot, ".tpf") == 0;
}

/* Sub-test 1 — chrbnd contains at least one TPF entry. Some chrbnds embed
 * textures via separate BXF4 references; in that case the test SKIPs. */
static void test_find_tpf_entry(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    void  *bytes = NULL;
    size_t size  = 0;
    sf_result_t r = er_extract_from_data0(k_chrbnd_path, &bytes, &size);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, r);

    sf_bnd4_t *bnd = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
                      sf_bnd4_read_from_memory(&bnd, (const uint8_t *)bytes, size, NULL));

    bool         found       = false;
    const size_t entry_count = sf_bnd4_file_count(bnd);
    for (size_t i = 0; i < entry_count; i++) {
        const sf_binder_file_t *f = sf_bnd4_get_file(bnd, i);
        if (f && name_has_tpf_suffix(f->name_utf8)) {
            found = true;
            break;
        }
    }
    sf_bnd4_destroy(bnd);
    sf_free(NULL, bytes);

    if (!found) {
        TEST_IGNORE_MESSAGE("no .tpf entry inside c0000.chrbnd in this ER version");
    }
}

/* Sub-test 2 — parse the .tpf entry and confirm at least one texture. */
static void test_tpf_has_textures(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    void  *bytes = NULL;
    size_t size  = 0;
    sf_result_t r = er_extract_from_data0(k_chrbnd_path, &bytes, &size);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, r);

    sf_bnd4_t *bnd = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
                      sf_bnd4_read_from_memory(&bnd, (const uint8_t *)bytes, size, NULL));

    const sf_binder_file_t *tpf_entry  = NULL;
    const size_t            entry_count = sf_bnd4_file_count(bnd);
    for (size_t i = 0; i < entry_count; i++) {
        const sf_binder_file_t *f = sf_bnd4_get_file(bnd, i);
        if (f && name_has_tpf_suffix(f->name_utf8)) {
            tpf_entry = f;
            break;
        }
    }
    if (!tpf_entry) {
        sf_bnd4_destroy(bnd);
        sf_free(NULL, bytes);
        TEST_IGNORE_MESSAGE("no .tpf entry inside c0000.chrbnd in this ER version");
    }

    sf_tpf_t *tpf = NULL;
    sf_result_t pr =
        sf_tpf_read_from_memory(&tpf, tpf_entry->data, tpf_entry->size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, pr);
    TEST_ASSERT_NOT_NULL(tpf);
    TEST_ASSERT_GREATER_THAN(0, (int)sf_tpf_texture_count(tpf));

    sf_tpf_destroy(tpf);
    sf_bnd4_destroy(bnd);
    sf_free(NULL, bytes);
}

/* Sub-test 3 — first texture has a non-empty payload. */
static void test_first_texture_has_bytes(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    void  *bytes = NULL;
    size_t size  = 0;
    sf_result_t r = er_extract_from_data0(k_chrbnd_path, &bytes, &size);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, r);

    sf_bnd4_t *bnd = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
                      sf_bnd4_read_from_memory(&bnd, (const uint8_t *)bytes, size, NULL));

    const sf_binder_file_t *tpf_entry   = NULL;
    const size_t            entry_count = sf_bnd4_file_count(bnd);
    for (size_t i = 0; i < entry_count; i++) {
        const sf_binder_file_t *f = sf_bnd4_get_file(bnd, i);
        if (f && name_has_tpf_suffix(f->name_utf8)) {
            tpf_entry = f;
            break;
        }
    }
    if (!tpf_entry) {
        sf_bnd4_destroy(bnd);
        sf_free(NULL, bytes);
        TEST_IGNORE_MESSAGE("no .tpf entry inside c0000.chrbnd in this ER version");
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
    sf_bnd4_destroy(bnd);
    sf_free(NULL, bytes);
}

/* Sub-test 4 — first texture's bytes start with DDS magic ("DDS "). For
 * PC TPFs the texture bytes are stored verbatim as a DDS file (the helper
 * auto-decompresses any per-texture DCP_EDGE wrap when flags1 == 2|3). */
static void test_first_texture_is_dds(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    void  *bytes = NULL;
    size_t size  = 0;
    sf_result_t r = er_extract_from_data0(k_chrbnd_path, &bytes, &size);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, r);

    sf_bnd4_t *bnd = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
                      sf_bnd4_read_from_memory(&bnd, (const uint8_t *)bytes, size, NULL));

    const sf_binder_file_t *tpf_entry   = NULL;
    const size_t            entry_count = sf_bnd4_file_count(bnd);
    for (size_t i = 0; i < entry_count; i++) {
        const sf_binder_file_t *f = sf_bnd4_get_file(bnd, i);
        if (f && name_has_tpf_suffix(f->name_utf8)) {
            tpf_entry = f;
            break;
        }
    }
    if (!tpf_entry) {
        sf_bnd4_destroy(bnd);
        sf_free(NULL, bytes);
        TEST_IGNORE_MESSAGE("no .tpf entry inside c0000.chrbnd in this ER version");
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
    sf_bnd4_destroy(bnd);
    sf_free(NULL, bytes);
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
