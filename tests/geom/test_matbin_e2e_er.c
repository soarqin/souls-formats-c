/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * T27 — Phase-6 e2e: MATBIN against Elden Ring's allmaterial.matbinbnd.dcx.
 *
 * Pipeline:
 *   1. er_extract_from_data0("/material/allmaterial.matbinbnd.dcx") yields a
 *      plaintext BND4 (outer DCX_KRAK already unwrapped by the helper).
 *   2. sf_bnd4_read_from_memory parses the binder. The probe in
 *      .sisyphus/evidence/task-5-matbin-survey.txt confirms 15103 entries
 *      with the eight-variant ParamType distribution.
 *   3. The first `.matbin` entry is parsed via sf_matbin_read_from_memory.
 *      Assertions: shader_path non-empty, sampler_count > 0 (every shipping
 *      ER material binds at least one texture). param_count is not strictly
 *      > 0 in the survey, so the test only asserts the field is reachable.
 *   4. sf_matbin_write_to_memory re-serializes and compares byte-for-byte
 *      against the input entry — the writer's contract is a faithful
 *      round-trip when starting from a parsed-from-bytes MATBIN.
 *
 * SKIPs gracefully when the ER copy or Oodle DLL is missing.
 */

#include "er_test_helper.h"

#include "souls_formats/sf_bhd5.h"
#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_matbin.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static bool env_ok;

#define MATBIN_BND_PATH "/material/allmaterial.matbinbnd.dcx"

/* ER's Data0 stores allmaterial.matbinbnd.dcx under a true 64-bit path
 * hash that the production sf_path_hash_64 (zero-extended 32-bit, 37u
 * multiplier) does not yet compute. The same workaround is used by
 * tests/probes/probe_matbin_paramtypes.c — a 64-bit fold with a 133u
 * multiplier and no leading-slash prefix. Once sf_path_hash_64 is
 * extended to mirror ER's true algorithm this fallback will fall out
 * naturally (the standard path will hit first). */
static uint64_t er_path_hash_64_alt(const char *path)
{
    uint64_t h = 0;
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
        unsigned char c = (*p == '\\') ? '/' : *p;
        if (c >= 'A' && c <= 'Z') {
            c = (unsigned char)(c - 'A' + 'a');
        }
        h = (uint64_t)c + 133u * h;
    }
    return h;
}

static sf_result_t er_extract_with_fallback(const char *path,
                                            void **out, size_t *out_size)
{
    sf_result_t r = er_extract_from_data0(path, out, out_size);
    if (r == SF_OK) {
        return r;
    }
    if (r != SF_ERR_NOT_FOUND) {
        return r;
    }

    void  *raw      = NULL;
    size_t raw_size = 0;
    r = sf_bhd5_extract_by_hash_64(er_helper_get_bhd5_for_testing(),
                                   er_path_hash_64_alt(path),
                                   &raw, &raw_size, NULL);
    if (r != SF_OK) {
        return r;
    }

    sf_dcx_type_t     type    = SF_DCX_TYPE_UNKNOWN;
    const sf_result_t sniff_r = sf_dcx_sniff(raw, raw_size, &type);
    const bool        is_dcx  =
        sniff_r == SF_OK && type != SF_DCX_TYPE_NONE && type != SF_DCX_TYPE_UNKNOWN;

    if (is_dcx) {
        void         *decompressed = NULL;
        size_t        decomp_size  = 0;
        sf_dcx_type_t out_type     = SF_DCX_TYPE_UNKNOWN;
        r = sf_dcx_decompress(raw, raw_size, &decompressed, &decomp_size,
                              &out_type, NULL);
        sf_free(NULL, raw);
        if (r != SF_OK) {
            return r;
        }
        *out      = decompressed;
        *out_size = decomp_size;
    } else {
        *out      = raw;
        *out_size = raw_size;
    }
    return SF_OK;
}

static bool name_ends_with_matbin(const char *name)
{
    if (!name) {
        return false;
    }
    const size_t name_len   = strlen(name);
    const char   suffix[]   = ".matbin";
    const size_t suffix_len = sizeof(suffix) - 1u;
    if (name_len < suffix_len) {
        return false;
    }
    return memcmp(name + name_len - suffix_len, suffix, suffix_len) == 0;
}

/* Extract allmaterial.matbinbnd.dcx, parse the outer BND4, and find the
 * first .matbin entry.  On success returns SF_OK and populates the out
 * parameters; caller frees *out_bnd via sf_bnd4_destroy().  SF_ERR_NOT_FOUND
 * means the archive did exist but did not contain any .matbin entries
 * (extremely unlikely; would indicate a corrupted ER install). */
static sf_result_t load_first_matbin_entry(sf_bnd4_t **out_bnd,
                                           const sf_binder_file_t **out_entry)
{
    *out_bnd   = NULL;
    *out_entry = NULL;

    void  *bnd_bytes = NULL;
    size_t bnd_size  = 0;
    sf_result_t r    = er_extract_with_fallback(MATBIN_BND_PATH, &bnd_bytes, &bnd_size);
    if (r != SF_OK) {
        if (bnd_bytes != NULL) {
            sf_free(NULL, bnd_bytes);
        }
        return r;
    }

    sf_bnd4_t *bnd = NULL;
    r = sf_bnd4_read_from_memory(&bnd, (const uint8_t *)bnd_bytes, bnd_size, NULL);
    sf_free(NULL, bnd_bytes);
    if (r != SF_OK) {
        return r;
    }

    const size_t count = sf_bnd4_file_count(bnd);
    for (size_t i = 0; i < count; ++i) {
        const sf_binder_file_t *file = sf_bnd4_get_file(bnd, i);
        if (!file || !file->name_utf8 || !file->data || file->size == 0) {
            continue;
        }
        if (name_ends_with_matbin(file->name_utf8)) {
            *out_bnd   = bnd;
            *out_entry = file;
            return SF_OK;
        }
    }

    sf_bnd4_destroy(bnd);
    return SF_ERR_NOT_FOUND;
}

/* Sub-test 1 — the binder extraction itself yields a usable .matbin entry. */
static void test_extract_first_matbin_entry(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    sf_bnd4_t              *bnd   = NULL;
    const sf_binder_file_t *entry = NULL;
    sf_result_t             r     = load_first_matbin_entry(&bnd, &entry);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r == SF_ERR_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("allmaterial.matbinbnd.dcx not present in this ER copy");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(bnd);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_NOT_NULL(entry->name_utf8);
    TEST_ASSERT_NOT_NULL(entry->data);
    TEST_ASSERT_GREATER_THAN(0, (int)entry->size);

    /* MATBIN magic = "MAB\0" then little-endian u32 version == 2. */
    TEST_ASSERT_GREATER_OR_EQUAL_size_t((size_t)8, entry->size);
    TEST_ASSERT_EQUAL_UINT8('M', entry->data[0]);
    TEST_ASSERT_EQUAL_UINT8('A', entry->data[1]);
    TEST_ASSERT_EQUAL_UINT8('B', entry->data[2]);
    TEST_ASSERT_EQUAL_UINT8('\0', entry->data[3]);

    sf_bnd4_destroy(bnd);
}

/* Sub-test 2 — sf_matbin_read_from_memory parses cleanly and exposes a
 * shader path and at least one sampler.  Param count is checked for
 * reachability but not >0 (the survey shows a long tail of materials
 * with all-zero params). */
static void test_parse_matbin_fields(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    sf_bnd4_t              *bnd   = NULL;
    const sf_binder_file_t *entry = NULL;
    sf_result_t             r     = load_first_matbin_entry(&bnd, &entry);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r == SF_ERR_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("allmaterial.matbinbnd.dcx not present in this ER copy");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, r);

    sf_matbin_t *mat = NULL;
    sf_result_t  pr  = sf_matbin_read_from_memory(&mat, entry->data, entry->size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, pr);
    TEST_ASSERT_NOT_NULL(mat);

    const char *shader = sf_matbin_shader_path(mat);
    TEST_ASSERT_NOT_NULL(shader);
    TEST_ASSERT_GREATER_THAN(0, (int)strlen(shader));

    const char *source = sf_matbin_source_path(mat);
    TEST_ASSERT_NOT_NULL(source);
    /* source path is allowed to be empty per upstream — only reachability matters. */

    /* Reachability check for params — value may be zero. */
    (void)sf_matbin_param_count(mat);

    /* Every shipping ER material binds at least one sampler. */
    TEST_ASSERT_GREATER_THAN(0, (int)sf_matbin_sampler_count(mat));

    sf_matbin_destroy(mat);
    sf_bnd4_destroy(bnd);
}

/* Sub-test 3 — byte-for-byte round-trip: read → write must reproduce the
 * exact entry payload.  Validates that no field is dropped, reordered, or
 * silently re-encoded by the writer. */
static void test_matbin_roundtrip(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    sf_bnd4_t              *bnd   = NULL;
    const sf_binder_file_t *entry = NULL;
    sf_result_t             r     = load_first_matbin_entry(&bnd, &entry);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r == SF_ERR_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("allmaterial.matbinbnd.dcx not present in this ER copy");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, r);

    sf_matbin_t *mat = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_matbin_read_from_memory(&mat, entry->data, entry->size, NULL));

    void   *out_bytes = NULL;
    size_t  out_size  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_matbin_write_to_memory(mat, &out_bytes, &out_size, NULL));
    TEST_ASSERT_NOT_NULL(out_bytes);
    TEST_ASSERT_EQUAL_size_t(entry->size, out_size);
    TEST_ASSERT_EQUAL_MEMORY(entry->data, out_bytes, entry->size);

    sf_free(NULL, out_bytes);
    sf_matbin_destroy(mat);
    sf_bnd4_destroy(bnd);
}

int main(void)
{
    UNITY_BEGIN();
    env_ok = er_helper_is_available();
    if (env_ok) {
        env_ok = er_helper_init() == SF_OK;
    }
    RUN_TEST(test_extract_first_matbin_entry);
    RUN_TEST(test_parse_matbin_fields);
    RUN_TEST(test_matbin_roundtrip);
    return UNITY_END();
}
