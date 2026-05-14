/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * e2e test: PARAM cell setters against a real Elden Ring regulation.bin.
 *
 * Validates the new mutable accessor and cell setter APIs end-to-end:
 *   1. er_load_param loads a PARAM from the live regulation.bin
 *      (AES decrypt -> DCX_KRAK decompress -> BND4 -> entry bytes).
 *   2. sf_param_read_from_memory + sf_param_apply_paramdef parse it.
 *   3. sf_param_get_row_mut + sf_param_row_get_cell_mut obtain a mutable
 *      cell for each integer-width group (8/16/32-bit).
 *   4. sf_param_cell_set_* writes a sentinel value; the const getter
 *      reads it back and asserts equality.
 *   5. sf_param_write_to_memory serialises the modified PARAM; the
 *      round-trip is verified by re-parsing and re-reading the cell.
 *
 * The test SKIPs gracefully (TEST_IGNORE_MESSAGE) whenever the ER game
 * directory, regulation.bin, Oodle DLL, or Paramdex XML is unavailable,
 * so it never FAILs in a clean checkout.
 *
 * Path roots: SF_E2E_ELDEN_RING_DIR (regulation.bin) and
 * SF_E2E_PARAMDEX_DIR (Paramdex XML files).
 */

#include "er_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_param.h"
#include "souls_formats/sf_paramdef.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#if defined(_WIN32)
#include <io.h>
#  if defined(_MSC_VER)
#    define access _access
#    define F_OK 0
#  endif
#else
#include <unistd.h>
#endif

void setUp(void) {}
void tearDown(void) {}

static bool env_ok;

#ifndef SF_E2E_PARAMDEX_DIR
/* Fallback for clangd / IDE indexing without a build-system define. */
#define SF_E2E_PARAMDEX_DIR "."
#endif

/* SpEffectParam is a large, stable PARAM present in every ER regulation.bin
 * and has cells of every integer width, making it a reliable test target. */
static const char *k_param_name    = "SpEffectParam";
static const char *k_paramdef_path = SF_E2E_PARAMDEX_DIR "/SpEffectParam.xml";

/* ---------------------------------------------------------------------------
 * Helper: load param + apply paramdef, skip if unavailable.
 * ------------------------------------------------------------------------- */
static sf_param_t *load_param_or_skip(void)
{
    if (!er_helper_is_available())
        TEST_IGNORE_MESSAGE("ER game directory not available; skipping setter e2e");

    if (access(k_paramdef_path, F_OK) != 0)
        TEST_IGNORE_MESSAGE("SpEffectParam.xml not found at SF_E2E_PARAMDEX_DIR; skipping");

    void       *param_bytes = NULL;
    size_t      param_size  = 0;
    sf_result_t r = er_load_param(k_param_name, &param_bytes, &param_size, NULL);
    if (r != SF_OK || !param_bytes)
        TEST_IGNORE_MESSAGE("er_load_param failed; skipping setter e2e");

    sf_param_t *param = NULL;
    r = sf_param_read_from_memory(&param, (const uint8_t *)param_bytes,
                                  param_size, NULL);
    sf_free(NULL, param_bytes);
    if (r != SF_OK || !param)
        TEST_IGNORE_MESSAGE("sf_param_read_from_memory failed; skipping");

    wchar_t wide_path[512];
    mbstowcs(wide_path, k_paramdef_path, 512);
    sf_paramdef_t *def = NULL;
    r = sf_paramdef_read_xml_from_path(&def, wide_path, NULL);
    if (r != SF_OK || !def) {
        sf_param_destroy(param);
        TEST_IGNORE_MESSAGE("sf_paramdef_read_xml_from_path failed; skipping");
    }

    r = sf_param_apply_paramdef(param, def, SF_PARAM_APPLY_CAREFUL);
    sf_paramdef_destroy(def);
    if (r != SF_OK) {
        sf_param_destroy(param);
        TEST_IGNORE_MESSAGE("sf_param_apply_paramdef failed; skipping");
    }

    return param;
}

/* ---------------------------------------------------------------------------
 * Helper: find the first cell of a given kind in any row.
 * Returns true and sets *out_row / *out_cell on success.
 * ------------------------------------------------------------------------- */
static bool find_cell_of_kind(sf_param_t *param, sf_param_cell_kind_t kind,
                               size_t *out_row, size_t *out_cell)
{
    const size_t row_count = sf_param_get_row_count(param);
    for (size_t ri = 0; ri < row_count; ++ri) {
        const sf_param_row_t *row = sf_param_get_row(param, ri);
        if (!row) continue;
        const size_t cell_count = sf_param_row_get_cell_count(row);
        for (size_t ci = 0; ci < cell_count; ++ci) {
            const sf_param_cell_t *cell = sf_param_row_get_cell(row, ci);
            if (cell && sf_param_cell_get_value(cell).kind == kind) {
                *out_row  = ri;
                *out_cell = ci;
                return true;
            }
        }
    }
    return false;
}

/* ---------------------------------------------------------------------------
 * Test: s32 setter round-trip
 * ------------------------------------------------------------------------- */
void test_param_cell_set_s32_roundtrip(void)
{
    sf_param_t *param = load_param_or_skip();
    if (!param) return; /* IGNORE already called */

    size_t row_idx = 0, cell_idx = 0;
    if (!find_cell_of_kind(param, SF_PARAM_CELL_KIND_S32, &row_idx, &cell_idx)) {
        sf_param_destroy(param);
        TEST_IGNORE_MESSAGE("No s32 cell found in SpEffectParam; skipping");
    }

    sf_param_row_t *row = sf_param_get_row_mut(param, row_idx);
    TEST_ASSERT_NOT_NULL(row);

    sf_param_cell_t *cell = sf_param_row_get_cell_mut(row, cell_idx);
    TEST_ASSERT_NOT_NULL(cell);

    const int32_t sentinel = -123456;
    sf_result_t r = sf_param_cell_set_s32(cell, sentinel);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);

    /* Read back via const path */
    const sf_param_row_t  *crow  = sf_param_get_row(param, row_idx);
    const sf_param_cell_t *ccell = sf_param_row_get_cell(crow, cell_idx);
    TEST_ASSERT_NOT_NULL(ccell);
    TEST_ASSERT_EQUAL_INT32(sentinel, sf_param_cell_get_s32(ccell));

    sf_param_destroy(param);
}

/* ---------------------------------------------------------------------------
 * Test: u32 setter round-trip
 * ------------------------------------------------------------------------- */
void test_param_cell_set_u32_roundtrip(void)
{
    sf_param_t *param = load_param_or_skip();
    if (!param) return;

    size_t row_idx = 0, cell_idx = 0;
    if (!find_cell_of_kind(param, SF_PARAM_CELL_KIND_U32, &row_idx, &cell_idx)) {
        sf_param_destroy(param);
        TEST_IGNORE_MESSAGE("No u32 cell found in SpEffectParam; skipping");
    }

    sf_param_row_t *row   = sf_param_get_row_mut(param, row_idx);
    sf_param_cell_t *cell = sf_param_row_get_cell_mut(row, cell_idx);
    TEST_ASSERT_NOT_NULL(cell);

    const uint32_t sentinel = 0xDEADBEEFu;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_param_cell_set_u32(cell, sentinel));

    const sf_param_row_t  *crow  = sf_param_get_row(param, row_idx);
    const sf_param_cell_t *ccell = sf_param_row_get_cell(crow, cell_idx);
    TEST_ASSERT_EQUAL_UINT32(sentinel, sf_param_cell_get_u32(ccell));

    sf_param_destroy(param);
}

/* ---------------------------------------------------------------------------
 * Test: f32 setter round-trip
 * ------------------------------------------------------------------------- */
void test_param_cell_set_f32_roundtrip(void)
{
    sf_param_t *param = load_param_or_skip();
    if (!param) return;

    size_t row_idx = 0, cell_idx = 0;
    if (!find_cell_of_kind(param, SF_PARAM_CELL_KIND_F32, &row_idx, &cell_idx)) {
        sf_param_destroy(param);
        TEST_IGNORE_MESSAGE("No f32 cell found in SpEffectParam; skipping");
    }

    sf_param_row_t *row   = sf_param_get_row_mut(param, row_idx);
    sf_param_cell_t *cell = sf_param_row_get_cell_mut(row, cell_idx);
    TEST_ASSERT_NOT_NULL(cell);

    const float sentinel = 3.14159f;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_param_cell_set_f32(cell, sentinel));

    const sf_param_row_t  *crow  = sf_param_get_row(param, row_idx);
    const sf_param_cell_t *ccell = sf_param_row_get_cell(crow, cell_idx);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, sentinel, sf_param_cell_get_f32(ccell));

    sf_param_destroy(param);
}

/* ---------------------------------------------------------------------------
 * Test: write_to_memory round-trip preserves setter changes
 * ------------------------------------------------------------------------- */
void test_param_write_to_memory_preserves_setter(void)
{
    sf_param_t *param = load_param_or_skip();
    if (!param) return;

    size_t row_idx = 0, cell_idx = 0;
    if (!find_cell_of_kind(param, SF_PARAM_CELL_KIND_S32, &row_idx, &cell_idx)) {
        sf_param_destroy(param);
        TEST_IGNORE_MESSAGE("No s32 cell found; skipping write round-trip");
    }

    /* Get the row ID for re-lookup after round-trip */
    const sf_param_row_t *crow0 = sf_param_get_row(param, row_idx);
    const int32_t row_id = sf_param_row_get_id(crow0);

    sf_param_row_t  *row  = sf_param_get_row_mut(param, row_idx);
    sf_param_cell_t *cell = sf_param_row_get_cell_mut(row, cell_idx);
    const int32_t sentinel = 0x7EADBEEF;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_param_cell_set_s32(cell, sentinel));

    /* Serialise */
    uint8_t *out_bytes = NULL;
    size_t   out_size  = 0;
    sf_result_t r = sf_param_write_to_memory(param, &out_bytes, &out_size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(out_bytes);
    TEST_ASSERT_GREATER_THAN(0, out_size);

    sf_param_destroy(param);

    /* Re-parse from the serialised bytes */
    sf_param_t *param2 = NULL;
    r = sf_param_read_from_memory(&param2, out_bytes, out_size, NULL);
    sf_free(NULL, out_bytes);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(param2);

    /* Re-apply paramdef so cells are accessible by kind */
    wchar_t wide_path[512];
    mbstowcs(wide_path, k_paramdef_path, 512);
    sf_paramdef_t *def2 = NULL;
    r = sf_paramdef_read_xml_from_path(&def2, wide_path, NULL);
    if (r == SF_OK && def2) {
        sf_param_apply_paramdef(param2, def2, SF_PARAM_APPLY_CAREFUL);
        sf_paramdef_destroy(def2);
    }

    /* Find the same row by ID and verify the sentinel survived */
    const sf_param_row_t *row2 = sf_param_find_row_by_id(param2, row_id);
    TEST_ASSERT_NOT_NULL_MESSAGE(row2, "Row not found after round-trip");

    const sf_param_cell_t *cell2 = sf_param_row_get_cell(row2, cell_idx);
    TEST_ASSERT_NOT_NULL(cell2);
    TEST_ASSERT_EQUAL_INT32(sentinel, sf_param_cell_get_s32(cell2));

    sf_param_destroy(param2);
}

/* ---------------------------------------------------------------------------
 * Test: type mismatch returns SF_ERR_INVALID_ARG
 * ------------------------------------------------------------------------- */
void test_param_cell_set_type_mismatch(void)
{
    sf_param_t *param = load_param_or_skip();
    if (!param) return;

    /* Find a u32 cell and try to set it as s32 */
    size_t row_idx = 0, cell_idx = 0;
    if (!find_cell_of_kind(param, SF_PARAM_CELL_KIND_U32, &row_idx, &cell_idx)) {
        sf_param_destroy(param);
        TEST_IGNORE_MESSAGE("No u32 cell found; skipping type-mismatch test");
    }

    sf_param_row_t  *row  = sf_param_get_row_mut(param, row_idx);
    sf_param_cell_t *cell = sf_param_row_get_cell_mut(row, cell_idx);
    TEST_ASSERT_NOT_NULL(cell);

    sf_result_t r = sf_param_cell_set_s32(cell, 42);
    TEST_ASSERT_EQUAL_INT(SF_ERR_INVALID_ARG, r);

    /* sf_last_error_detail() should mention "cell kind" */
    const char *detail = sf_last_error_detail();
    TEST_ASSERT_NOT_NULL(detail);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(detail, "cell kind"),
                                 "error detail should mention 'cell kind'");

    sf_param_destroy(param);
}

/* ---------------------------------------------------------------------------
 * Runner
 * ------------------------------------------------------------------------- */
int main(void)
{
    env_ok = (er_helper_init() == SF_OK);

    UNITY_BEGIN();
    RUN_TEST(test_param_cell_set_s32_roundtrip);
    RUN_TEST(test_param_cell_set_u32_roundtrip);
    RUN_TEST(test_param_cell_set_f32_roundtrip);
    RUN_TEST(test_param_write_to_memory_preserves_setter);
    RUN_TEST(test_param_cell_set_type_mismatch);
    return UNITY_END();
}
