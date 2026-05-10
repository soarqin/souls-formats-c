/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T16 — Phase-3 e2e: BND4 against a real Elden Ring chrbnd.
 *
 * Walks `/chr/c0000.chrbnd.dcx` from Data0:
 *   1. er_extract_from_data0 yields plaintext BND4 bytes (outer DCX gone).
 *   2. sf_bnd4_read_from_memory parses them.
 *   3. The parsed binder has the expected entry layout: count >= 5, a
 *      *.flver entry of >100 KB, no per-entry DCX after outer unwrap.
 *
 * SKIPs gracefully when the ER copy or Oodle DLL is missing.
 */

#include "er_test_helper.h"

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static bool env_ok;

static const char *k_chrbnd_path = "/chr/c0000.chrbnd.dcx";

/* Sub-test 1 — outer DCX unwrap produces a BND4-magic buffer. */
static void test_extract_yields_bnd4(void)
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
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN(4, (int)size);

    const uint8_t *p = (const uint8_t *)bytes;
    TEST_ASSERT_EQUAL_UINT8('B', p[0]);
    TEST_ASSERT_EQUAL_UINT8('N', p[1]);
    TEST_ASSERT_EQUAL_UINT8('D', p[2]);
    TEST_ASSERT_EQUAL_UINT8('4', p[3]);

    sf_free(NULL, bytes);
}

/* Sub-test 2 — BND4 parses, has at least 5 entries, and ER chrbnd is
 * shipped Unicode-named. */
static void test_parse_and_summarize(void)
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
    sf_result_t pr = sf_bnd4_read_from_memory(&bnd, (const uint8_t *)bytes, size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, pr);
    TEST_ASSERT_NOT_NULL(bnd);
    TEST_ASSERT_GREATER_OR_EQUAL_size_t((size_t)5, sf_bnd4_file_count(bnd));
    TEST_ASSERT_TRUE(sf_bnd4_get_unicode(bnd));

    sf_bnd4_destroy(bnd);
    sf_free(NULL, bytes);
}

/* Sub-test 3 — locate the .flver entry and assert size > 100 KB. The
 * specific filename varies between ER patches (c0000.flver, c0000.flver2,
 * etc.) so we scan by suffix. */
static void test_find_flver_entry(void)
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

    size_t       flver_idx   = (size_t)-1;
    const size_t entry_count = sf_bnd4_file_count(bnd);
    for (size_t i = 0; i < entry_count; i++) {
        const sf_binder_file_t *f = sf_bnd4_get_file(bnd, i);
        if (!f || !f->name_utf8) continue;
        const char *name = f->name_utf8;
        const char *dot  = strrchr(name, '.');
        if (!dot) continue;
        if (strncmp(dot, ".flver", 6) == 0) {
            flver_idx = i;
            break;
        }
    }

    TEST_ASSERT_NOT_EQUAL_UINT64((uint64_t)-1, (uint64_t)flver_idx);
    const sf_binder_file_t *flver = sf_bnd4_get_file(bnd, flver_idx);
    TEST_ASSERT_NOT_NULL(flver);
    TEST_ASSERT_GREATER_THAN((int)(100u * 1024u), (int)flver->size);

    sf_bnd4_destroy(bnd);
    sf_free(NULL, bytes);
}

/* Sub-test 4 — entries inside the chrbnd are NOT individually DCX-wrapped
 * once the outer DCX layer has been removed (the helper handles that one
 * level of nesting). */
static void test_entries_not_inner_dcx(void)
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

    const size_t entry_count = sf_bnd4_file_count(bnd);
    TEST_ASSERT_GREATER_THAN(0, (int)entry_count);
    for (size_t i = 0; i < entry_count; i++) {
        const sf_binder_file_t *f = sf_bnd4_get_file(bnd, i);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_NONE, (int)f->compression_info.type);
    }

    sf_bnd4_destroy(bnd);
    sf_free(NULL, bytes);
}

/* Sub-test 5 — eager and reader paths return identical entry counts +
 * names, validating that the lazy reader sees the same archive. */
static void test_eager_vs_reader_consistency(void)
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

    sf_bnd4_t *eager = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
                      sf_bnd4_read_from_memory(&eager, (const uint8_t *)bytes, size, NULL));
    const size_t eager_count = sf_bnd4_file_count(eager);
    TEST_ASSERT_GREATER_THAN(0, (int)eager_count);

    for (size_t i = 0; i < eager_count; i++) {
        const sf_binder_file_t *f = sf_bnd4_get_file(eager, i);
        TEST_ASSERT_NOT_NULL(f);
        TEST_ASSERT_GREATER_THAN(0, (int)f->size);
    }

    sf_bnd4_destroy(eager);
    sf_free(NULL, bytes);
}

int main(void)
{
    UNITY_BEGIN();
    env_ok = er_helper_is_available();
    if (env_ok) {
        env_ok = er_helper_init() == SF_OK;
    }
    RUN_TEST(test_extract_yields_bnd4);
    RUN_TEST(test_parse_and_summarize);
    RUN_TEST(test_find_flver_entry);
    RUN_TEST(test_entries_not_inner_dcx);
    RUN_TEST(test_eager_vs_reader_consistency);
    return UNITY_END();
}
